# UsrAI 技术文档（NewAOE 3.0.7h）

> 本文由我通读源码独立撰写。**没有引用项目内任何既有笔记/记忆文件**，所有结论都来自下面列出的源码，
> 并附 `文件:行号`。凡是我没有亲自读证的地方，一律进 §9.4「待验证清单」，不当作结论使用。

## 阅读范围

| 文件 | 内容 | 读法 |
|---|---|---|
| `AI.cpp` / `ai.h` | AI 线程与五个对外接口 | 全读 |
| `GlobalVariate.h` | `tagInfo`/`tagObj`/`tagBuilding`/`tagResource`/`tagHuman`/`tagFarmer`/`tagArmy`/`instruction`/`tagGame` | 全读 |
| `Core.cpp` | 主循环、`updateByPlayer`、`updateCommon`、`infoShare`、`manageOrder`、`handleFarmer/Military/BuildingAction`、`deduplicateInstructions`、`preValidateInstruction` | 关键段全读 |
| `Core_List.cpp` | 关系表（关系阶段机）、`addRelation`/`suspendRelation`、`object_Attack`、`object_Gather`、`conduct_Attacked`、`manageMontorAct`、转化实现、`relation_Event_static` | 关键段全读 |
| `Core_CondiFunc.h` | `relation_Object`、`detail_EventPhase`、条件函数清单 | 全读 |
| `Map.cpp` | `findNearestValidTerrainBlock`、视野块计算 | 关键段 |
| `enemyai.cpp` | 波次时间表/编成、波次目标优先级、守军与追击上限、祭司猎手 | 关键段 |
| `Building.cpp/h`、`Army.h`、`config.h`、`config.json` | 索敌接口、常量与数值 | 关键段 |
| `UsrAI.cpp`（2014 行）/`UsrAI.h` | 现有 AI | 全读 |

---

## 目录
1. 运行框架：AI 是怎么被跑起来的
2. AI 的数据视野（`tagInfo`）
3. 指令的真实语义
4. 经济机制
5. 战斗机制
6. 对手（EnemyAI）
7. 现有 `UsrAI.cpp` 解析
8. 改造路线
9. 附录

---

# 1. 运行框架

## 1.1 一帧的完整时序

游戏侧入口是 `MainWidget::gameDataUpdate()`（`MainWidget.cpp:2245`）：

```
Core::gameUpdate()                      // 世界推进一帧（Core.cpp:32）
├─ ++CoreExecuteFrames
├─ 清视野 / init_Map_UseToMonitor
├─ updateByObject()                     // 所有单位/建筑状态推进（生产、建造、移动、攻击…）
├─ recalculateHumanPopulation()
├─ loadRelationMap()
├─ reset_ObjectExploreAndVisible()      // 探索/可见刷新
├─ judge_Crush()                        // 碰撞
├─ manageOrder(0)                       // ★ 消费【用户AI】上一轮排队的指令
├─ manageOrder(1)                       // 消费敌方AI指令
├─ GenerateHumanLock = 0                // 每帧只允许生产 1 个人
└─ interactionList->update()            // ★ 关系阶段机：真正"执行动作"的地方

tagUsrGame.tryLock() / tagEnemyGame.tryLock()
Core::infoShare()                       // 生成两个 AI 的本帧快照（Core.cpp:678）
emit startAI()                          // 唤醒两个 AI 线程
```

AI 线程侧（`AI.cpp:53-71`）：

```
run(): 无限循环
  condition.wait(&mutex)         // 睡着
  → 被 startProcessing() 唤醒（MainWidget.cpp:2265 每帧 emit）
  → if (g_frame > 10) { processData(); }      // 读本帧快照，下指令
  → CommitInstruction()          // 把指令推进共享队列
  → 继续 wait
```

**关键结论：AI 的指令在"下一帧"才生效。**
第 N 帧：世界推进 → `manageOrder(0)` 消费的是第 N−1 帧 AI 下的令 → `infoShare()` 生成第 N 帧快照 →
AI 读第 N 帧快照下指令 → 这些指令在第 N+1 帧的 `manageOrder(0)` 才被处理。
所以 **"看到 → 生效"固定延迟一帧**，做时间敏感的配合时要算进去。

## 1.2 AI 线程的三个副作用（都是坑）

1. **会丢唤醒、会跳帧。** `run()` 用 `QMutexLocker locker(&mutex)` 把整个 `processData()` 包住
   （`AI.cpp:55-64`），而 `startProcessing()` 是 `mutex.tryLock()`（`AI.cpp:38-45`）：AI 还没跑完时，
   这一帧的唤醒会被静默丢弃。
   → **永远不要假设 `info.GameFrame` 每帧 +1**，它可能一次跳几帧。任何"按帧递增计数"的逻辑都不安全。
2. **`getInfo()` 是整份深拷贝，还带锁**（`GlobalVariate.cpp:1094-1097`）。
   一帧调一次就够，不要在每个子函数里重复 `getInfo()`。
3. `UsrAI` **绕过了** `AI::AddToIns` / `CommitInstruction`：`UsrAI::AddToIns`（`UsrAI.h:47-55`）
   直接加锁往共享队列 `UsrIns.instructions` 里塞，id 用 `UsrIns.g_id++`。
   所以 `AI::CommitInstruction()` 对 UsrAI 是空操作，**指令是"下了就进队列"**。

## 1.3 指令队列的三条硬规则（最容易踩）

`Core::manageOrder(id)`（`Core.cpp:1358-1519`）依次做四件事：

| 步骤 | 代码 | 规则 |
|---|---|---|
| ① 预校验 | `preValidateInstruction`（1241-1267） | **只针对 `INS_BUILDINGACTION`**：科技前置没满足的，在去重**之前**就剔除、写入真实错误码。 |
| ② 按 SN 去重 | `deduplicateInstructions`（1221-1237） | 用 `std::map<int,instruction>` 以 `SN` 为键 → **同一对象同帧多条指令只保留最后一条**。 |
| ③ 数量上限 | 1388-1390 | `int ObjCnt = build.size()+human.size(); while(!empty && ObjCnt--)` → **每帧最多处理"对象总数"条指令**。 |
| ④ 清空 | 1517 | `NowIns->instructions = std::queue<instruction>();` → **没被处理完的指令直接丢掉，不会留到下一帧**。 |

由此推出三条必须遵守的规则：

- **规则 A（重复无益）**：同一帧对同一对象下多条指令，只有最后一条活。
- **规则 B（预算有限）**：一帧里下的指令条数不要超过自己的对象数（`buildings.size()+farmers.size()+armies.size()`）。
  正常写法一帧几条，不会触发；但如果写"遍历全员下 N 条"的批量操作，就要留意。
- **规则 C（顺序 = SN 升序）**：去重后是按 `SN` 升序重排再执行的（`std::map` 的键序），
  **不是下达顺序**。所以"同帧多模块抢同一个单位"时，胜者是**那道指令文本上最后写下的**（规则 A），
  但"不同 SN 之间的执行先后"由 SN 决定，跟代码顺序无关。

