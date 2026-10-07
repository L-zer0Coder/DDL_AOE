# -*- coding: utf-8 -*-
# 第 12 批: case 3 厂区决胜 —— 分头拆塔 + 塔清光后当肉盾 + 祭司等分配完成再上
p = 'UsrAI.cpp'
lines = open(p, 'rb').read().split(b'\n')

def enc(s):
    return s.encode('utf-8').replace(b'\n', b'\r\n').split(b'\n')

def rep(a, b, s, tag):
    old = lines[a-1].decode('utf-8', 'replace').strip()[:56]
    arr = enc(s) if s else []
    if arr and arr[-1] == b'':
        arr = arr[:-1]
    lines[a-1:b] = arr
    print('[OK] %-12s %d-%d  was: %s' % (tag, a, b, old))

CASE3 = '''        case 3:{   // ---- 厂区决胜: 分头拆塔; 塔清完就贴上去给祭司挡枪 ----
            if(factorySN==-1){ counterState=2; break; }   // 厂没进视野 -> 回去继续推进
            if(scoutSN==-1){                              // 斥候继续当先锋(死了补)
                for(auto&a:info.armies){
                    if(a.Sort==AT_SCOUT){ scoutSN=a.SN; break; }
                }
            }
            if(scoutSN==-1){
                for(auto&b:info.buildings){
                    if(b.Type==BUILDING_STABLE&&b.Percent>=100&&b.Project==ACT_NULL&&
                       info.Meat>=BUILDING_STABLE_CREATE_SCOUT_FOOD){
                        BuildingAction(b.SN,BUILDING_STABLE_CREATE_SCOUT);
                        break;
                    }
                }
            }
            manageScout();
            attackTowers();        // 有守军先打人; 没守军分头拆塔; 塔没了就贴厂当肉盾
            priestConvert();
            break;
        }'''

TOWERS = '''// ================= [AI] 厂区: 分头拆塔 =================
//   有守军 -> 交给 armyAttack(野战: 退/打)
//   没守军 -> 每个兵认领"离自己最近的活塔"去打; 塔全清光 -> 往厂靠(<=4格)当肉盾
//   每个兵都有塔可打(或塔没了) -> towerReady=true, 祭司可以上
void UsrAI::attackTowers(){
    if(!info.enemy_armies.empty()){ armyAttack(); return; }      // 有守军 -> 先打人

    int nArmy=0,nAssigned=0;
    for(auto&a:info.armies){
        if(a.SN==priestSN||a.SN==scoutSN)continue;
        if(a.NowState!=HUMAN_STATE_IDLE)continue;
        nArmy++;
        int tSN=-1;
        auto it=towerAssign.find(a.SN);
        if(it!=towerAssign.end()){                               // 认领过的: 塔还在就继续打
            for(auto&eb:info.enemy_buildings){
                if(eb.SN==it->second&&eb.Type==BUILDING_ARROWTOWER){ tSN=it->second; break; }
            }
        }
        if(tSN==-1){                                             // 没认领 / 塔没了 -> 找最近的活塔
            int best=1e9;
            for(auto&eb:info.enemy_buildings){
                if(eb.Type!=BUILDING_ARROWTOWER)continue;
                int d=max(abs(eb.BlockDR-a.BlockDR),abs(eb.BlockUR-a.BlockUR));
                if(d<best){ best=d; tSN=eb.SN; }
            }
            if(tSN!=-1)towerAssign[a.SN]=tSN;
        }
        if(tSN==-1)continue;                                     // 一座塔都没有了
        nAssigned++;
        if(a.WorkObjectSN!=tSN)HumanAction(a.SN,tSN);
    }

    if(nAssigned>0){                                             // 还有塔要拆
        towerReady=(nArmy>0&&nAssigned>=nArmy);                  // 每个兵都有塔 -> 祭司可以上
        return;
    }
    towerReady=true;                                             // 塔清光了
    for(auto&a:info.armies){                                     // 往厂靠, 挡在祭司前面
        if(a.SN==priestSN||a.SN==scoutSN)continue;
        if(a.NowState!=HUMAN_STATE_IDLE)continue;
        if(max(abs(a.BlockDR-factoryBlockDR),abs(a.BlockUR-factoryBlockUR))>4)
            HumanMove(a.SN,factoryBlockDR*BLOCKSIDELENGTH,factoryBlockUR*BLOCKSIDELENGTH);
    }
}'''

G = '''unordered_map<int,int> towerAssign;   // [AI] 兵SN -> 认领的敌方箭塔SN
bool towerReady=false;                // [AI] 是否"每个兵都认领到塔"(= 祭司可以上了)'''

# 从后往前
c3 = [i for i, l in enumerate(lines) if b'case 3:{' in l][0]
rep(c3+1, c3+9, CASE3, 'case 3')
end = [i for i, l in enumerate(lines) if b'void UsrAI::CalmAndCrazy' in l][0]
# counterAttack 结束的 "}\n\n\n\n// ==== 祭司 ====" 之前插入函数
for i in range(end-1, end-8, -1):
    if b'[AI] 祭司 ===' in lines[i]:
        ins = i + 1
        break
lines[ins:ins] = [b''] + enc(TOWERS) + [b'']
print('[OK] attackTowers 插入到行', ins+1)

gi = [i for i, l in enumerate(lines) if b'priestGoDR=-1,priestGoUR=-1' in l][0]
rep(gi+1, gi+1, G, 'tower globals')

open(p, 'wb').write(b'\n'.join(lines))

txt = open(p, 'r', encoding='utf-8', newline='').read()
old = '''        int d=max(abs(priestBlockDR-factoryBlockDR),abs(priestBlockUR-factoryBlockUR));
        if(d>DIS_PRIEST){ 
            priestFollow(); 
            break; 
        }                             // ② 还太远
        HumanAction(priestSN,factorySN);             // 血够 + 够近 -> 上
        break;'''.replace('\n', '\r\n')
new = '''        int d=max(abs(priestBlockDR-factoryBlockDR),abs(priestBlockUR-factoryBlockUR));
        if(d>DIS_PRIEST){                            // 还太远 -> 直接朝厂走(路上挨打不管)
            HumanMove(priestSN,factoryBlockDR*BLOCKSIDELENGTH,factoryBlockUR*BLOCKSIDELENGTH);
            break;
        }
        if(!towerReady)break;                        // 兵还没围住塔 -> 到了也先别动
        HumanAction(priestSN,factorySN);             // 上
        break;'''.replace('\n', '\r\n')
print('calm matched:', txt.count(old))
txt = txt.replace(old, new)
open(p, 'w', encoding='utf-8', newline='').write(txt)

h = 'UsrAI.h'
hs = open(h, 'rb').read().decode('utf-8')
add = '    void attackTowers();           // 厂区: 分头拆塔, 塔没了就当肉盾\r\n'
anchor = '    void armyAttack();             // 部队: 退/打/进'
if add not in hs:
    hs = hs.replace(anchor, anchor + '\r\n' + add.rstrip('\r\n'))
    print('h added')
open(h, 'wb').write(hs.encode('utf-8'))
print('done')
