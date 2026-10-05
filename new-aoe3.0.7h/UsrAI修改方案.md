# UsrAI.cpp 修改方案（待审批 · 第四版 · 含离线仿真证据）

> **状态：全部未落代码**，等你逐条批准。
> 定位更新：**以你当前的 `UsrAI.cpp` + 你发的那份秘籍为准**，项目说明文档只当背景参考
> （且它是初版，数值与代码有出入：文档写"守军 15 格迎击 / 18 格放弃"，代码
> `enemyai.cpp:47/50` 是 `DEFENSE_ALERT_RANGE 20` / `DEFENSE_CHASE_LIMIT 25`）。
> 依据：引擎源码 `Core.cpp` / `Core_List.cpp` / `Core_CondiFunc.cpp` / `GlobalVariate.*` /
> `Map.cpp` / `enemyai.cpp` / `config.json` + `UsrAI.cpp` 全文 2822 行。
> 22:26 你叫停后，当晚 20 处改动已全部回退，`g++ -fsyntax-only` 通过（0 error），
> 备份在 `UsrAI.cpp.bak_before_rollback_2232`。

---

## 零、三个必须先对齐的机制（后面所有判断都基于它）

| # | 机制 | 依据 |
|---|---|---|
| A | **`NowState` 是 `infoShare` 重算的**：`WorkObjectSN!=-1` 且指向"人类"→ `ATTACKING`，指向建筑/资源 → `WORKING`；无目标且 `DR0≠DR` → `WALKING`；否则 `IDLE` | `Core.cpp:694-710` |
| B | **走路的 `DR0/UR0` 是引擎设的目的地，且移动途中会被引擎改写**（`findPath` 跨海/跨大陆时用 `GetSameBlockInLine` 换掉终点，对玩家仍返回 `ACTION_SUCCESS`） | `Core_List.cpp:2027-2041`、`1822-1825` |
| C | **打断建造 = 永久烂尾 + 钱不退**（`suspendRelation` 只退 `_TS`，建造那笔不记 `_TS`）；**每帧同 SN 只留最后一条指令，超上限静默丢弃** | `Core_List.cpp:450-469`、`Core.cpp:1386-1390` |

---

## 一、冲突代码（你新提的两条，都已定位到确切行号）

### C1　祭司探路横跳 —— 结论：不需要拉黑制，只要"方向锁定 + 两层兜底"

**你说的两条硬要求，我核实后的现状**：

| 健壮性要求 | 现状 | 结论 |
|---|---|---|
| **不会走到永远走不到的地方** | `frontier()` 只保证"自己是 Open 空地"，不保证可达；引擎 `findPath` 最差会把祭司送到**最近的可达格**（`Core_List.cpp:2153-2172`），并把 `DR0/UR0` 改成那个格 | 引擎已经兜了一半，但**我们的代码看不出来**，于是只能靠"拉黑"这种间接手段 |
| **同一帧不发多条命令** | 4 个下发点（`675`/`723`/`796`/`935`）**每个后面都有 `return` ⇒ 一帧只有一条** ✅ | 只有**回家段 `homeDR` 没有锁定**（`635` 挑一次就 `HumanMove`，此后每帧只要条件成立就再发一次）需要补 |

**⇒ 所以真正要做的是：把"目标锁定"统一成一种写法（你已经在 `dodgeDR`/`seekDR` 上写对了的那种），再加两层兜底。**

#### 锁定的重选条件（4 个分支满足任一就换目标）

| # | 条件 | 判据 | 类型 |
|---|---|---|---|
| 1 | **到了** | `abs(priestBlockDR-tgtDR)<=1 && abs(priestBlockUR-tgtUR)<=1` | 状态 |
| 2 | **那格走不了了** | `!usable(tgtDR,tgtUR)`（Open 空地 + 在界内） | 状态 |
| 3 | **引擎把终点改了** | 移动中（`priestState==WALKING`）且 `DR0/UR0` 换算出的格 ≠ 我们给的 `tgtDR/tgtUR` | 状态（`getPriest()` 加读 `a.DR0/a.UR0`） |
| 4 | **超时**（兜底） | `info.GameFrame - 下令帧 > 40 + 距离*25` | 帧数（你已批准） |

超时公式的来历：祭司速度 2.03 格/秒 = 0.081 格/帧，即 12.3 帧/格；
`25 帧/格` ≈ 2 倍余量，`40` 是起步开销（起步/被挤/寻路重算）。
- 距 10 格 → 290 帧（约 11.6 秒）
- 距 50 格 → 1290 帧（约 51 秒）