## 1.4 返回码与 `ins_ret`

- 每条指令执行完写回 `tagAIGame->insertInsRet(cur.id, cur)`（`Core.cpp:1509`），值是 `cur.ret`。
- `tagGame::update()`（`GlobalVariate.cpp:1054-1077`）每帧把上一帧的 `ins_ret` **继承**给新快照，
  并**只保留最后 100 条**（`erase(begin())` 逐个删旧）。
- `AI::clearInsRet()` 是虚函数，全工程**没有任何地方调用**（我 grep 过全仓）→
  结果是 `ins_ret` 长期保留最近约 100 条历史结果，查一条旧指令的返回码是可行的。
- 返回值含义见 §9.2。**`0` 才是成功。**

## 1.5 所有列表的顺序每帧被打乱（必须知道）

`tagGame::update()` 末尾（`GlobalVariate.cpp:1066-1076`）对所有 7 个列表调用 `WLHHunYao()` 洗牌
（`GlobalVariate.h:252-262`，自己实现的 Fisher-Yates）：

```
Info->buildings / farmers / armies / enemy_buildings / enemy_farmers / enemy_armies / resources
```

→ **任何依赖 `info.farmers[0]`、`info.resources[i]` 顺序的逻辑，行为都是随机的。**
"挑第一个 xxx"必须在循环里显式比较（距离最优 / 优先级最高 / SN 最小）。

---

# 2. AI 的数据视野

## 2.1 `tagInfo` 字段（`GlobalVariate.h:217-242`）

| 字段 | 说明 |
|---|---|
| `buildings` / `farmers` / `armies` | 我方建筑 / 农民类 / 军队类 |
| `enemy_buildings` / `enemy_farmers` / `enemy_armies` | 敌方（**受可见性过滤，见 §2.3**） |
| `resources` | 资源、动物尸体、树木（**静态资源受探索过滤**） |
| `ins_ret` | `map<指令id, 返回码>`，保留最近约 100 条 |
| `theMap` | 地形：`(*theMap)[dr][ur].type/height`；未探索格 = `MAPPATTERN_UNKNOWN` |
| `exploredUpdate` | 本帧新探索的格子 |
| `GameFrame` | 当前帧（25 帧/秒，见 §9.1） |
| `civilizationStage` | 石器/工具/铜器/铁器 |
| `Wood/Meat/Stone/Gold` | 四种资源 |
| `Human_Num` / `Human_MaxNum` | 当前人口（可为 0.5 的倍数）/ 人口上限 |

**人口上限的算法**：`Development::getMaxHumanNum() = get_homeNum() * HOUSE_HUMAN_NUM`（`Development.h:63`），
`HOUSE_HUMAN_NUM = 4`（`config.json:49`）→ **上限 = 房子数 × 4**。
开局只有 2 栋房 → 上限 8。这是"20 村民 + 十几个复合弓"路线最硬的约束。

## 2.2 对象结构体要点

- `tagObj`：`SN`、`BlockDR/UR`（块坐标）。
- `tagBuilding`：`Type`、`Blood/MaxBlood`、`Percent`（建造完成度）、`Project`、`ProjectPercent`、`Cnt`（仅农田）。
- `tagResource`：`DR/UR`（细节坐标）、`Type`、`ProductSort`、`Cnt`、`Blood`、`WorkObjectSN`。
- `tagHuman`：`DR/UR`（当前细节坐标）、`DR0/UR0`（目的地）、`NowState`、`WorkObjectSN`、`Blood/MaxBlood`、
  `attack`、`rangedDefense`、`meleeDefense`。
- `tagFarmer`：`+ ResourceSort`（手持资源种类）、`Resource`（手持数量）、`FarmerSort`。
- `tagArmy`：`+ Sort`（兵种）、`status`、`ifAttack`、`timelock`、`ConvertCooldown`（祭司剩余冷却，**毫秒**）等。

## 2.3 可见性规则（`Core.cpp:477-481, 502-507, 586-587, 619-623`）

| 对象 | 谁能看到 |
|---|---|
| 敌方单位（军队/农民） | 只有 `visible == 1` 才进我方快照 |
| 敌方建筑 | `explored == 1` 或 `visible == 1` |
| 静态资源（树/浆果/金矿/石） | 必须 `explored == 1`（**探过就永久可见**，哪怕之后没视野） |
| 动物 | 无过滤（引擎全部给出） |

补充：敌方单位一旦"实际攻击过我方"会临时显形，敌方建筑攻击过就永久保留（`Core_List.cpp:1073-1083`）。
**敌方 AI 的快照没有这个过滤**（`i != 0` 分支），也就是说对手是"全图视野"。

## 2.4 三个陷阱字段

### (1) `NowState` 不是引擎的原始状态，是 `infoShare` 重算的

`updateByPlayer` 里先填 `interactionList->getNowPhaseNum()`（`Core.cpp:454`），
随后 `infoShare` 用下面这套规则**整个覆盖**（`Core.cpp:694-718`）：

| 条件 | AI 看到的 NowState |
|---|---|
| `WorkObjectSN != -1` 且目标 SN 在"人类集合"里（目标是单位） | `ATTACKING (3)` |
| `WorkObjectSN != -1` 且目标不是单位（资源/建筑） | `WORKING (2)` |
| `WorkObjectSN == -1` 且 `DR0/UR0 != DR/UR` | `WALKING (1)` |
| 其余 | `IDLE (0)` |

推论（很重要）：
- **祭司对"敌方建筑"（如攻城武器厂）下令转化时，`NowState` 是 `WORKING`，不是 `ATTACKING`。**
  想判断"祭司正在转化厂"只能用 `WorkObjectSN == 厂SN`。
- 判"到位"用 `NowState == IDLE`，可靠（`MoveObject::updateMove` 到位后会把 DR0/UR0 归零）。

### (2) `WorkObjectSN` = 关系表里当前行动的**目标 SN**

来源 `interactionList->getObjectSN(human)`（`Core.cpp:459`）。`-1` 表示没有行动。
它是判断"这个人现在在干什么"的**唯一权威字段**。

### (3) `Project` 有两套语义（`Core.cpp:599-607`）

| 建筑 | `Project` 的含义 |
|---|---|
| **箭塔** | 当前锁定/正在走向的攻击目标 **SN**（`-1` = 无目标） |
| 其它建筑 | `build->getActNum()`，即当前正在执行的**建筑行动编号**；空闲时为 `ACT_NULL = 0` |

`tagBuilding.Project` 的初值是 `-1`（`GlobalVariate.cpp:961`），但引擎侧 `Building` 的行动号空闲时是
`ACT_NULL(0)`，所以判"建筑空闲"应该写 `Project == ACT_NULL`（并容忍 `-1`）。

## 2.5 SN 编码

`AI::isHuman/isBuilding`（`AI.cpp:108-116`）用 `SN / 10000` 当大类：
`SN/10000 == SORT_ARMY / SORT_FARMER / SORT_BUILDING / SORT_Building_Resource`。
即 **SN 的高位就是 `SORT_*` 分类**，可以在不知道对象类型时先粗判。

