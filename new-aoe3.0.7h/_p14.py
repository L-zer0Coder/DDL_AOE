# -*- coding: utf-8 -*-
# 第 14 批: 兵先贴塔(4格)当肉盾, 贴齐了祭司再上
p = 'UsrAI.cpp'
lines = open(p, 'rb').read().split(b'\n')

def enc(s):
    return s.encode('utf-8').replace(b'\n', b'\r\n').split(b'\n')

start = None
end = None
for i, l in enumerate(lines):
    if l.strip() == b'int nArmy=0,nAssigned=0;':
        start = i
    if b'towerReady=true;' in l and b'塔清光了' in l:
        end = i
print('start(行)', start+1, 'end(行)', end+1)
print('  start:', lines[start].decode('utf-8','replace').strip())
print('  end  :', lines[end].decode('utf-8','replace').strip())

NEW = '''    int nArmy=0,nAssigned=0;
    for(auto&a:info.armies){
        if(a.SN==priestSN||a.SN==scoutSN)continue;
        nArmy++;                                                 // 走路的兵也算(他正在去贴塔)
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
        int tDR=-1,tUR=-1;                                       // 这座塔在哪
        for(auto&eb:info.enemy_buildings){
            if(eb.SN==tSN){ tDR=eb.BlockDR; tUR=eb.BlockUR; break; }
        }
        if(tDR==-1)continue;
        if(max(abs(tDR-a.BlockDR),abs(tUR-a.BlockUR))>4){         // 还没贴上去 -> 先走到塔边当肉盾
            if(a.NowState==HUMAN_STATE_IDLE){
                int sx=(a.BlockDR>tDR?1:(a.BlockDR<tDR?-1:0));
                int sy=(a.BlockUR>tUR?1:(a.BlockUR<tUR?-1:0));
                HumanMove(a.SN,max(2,min(97,tDR+sx*4))*BLOCKSIDELENGTH,
                                max(2,min(97,tUR+sy*4))*BLOCKSIDELENGTH);
            }
            continue;
        }
        nAssigned++;                                             // 已贴到塔边(肉盾就位)
        if(a.NowState!=HUMAN_STATE_IDLE)continue;
        if(a.WorkObjectSN!=tSN)HumanAction(a.SN,tSN);             // 贴住了再打
    }

    if(nAssigned>0){
        towerReady=(nArmy>0&&nAssigned>=nArmy);                   // 每个兵都贴到位 -> 祭司上
        return;
    }
    towerReady=true;                                             // 塔清光了'''

lines[start:end+1] = enc(NEW)
open(p, 'wb').write(b'\n'.join(lines))
print('written')