**这 4 条为什么能覆盖"永远走不到"**：
- 目标格本身不可走 → 条件 2 兜住；
- 目标格可走但被建筑/水面包住 → 寻路改写终点 → 条件 3 立刻换；
- 寻路反复失败、`nullPath` 重试把祭司钉住 → 条件 4 超时换；
- 走得到 → 条件 1 正常重选。

**⇒ 不需要 `badFrontier` 拉黑表**（它现在 6 处引用全部只为这一个拉黑点：`67` 声明 + `835/860/889/890` 过滤 + `926` 写入）。

#### 具体改动（5 条）

| # | 动作 | 位置 | 减少 |
|---|---|---|---|
| C1-1 | **删掉回家段的"遇敌躲避"** —— 你确认回家不会遇敌，局部避障（`683-729`）本就是兜底 | `648-674` | 27 行 |
| C1-1b | **给回家段补方向锁定** —— 现在 `homeDR` 挑一次就发，之后每帧条件成立就再发一次，是唯一"一帧可能重复下令"的地方 | `635-680` | —（加约 6 行） |
| C1-2 | **`tgtDR` 换成上面的 4 分支锁定**，删 `tgtWalked` + `badFrontier` 整张表 | `915-935` | 约 12 行 |
| C1-3 | **删 `phase==-1`「绕基地三座建筑」** —— 开局视野已覆盖老家周边（`MAP.cpp:1522-1530` 市中心 ±8 格），箭塔也在附近；这个 phase 还要 `cx/cy` 在三座建筑间轮换，最复杂收益最低。保留 `phase 0`（自己那一角）+ `phase 1`（绕中心） | `810-848` + `538-541` | 约 42 行 |
| C1-4 | 删死变量 `curDR/curUR`（`542` 声明、`797-798` 赋值，赋值后从未被读） | `542` / `797-798` | 3 行 |
| C1-5 | 删假注释 `MOVE_TIMEOUT`（**只在注释里、代码从未实现**） | `740` | 2 行 |

**改完后 `priestExplore` 骨架**（现在 403 行 → 约 250 行）：

```
① getPriest(+读 DR0/UR0) + if(!priestExploring)return
② 三个 lambda: usable / frontier / dangerNear                 ← 保留
③ 找最近危险(ex,ey,dist)                                     ← 保留
④ 回家: 挑箭塔四邻(只挑一次) + 方向锁定 + 到家 priestExploring=false
⑤ 躲敌: 8 方向 + dodgeDR 方向锁定                              ← 保留(你加的局部避障)
⑥ 追瞪羚: 最近瞪羚四邻 + seekDR 方向锁定                       ← 保留
⑦ 常规探索: phase 0/1 选候选点 + tgtDR 四分支锁定              ← 只改这里
```

#### 离线仿真实测（map1, 2000 帧, 统计"AI 下发的 HumanMove 指令数"）

| 方案 | 总指令数 | 最高频目标 | 相邻指令间隔(中位) |
|---|---|---|---|
| **修复前（当前代码）** | **330** | `home(16,68)` **296 次** | **1 帧** |
| **修复后（C1-1b 加方向锁定）** | **36** | `explore(34)` 3 次 | 11 帧 |

**指令数 330 → 36，减少 89%。** 这个对比只统计"AI 下发了多少条 HumanMove"，**与引擎模型无关**，
所以结论是硬的。它对应你日志里 `00:00:48` 和 `00:00:52` 连续两条**同一坐标** —— 就是 `homeDR`
没有锁定、每帧重发同一条。真实引擎里每帧重发还会触发 `suspendRelation` 清路径，后果比仿真更重。

**风险**：
- 删 `phase -1` 后探索范围略小（不再专门绕基地三座建筑各 30 格），但老家周边开局就在视野里 —— **可接受**。
- 条件 3 依赖 `priestState==WALKING`（`IDLE` 时引擎会把 `DR0` 重置成当前位置，读不到原终点），所以只在移动中判断。
- 条件 4 引入帧数（`info.GameFrame`）。这是**唯一新增的帧数用法**，且只作兜底、不参与主判据。
- 全部是"删"和"改判断"，不新增功能、不新增跨帧状态量。

### C2　斥候造不出来 —— 人口被复合弓/方阵兵吃干