---

# 3. 指令的真实语义

## 3.1 五个接口 → `instruction`

`AI.cpp:11-30`：

| 接口 | instruction 类型 | 参数映射 |
|---|---|---|
| `HumanMove(SN, DR0, UR0)` | `INS_HUMANMOVE` | 细节坐标（**不是块坐标**） |
| `HumanAction(SN, obSN)` | `INS_HUMANACTION` | 目标对象 SN |
| `HumanBuild(SN, 建筑类型, BlockDR, BlockUR)` | `INS_HUMANBUILD` | **块坐标** |
| `BuildingAction(SN, Action)` | `INS_BUILDINGACTION` | option = 行动号 |
| `PinPointStrike(SN, DR0, UR0)` | `INS_PINPOINT_STRIKE` | 细节坐标，仅投石车 |

细节坐标 → 块坐标：`block = (int)(detail / BLOCKSIDELENGTH)`，`BLOCKSIDELENGTH = 35.777`（§9.1）。
块中心 = `(block + 0.5) * BLOCKSIDELENGTH`。

## 3.2 `HumanAction` 的分流（`Core.cpp:1432-1462`）

`manageOrder` 按**主体**种类走：

| 主体 | 处理函数 |
|---|---|
| `SORT_FARMER` | `handleFarmerAction` |
| `SORT_BUILDING` **且是箭塔** | `handleMilitaryAction`（**只有箭塔能对目标下令**，其它建筑直接 `ACTION_INVALID_ACTION`） |
| `SORT_ARMY` | `handleMilitaryAction` |
| 其它 | `ACTION_INVALID_SN` |

### 农民（`Core.cpp:985-1097`）

| 目标类型 | 结果 |
|---|---|
| 静态资源 / 动物 | `CoreEven_Gather` → **去采集** |
| 己方建筑 | 背包里**有**匹配资源 且 建筑已建成 → `CoreEven_Gather`（**上交**）；否则 → `CoreEven_FixBuilding`（**修理/帮建**） |
| 敌方建筑 | `CoreEven_Attacking`（**农民也能打建筑**） |
| 己方农田（`SORT_Building_Resource`） | 可采 → 采集；否则 → 修理 |
| 敌方军队/农民 | `CoreEven_Attacking` |
| **自己（`self == obj`）** | **`deleteSelf` —— 自毁**（`Core.cpp:994-1001`） |

> "上交还是修理"由 **背包**决定，这是引擎自动分的。所以**不要对背着资源的农民下发别的采集目标**，
> 否则那趟货就白跑了（他还没交）。

### 军队 / 箭塔（`Core.cpp:1100-1153`）

| 目标 | 结果 |
|---|---|
| 敌方单位 / 敌方建筑 | `CoreEven_Attacking` → 祭司 = **转化**，其它 = 攻击 |
| 己方单位 且 主体是祭司 | `CoreEven_Attacking` → **治疗**（`Core.cpp:1116-1120`） |
| 己方单位 且 主体不是祭司 | 不允许 |
| 动物 | **不允许**（`ACTION_INVALID_OBSN`） |
| 运输船 | `CoreEven_Transport` |
| **自己** | **自毁**（`Core.cpp:1106-1113`） |

### `BuildingAction`（`Core.cpp:1156-1198`）

- `option == 0` → **停止当前行动**（`suspendRelation`）。
- `option == 自己的SN` → **自毁建筑**。
- 否则 → 走 `addRelation(building, CoreEven_BuildingAct, option)`。

## 3.3 关系表（relation）——"能不能覆盖"的真相

整个引擎的动作是个**关系阶段机**：`relate_AllObject[对象] = 当前行动`（`relation_Object`，见 `Core_CondiFunc.h:89-206`）。

`addRelation(object1, object2, eventType, respond = true)`（`Core_List.cpp:141-259`）：

```cpp
if (relate_AllObject[object1].isExist && relate_AllObject[object1].respondConduct)
    suspendRelation(object1);          // 159 行：先撤销旧行动
if (!relate_AllObject[object1].isExist) {
    ... 一堆校验 ...
    relate_AllObject[object1] = ...;   // 242 行：respondConduct = respond
    return ACTION_SUCCESS;
}
return ACTION_INVALID_ISNTFREE;        // 259 行：已有行动且不可覆盖 -> 错误码 14
```

要点：
1. **`respond` 默认 `true`**（`Core_List.h:31-34`）。所以"新指令能覆盖旧指令"——**代价是旧进度全丢**。
2. 只有少数调用传 `false`（我 grep 出全部 5 处）：动物逃跑/反击（`1554`、`1558`、`1674`）和投射物（`1144`、`1227`）。
   这些**不能被普通指令覆盖**，只能先 `INS_CANCEL` 或等它结束。
3. **`BuildingAction` 的重载完全没有这套机制**（`Core_List.cpp:412-448`）：
   `if (build 存在 && !relate_AllObject[build].isExist) {...} return ACTION_INVALID_ISNTFREE;`
   → **建筑正在生产/研究时，重复下 `BuildingAction` = 稳定返回 14，且拿不到任何效果。**
4. `suspendRelation`（`Core_List.cpp:450-469`）会：**退还建筑预扣资源**、清空移动路径、`initAction()` 重置行动、
   清采集计时器。

### 由此得出"重复下令"的三种后果

| 情况 | 后果 |
|---|---|
| 对**单位**重复下同一目标 | 旧关系被 suspend 重建 → **进度清零**（对箭塔 = 永远打不出箭；对采集 = 白跑一趟） |
| 对**单位**重复下不同目标 | 同上，等于强制换目标（这是"拉扯/撤退"能生效的原理） |
| 对**建筑**重复下行动 | 返回 14，无事发生，但**每帧刷一条错误日志** |

`Core.cpp:600-602` 里引擎作者自己留了注释，说明箭塔曾经因为"AI 每帧重发"而永远开不了火——这就是第 1 种情况。

## 3.4 自动索敌与复仇：不需要 AI 也能开打

`Core_List::update()`（`Core_List.cpp:107-125`）每帧先跑 `manageMontorAct()`：

```
遍历地图的 (视野格 × 对象格) 交叉
  → ob_m->isMonitorObject(ob_ed) 为真则视为候选
  → 每个 ob_m 只保留距离最近的一个 ob_ed
  → addRelation(ob_m, ob_ed, CoreEven_Attacking)      // 1678 行
```

| 类 | `isMonitorObject` 的判据 | 出处 |
|---|---|---|
| `Army` | **任何敌方玩家对象**（单位或建筑） | `Army.h:38-41` |
| `Building` | **只有箭塔**：任何敌方玩家对象 | `Building.cpp:181-187` |
| `Animal` | 自己的逻辑 | `Animal.cpp:208` |

另外 `conduct_Attacked()`（`Core_List.cpp:1519-1587`）实现"被打了就还手/逃跑"：
农民被攻击会**逃命**（往回跑 3.5 格，`1575-1579`）。

**结论**：我方单位/箭塔在"空闲 + 敌人在视野内"时会**自动打最近的敌人**，自动索敌不是我方需要实现的功能。
AI 的价值在于**选择**（选谁打、什么时候撤、站哪里），而不是"触发攻击"。

## 3.5 距离限制：攻击/转化没有上限

`ACTION_INVALID_DISTANCE_FAR` 只在 `config.h:257` 定义，**全工程没有任何地方返回它**（我 grep 过）。
→ **下令攻击 70 格外的目标不会失败，单位会自己走过去。**
这是"祭司横穿全图去转化一个远处的敌人"能发生的原因，写策略时必须自己加距离门。

---

# 4. 经济机制

## 4.1 采集是一条 13 阶段的自动流水线

`relation_Event_static[CoreEven_Gather]`（`Core_List.cpp:2496-2543`）：

| 阶段 | 动作 | 说明 |
|---|---|---|
| 0→1 | Move | 走向目标 |
| 1→2 | Attack | **打猎**：攻击猎物直到"可采集"（1↔2 循环） |
| 3 | JumpPhase | 猎物可采集后跳到 6 |
| 4→7 | Gather | **持续采集**（4↔7 循环），直到目标不可采集 |
| 8 | JumpPhase | 目标采空 → 若背包为空直接跳到 12（结束） |
| 9 | Move | 走向**交付建筑**（仓库/谷仓） |
| 10 | ResourceIn | **放下资源** |
| 11 | Move | **走回原资源点**（9↔12 循环） |
| 12 | JumpPhase | 背包空 → 结束 |

**核心结论：一次 `HumanAction(农民, 资源)` 就会自动"采集→回仓→再回来"循环，直到资源枯竭。**
所以：
- 目标还有效时**绝对不要重发指令**（会把整条流水线推倒重来）；
- AI 真正需要做的只有一件事：**发现"目标采空了/没了"→ 换目标**。

采集节奏由 `gatherNextFrame` 控制（`Core_List.cpp:1247-1262、1310`），采集一次后要等若干帧才能再采。

## 4.2 交付建筑

`set_distance_AllowWork()`（`Core_CondiFunc.h:135-158`）：可工作距离 = `目标边长/2 + 2*CRASHBOX_SINGLEOB`；
船坞、捕鱼有特例。也就是说**农民必须走到资源/建筑贴身的距离**才开工，站位空间是硬需求
（树/矿四邻至少要有 1 格空地，否则农民站不上去 → 这正是"卡住不动"的物理原因）。

## 4.3 农田

农田是 `SORT_Building_Resource`（引擎里的 `Building_Resource`），在 AI 快照里 `Type` 被统一写成
`BUILDING_FARM`（`Core.cpp:609-613`），`Cnt` = 剩余可采集量。
- `HumanAction(农民, 农田)`：有货可采 → 采集；否则 → 修理（`Core.cpp:1030-1043`）。

## 4.4 人口

- `Human_MaxNum = 房子数 × 4`。
- `GenerateHumanLock`（`Core.cpp:77`）保证**每帧最多生产出 1 个人**——所以"多中心同时造人"不会加速。

## 4.5 建造放置规则

`HumanBuild` → `addRelation(农民, BlockDR, BlockUR, CoreEven_CreatBuilding, true, 类型)`
→ `is_BuildingCanBuild(...)`（`Core_List.cpp:363-397`）依次检查：地形/障碍、是否禁用、
是否解锁（科技）、资源是否足够；通过后**先扣资源**、立刻生成建筑地基、再转成 `CoreEven_FixBuilding`。

失败码：`ACTION_INVALID_HUMANBUILD_*`（重叠/越界/未探索/高度不一致/未解锁）与 `ACTION_INVALID_RESOURCE`。

移动落点的选择（`Map::findNearestValidTerrainBlock`，`Map.cpp:1906-1945`）是**从目标格 BFS 扩散**：
返回第一个"地形合法**且**该格没有对象"的格子；如果整张图都占满了，就退回"第一个地形合法的格子"（哪怕被占）。

→ **多个人给同一个坐标必然互相挤**（这是"集结点必须是一片区域"的引擎级原因）。

---

# 5. 战斗机制

## 5.1 攻击关系

`relation_Event_static[CoreEven_Attacking]`（`Core_List.cpp:2484-2486`）是 2 阶段循环：
`Move(靠近)` ↔ `Attack`。伤害在 `object_Attack`（`Core_List.cpp:1059-1202`）里按帧结算；
远程兵投出 `Missile`（有飞行时间），命中再判伤害。

## 5.2 箭塔

| 项 | 值 | 出处 |
|---|---|---|
| 攻击距离 | `DIS_ARROWTOWER = 7` | `config.json:261` |
| 视野 | `VISION_ARROWTOWER = 10`（+ 科技加成 `getArrowTowerRangeAddition()`） | `config.json:262`、`Building.cpp:175-176` |
| 自动索敌 | 视野内**最近的**敌方对象 | `Building.cpp:181-187` + `manageMontorAct` |
| 手动指定 | `HumanAction(塔SN, 目标SN)`，`Project` 会反映该目标 | `Core.cpp:1445-1452`、`599-607` |
| 清目标 | 没有 AI 接口；引擎侧是 `INS_CANCEL` | `Core_List.cpp:1996-2001` |
| 其它建筑 | **不能** `HumanAction`，只有箭塔可以 | `Core.cpp:1445-1449` |

## 5.3 祭司转化

`object_Attack` 里的祭司分支（`Core_List.cpp:2353-2394`）+ 阶段机（`Core_List.cpp:605-614`）：

| 项 | 事实 |
|---|---|
| 触发 | `HumanAction(祭司, 敌方目标)`；祭司射程 `DIS_PRIEST = 12`（`config.json:409`） |
| 耗时 | **随机 2000~6000 毫秒**（`Core_List.cpp:610-614`），期间要一直待在射程内 |
| 成功后 | `setConvertRestEndFrame(g_frame + PRIEST_REST_TIME*1000/TimePerFrame)`，`PRIEST_REST_TIME = 20` 秒（`config.json:413`） |
| 单位效果 | `change_HumanRepresent`（`Core_List.cpp:916-934`）：改阵营、**`freezeStats()` 冻结属性**、切科技指针、结束旧行动 → **单位变成我方的，`Sort`/`Num` 不变**，所以"转来的弓箭手"是 `AT_BOWMAN` 的敌方单位，会出现在我方 `info.armies` 里 |
| 建筑效果 | 必须**贴邻**：`isNear_Manhattan(...) <= 边长/2 + 2*CRASHBOX`（`Core_List.cpp:2374-2384`）；未建成的建筑不能转；转来后会把"房子/中心计数"和"已建成该建筑"迁移到新主人科技树（`936-994`） |
| 冷却可见性 | `tagArmy.ConvertCooldown`（毫秒，0 = 可转） |
| 互斥性 | 转化和移动互斥：新指令会 suspend 旧关系（§3.3）→ **只能"到位站着转"** |