**先确认你说的是对的，机制上完全成立**：

| 事实 | 位置 |
|---|---|
| 造斥候的**唯一入口**是 `manageScout()` 里那几行 | `2070-2075` |
| 而 `manageScout()` **只在 `counterState>=2` 时被调用** | `2304` / `2330` |
| `counterState` 1→2 的门是 `info.Human_Num>45` | `2259` |
| **复合弓生产没有任何人口上限** | `1025-1026`（`if(compTech&&肉够&&金够) BuildingAction(...)`） |
| **方阵兵生产也没有任何人口上限** | `1009-1011` |
| 人口硬上限 50，且**市镇中心算 1 个房子** | `Development.h:63` `getMaxHumanNum()=homeNum*4`，`humanNum_Top=50` |

**闭环**：`trainArmy` 每帧无条件造复合弓/方阵兵 → 人口被顶到 `Human_MaxNum`(50) →
等到 `counterState` 终于跨过 45 进 case 2 时，人口已满 → `BuildingAction(斥候)` 返回
`ACTION_INVALID_BUILDACT_MAXHUMAN` → `scoutSN` 恒为 -1 → `anchorDR` 恒为 -1 →
**case 2 卡在 ① 死循环，永远进不了 case 3**（这和我第一轮诊断的完全一致）。

**建议改法（两处，都很小）**：

1. **给造兵留人口名额** —— `1025` 和 `1009` 各加一个门：
   ```cpp
   && info.Human_Num < info.Human_MaxNum-1     // 永远留 1 个名额给斥候
   ```
2. **把造斥候从 `manageScout` 提到 `trainArmy` 的马厩分支**（`trainArmy` 现在完全没有
   `BUILDING_STABLE` 分支，马厩全程闲着）：
   ```cpp
   if(b.Type==BUILDING_STABLE && b.Percent>=100 && b.Project==ACT_NULL
      && info.Meat>=BUILDING_STABLE_CREATE_SCOUT_FOOD
      && countScout(info.armies)<1){
       BuildingAction(b.SN,BUILDING_STABLE_CREATE_SCOUT);
   }
   ```
   这样斥候**不依赖 `counterState`**——从马厩建好那一刻起就开始试造。

**风险**：低。两处都是"加门"，不删现有逻辑。第 2 条是纯新增分支（马厩分支目前是空的）。

**要注意的副作用**：留 1 个名额意味着人口上限 50 时停在 49，复合弓会少造 1 个。
如果你觉得这个代价不可接受，替代方案是"**只在我方还没有斥候时才留名额**"（有斥候了就放开造满）——
我可以按你选的写。

---

### C3　双猎人"到位"死锁 —— 仿真报出，但你已实机排除，**本轮不改**

离线仿真(map1) 报出：`gazelleState` 停在 1（到位中）→ 猎人仓库不建 → 市场门不开 → 全线停摆。
机理：两名猎人目标只差 1 格 `(40,84)`/`(39,84)`（`1978-1979`），碰撞盒 11.92px=0.33 格 < 实际 1.04 格
→ 互顶；到位判定用整数块坐标 `|38-40|=2 > ±1` 恒不通过。

**你的反馈：「猎人落脚点经过多次实验肯定没问题」** ⇒ 判定为**仿真器与真实引擎的差异**（我的引擎模型不模拟
碰撞的随机换向/绕行），**本轮不动**。仅留作实机观察项：若哪天看到两个猎人在猎场边缘不动，回来处理。

## 二、废代码 / 冗余 / 过度复杂

### M1　金矿造仓库（你第 14 条："完全按瞪羚那的逻辑就行，当前代码太臃肿"）

**位置**：`buildGoldStock()` 全文 + `goldReserve` + 2 个辅助物

**现状**：在 `checkEnv` 之外又叠了 4 层——`goldStockCount()` 上限计数、`goldSpot` 挖空自动重选、
"选矿时跳过有仓库的矿"、`GOLD_STOCK_MAX=2`。

**建议改法**：砍到和 `huntGazelle` 的 `gazelleState==3` 同构，只有三步：