实际节奏：每次转化 ≈ 2~6 秒 + 20 秒休整 ≈ 25 秒/个 → 2~3 分钟的战斗约能转 5~6 个。

## 5.4 胜负条件

- **胜**：`player[0]->build` 中存在 `BUILDING_SIEGE && isConverted() && !isDie()`（`MainWidget.cpp:2362-2364`）。
  即"转化来的攻城武器厂"。
- **负**：`sel->getSecend() >= GAME_LOSE_SEC(1800 秒)`；或祭司消失；或市中心曾经有过现在没有（`MainWidget.cpp:2332-2358`）。
- 判负/判胜后 `HandleGameOver()` 会**直接 `exit(0)`**，所以测试时进程会瞬间消失。

---

# 6. 对手（EnemyAI）

## 6.1 波次时间表与编成（`enemyai.cpp:43-45、2243-2453`）

帧率 = 25 帧/秒（`TimePerFrame = 40` ms，`config.json:12`）。

| 波 | 触发帧 | 时间 | 编成 |
|---|---|---|---|
| 1 | `FAT = 6000` | 4:00 | **2× 棍棒兵(`AT_CLUBMAN`) + 1× 弓箭手(`AT_BOWMAN`)** |
| 2 | `SAT = 13500` | 9:00 | 第一波残兵 + **2× 方阵 + 1× 阔剑 + 1× 复合弓 + 2× 战车弓** |
| 3 | `TAT = 21000` | 14:00 | 前两波残兵 + **2× 投石车 + 2× 阔剑 + 2× 复合弓 + 1× 战车弓 + 1× 战车 + 1× 骑兵 + 1× 方阵** |

`onWaveAttack` 的判定顺序是**倒序**（`2006-2019`：先 TAT 再 SAT 再 FAT），所以拖到后面时后面的波会先触发。

## 6.2 波次兵打谁（`enemyai.cpp:1070-1169`）

`FindWaveTargetByPriority` 的优先级：

1. **正在打我的**（`FindThreatToArmy`，`1070-1125`）——来源包括：我方军队/农民 `WorkObjectSN == 它`，
   **以及 `Project == 它` 的我方箭塔**（`1086-1091`）。一旦锁定，**只有该目标死亡才解锁**（`1142-1148`）。
2. 玩家**祭司**（最近）。
3. 玩家**农民**（最近，且避开同波其他人已锁的农民）。

> 这就是"用箭塔钓鱼"的机制来源：把塔的 `Project` 指向某个近战兵，那个兵会立刻掉头来砍塔，
> 于是站在塔边的祭司只会面对远程攻击。

## 6.3 波次的撤退条件

- 第 1 波：**杀够 3 个农民**就撤退（`2264-2276`）。
- 第 2 波：**杀够 8 个农民**就撤退（`2347-2361`）。
- 每波"派出去的兵全死"→ 该波 `completed`。

→ **波次兵的目标是骚扰农民/祭司，不是拆家。** 所以"保住农民、别让祭司被锁"就是防守的全部意义。

## 6.4 守军与追击上限（`enemyai.cpp:46-51、2043、2119-2134`）

| 项 | 值 |
|---|---|
| "守军"判定半径 | `radius_Inner = 20`（距攻城厂 ≤20 格的对象编入 `Defend_Center_Enemy`） |
| 近战守军追击上限 | `DEFENSE_CHASE_LIMIT = 25` 格（远程再扣自身射程） |
| 越界后 | 放弃目标、`HumanMove` 回 `DefenseHome` |
| 重下令间隔 | `DEFENSE_ORDER_INTERVAL = 20` 帧 |

## 6.5 祭司猎手 —— 整局唯一的死亡线（`enemyai.cpp:676-697、2085-2117`）

- **触发**：我方祭司**当前可见** 且 **距敌攻城厂 ≤ `PRIEST_GUARD_RANGE = 20` 格**。
- **编成**：从厂区 20 格内的守军里抽 3 骑兵 + 2 战车弓（`enemyai.cpp` 初始化段）。
- **锁定行为**：`PriestGuardTarget[猎手] = 祭司`，`++it; continue;`（`2114-2115`）**跳过了 §6.4 的追击上限判定**
  → **猎手追祭司没有距离上限**。
- **解锁**：判活用的是 `EnemyPriestAlive`，而它是在"敌方（敌AI）可见列表"里找 SN——**本质是"祭司还在不在敌人视野里"**。
  骑兵速度 4.07 > 祭司 2.03，所以一旦被锁定基本甩不掉。

→ **祭司一旦进入敌厂 20 格内且被看见，就等于判负（祭司死 → 判负）。**
任何"大军压境"的方案都必须是"先让部队进去引开/清掉猎手，祭司在 20 格外等"。

## 6.6 投石车的目标优先级（`enemyai.cpp:1270-1295`）

`FindStoneThrowerWaveTarget`：**箭塔 > 弓箭手类(`AT_BOWMAN`/`AT_IMPROVED`/`AT_COMPOSITE_BOWMAN`/`AT_CHARIOT_ARCHER`)
> 其它建筑 > 祭司 > 农民 > 其它单位**。

→ 投石车会**主动优先砸我方复合弓**，所以"复合弓为主"的打法必须做距离控制（拉扯）。

## 6.7 敌方初始阵容（`enemyai.cpp` 初始化 + 地图文件）

敌方 = 攻城武器厂 + 5 箭塔 + **51 个兵**（7 阔剑 / 9 方阵 / 9 复合弓 / 1 弓 / 2 斧 / 7 战车弓 / 4 战车 / 6 骑兵 / 6 投石车），
其中**距厂 ≤20 格的约 32 个**（含 5 骑兵 + 4 战车弓 → 猎手必满编 3+2）。

---

# 7. 现有 `UsrAI.cpp` 解析（2014 行）

## 7.1 全局状态清单

| 类别 | 变量 |
|---|---|
| 快照/地图 | `info`、`MAP[100][100]`（`Unknown=-1/Ocean=-2/Open=1`，资源 `+100`，我方建筑 `+1000`，敌方建筑 `+2000`） |
| 基地唯一对象 | `centerSN/BlockDR/UR`、`homeSN/Block`、`granaryBlock`、`oriStockBlock`、`arrowTowerSN/Block`、`priestSN/Block/State` |
| 村民派活 | `farIsgotten`（每帧清空的去重表）、`resIsgotten`（**只在 224/240 写、从不读 → 死代码**）、`bushFarmer`、`goldFarmer`（**永久标签，只写不删**） |
| 计数 | `bushNum`、`gazelleNum`、`killGazelle`、`woodNum`、`farmNum`（**都是累计值，却被当"当前人数/当前数量"用**） |
| 打猎 | `gazelleState(0-4)`、`gazelleSpotDR/UR`、`gazelleHunter1/2SN`、`gazelleTargetSN` |
| 建造者 | `homeBuilderSN`、`homeBuilderDR/UR/State` |
| 军事 | `marketBlock`、`armyCampBlock`、`stableBlock`、`counterState(0/1/2)`、`factorySN/Block`、`scoutSN`、`victoryDR/UR` |
| 开关 | `getOnlyOnce`、`phaseChange`、`phaseNum(20→25)`、`storageStarted`、`priestExploring` |

## 7.2 逐帧执行顺序（`processData`，`UsrAI.cpp:128-323`）

1. `info = getInfo()`（130）
2. `centerUpgrade()`（132）—— 升铜：有市场+靶场+肉≥800 且中心 `Project==ACT_NULL`
3. `farIsgotten.clear()`（134）
4. `getBaseInfo()`（135，仅一次）—— 抓唯一对象 SN
5. `betterMap()`（137）—— 重建 `MAP`
6. `priestExplore()`（139）—— 祭司探路/躲避/回家/凑瞪羚
7. `waveBattle()`（141）—— 祭司转化 + 箭塔目标 + 1v1 牵制 + 风筝位
8. `counterAttack()`（142）—— `counterState` 0/1/2 状态机
9. `CalmAndCrazy()`（143）—— 找敌厂、`counterState==2` 时派祭司去转化
10. 挑建设者（146-151）
11. `manageBuild()`（155）—— 造人/修塔/造房/建筑顺序/帮建/农田/浆果队/兜底派活
12. `trainArmy()`（157）
13. `storageStarted=false` + `huntGazelle()`（160-161）
14. 第二猎人帮建仓库（164-178）
15. **内联资源派活**（182-322）：浆果/瞪羚尸体（含 19 帧节流 `assignedFrame`）→ 木工补到 3 → 铜器后金工补到 3 + 金矿接续

> 同帧抢同一个 SN 时**最后一条生效**（§1.3 规则 A）。按上面顺序，
> `CalmAndCrazy` 在最后 → 它最"硬"；`GOGOGO`（在 `counterAttack` 里）会覆盖 `waveBattle` 给部队下的令。

## 7.3 模块判据表

| 模块 | 依赖的判据 |
|---|---|
| `centerUpgrade` | `haveBuilding(MARKET/RANGE) && Meat>=800 && center.Project==ACT_NULL` |
| `manageBuild` 造人 | `haveNum < phaseNum`（**无 `Project` 门，见 §7.5-1**） |
| 修箭塔 | `b.Blood < b.MaxBlood` 且无人 `WorkObjectSN==塔` |
| 造房 | `spaceNum<=3 && maxNum<50` 且 `homeBuilderState==IDLE` |
| 建筑顺序 | 市场 → 兵营 → 靶场 → 马厩(工具) → 学院(铜器+马厩) |
| 农田 | `Meat<800 && 有市场 && wood>=75`，上限 8 块 |
| `trainArmy` | 仅 `counterState<=1`；升战斧→阔剑→后勤；兵营出 2 斧、靶场出 2 弓 |
| `counterAttack` 0→1 | 见过投石车 且 当前无敌兵 |
| `counterAttack` 1→2 | `Human_Num >= 50` |
| `counterAttack` 2 | 取第一个可见敌兵坐标 → `GOGOGO`（**遍历所有 armies，含祭司**） |
| `waveBattle` 祭司 | 优先投石车，否则"最近"，**②分支无距离上限** |
| `waveBattle` 1v1 | `spare` = 非祭司且 `WorkObjectSN==-1`；防守期只处理距塔 20 格内 |

## 7.4 数据结构层面的设计缺陷

1. **`bushFarmer` / `goldFarmer` 是只写不删的永久标签**（我 grep 过：赋值在 226/242/1346/1372/1525，
   无处 `erase`）。农民一旦沾过浆果/金矿，就**永远退出通用派活池**（`findFarmer`/`assignWoodcutter` 都排除它们）。
2. **`bushNum` / `gazelleNum` / `woodNum` / `killGazelle` 是累计值**，但被当"当前数量"使用：
   `bushNum>=6`（202）会**永久跳过所有浆果**；`woodNum>=3`（1050）是"开始盖建筑"的总开关。
3. **派活入口分散在 5 处**（`processData` 内联段、`manageBuild` 的浆果队/种田/兜底、`assignWoodcutter`、
   `assignGoldMiner`、`huntGazelle`），彼此靠 `farIsgotten` 和"谁先跑"协调 → **执行顺序决定谁能抢到人**。
4. **没有统一的"村民在干什么"表**：只能每帧反查 `WorkObjectSN` 的类型。

## 7.5 我在这轮读代码时确认的问题（按严重度）

### ★1 `BuildingAction` 无门重复发，刷 ISNTFREE（`UsrAI.cpp:992`）
```cpp
if(haveNum<phaseNum)BuildingAction(centerSN,BUILDING_CENTER_CREATEFARMER);
```
没有任何 `Project==ACT_NULL` 判断 → 市中心只要在造人，**每帧都多一条注定返回 14 的指令**
（§3.3 第 3 点）。这是"日志里 ISNTFREE 占绝大多数"的结构性来源，也是纯浪费。
其余 `BuildingAction` 调用点大多有 `Project!=ACT_NULL → continue` 的门（862/1084/1719），是安全的。

### ★2 箭塔索敌逻辑写反了（`UsrAI.cpp:1827-1847`）
```cpp
project = b.Project;                 // 读了，但后面从没用过（编译告警级）
...
for(auto&ea:info.enemy_armies){
    if(ea.SN!=towerPick)continue;    // towerPick 是"列表里第一个在 VISION 内的敌人"
    if(d<=r)keep=true;               // keep 判的是【新挑的目标】在不在射程
}
if(!keep&&towerPick!=-1)HumanAction(arrowTowerSN,towerPick);
```
两个毛病：(a) `keep` 应该判"**当前已锁的 `project`** 是否还有效"，却判了新目标；
(b) `towerPick` 取自被洗牌过的列表（§1.5）→ **每帧可能换一个目标**，而换目标 = suspend 重建 = 塔永远打不出箭（§3.3）。
现在塔实际能开火，靠的是引擎自带的自动索敌（§3.4），这段代码基本在帮倒忙。

### ★3 祭司会自己走向远处的敌人（`UsrAI.cpp:1790-1800`）
"没投石车 → 取最近的敌人"这个分支**只有 `if(d<=dist)`，没有距离上限**（对比 ①投石车分支有 `d<=pRange`）。
而 §3.5 说明攻击指令没有距离上限 → 只要斥候照亮敌营，祭司就会拿到一个 70 格外的目标并**自己走过去**，
接着进入敌厂 20 格 → 被猎手锁定（§6.5）→ 死亡 → 判负。

### ★4 `GOGOGO` 会把祭司一起带走（`UsrAI.cpp:1678-1682` 被 `1763` 调用）
```cpp
void UsrAI::GOGOGO(int dr,int ur){
    for(auto&a:info.armies){ HumanMove(a.SN, ...); }   // 含祭司
}
```
配合 ★3 的机制，这是"祭司怎么跑到 20 格内"的头号嫌疑。

### ★5 `trainArmy` 在 `counterState>=2` 之后完全不造兵（`UsrAI.cpp:855-905`）
函数体是 `if(counterState<=1){...return;}`，**状态 2 之后一次 `BuildingAction` 都不会发** →
第二波之后没有兵源，"边打边造兵"根本不存在。