```
① int st=checkEnv(RESOURCE_GOLD,bySN);
   if(st!=0)return false;                       // 已有仓库/在建 → 这一环结束
② if(goldSpotDR==-1){ ...找一口离市中心最近、没人占的矿... }
③ if(info.Wood<BUILD_STOCK_WOOD){ assignWoodcutter(); return false; }
   if(storageStarted)return false;
   if(!findBuildSpot(goldSpotDR,goldSpotUR,3,2,4,ox,oy))return false;
   int fSN=findFarmer(goldSpotDR,goldSpotUR); if(fSN==-1)return false;
   HumanBuild(fSN,BUILDING_STOCK,ox,oy); storageStarted=true; farIsgotten[fSN]=true; return true;
```

同步删除：`GOLD_STOCK_MAX`、`goldStockCount()`（`.cpp`+`.h`）、`checkEnvAt` 注释定义块、
`.h` 里 `checkEnvAt`/`goldStockCount` 两个声明；`goldReserve` 条件简化为只看 `checkEnv(...)==0`。

**理由**：`checkEnv` 是全局判据（任意一口矿旁有仓库就不建），所以"上限计数"和"动态换矿"本来就是多余的。
砍完约 55 行 → 30 行，且与瞪羚完全同构。

**风险**：低。唯一变化是"矿采空后不再自动换下一口矿"——但全局 `checkEnv` 反正会挡住第二座仓库，等于没差别。

---

### M4　零调用清理（你第 9/10/11 条）

| 符号 | 位置 | 现状 | 建议 |
|---|---|---|---|
| `hasUnfinishedBuilding()` | `.cpp`1045 + `.h`37 | 全文件零调用；`manageBuild` 的"有无在建的"是自己遍历 `Percent<100` 实现的 | 删 |
| `stableBlockDR/UR` | `108-109` 声明 + `1316` 使用 | 从未赋值（一直 -1），学院"挨着马厩建"永远不成立 | 删（你已确认"不需要"） |
| `static bool onlyOnce` | `huntGazelle` state3 | 声明即废 | 删 |
| `stockSN` | 同上 | 算完丢弃 | 删 |
| `homeSN / homeBlockDR / homeBlockUR` | 声明 + `getBaseInfo` 赋值 | 赋值了但**全文件无读取方**（建房锚点用的是遍历 `BUILDING_HOME` 拿到的 `b.BlockDR`） | 删 |
| `homeBuilderDR / homeBuilderUR` | 声明 + `gethomeBuilder` 每帧赋值 | 同上，只用到 `homeBuilderState` | 删 |
| 两行重复的 `if(badFrontier.count(...))continue;` | `priestExplore` phase1 | 完全相同写了两遍 | 删一行 |

**风险**：无（都零引用）。删前我会再 grep 一遍确认没有间接调用。

---

## 三、其余待批准项

### M3　`NowState` 判据（你第 17 条"确实不了解"）

| 行 | 现状 | 问题 |
|---|---|---|
| `priestConvert` 2202 | `if(a.NowState==HUMAN_STATE_ATTACKING)break;` | 祭司转**建筑**时是 `WORKING`（机制 A），这个门挡不住 → 会对同一目标重复下令 |
| `waveBattle` 2518 | 同上写法 | 同上 |
| `CalmAndCrazy` 2814 | `if(a.NowState!=HUMAN_STATE_IDLE)break;` | ✅ 已经是对的，不改 |

**改法**：前两处统一成和 `priestFollow`（2216）一样的
`if(a.NowState!=HUMAN_STATE_IDLE&&a.NowState!=HUMAN_STATE_WALKING)break;`

**风险**：低。

### M2　排兵布阵：远程在后、近战在前（你第 15 条）

**位置**：`rallyArmy()` 的格子评分（约 2398-2420）

**现状**：评分是 `dd = max(|a.BlockDR-dr|, |a.BlockUR-ur|)` —— **离该兵自己最近**，
所以谁先被派谁占格，近战远程混在一起。`isMeleeSort()`（`522` 行）你写好了但**零调用**。

**改法**：评分换成"该格离敌人方向有多近"，近战抢最小、远程抢最大：

```cpp
int ex=(anchorDR!=-1)?anchorDR:(2*centerBlockDR-rallyDR);
int ey=(anchorUR!=-1)?anchorUR:(2*centerBlockUR-rallyUR);
bool melee=isMeleeSort(a.Sort);
int dd=max(abs(dr-ex),abs(ur-ey));
dd = melee? dd : -dd;
```

**风险**：低。`taken` 保证每格一兵。

---

## 四、我判断**不需要**改的（附你的原话，供否决）