### ★6 `counterState 1→2` 的门是人口 ≥50（`UsrAI.cpp:1702`）
`Human_Num>=50` 要求 13 栋房（人口上限=房×4）。这等于**强制把进攻拖到很晚**，
与"第 2 波后集结、11 分钟开打"完全冲突。

### ★7 金矿没有"可抵达"检查（`UsrAI.cpp:1481-1535`）
`assignWoodcutter` 有"树四邻必须有空地"的检查（1419-1431），`assignGoldMiner` **完全没有** →
选中嵌在矿脉里的矿，农民站不上去、指令反复失败（§4.2）。

### ★8 祭司保护圈的注释和实现不符（`UsrAI.cpp:1811-1813`）
注释写"只要不出箭塔保护圈(离塔≤PRIEST_HARNESS)"，但 1813-1824 的条件里**没有任何位置判断**；
`PRIEST_HARNESS` 只在风筝位（1942）真正用到。

### ★9 用帧数做节流（`UsrAI.cpp:183,206-207,249`）
`assignedFrame` 19 帧节流；按 §1.2 第 1 点，`GameFrame` 可能跳帧 → 这类节流不可靠（也会漏派活）。

### ★10 建田没有木头预算（`UsrAI.cpp:1203、1286`）
只要 `Meat<800` 且木头 ≥75 就建田，上限 8 块（=600 木），**会和市场/兵营/靶场/马厩/学院抢木头**；
同期建筑总需求 755 木，房子 30 木/栋，木头科技约 550 木 → 木头必然成为瓶颈。

### ★11 房子是人口瓶颈（`UsrAI.cpp:1014`）
`spaceNum<=3` 才建房（快满了才动手），而 20 村民 + 12 复合弓 = 32 人口需要 8 栋房。
建兵时很容易撞上 `MAXHUMAN(11)`。

### 其它安全隐患（不算 bug，但要知道）
- `farmerAt()`（1557-1564）用"≤1 格"判到位，能容忍寻路误差，是合理的。
- `priestExplore` 里多处用 `static` 变量做状态，跨局不会重置（**一局一进程的话没问题**）。
- `int best=1e18` 之类会被截断成 `INT_MAX`（编译器告警），行为安全。

---

# 8. 改造路线

> 目标：**在"能赢"的前提下压缩到 21000 帧（14 分）以内**（第三波 21000 帧触发，赶在它前面结束就少一波）。

## 8.1 P0：村民统一管理（地基）

**一张表 + 两个函数**，把所有派活入口收归一处：

```cpp
map<int,int> farmer_state;        // 村民SN -> FARMER_xxx（工作值）
```

- `initFarmerState()`：每帧登记"`info` 里有、表里没有"的新村民；清掉已消失的；建设者常驻 `FARMER_BULID`。
- `manageFarmers()`：
  1. 按 stage 定策略（起步/工具/铜器/冲刺）；
  2. 遍历村民，**第一个需要处理的处理完就 `return`**（每帧最多动一人 → 不会同帧冲突）；
  3. `switch(farmer_state)`：**空闲**的按 stage 配额派新工种；**已有工种**的只重新定位目标；
  4. 三个过滤：**无人占用**（`resWorkersNow`，由 `info.farmers[].WorkObjectSN` 每帧统计）、
     **可抵达**（四邻有 `Open`）、**还有量**（`Cnt>0`）。

为什么必须放在第一位：§7.4 的四个缺陷全都从这里长出来；`bushFarmer/goldFarmer/bushNum/woodNum`
一旦被"实时统计 + 一张表"取代，7 个现象（采不满浆果、种田吃光木头、后期没人砍树、
科技升不上去、卡矿点、木工池被抽干）会同时消失。

**"目标要不要重发"的判据（照 §4.1 的流水线特性）**：
```
手上有货(ResourceSort!=-1)         -> 别动（他要去交）
目标还在(WorkObjectSN != -1 且对象存在) -> 别动（流水线自己在循环）
否则                               -> 按登记工种重新找目标
```

## 8.2 P1：防御（决定能不能活到 P3）

1. **箭塔钓鱼**：7 格内挑"目标不是我(塔)"的敌人打；当前锁的已经是这种就保持；
   **所有敌人都指向塔时不切换**。目标没变**绝不重发**（§3.3）。收益来自 §6.2 的仇恨机制。
2. **波次识别**：敌兵"从无到有"就 `waveNo++`（状态驱动，不用帧），`FAT/SAT/TAT` 做兜底。
3. **每波策略**：
   - 第 1 波（2 棍棒+1 弓）：不造兵不补塔；祭司**先转弓箭手**（转来的弓兵去敌营方向探路当耗材）**再转棍棒**；
   - 第 2 波（+2 方阵 1 阔剑 1 复合弓 2 战车弓）：部队**只打远程**（复合弓/战车弓）护祭司，近战交给塔；
     祭司**只转 2 个方阵**；
   - 第 3 波：同第 2 波，投石车最后处理（它目标优先级里箭塔最高，见 §6.6）。
4. **祭司自保**：转化目标必须有距离上限；**永不出 `PRIEST_HARNESS`**；
   进攻期绝不靠近敌厂 20 格（§6.5）。

## 8.3 P2：节奏（决定速度）

1. **打猎**：全部打死**再**建仓库，仓库位置取**所有死羊的几何中心**附近（否则有人要跑很远）。
2. **木头预算**：给"下一个要建的建筑 + 农田"留出木头，再决定要不要建田。
3. **房子提前建**：把 `spaceNum<=3` 放宽（如 ≤6），避免撞 `MAXHUMAN`。
4. **靶场靠地图中间**：出兵后加入战斗的行军距离更短；配合"8 分双靶场、10 分三靶场"。

## 8.4 P3：进攻（胜利链路）

1. **集结**（必须写成函数）：集结点 = "探路时被咬的位置 / 视野内出现敌军的位置"再**向自家退几格**；
   **必须是一片区域**（3×3~5×5，一格一个兵，见 §4.5）；祭司站**离敌最远**的那格。
2. **拉扯**：突击者循环 4 步（选最靠敌的兵 → 无敌人前进/被咬或近敌则退回初始位 → 回到初始位则取消突击者身份 →
   `info` 无敌军则重选；突击者死亡同样重选）。这是为了对付 §6.6 里"投石车优先砸弓"和近战冲锋。
3. **祭司贴厂**：转化攻城武器厂 = 胜利，但必须**部队先吸引火力**、祭司满血再上。
4. **农民冲锋**：开战之后，砍树的村民已经没有价值了，调往前线当肉盾给祭司制造机会。

## 8.5 代码结构

`processData` 只做调度，条件写在入口：