| 项 | 结论 | 你的理由 |
|---|---|---|
| `findBuildSpot` 不查坡度/高度 | 不改 | "一般不会遇到斜坡问题" |
| `spotBusy` 漏查资源/地基/敌建筑 | 不改 | "只用来查看是不是有人在这里" |
| `dist` 在 `priestExplore` 跨段复用 | 不改 | "确实是已知问题"但"一改就要改很多地方" |
| 时代判断三种写法混用 | 不改 | "我们现在没有铁器时代" |
| `findBuildSpot` 的 `minR` 死参数 | 不改 | "确实，但不是很想优化" |
| 帧数节流 `assignedFrame` / `goHomeFrame` | 保留 | "我觉得还是要加上帧数判断的，可以少一点" |
| 弓箭手探敌营当耗材 | 不改 | "目前还没接" |
| 双人打猎两人相邻 | 不改 | "没有顶到，两个猎人是有空间的" |

---

## 五、验证方式

1. 语法：`g++ -fsyntax-only -std=c++14 -DQT_DEPRECATED_WARNINGS -I. -I<Qt>/include -I<Qt>/include/Qt{Core,Gui,Widgets,Multimedia,Network} UsrAI.cpp`
   （本机需把 `TEMP/TMP/TMPDIR` 指到工作区内目录，否则 g++ 会静默失败）
2. 8 倍速跑一局，看三个打点：
   - `[CA] st=? rush=?/5 rally=? fact=?` —— `st` 能否从 0 一路推到 3（现在卡在 2）
   - `[GATE] stock=? gazelleSt=? wood=?` —— 猎人仓库建成后应消失
   - 祭司的 `HumanMove` 日志 —— 目标是**一条直线走到底**，不再每 8 帧换

---

## 六、批复表（最终版 · 圈哪几条我就改哪几条）

| 编号 | 内容 | 位置 | 行数变化 | 批准？ |
|---|---|---|---|---|
| **C1-1b** ⭐ | **回家段 `homeDR` 补方向锁定**（仿真证实 330→36） | `635-680` | +6 | ☐ |
| **C1-1** | 删回家段的"遇敌躲避"（你确认回家不会遇敌） | `648-674` | −27 | ☐ |
| **C1-2** | `tgtDR` 换成四分支锁定（到了/不可走/**引擎改了终点**/**超时**），删 `tgtWalked` + `badFrontier` | `915-935` | −12 | ☐ |
| **C1-2b** | 超时阈值 `40 + 距离*25` 帧 | 同上 | — | ☐ |
| **C1-3** | 删 `phase==-1`「绕基地三座建筑」+ `cx/cy` | `810-848` | −42 | ☐ |
| **C1-4** | 删死变量 `curDR/curUR` | `542` / `797-798` | −3 | ☐ |
| **C1-5** | 删假注释 `MOVE_TIMEOUT` | `740` | −2 | ☐ |
| **C2-1** | 复合弓 `1025` + 方阵兵 `1009` 加 `&& info.Human_Num < info.Human_MaxNum-1` | 2 处 | +2 | ☐ |
| **C2-2** | 造斥候移入 `trainArmy` 的 `BUILDING_STABLE` 分支（"没有斥候就造"） | `938` 起 | +8 | ☐ |
| **M1** | 金矿逻辑简化（砍到与 `huntGazelle` 同构） | `1772-1840` | −25 | ☐ |
| **M4** | 零调用清理 7 项（`hasUnfinishedBuilding` / `stableBlockDR` / `onlyOnce` / `stockSN` / `homeSN`组 / `homeBuilderDR`组 / 重复行） | 多处 | −30 | ☐ |
| **M3** | `NowState` 判据 `2202` / `2518` → `!=IDLE && !=WALKING` | 2 处 | 改2行 | ☐ |
| **M2** | 排兵布阵分层（接上 `isMeleeSort`） | `2398-2420` | 改判据 | ☐ |
| — | C3 双猎人（**你已排除，本轮不改**） | — | — | ☐ 确认 |
| — | 第四节"不改"的 8 项 | — | — | ☐ 确认 |

**建议的最小起步批次**：`C1-1b + C1-1 + C1-2 + C1-2b + C1-5`
—— 这 5 条全是围绕"祭司每帧重发"这一件事，改完 `priestExplore` 从 403 行降到约 290 行。

每条改完跑一次 `g++ -fsyntax-only`，改动一律带 `// [AI]` 注释。