```cpp
info = getInfo();
farIsgotten.clear();
if(!getOnlyOnce) getBaseInfo();
betterMap();
updateStageAndQuota();     // stage / 配额 / 木头预算
waveTick();                // 波次
centerUpgrade();           // 时代
initFarmerState();         // ① 登记新村民
manageFarmers();           // ② 村民管理（唯一派活入口）
huntGazelle();             // ③ 打猎
manageBuild();             // ④ 建造/科技
trainArmy();               // ⑤ 造兵
priestExplore();           // ⑥ 祭司探路/回家
waveBattle();              // ⑦ 防御
counterAttack();           // ⑧ 集结/拉扯/贴厂
CalmAndCrazy();            // ⑨ 转化厂（保持最后说话）
```

顺序遵守 §1.3 规则 A：**后写的赢**，所以"最硬的决策"放最后。

---

# 9. 附录

## 9.1 关键常量（`config.json` / `config.h`）

| 项 | 值 |
|---|---|
| `TimePerFrame` | 40 ms → **25 帧/秒**；30 分钟上限 = 45000 帧 |
| `BLOCKSIDELENGTH` | 35.777（地图 100×100 块） |
| 攻击/视野 | 箭塔 `DIS=7`/`VISION=10`；祭司 `DIS=12`/`VISION=12`；复合弓 `DIS=7`/`VISION=9`/`SPEED=2.44`/`ATK=5`/`BLOOD=45` |
| 祭司 | `PRIEST_REST_TIME=20`s；转化耗时随机 2~6 s |
| 造价 | 复合弓 40 肉+20 金（30 s）；复合弓科技 180 肉+100 木（40 s）；升铜 800 肉 |
| 建筑木耗 | 市场 150 / 靶场 150 / 兵营 125 / 马厩 150 / 学院 180 / 仓库 120 / 谷仓 120 / 房 30 / 农田 75 |
| 人口 | `HOUSE_HUMAN_NUM=4` → 上限 = 房子数 × 4 |
| 波次 | FAT 6000 / SAT 13500 / TAT 21000 |
| 敌方守军 | 厂区 20 格内为守军；近战追击上限 25 格；祭司猎手触发 `PRIEST_GUARD_RANGE=20` |

`HUMAN_STATE`：`IDLE=0 / WALKING=1 / WORKING=2 / ATTACKING=3`（`config.h:228-233`，AI 侧语义见 §2.4）。
`AT_ARMY`：`CLUBMAN=0 / SLINGER=1 / BOWMAN=2 / SCOUT=3 / SWORDSMAN=4 / IMPROVED=5 / CAVALRY=6 / SHIP=7 /
STONE_THROWER=8 / PRIEST=9 / HOPLITE=10 / CHARIOT=11 / CHARIOT_ARCHER=12 / BROADSWORDSMAN=13 / COMPOSITE_BOWMAN=14`。
`BUILDING_TYPE`：`HOME=0 / GRANARY=1 / CENTER=2 / STOCK=3 / FARM=4 / MARKET=5 / ARROWTOWER=6 / ARMYCAMP=7 /
STABLE=8 / RANGE=9 / DOCK=10 / SIEGE=11 / COLLAGE=12 / TEMPLE=13 / WALL=14`。
`RESOURCE_*`：`BUSH / TREE / STONE / GAZELLE / ELEPHANT / LION / GOLD / FISH`。

## 9.2 错误码（`config.h:250-280`，按枚举顺序）

| 码 | 常量 | 码 | 常量 |
|---|---|---|---|
| 0 | `ACTION_SUCCESS` | 13 | `ACTION_INVALID_BUILDACT_NEEDBUILT` |
| 1-5 | `ACTION_INVALID_HUMANBUILD_*`（含 OVERLAP/UNEXPLORE/OVERBORDER/DIFFERENTHIGH/LOCK） | 14 | `ACTION_INVALID_ISNTFREE` |
| 6 | `ACTION_INVALID_DISTANCE_FAR`（**从未被返回**） | 15 | `ACTION_INVALID_NULLGOALOBJECT` |
| 7 | `ACTION_INVALID_FULLY_LOAD` | 16 | `ACTION_INVALID_NULLWORKER` |
| 8 | `ACTION_INVALID_POSITION_NOT_FIT` | 17 | `ACTION_INVALID_PINPOINT_NOT_FIT` |
| 9-10 | `ACTION_INVALID_HUMANACTION_*` | 18 | `ACTION_INVALID_PRIEST_TARGET_ERROR` |
| 11 | `ACTION_INVALID_BUILDACT_MAXHUMAN` | 19 | `ACTION_INVALID_UPGRADE_TIME` |
| 12 | `ACTION_INVALID_BUILDACT_LOCK` | 20-25 | `RESOURCE / BUILDINGNUM / OBSN / LOCATION / ACTION / SN` |

## 9.3 地图与开局

- 地图与旋转**都是随机的**（`Core.cpp:1282-1287` 会打印本局地图名与旋转角）→ **AI 里不能出现任何绝对坐标**。
- 四张图开局阵容同构：我方 = 1 祭司 + 8 农民 + 市中心 / 箭塔 / 2 房 / 仓库 / 谷仓，**0 其他军队**。
- 市中心 → 敌方攻城厂直线距离 86~101 格（四张图不同），**绝对距离不可依赖**；
  可以依赖的是"**敌人在我家市中心的对角**"（用 `centerBlockDR/UR` 判角落，全相对量）。

## 9.4 待验证清单（我没读到 / 没实证的，不要当结论）

1. `CONVERT_COOLDOWN` 之外，**祭司转化在"目标建筑正在生产"时是否有额外限制**（我只读了 `object_Attack` 分支）。
2. 时代升级链：`BUILDING_CENTER_UPGRADE` 是 1~2 级链（`Building.cpp:230-234` 用 `getActLevel` 区分石器→工具 / 工具→铜器）。
   **同一 Action 需要调用两次**？错误码 19（`UPGRADE_TIME`）的精确触发条件我只读到"`oper==2` = 时代升级建筑前置不足"（`Core_List.cpp:433-438`），
   没有继续追 `get_isBuildActionAble` 的实现。
3. `Project` 在"新建好、从未执行过行动"的非箭塔建筑上是 `0` 还是 `-1`（我按 `initAction()` 推是 `ACT_NULL=0`，但没实测）。
4. 采集一次的数量 `get_quantityGather()` 与 `gatherIntervalFrames()` 的具体值（影响"几人采一处"的收益）。
5. 各兵种攻击间隔/伤害公式（`calculateDamage`）我只读了调用点，没读公式。
6. 敌方 AI 的"进攻/防守"全局姿态切换条件（`ENEMY_STATUS_ATTACK/DEFEND`，`MainWidget.cpp:3325-3333`）。

---

## 附：本文与原项目文档的关系

- `AI接口使用指南.md`：接口用法说明（面向"怎么调 API"）。**本文补充了它没写的执行语义**
  （指令延迟一帧、按 SN 去重、每帧执行上限、列表被洗牌、`respond` 覆盖机制、`Project` 双语义、`NowState` 重算规则）。
- `游戏编程玩法.md`：玩法层封装函数示例。本文 §3.2 的分流表是它的精确版。
- 本文 §7.5 的问题清单是**我读代码自己找的**，每条都带行号，可以逐条复核。
