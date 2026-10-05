"""
UsrAI.cpp 离线逐帧仿真器
=========================
目的：在不启动 Qt/游戏的前提下，按 UsrAI.cpp 当前的决策逻辑逐帧推演，找出逻辑死锁 / 流程问题。

地图: map1.njust, rotate=0 (MapRotation 在 rotate==0 时不改动任何坐标)
参照: D:/DDL_AOE/new-aoe3.0.7h/{UsrAI.cpp, config.json, Map.cpp, Core.cpp}

【引擎侧简化假设 —— 仿真精度有限，只保证"逻辑流程"正确，不保证数值】
  A1 单位移动: 直线走向目标, 速度 = config 速度/25 格每帧; 到达 1 格内算到位
  A2 寻路失败/碰撞: **不模拟**。但记录"每帧对同一单位下发的指令数", 用于观察是否原地打转
  A3 采集: 到达资源后每帧采 0.5, 装满 10 走最近同类仓库交货, 背包空后继续
  A4 建造: 下达 HumanBuild 时立即扣木, Percent 按 TIME_BUILD(秒)*25 帧/秒 涨
  A5 战斗: 第一波 6000 帧之前无战斗, 之后简化为"敌人扣血"
  A6 人口: Human_MaxNum = min((房子数+1)*4, 50), 造兵/造人后校验上限
  A7 科技: 造科技只扣资源, 不改变属性(不影响决策路径)
  A8 NowState 重算严格按 Core.cpp:694-710

【严格照抄的部分】UsrAI.cpp 的 processData 全流程与各模块判断条件(含今天落地的 A~E 五批次)
"""
import json, math, sys, io

BS = 35.77708763999664          # BLOCKSIDELENGTH
TF = 40                          # TimePerFrame(ms)
FPS = 1000.0 / TF                # 25 帧/秒

# ---------------- 常量(config.json 摘录) ----------------
INIT_WOOD, INIT_MEAT, INIT_STONE, INIT_GOLD = 200, 200, 150, 0
BUILD_WOOD = {'HOME':30,'GRANARY':120,'STOCK':120,'FARM':75,'MARKET':150,
              'ARMYCAMP':125,'RANGE':150,'STABLE':150,'COLLAGE':180,'DOCK':100}
TIME_BUILD = {'HOME':20,'GRANARY':30,'STOCK':30,'FARM':30,'MARKET':40,
              'ARMYCAMP':30,'RANGE':40,'STABLE':40,'COLLAGE':40}
BUILD_STONE = {'ARROWTOWER':150}
CREATE_FARMER_FOOD = 50
SCOUT_FOOD = 100
FARM_SPEED = 2.4393
SOLDIER_SPEED = {'PRIEST':2.0328,'SCOUT':4.0656,'CAVALRY':4.0656,'CHARIOT':4.0656,
                 'CHARIOT_ARCHER':4.0656,'CLUBMAN':2.4393,'BROADSWORD':2.4393,
                 'COMPOSITE_BOW':2.4393,'BOWMAN':2.4393,'HOPLITE':1.8295,
                 'STONETHROWER':1.6262}
GATHER_PER_FRAME = 0.5
CARRY_LIMIT = 10
HOUSE_HUMAN_NUM = 4
HUMAN_TOP = 50
PRIEST_REST_TIME = 20
UPGRADE_TOOLAGE_FOOD = 500
UPGRADE_BRONZE_FOOD = 800
UPGRADE_TIME = 60
MARKET_WOOD_UP_FOOD, MARKET_WOOD_UP_WOOD = 120, 75
FARM_UP_FOOD, FARM_UP_WOOD = 200, 50      # 驯养动物=犁
GOLD_UP_FOOD, GOLD_UP_WOOD = 120, 100

BT = {0:'HOME',1:'GRANARY',2:'CENTER',3:'STOCK',4:'FARM',5:'MARKET',6:'ARROWTOWER',
      7:'ARMYCAMP',8:'STABLE',9:'RANGE',10:'DOCK',11:'SIEGE',12:'COLLAGE'}
AT = {0:'CLUBMAN',1:'SLINGER',2:'BOWMAN',3:'SCOUT',4:'SWORDSMAN',5:'IMPROVED',
      6:'CAVALRY',7:'SHIP',8:'STONETHROWER',9:'PRIEST',10:'HOPLITE',11:'CHARIOT',
      12:'CHARIOT_ARCHER',13:'BROADSWORD',14:'COMPOSITE_BOW'}
AN = {0:'TREE',1:'GAZELLE',2:'ELEPHANT',3:'LION'}
SR = {0:'BUSH',1:'STONE',2:'GOLDORE',3:'FISH'}

CNT = {'BUSH':150,'TREE':75,'GAZELLE':150,'GOLDORE':400,'STONE':250,'ELEPHANT':300}
GAZELLE_BLOOD = 8
GATHERSPEED = {'BUSH':0.02,'TREE':0.02,'GAZELLE':0.02,'GOLDORE':0.02,'STONE':0.02,'ELEPHANT':0.02}

# ---------------- 地图 ----------------
class World:
    def __init__(self, mappath):
        d = json.load(io.open(mappath, encoding='utf-8'))
        self.terrain = {}          # (dr,ur) -> 1陆地 / 14海洋
        self.explored = set()
        for k, v in d.items():
            if k.startswith('Cell_'):
                self.terrain[(v['BlockDR'], v['BlockUR'])] = v['Type']
        self.land = lambda dr, ur: self.terrain.get((dr,ur),1) != 14
        # 建筑/单位/资源
        self.buildings = []; self.farmers = []; self.armies = []
        self.eres = {}; self.eres_list = []
        self.enemy_buildings = []; self.enemy_armies = []
        for k, v in d.items():
            if k.startswith('Building_'):
                t = BT[v['Num']]
                if v.get('Own') == 'WLH':
                    self.buildings.append(dict(SN=self._sn(), Type=t, dr=v['BlockDR'],
                        ur=v['BlockUR'], Percent=100, Project=0, Cnt=(250 if t=='FARM' else -1), PT=0,
                            Blood={'CENTER':600}.get(t,350), maxblood={'CENTER':600}.get(t,350)))
                else:
                    self.enemy_buildings.append(dict(Type=t, dr=v['BlockDR'], ur=v['BlockUR']))
            elif k.startswith('Human_'):
                p = int(v['DR']/BS+0.5); u = int(v['UR']/BS+0.5)
                if v.get('Own') == 'WLH':
                    if v.get('Sort') == 'Army':
                        self.armies.append(self._unit(AT.get(v['Num'],'PRIEST'), p, u))
                    else:
                        self.farmers.append(self._farmer(p, u))
            elif k.startswith('Animal_'):
                t = AN.get(v['Num'],'TREE')
                self._res(t, int(v['DR']/BS+0.5), int(v['UR']/BS+0.5),
                          GAZELLE_BLOOD if t in ('GAZELLE','LION') else (-1 if t=='TREE' else 45))
            elif k.startswith('StaticRes_'):
                self._res(SR.get(v['Num'],'BUSH'), v['BlockDR'], v['BlockUR'], -1)
        # divideTheMap_commonPlay: 所有 Visible/Explored 清零, 只有市中心 +-8
        for b in self.buildings:
            if b['Type'] == 'CENTER':
                for i in range(-8,9):
                    for j in range(-8,9):
                        if 0<=b['dr']+i<100 and 0<=b['ur']+j<100:
                            self.explored.add((b['dr']+i, b['ur']+j))
        self.Wood, self.Meat, self.Gold, self.Stone = INIT_WOOD, INIT_MEAT, INIT_GOLD, INIT_STONE
        self.frame = 0
        self.log = []
        self.moveLog = {}          # SN -> 最近 200 帧内被下指令的帧号
    _n = [30000]
    def _sn(self):
        self._n[0] += 1; return self._n[0]
    def _unit(self, sort, p, u):
        hp = {'PRIEST':100,'SCOUT':60,'CAVALRY':150,'CHARIOT':100,'CHARIOT_ARCHER':70,
              'CLUBMAN':40,'BROADSWORD':70,'COMPOSITE_BOW':45,'BOWMAN':35,'HOPLITE':120,
              'STONETHROWER':75}.get(sort,50)
        return dict(SN=self._sn(), Sort=sort, dr=p, ur=u, state=0, work=-1,
                    blood=hp, maxblood=hp, cd=0, D0=None, U0=None, atk=int(self._sn()))
    def _farmer(self, p, u):
        return dict(SN=self._sn(), dr=p, ur=u, state=0, work=-1, rsort=-1, rnum=0,
                    D0=None, U0=None)
    def _res(self, t, p, u, blood):
        r = dict(SN=self._sn(), Type=t, dr=p, ur=u, Cnt=CNT[t], Blood=blood, work=-1)
        self.eres[r['SN']] = r; self.eres_list.append(r); return r

# ================= 引擎简化模型 =================
def engine_step(W):
    W.frame += 1
    F = W.frame
    for b in W.buildings:
        if b['Percent'] < 100:
            b['Percent'] = min(100, b['Percent'] + 100.0/(TIME_BUILD.get(b['Type'],30)*FPS))
    # 单位移动
    for a in W.armies:
        sp = SOLDIER_SPEED.get(a['Sort'],2.4)/FPS
        if a['D0'] is not None:
            dd, uu = a['D0']-a['dr'], a['U0']-a['ur']
            dist = math.hypot(dd,uu)
            if dist <= 1.2:
                a['state'] = 0; a['D0']=a['U0']=None
            else:
                a['dr'] += dd/dist*sp; a['ur'] += uu/dist*sp; a['state'] = 1
        if a['Sort']=='PRIEST' and a['cd']>0: a['cd'] -= 1
    for f in W.farmers:
        sp = FARM_SPEED/FPS
        if f['D0'] is not None:
            dd, uu = f['D0']-f['dr'], f['U0']-f['ur']
            dist = math.hypot(dd,uu)
            if dist <= 1.2:
                f['state'] = 0; f['D0']=f['U0']=None
            else:
                f['dr'] += dd/dist*sp; f['ur'] += uu/dist*sp; f['state'] = 1
    # 采集(简化): 农夫 work 指向资源且到位 -> 每帧采 0.5, 每 20 帧结算一次交货
    for f in W.farmers:
        r = W.eres.get(f['work'])
        if r and abs(r['dr']-f['dr'])<=1.5 and abs(r['ur']-f['ur'])<=1.5:
            g = GATHER_PER_FRAME
            if r['Cnt'] > 0:
                r['Cnt'] = max(0, r['Cnt']-g); f['rnum'] += g
                if f['rnum'] >= CARRY_LIMIT or r['Cnt']<=0:
                    self_yield(W, r, f)
    # NowState 重算 (Core.cpp:694-710)
    humans = set()
    for a in W.armies: humans.add(a['SN'])
    for f in W.farmers: humans.add(f['SN'])
    for a in W.armies:
        a['state'] = 3 if (a['work']!=-1 and a['work'] in humans) else (2 if a['work']!=-1 else (1 if a['D0'] is not None else 0))
    for f in W.farmers:
        f['state'] = 3 if (f['work']!=-1 and f['work'] in humans) else (2 if f['work']!=-1 else (1 if f['D0'] is not None else 0))

def self_yield(W, r, f):
    t = r['Type']
    if t in ('BUSH','GAZELLE','ELEPHANT'): W.Meat += f['rnum']
    elif t == 'TREE': W.Wood += f['rnum']
    elif t == 'GOLDORE': W.Gold += f['rnum']
    elif t == 'STONE': W.Stone += f['rnum']
    f['rnum'] = 0
    if r['Cnt'] <= 0: r['work'] = -1     # 采空 -> 引擎会解除关系

def infoShare(W):
    """返回 AI 看到的 info 快照(简化), 并让 betterMap 重建 MAP"""
    MAP = [[0]*100 for _ in range(100)]
    for (dr,ur),t in W.terrain.items():
        if (dr,ur) in W.explored: MAP[dr][ur] = 1 if t != 14 else 0
    for r in W.eres_list:
        if (r['dr'],r['ur']) in W.explored:
            MAP[r['dr']][r['ur']] = {'BUSH':1,'TREE':2,'GAZELLE':4,'ELEPHANT':4,
                                     'LION':4,'GOLDORE':3,'STONE':3,'FISH':5}[r['Type']]+100
    for b in W.buildings:
        sz = 2 if b['Type'] in ('HOME','ARROWTOWER') else 3
        for i in range(sz):
            for j in range(sz):
                if b['dr']+i<100 and b['ur']+j<100: MAP[b['dr']+i][b['ur']+j] = 200
    for b in W.enemy_buildings:
        sz = 2 if b['Type'] in ('HOME','ARROWTOWER') else 3
        for i in range(sz):
            for j in range(sz):
                if b['dr']+i<100 and b['ur']+j<100: MAP[b['dr']+i][b['ur']+j] = 300
    return MAP

# ================= AI: 严格照抄 UsrAI.cpp =================
class AI:
    def __init__(self, W):
        self.W=W; self.MAP=infoShare(W)
        self.centerSN=-1; self.cdr=-1; self.cur=-1
        self.granaryDR=-1; self.granaryUR=-1
        self.oriStockDR=-1; self.oriStockUR=-1
        self.towerSN=-1; self.tdr=-1; self.tur=-1
        self.gazelleState=0; self.gzDR=-1; self.gzUR=-1
        self.hunter1=-1; self.hunter2=-1; self.gzTarget=-1
        self.goldDR=-1; self.goldUR=-1
        self.priestSN=-1; self.pdr=-1; self.pur=-1; self.pstate=-1
        self.priestExploring=True; self.badFrontier={}
        self.getOnlyOnce=False; self.farIsgotten={}
        self.homeBuilderSN=-1; self.homeBuilderState=-1
        self.killGazelle=0; self.storageStarted=False; self.farmNum=0
        self.FARM_MAX_TOOL=4; self.FARM_MAX_BRONZE=16
        self.phaseNum=20; self.phaseChange=False
        self.marketDR=-1; self.marketUR=-1; self.campDR=-1; self.campUR=-1
        self.bushFarmer={}; self.goldFarmer={}; self.woodFarmer={}
        self.PRIEST_HARNESS=5
        self.counterState=0; self.rushRound=0
        self.goHomeFrame=5250; self.gazelleWantNum=6
        self.assignedFrame=-1
        self.phase=-1; self.radius=40; self.curDR=-1; self.curUR=-1
        self.goldSeenOnce=False
        self.homeDR=-1; self.homeUR=-1
        self.dodgeDR=-1; self.dodgeUR=-1
        self.seekDR=-1; self.seekUR=-1
        self.cx=-1; self.cy=-1
        self.tgtDR=-1; self.tgtUR=-1; self.tgtWalked=False
        self.marketTech=0; self.marketTechId=-1
        self.clubUp=False; self.broadTech=False; self.compTech=False; self.logistics=False
        self.armyTechId=-1; self.armyTechType=0; self.rangeTechId=-1
        self.woodWant=3; self.goldWant=3
        self.ev=[]          # 事件流
        self.priestMoves=[] # (frame, kind, dr, ur) kind: 'explore'/'home'/'seek'/'dodge'
        self.FIX_C1B=False    # True=应用祭司横跳修复(回家段+探索段 四分支锁定)
        self.moveDR=-1; self.moveUR=-1; self.moveFrame=-1; self.tgtFrame=-1
    def say(self,f,t): self.ev.append("f%-5d %s"%(f,t))

    # ---------- betterMap / getBaseInfo ----------
    def betterMap(self): self.MAP = infoShare(self.W)
    def getBaseInfo(self):
        W=self.W
        for b in W.buildings:
            if b['Type']=='CENTER': self.centerSN=b['SN']; self.cdr=b['dr']; self.cur=b['ur']
            elif b['Type']=='GRANARY': self.granaryDR=b['dr']; self.granaryUR=b['ur']
            elif b['Type']=='STOCK': self.oriStockDR=b['dr']; self.oriStockUR=b['ur']
            elif b['Type']=='ARROWTOWER' and self.towerSN==-1:
                self.towerSN=b['SN']; self.tdr=b['dr']; self.tur=b['ur']
        self.getOnlyOnce=True

    # ---------- 工具 ----------
    def haveBuilding(self,t): return any(b['Type']==t and b['Percent']>=100 for b in self.W.buildings)
    def usable(self,dr,ur): return 0<=dr<100 and 0<=ur<100 and self.MAP[dr][ur]==1
    def frontier(self,dr,ur):
        if not self.usable(dr,ur): return False
        for dx in range(-2,3):
            for dy in range(-2,3):
                if 0<=dr+dx<100 and 0<=ur+dy<100 and self.MAP[dr+dx][ur+dy]==0: return True
        return False
    def dangerNear(self,dr,ur):
        for a in self.W.armies:
            if a['Sort']=='PRIEST': continue
        for e in self.W.enemy_armies:
            if (e['dr']-dr)**2+(e['ur']-ur)**2 < 64: return True
        for r in self.W.eres_list:
            if r['Type']=='LION' and r['Blood']>0 and (r['dr']-dr)**2+(r['ur']-ur)**2<64: return True
        return False
    def liveGazelleNum(self):
        n=0
        for r in self.W.eres_list:
            if r['Type']=='GAZELLE' and r['Blood']>0 and (r['dr'],r['ur']) in self.W.explored: n+=1
        return n
    def spotBusy(self,dr,ur,size):
        for f in self.W.farmers:
            if dr<=f['dr']<dr+size and ur<=f['ur']<ur+size: return True
        for a in self.W.armies:
            if dr<=a['dr']<dr+size and ur<=a['ur']<ur+size: return True
        return False
    def findBuildSpot(self,bd,bu,size,minR,maxR):
        for dr in range(bd-maxR,bd+maxR+1):
            for ur in range(bu-maxR,bu+maxR+1):
                if dr<0 or dr>=100 or ur<0 or ur>=100: continue
                ok=all(self.MAP[dr+i][ur+j]==1 for i in range(size) for j in range(size))
                if ok and not self.spotBusy(dr,ur,size): return (dr,ur)
        return (-1,-1)
    def findFarmer(self,bd,bu):
        best=-1; bd2=1e18
        for f in self.W.farmers:
            if f['SN']==self.homeBuilderSN: continue
            if f['state']!=0: continue
            if f['SN'] in self.farIsgotten: continue
            d=max(abs(f['dr']-bd),abs(f['ur']-bu))
            if d<bd2: bd2=d; best=f['SN']
        return best
    def farmer(self,sn):
        for f in self.W.farmers:
            if f['SN']==sn: return f
        return None
    def army(self,sn):
        for a in self.W.armies:
            if a['SN']==sn: return a
        return None
    def building(self,sn):
        for b in self.W.buildings:
            if b['SN']==sn: return b
        return None
    def farmerAt(self,sn,dr,ur):
        f=self.farmer(sn)
        return f is not None and abs(f['dr']-dr)<=1 and abs(f['ur']-ur)<=1
    def isLiveGazelle(self,sn):
        r=self.W.eres.get(sn)
        return r is not None and r['Type']=='GAZELLE' and r['Blood']>0
    def findGazelle(self,bd,bu,maxR=1e18):
        best=-1; bd2=1e18; gdr=gur=-1
        for r in self.W.eres_list:
            if r['Type']!='GAZELLE' or r['Blood']<=0: continue
            if (r['dr'],r['ur']) not in self.W.explored: continue
            d=max(abs(r['dr']-bd),abs(r['ur']-bu))
            if d>maxR or d>=bd2: continue
            bd2=d; best=r['SN']; gdr=r['dr']; gur=r['ur']
        return best,gdr,gur
    def checkEnv(self, typ, dr, ur):
        """全局判据(用户已决定用全局 checkEnv): 任意同类资源 5 格内有对应仓库"""
        need = 'GRANARY' if typ=='BUSH' else 'STOCK'
        by=-1
        for r in self.W.eres_list:
            if r['Type']!=typ: continue
            if typ=='GAZELLE' and r['Blood']>0: continue
            if r['Cnt']<=0: continue
            for b in self.W.buildings:
                if b['Type']!=need: continue
                if max(abs(b['dr']-r['dr']),abs(b['ur']-r['ur']))>5: continue
                if b['Percent']>=100: return 2
                if by==-1: by=b['SN']
        return (1 if by!=-1 else 0)
    # ---- 今天落地的三个 helper ----
    def bushDone(self,sn):
        f=self.farmer(sn)
        if f is None: return False
        if f['state']!=0: return False
        if f['rsort']!=-1: return False
        r=self.W.eres.get(f['work'])
        if r and r['Type']=='BUSH' and r['Cnt']>0: return False
        return True
    def farmTeamOnDuty(self):
        farms={b['SN'] for b in self.W.buildings if b['Type']=='FARM'}
        return sum(1 for f in self.W.farmers if f['SN'] in self.bushFarmer and f['work'] in farms)
    def canFarm(self,sn):
        if sn in self.bushFarmer: return self.bushDone(sn)
        if self.W.buildings and self.haveBuilding('FARM'): pass
        if not self.civBronze(): return False
        need=sum(1 for f in self.W.farmers if f['SN'] in self.bushFarmer)
        need=min(need,6)
        return self.farmTeamOnDuty()>=need
    def civBronze(self): return getattr(self,'_civ',1)>=3
    def civToolage(self): return getattr(self,'_civ',1)>=2

    # ================= centerUpgrade (389-408) =================
    def centerUpgrade(self):
        W=self.W
        if self._civ != 3:
            c=self.building(self.centerSN)
            if c and c['Project']==0 and self.haveBuilding('MARKET') and self.haveBuilding('RANGE') \
               and W.Meat>=UPGRADE_BRONZE_FOOD:
                c['Project']=1; c['PT']=UPGRADE_TIME*FPS
                self.say(W.frame,"市镇中心 开始升铜器时代 (肉=%d,需市场+靶场)"%W.Meat)
        if not self.phaseChange and self._civ==3:
            self.phaseNum=25; self.phaseChange=True
            self.say(W.frame,"★进入铜器时代 -> 人口上限放宽到 25")
        for b in W.buildings:
            if b['Type']=='CENTER' and b['Project']==1:
                b['PT']-=1
                if b['PT']<=0:
                    b['Project']=0; self._civ=3
                    self.say(W.frame,"★市镇中心 升铜器时代 完成")

    # ================= priestExplore (533-936, 照抄) =================
    def priestExplore(self):
        W=self.W; a=self.army(self.priestSN)
        if a is None:
            for x in W.armies:
                if x['Sort']=='PRIEST': a=x; self.priestSN=x['SN']; break
            if a is None: return
        self.pdr,self.pur,self.pstate = a['dr'],a['ur'],a['state']
        if not self.priestExploring: return
        DANGER=64
        ex=ey=-1; dist=1e18
        for e in W.enemy_armies:
            d=(e['dr']-a['dr'])**2+(e['ur']-a['ur'])**2
            if d<dist: dist=d; ex=e['dr']; ey=e['ur']
        for r in W.eres_list:
            if r['Type']=='LION' and r['Blood']>0:
                d=(r['dr']-a['dr'])**2+(r['ur']-a['ur'])**2
                if d<dist: dist=d; ex=r['dr']; ey=r['ur']
        if not self.goldSeenOnce:
            for r in W.eres_list:
                if r['Type']=='GOLDORE' and (r['dr'],r['ur']) in W.explored:
                    self.goldSeenOnce=True; self.say(W.frame,"祭司 第一次看见金矿"); break
        # 2. 回家
        if (self.liveGazelleNum()>=self.gazelleWantNum and self.goldSeenOnce) or W.frame>=self.goHomeFrame:
            if W.frame>=self.goHomeFrame: self.goldSeenOnce=True
            if abs(a['dr']-self.tdr)>self.PRIEST_HARNESS or abs(a['ur']-self.tur)>self.PRIEST_HARNESS:
                if self.homeDR==-1:
                    for dx,dy in ((0,1),(1,0),(0,-1),(-1,0)):
                        if self.usable(self.tdr+dx,self.tur+dy): self.homeDR,self.homeUR=self.tdr+dx,self.tur+dy; break
                if self.homeDR!=-1:
                    tdr,tur=self.homeDR,self.homeUR
                    if not self.FIX_C1B:
                        a['D0'],a['U0']=tdr,tur
                        self.priestMoves.append((W.frame,'home',tdr,tur))
                    else:
                        need=False
                        if self.moveDR!=tdr or self.moveUR!=tur: need=True
                        elif not self.usable(tdr,tur): need=True
                        elif W.frame-self.moveFrame>40+max(abs(a['dr']-tdr),abs(a['ur']-tur))*25: need=True
                        if need:
                            self.moveDR,self.moveUR,self.moveFrame=tdr,tur,W.frame
                            a['D0'],a['U0']=tdr,tur
                            self.priestMoves.append((W.frame,'home',tdr,tur))
                return
            self.priestExploring=False
            self.say(W.frame,"★★祭司 回家完成, 退出探索 (瞪羚%d 金矿%s)"%(self.liveGazelleNum(),self.goldSeenOnce))
            return
        # 3. 躲兵 (dodgeDR 方向锁定)
        if ex!=-1 and dist<DANGER and self.priestExploring:
            tdr=tur=-1; d2=-1e18
            for dx,dy in ((1,1),(1,0),(0,1),(-1,0),(-1,-1),(-1,0),(0,-1),(1,-1)):
                nx,ny=a['dr']+dx*6, a['ur']+dy*6
                if not self.usable(nx,ny): continue
                if self.dangerNear(nx,ny): continue
                dd=(nx-ex)**2+(ny-ey)**2
                if dd>d2: d2=dd; tdr=nx; tur=ny
            needNew=False
            if self.dodgeDR==-1: needNew=True
            elif abs(a['dr']-self.dodgeDR)<=1 and abs(a['ur']-self.dodgeUR)<=1: needNew=True
            elif not self.usable(self.dodgeDR,self.dodgeUR): needNew=True
            elif self.dangerNear(self.dodgeDR,self.dodgeUR): needNew=True
            if needNew:
                if tdr!=-1:
                    self.dodgeDR,self.dodgeUR=tdr,tur
                    a['D0'],a['U0']=tdr,tur
                    self.priestMoves.append((W.frame,'dodge',tdr,tur))
                else:
                    self.dodgeDR=self.dodgeUR=-1
        # 4. 追瞪羚 (seekDR 锁定)
        if self.liveGazelleNum()<self.gazelleWantNum or not self.goldSeenOnce:
            gdr=-1; gur=-1; gd=1e18
            for r in W.eres_list:
                isGaz = r['Type']=='GAZELLE'
                isGold = (r['Type']=='GOLDORE' and not self.goldSeenOnce)
                if not isGaz and not isGold: continue
                if isGaz and r['Blood']<=0: continue
                if r['Cnt']<=0: continue
                if (r['dr'],r['ur']) not in W.explored: continue
                d=(r['dr']-a['dr'])**2+(r['ur']-a['ur'])**2
                if d<=36: continue
                if d>1600: continue
                if d<gd: gd=d; gdr=r['dr']; gur=r['ur']
            ox=oy=-1
            if gdr!=-1:
                for dx,dy in ((0,1),(1,0),(0,-1),(-1,0)):
                    if self.usable(gdr+dx,gur+dy) and not self.dangerNear(gdr+dx,gur+dy):
                        ox,oy=gdr+dx,gur+dy; break
            if ox==-1: self.seekDR=self.seekUR=-1
            elif ox!=self.seekDR or oy!=self.seekUR:
                self.seekDR,self.seekUR=ox,oy
                a['D0'],a['U0']=ox,oy
                self.priestMoves.append((W.frame,'seek',ox,oy))
                return
            else:
                return
        # 5. 常规探索选点
        bd=bu=-1; dmin=1e18
        if self.phase==-1:
            if self.cx==-1: self.cx,self.cy=self.cdr,self.cur
            elif self.cx==self.cdr: self.cx,self.cy=self.granaryDR,self.granaryUR
            elif self.cx==self.granaryDR: self.cx,self.cy=self.oriStockDR,self.oriStockUR
            for dr in range(2,98):
                for ur in range(2,98):
                    if max(abs(dr-self.cx),abs(ur-self.cy))>30: continue
                    if not self.frontier(dr,ur): continue
                    if dr*100+ur in self.badFrontier: continue
                    if self.dangerNear(dr,ur): continue
                    d2=(dr-a['dr'])**2+(ur-a['ur'])**2
                    if d2<dmin: dmin=d2; bd,bu=dr,ur
            if bd==-1: self.phase=0
        elif self.phase==0:
            drMin = 2 if self.cdr<50 else self.cdr; drMax = self.cdr if self.cdr<50 else 97
            urMin = 2 if self.cur<50 else self.cur; urMax = self.cur if self.cur<50 else 97
            for dr in range(drMin,drMax+1):
                for ur in range(urMin,urMax+1):
                    if not self.frontier(dr,ur): continue
                    if dr*100+ur in self.badFrontier: continue
                    if self.dangerNear(dr,ur): continue
                    d2=(dr-a['dr'])**2+(ur-a['ur'])**2
                    if d2<dmin: dmin=d2; bd,bu=dr,ur
            if bd==-1: self.phase=1; self.radius=40; self.say(W.frame,"祭司 探完自己那一角 -> 转绕中心")
        else:
            for rr in range(self.radius,3,-1):
                bd=bu=-1; dmin=1e18
                for dr in range(2,98):
                    for ur in range(2,98):
                        dd=max(abs(dr-50),abs(ur-50))
                        if dd<rr-1 or dd>rr+1: continue
                        if not self.frontier(dr,ur): continue
                        if dr*100+ur in self.badFrontier: continue
                        if self.dangerNear(dr,ur): continue
                        d2=(dr-a['dr'])**2+(ur-a['ur'])**2
                        if d2<dmin: dmin=d2; bd,bu=dr,ur
                if bd!=-1: self.radius=rr; break
            if bd==-1:
                if self.liveGazelleNum()<self.gazelleWantNum: self.radius=40
                else: self.phase=2; self.say(W.frame,"祭司 探索阶段2 结束")
        if bd==-1: return
        if bd!=self.tgtDR or bu!=self.tgtUR:
            self.tgtDR,self.tgtUR=bd,bu
            a['D0'],a['U0']=bd,bu
            self.priestMoves.append((W.frame,'explore',bd,bu))
            return
    def huntGazelle(self):
        W=self.W
        if self.gazelleState==4: return
        if self.gazelleState==0:
            if self.liveGazelleNum()<self.gazelleWantNum: return
            sn,gdr,gur=self.findGazelle(self.cdr,self.cur)
            if sn==-1: return
            self.gzDR,self.gzUR=gdr,gur
            for dx,dy in ((0,1),(1,0),(0,-1),(-1,0)):
                nr,nu=gdr+dx,gur+dy
                if 0<=nr<100 and 0<=nu<100 and self.MAP[nr][nu]==1:
                    self.gzDR,self.gzUR=nr,nu; break
            if self.hunter1==-1: self.hunter1=self.findFarmer(self.gzDR,self.gzUR)
            if self.hunter1==-1: return
            self.farIsgotten[self.hunter1]=True
            if self.hunter2==-1: self.hunter2=self.findFarmer(self.gzDR,self.gzUR)
            if self.hunter2==-1: return
            self.farIsgotten[self.hunter2]=True
            f1,f2=self.farmer(self.hunter1),self.farmer(self.hunter2)
            f1['D0'],f1['U0']=self.gzDR,self.gzUR
            f2['D0'],f2['U0']=self.gzDR-1,self.gzUR
            self.gazelleState=1
            self.say(W.frame,"打猎 state0->1 派两名猎人(%d,%d)去(%d,%d)"%(self.hunter1,self.hunter2,self.gzDR,self.gzUR))
            return
        if self.gazelleState==1:
            self.farIsgotten[self.hunter1]=True; self.farIsgotten[self.hunter2]=True
            if W.frame%400==0:
                f1=self.farmer(self.hunter1); f2=self.farmer(self.hunter2)
                self.say(W.frame,"  [诊断] 猎场=(%d,%d) h1 pos=(%.2f,%.2f) D0=%s | h2 pos=(%.2f,%.2f) D0=%s"
                    %(self.gzDR,self.gzUR,f1['dr'],f1['ur'],f1['D0'],f2['dr'],f2['ur'],f2['D0']))
            if not self.farmerAt(self.hunter1,self.gzDR,self.gzUR): return
            if not self.farmerAt(self.hunter2,self.gzDR,self.gzUR): return
            self.gazelleState=2
            self.say(W.frame,"打猎 state1->2 两名猎人到位, 开始猎")
        if self.gazelleState==2:
            self.farIsgotten[self.hunter1]=True; self.farIsgotten[self.hunter2]=True
            if not self.isLiveGazelle(self.gzTarget):
                sn,gdr,gur=self.findGazelle(self.gzDR,self.gzUR,20)
                self.gzTarget=sn
                if sn==-1 or self.killGazelle>=6:
                    self.gazelleState=3
                    self.say(W.frame,"打猎 state2->3 结束(杀了%d只), 去建仓库"%self.killGazelle)
                    return
                f1,f2=self.farmer(self.hunter1),self.farmer(self.hunter2)
                f1['work']=f2['work']=sn; f1['D0']=f1['U0']=None; f2['D0']=f2['U0']=None
                r=W.eres[sn]; r['work']=self.hunter1
                self.killGazelle+=1
            return
        if self.gazelleState==3:
            by=-1; st=self.checkEnv('GAZELLE',0,0)
            if st==2:
                self.gazelleState=4; return
            if W.Wood<BUILD_WOOD['STOCK']:
                self.assignWoodcutter(); return
            ox,oy=self.findBuildSpot(self.gzDR,self.gzUR,3,2,4)
            if ox!=-1:
                W.Wood-=BUILD_WOOD['STOCK']
                W.buildings.append(dict(SN=W._sn(),Type='STOCK',dr=ox,ur=oy,Percent=0,Project=0,Cnt=-1,PT=0))
                self.storageStarted=True
                f=self.farmer(self.hunter1)
                if f: f['work']=self.building(self.hunter1) if False else f['work']
                self.gazelleState=4
                self.say(W.frame,"★猎人仓库 下令建造 于(%d,%d) 木=%d"%(ox,oy,W.Wood))
                return
            self.gazelleState=4
            self.say(W.frame,"★猎人仓库 找不到位置, 放弃(永久)")
            return

    # ================= assignWoodcutter (照抄思路) =================
    def assignWoodcutter(self):
        W=self.W
        best=-1; bt=-1; bdr=bur=-1; bd2=1e18
        for b in W.buildings:
            if b['Type']!='STOCK' or b['Percent']<100: continue
            for r in W.eres_list:
                if r['Type']!='TREE' or r['Cnt']<=0: continue
                if not self.woodReachable(r['dr'],r['ur']): continue
                d=max(abs(r['dr']-b['dr']),abs(r['ur']-b['ur']))
                if d<bd2: bd2=d; best=r['SN']; bdr,bur=r['dr'],r['ur']
        if best==-1: return -1
        fs=self.findFarmer(bdr,bur)
        if fs==-1: return -1
        f=self.farmer(fs); f['work']=best; f['D0']=f['U0']=None
        W.eres[best]['work']=fs
        self.woodFarmer[fs]=True
        return fs
    def woodReachable(self,dr,ur):
        for dx in range(-1,2):
            for dy in range(-1,2):
                if self.MAP[dr+dx][ur+dy]==1: return True
        return False
    def assignGoldMiner(self):
        W=self.W; best=-1; bdr=bur=-1; bd2=1e18
        for b in W.buildings:
            if b['Type']!='STOCK' or b['Percent']<100: continue
            for r in W.eres_list:
                if r['Type']!='GOLDORE' or r['Cnt']<=0: continue
                d=max(abs(r['dr']-b['dr']),abs(r['ur']-b['ur']))
                if d<bd2: bd2=d; best=r['SN']; bdr,bur=r['dr'],r['ur']
        if best==-1: return -1
        fs=self.findFarmer(bdr,bur)
        if fs==-1: return -1
        f=self.farmer(fs); f['work']=best; f['D0']=f['U0']=None
        W.eres[best]['work']=fs
        self.goldFarmer[fs]=True
        return fs

    # ================= buildGoldStock (1772, 当前版本: 全局 checkEnv) =================
    def buildGoldStock(self):
        W=self.W
        st=self.checkEnv('GOLDORE',0,0)
        if st!=0: return False
        if self.goldDR==-1:
            best=1e18
            for r in W.eres_list:
                if r['Type']!='GOLDORE' or r['Cnt']<=0: continue
                busy=any(f['work']==r['SN'] for f in W.farmers)
                if busy: continue
                d=max(abs(r['dr']-self.cdr),abs(r['ur']-self.cur))
                if d<best: best=d; self.goldDR,self.goldUR=r['dr'],r['ur']
            if self.goldDR==-1: return False
        if W.Wood<BUILD_WOOD['STOCK']: self.assignWoodcutter(); return False
        if self.storageStarted: return False
        ox,oy=self.findBuildSpot(self.goldDR,self.goldUR,3,2,4)
        if ox==-1: return False
        fs=self.findFarmer(self.goldDR,self.goldUR)
        if fs==-1: return False
        W.Wood-=BUILD_WOOD['STOCK']
        W.buildings.append(dict(SN=W._sn(),Type='STOCK',dr=ox,ur=oy,Percent=0,Project=0,Cnt=-1,PT=0))
        self.storageStarted=True
        self.say(W.frame,"金矿仓库 下令建造 于(%d,%d) 木=%d"%(ox,oy,W.Wood))
        return True

    # ================= manageBuild (1107-, 照抄) =================
    def gethomeBuilder(self):
        for f in self.W.farmers:
            if f['SN']==self.homeBuilderSN: self.homeBuilderState=f['state']
    def humanMax(self):
        h=sum(1 for b in self.W.buildings if b['Type']=='HOME')
        c=1 if any(b['Type']=='CENTER' for b in self.W.buildings) else 0
        return min((h+c)*HOUSE_HUMAN_NUM, HUMAN_TOP)
    def manageBuild(self):
        W=self.W; F=W.frame
        self.gethomeBuilder()
        maxNum=self.humanMax(); haveNum=len(W.farmers)+len(W.armies)
        spaceNum=maxNum-haveNum
        farmLimit = self.FARM_MAX_BRONZE if self._civ>=3 else self.FARM_MAX_TOOL
        stockNum=sum(1 for b in W.buildings if b['Type']=='STOCK' and b['Percent']>=100)
        self.marketGate = (stockNum>=2 or self.gazelleState==4)
        goldReserve=0
        if self._civ>=3 and self.goldDR!=-1:
            if self.checkEnv('GOLDORE',0,0)==0: goldReserve=BUILD_WOOD['STOCK']
        # 造人
        if haveNum<self.phaseNum and W.Meat>=CREATE_FARMER_FOOD:
            c=self.building(self.centerSN)
            if c and c['Project']==0:
                c['Project']=2; c['PT']=20*FPS
                W.Meat-=CREATE_FARMER_FOOD
                self.say(F,"市镇中心 造农民(人口 %d/%d, phaseNum=%d)"%(haveNum,maxNum,self.phaseNum))
        for b in W.buildings:
            if b['Type']=='CENTER' and b['Project']==2:
                b['PT']-=1
                if b['PT']<=0:
                    b['Project']=0
                    W.farmers.append(W._farmer(self.cdr,self.cur))
        # 修箭塔
        for b in W.buildings:
            if b['Type']!='ARROWTOWER': continue
            if b['Blood']>=b['maxblood']: continue
            if any(f['work']==b['SN'] for f in W.farmers): continue
            fs=self.findFarmer(b['dr'],b['ur'])
            if fs!=-1:
                f=self.farmer(fs); f['work']=b['SN']; f['D0']=f['U0']=None
                self.farIsgotten[fs]=True
            break
        # 建房子
        if spaceNum<=3 and maxNum<50 and W.Wood>=BUILD_WOOD['HOME'] and self.homeBuilderState==0:
            for b in W.buildings:
                if b['Type']!='HOME': continue
                for dx,dy in ((-2,0),(0,2),(2,0),(0,-2)):
                    ndr,nur=b['dr']+dx,b['ur']+dy
                    if all(self.MAP[ndr+i][nur+j]==1 for i in range(2) for j in range(2)):
                        W.Wood-=BUILD_WOOD['HOME']
                        W.buildings.append(dict(SN=W._sn(),Type='HOME',dr=ndr,ur=nur,Percent=0,Project=0,Cnt=-1,PT=0))
                        f=self.farmer(self.homeBuilderSN)
                        if f: f['work']=-1
                        self.say(F,"建房 于(%d,%d) 木=%d"%(ndr,nur,W.Wood))
                        break
                break
        # ---- 木头闸门 ----
        if len(self.woodFarmer)>=1:
            # 市场科技
            if self.marketTech<3 and self.marketTechId<0 and self.haveBuilding('MARKET'):
                for b in W.buildings:
                    if b['Type']!='MARKET' or b['Percent']<100 or b['Project']!=0: continue
                    if self.marketTech==0 and W.Wood>=MARKET_WOOD_UP_WOOD and W.Meat>=MARKET_WOOD_UP_FOOD:
                        self.marketTechId=self._tech(b,'MARKET_WOOD',MARKET_WOOD_UP_FOOD,MARKET_WOOD_UP_WOOD)
                    elif self.marketTech==1 and self._civ==3 and W.Meat>=GOLD_UP_FOOD and W.Wood>=GOLD_UP_WOOD:
                        self.marketTechId=self._tech(b,'GOLD_UP',GOLD_UP_FOOD,GOLD_UP_WOOD)
                    elif self.marketTech==2 and self._civ==3 and W.Meat>=FARM_UP_FOOD and W.Wood>=FARM_UP_WOOD:
                        self.marketTechId=self._tech(b,'FARM_UP',FARM_UP_FOOD,FARM_UP_WOOD)
                    break
            if self.marketTechId>=0:
                self.say(F,"市场科技 marketTech=%d 研究完成(回执)"%self.marketTech)
                self.marketTech+=1; self.marketTechId=-1
            # 有在建的
            bs=-1
            for b in W.buildings:
                if b['Percent']>=100: continue
                if b['Type'] not in ('MARKET','ARMYCAMP','RANGE','STABLE','COLLAGE'): continue
                bs=b['SN']; break
            if bs!=-1:
                sent=0
                for f in W.farmers:
                    if sent>=2: break
                    if f['SN']==self.homeBuilderSN: continue
                    if f['state']!=0: continue
                    if f['SN'] in self.farIsgotten: continue
                    f['work']=bs; f['D0']=f['U0']=None
                    self.farIsgotten[f['SN']]=True; sent+=1
            want=-1; cost=0; bd,bu=self.cdr,self.cur
            if not self.haveBuilding('MARKET'):
                if self.marketGate: want='MARKET'; cost=BUILD_WOOD['MARKET']
                else: self._gateLog=(stockNum,self.gazelleState,W.Wood)
            elif not self.haveBuilding('ARMYCAMP'):
                want='ARMYCAMP'; cost=BUILD_WOOD['ARMYCAMP']
                if self.marketDR!=-1: bd,bu=self.marketDR,self.marketUR
            elif not self.haveBuilding('RANGE'):
                want='RANGE'; cost=BUILD_WOOD['RANGE']
                if self.campDR!=-1: bd,bu=self.campDR,self.campUR
            elif self._civ>=2 and not self.haveBuilding('STABLE'):
                want='STABLE'; cost=BUILD_WOOD['STABLE']
                if self.campDR!=-1: bd,bu=self.campDR,self.campUR
            elif self._civ>=3 and not self.haveBuilding('COLLAGE') and self.haveBuilding('STABLE'):
                want='COLLAGE'; cost=BUILD_WOOD['COLLAGE']
            if want!=-1 and W.Wood<cost: self.assignWoodcutter()
            if want!=-1 and W.Wood>=cost:
                for f in W.farmers:
                    if f['SN']==self.homeBuilderSN: continue
                    if f['state']!=0: continue
                    if f['SN'] in self.farIsgotten: continue
                    ox,oy=self.findBuildSpot(bd,bu,3,2,5)
                    if ox==-1: break
                    W.Wood-=cost
                    nb=dict(SN=W._sn(),Type=want,dr=ox,ur=oy,Percent=0,Project=0,Cnt=(250 if want=='FARM' else -1),PT=0)
                    W.buildings.append(nb); f['work']=nb['SN']; f['D0']=f['U0']=None
                    self.farIsgotten[f['SN']]=True
                    if want=='MARKET': self.marketDR,self.marketUR=ox,oy
                    if want=='ARMYCAMP': self.campDR,self.campUR=ox,oy
                    self.say(F,"★下令建造 %s 于(%d,%d) 木=%d"%(want,ox,oy,W.Wood))
                    break
        # 农田段
        if W.Meat<800 and self.haveBuilding('MARKET') and self.granaryDR!=-1 and W.Wood>=BUILD_WOOD['FARM']+goldReserve:
            self.farmNum=sum(1 for b in W.buildings if b['Type']=='FARM' and b['Cnt']>0)
            if self.farmNum<farmLimit:
                for f in W.farmers:
                    if f['SN']==self.homeBuilderSN or f['state']!=0: continue
                    if f['SN'] in self.farIsgotten: continue
                    if not self.canFarm(f['SN']): continue
                    ox,oy=self.findBuildSpot(self.granaryDR,self.granaryUR,3,2,5)
                    if ox==-1: break
                    W.Wood-=BUILD_WOOD['FARM']
                    nb=dict(SN=W._sn(),Type='FARM',dr=ox,ur=oy,Percent=0,Project=0,Cnt=250,PT=0)
                    W.buildings.append(nb); f['work']=nb['SN']; f['D0']=f['U0']=None
                    self.farIsgotten[f['SN']]=True
                    self.say(F,"★农田 下令建造 于(%d,%d)"%(ox,oy))
                    break
        # b 段: 花名册成员自己那丛没了 -> 去种田/建田
        farms={b['SN'] for b in W.buildings if b['Type']=='FARM' and b['Percent']>=100 and b['Cnt']>0}
        sent=0
        for f in list(W.farmers):
            if sent>=2: break
            if f['SN'] not in self.bushFarmer: continue
            r=W.eres.get(f['work'])
            if r and r['Type']=='BUSH' and r['Cnt']>0:
                continue
            if f.get('rnum',0)>0: continue
            if f['state']!=0: continue
            if f['SN']==self.homeBuilderSN: continue
            if f['SN'] in self.farIsgotten: continue
            ff=-1
            for bsn in farms:
                if not any(x['work']==bsn for x in W.farmers): ff=bsn; break
            if ff!=-1:
                f['work']=ff; f['D0']=f['U0']=None
                self.farIsgotten[f['SN']]=True; sent+=1
                continue
            self.farmNum=sum(1 for b in W.buildings if b['Type']=='FARM' and b['Cnt']>0)
            if self.farmNum>=farmLimit or W.Wood<BUILD_WOOD['FARM']+goldReserve: continue
            ox,oy=self.findBuildSpot(self.granaryDR,self.granaryUR,3,2,5)
            if ox==-1: continue
            W.Wood-=BUILD_WOOD['FARM']
            nb=dict(SN=W._sn(),Type='FARM',dr=ox,ur=oy,Percent=0,Project=0,Cnt=250,PT=0)
            W.buildings.append(nb); f['work']=nb['SN']; f['D0']=f['U0']=None
            self.farIsgotten[f['SN']]=True; sent+=1
        # c 段: 已建好无人种的田
        sent=0
        for bsn in farms:
            if sent>=2: break
            if any(x['work']==bsn for x in W.farmers): continue
            pick=-1
            for f in W.farmers:
                if f['SN']==self.homeBuilderSN: continue
                if f['SN'] in self.farIsgotten: continue
                if f.get('rnum',0)>0: continue
                if f['SN'] not in self.bushFarmer: continue
                if not self.bushDone(f['SN']): continue
                pick=f['SN']; break
            if pick!=-1:
                f=self.farmer(pick); f['work']=bsn; f['D0']=f['U0']=None
                self.farIsgotten[pick]=True; sent+=1; continue
            for f in W.farmers:
                if f['SN']==self.homeBuilderSN or f['state']!=0: continue
                if f['SN'] in self.farIsgotten: continue
                if not self.canFarm(f['SN']): continue
                f['work']=bsn; f['D0']=f['U0']=None
                self.farIsgotten[f['SN']]=True; sent+=1; break
        # d 段: 兜底 空田 -> 挖金
        sent=0
        for f in self.W.farmers:
            if sent>=2: break
            if f['SN']==self.homeBuilderSN: continue
            if f['state']!=0: continue
            if f['SN'] in self.farIsgotten: continue
            done=False
            if self.canFarm(f['SN']):
                for bsn in farms:
                    if any(x['work']==bsn for x in W.farmers): continue
                    f['work']=bsn; f['D0']=f['U0']=None
                    self.farIsgotten[f['SN']]=True; sent+=1; done=True; break
            if done: continue
            if self._civ>=3:
                g=0
                for x in W.farmers:
                    rr=W.eres.get(x['work'])
                    if rr and rr['Type']=='GOLDORE' and x['state'] in (1,2): g+=1
                if g>=self.goldWant: continue
                t=-1; bd2=1e18
                for r in W.eres_list:
                    if r['Type']!='GOLDORE' or r['Cnt']<=0: continue
                    if any(x['work']==r['SN'] for x in W.farmers): continue
                    d=max(abs(r['dr']-f['dr']),abs(r['ur']-f['ur']))
                    if d<bd2: bd2=d; t=r['SN']
                if t!=-1:
                    f['work']=t; f['D0']=f['U0']=None
                    W.eres[t]['work']=f['SN']; self.goldFarmer[f['SN']]=True
                    self.farIsgotten[f['SN']]=True; sent+=1
    def _tech(self,b,name,food,wood):
        self.W.Meat-=food; self.W.Wood-=wood
        self.say(self.W.frame,"市场科技 %s 开始研究"%name)
        return 1

    # ================= trainArmy (照抄) =================
    def trainArmy(self):
        W=self.W
        club=sum(1 for a in W.armies if a['Sort']=='CLUBMAN')
        bow=sum(1 for a in W.armies if a['Sort']=='BOWMAN')
        rsM=rsG=rsW=0
        for b in W.buildings:
            if b['Percent']<100 or b['Project']!=0: continue
            if b['Type']=='ARMYCAMP':
                if not self.clubUp: rsM+=100
                elif not self.broadTech: rsM+=140; rsG+=50
                elif not self.logistics: rsM+=180; rsG+=100
            if b['Type']=='RANGE' and not self.compTech:
                rsM+=180; rsW+=100
        maxNum=self.humanMax(); haveNum=len(W.farmers)+len(W.armies)
        for b in W.buildings:
            if b['Percent']<100 or b['Project']!=0: continue
            if b['Type']=='ARMYCAMP':
                if not self.clubUp and self.armyTechId<0 and W.Meat>=100:
                    W.Meat-=100; b['Project']=3; b['PT']=40*FPS
                    self.armyTechId=1; self.armyTechType=1
                    self.say(W.frame,"兵营 开始研究 战斧")
                elif self.clubUp and not self.broadTech and self.armyTechId<0 and self._civ>=3 and W.Meat>=140 and W.Gold>=50:
                    W.Meat-=140; W.Gold-=50; b['Project']=3; b['PT']=40*FPS
                    self.armyTechId=1; self.armyTechType=2
                    self.say(W.frame,"兵营 开始研究 阔剑")
                elif self.broadTech and not self.logistics and self.armyTechId<0 and W.Meat>=180 and W.Gold>=100:
                    W.Meat-=180; W.Gold-=100; b['Project']=3; b['PT']=60*FPS
                    self.armyTechId=1; self.armyTechType=3
                    self.say(W.frame,"兵营 开始研究 后勤")
                elif club<2 and W.Meat>=50+rsM:
                    W.Meat-=50+rsM; b['Project']=4; b['PT']=27*FPS
                    club+=1
                    self.say(W.frame,"兵营 造斧兵")
            if b['Type']=='COLLAGE':
                if W.Meat>=60+rsM and W.Gold>=40+rsG:
                    W.Meat-=60+rsM; W.Gold-=40+rsG; b['Project']=5; b['PT']=36*FPS
                    self.say(W.frame,"学院 造方阵兵")
            if b['Type']=='RANGE':
                if not self.compTech and bow<2 and W.Meat>=40+rsM and W.Wood>=20+rsW:
                    W.Meat-=40+rsM; W.Wood-=20+rsW; b['Project']=6; b['PT']=30*FPS
                    bow+=1; self.say(W.frame,"靶场 造弓箭手")
                if self._civ>=3 and not self.compTech and self.rangeTechId<0:
                    if W.Meat>=180 and W.Wood>=100:
                        W.Meat-=180; W.Wood-=100; b['Project']=7; b['PT']=40*FPS
                        self.rangeTechId=1
                        self.say(W.frame,"靶场 开始研究 复合弓")
                if self.compTech and W.Meat>=40+rsM and W.Gold>=20+rsG:
                    W.Meat-=40+rsM; W.Gold-=20+rsG; b['Project']=8; b['PT']=30*FPS
                    self.say(W.frame,"靶场 造复合弓(人口 %d/%d)"%(haveNum,maxNum))
        for b in W.buildings:
            if b['Project']==3 and b['PT']>0:
                b['PT']-=1
                if b['PT']<=0:
                    b['Project']=0
                    if b['Type']=='ARMYCAMP':
                        if self.armyTechType==1: self.clubUp=True; self.say(W.frame,"★战斧研究完成")
                        elif self.armyTechType==2: self.broadTech=True; self.say(W.frame,"★阔剑研究完成")
                        else: self.logistics=True; self.say(W.frame,"★后勤研究完成(兵营0.5人口)")
                    else: self.compTech=True; self.say(W.frame,"★复合弓研究完成")
                    if b['Type']=='RANGE': self.rangeTechId=-1
                    if b['Type']=='ARMYCAMP': self.armyTechId=-1
            elif b['Project'] in (4,5,6,8) and b['PT']>0:
                b['PT']-=1
                if b['PT']<=0:
                    b['Project']=0
                    s={'ARMYCAMP':'CLUBMAN','COLLAGE':'HOPLITE','RANGE':'COMPOSITE_BOW' if self.compTech else 'BOWMAN'}[b['Type']]
                    W.armies.append(W._unit(s,b['dr'],b['ur']))
                    self.say(W.frame,"  >> %s 出厂"%(s))
            elif b['Project']==7 and b['PT']>0:
                b['PT']-=1
                if b['PT']<=0: b['Project']=0; self.compTech=True; self.rangeTechId=-1; self.say(W.frame,"★复合弓研究完成")

    # ================= 资源分配循环 (processData 218-374) =================
    def resourceLoop(self):
        W=self.W; F=W.frame
        resWorkers={}
        for f in W.farmers:
            if f['work'] in W.eres: resWorkers[f['work']]=resWorkers.get(f['work'],0)+1
        bushNow=sum(1 for f in W.farmers if f['state'] in (1,2) and (W.eres.get(f['work']) or {}).get('Type')=='BUSH')
        gazNow=sum(1 for f in W.farmers if f['state'] in (1,2) and (W.eres.get(f['work']) or {}).get('Type')=='GAZELLE')
        for r in W.eres_list:
            if r['Type']=='BUSH' and (bushNow>=6 or len(self.bushFarmer)>=6): continue
            if r['Type']=='GAZELLE' and gazNow>=6: continue
            if F-self.assignedFrame<19: break
            if r['Type']=='BUSH' or (r['Type']=='GAZELLE' and self.gazelleState==4):
                if r['Type']=='GAZELLE' and r['Blood']>0: continue
                if r['Cnt']<=0: continue
                if resWorkers.get(r['SN'],0)>=1: continue
                fs=self.findFarmer(r['dr'],r['ur'])
                if fs==-1: continue
                st=self.checkEnv(r['Type'],r['dr'],r['ur'])
                if r['Type']=='GAZELLE': st=2
                f=self.farmer(fs)
                if st==2:
                    f['work']=r['SN']; f['D0']=f['U0']=None; r['work']=fs
                    f['rsort']=1 if r['Type']=='BUSH' else 2
                    if r['Type']=='BUSH': self.bushFarmer[fs]=True
                else: continue
                self.farIsgotten[fs]=True; self.assignedFrame=F
                break
        # 伐木补人
        woodNow=sum(1 for f in W.farmers if f['state'] in (1,2) and (W.eres.get(f['work']) or {}).get('Type')=='TREE')
        self.woodWant = 2 if self.haveBuilding('COLLAGE') else 3
        for i in range(woodNow,self.woodWant):
            if self.assignWoodcutter()==-1: break
        # 采金
        if self._civ>=3:
            goldNow=sum(1 for f in W.farmers if f['state'] in (1,2) and (W.eres.get(f['work']) or {}).get('Type')=='GOLDORE')
            self.goldWant = 4 if self.haveBuilding('COLLAGE') else 3
            for i in range(goldNow,self.goldWant):
                if self.assignGoldMiner()==-1: break

    # ================= processData =================
    def processData(self):
        W=self.W
        self.centerUpgrade()
        self.farIsgotten={}
        if self.gazelleState<4:
            if self.hunter1!=-1: self.farIsgotten[self.hunter1]=True
            if self.hunter2!=-1: self.farIsgotten[self.hunter2]=True
        if not self.getOnlyOnce: self.getBaseInfo()
        self.betterMap()
        self.priestExplore()
        if self.homeBuilderSN==-1:
            for f in W.farmers:
                if f['state']==0: self.homeBuilderSN=f['SN']; self.gethomeBuilder(); break
        self.storageStarted=False
        if self._civ>=3: self.buildGoldStock()
        self.manageBuild()
        self.trainArmy()
        self.storageStarted=False
        self.huntGazelle()
        self.resourceLoop()
        # 新建筑完成检测(每个 SN 只报一次)
        if not hasattr(self,'_ann'): self._ann=set()
        for b in W.buildings:
            if b['SN'] in self._ann: continue
            if b['Percent']>=100 and b['PT']<=0 and b['Type'] in ('MARKET','ARMYCAMP','RANGE','STABLE','COLLAGE','STOCK','FARM','HOME'):
                self._ann.add(b['SN'])
                self.say(W.frame,"  << %s 建好 于(%d,%d)" % (b['Type'],b['dr'],b['ur']))

# ================= 主循环 =================
def updateVision(W):
    """视野: 实际半径 = visionLen-1 (Coordinate.cpp:93-154), vision<=4 时是圆角方"""
    for a in W.armies:
        R={'PRIEST':11,'SCOUT':7,'CAVALRY':3,'CHARIOT':3,'CHARIOT_ARCHER':8,'CLUBMAN':3,
           'BROADSWORD':3,'COMPOSITE_BOW':8,'BOWMAN':6,'HOPLITE':3,'STONETHROWER':12}.get(a['Sort'],3)
        for dr in range(-R,R+1):
            for ur in range(-R,R+1):
                if abs(dr)+abs(ur)>R+2: continue
                p,q=int(a['dr'])+dr,int(a['ur'])+ur
                if 0<=p<100 and 0<=q<100: W.explored.add((p,q))
    for b in W.buildings:
        R={'CENTER':3,'ARROWTOWER':9,'HOME':3,'GRANARY':3,'STOCK':3,'MARKET':3,
           'ARMYCAMP':3,'RANGE':3,'STABLE':3,'COLLAGE':3,'FARM':3}.get(b['Type'],3)
        for dr in range(-R,R+1):
            for ur in range(-R,R+1):
                p,q=int(b['dr'])+dr,int(b['ur'])+ur
                if 0<=p<100 and 0<=q<100: W.explored.add((p,q))
    for f in W.farmers:
        for dr in range(-3,4):
            for ur in range(-3,4):
                p,q=int(f['dr'])+dr,int(f['ur'])+ur
                if 0<=p<100 and 0<=q<100: W.explored.add((p,q))

def snapshot(ai,W):
    roles={}
    for f in W.farmers:
        r=W.eres.get(f['work']); k='闲' if f['work']==-1 else r['Type']
        if f['SN'] in ai.bushFarmer and k!='BUSH': k+='(浆果队)'
        if f['SN'] in ai.woodFarmer: k='伐木'
        if f['SN'] in ai.goldFarmer: k='挖金'
        if f['SN']==ai.hunter1 or f['SN']==ai.hunter2: k='猎人'
        if f['SN']==ai.homeBuilderSN: k='建房者'
        roles[k]=roles.get(k,0)+1
    bl=[]
    for b in W.buildings:
        if b['Type'] in ('CENTER','GRANARY','STOCK','ARROWTOWER','MARKET','ARMYCAMP','RANGE','STABLE','COLLAGE'):
            bl.append(b['Type']+('*' if b['Percent']<100 else '')+'(%d,%d)'%(b['dr'],b['ur']))
    a=ai.army(ai.priestSN)
    pz = '(%d,%d)%s'%(a['dr'],a['ur'],'探索' if ai.priestExploring else '待命') if a else '-'
    return ("f%-5d 木%-4d 肉%-4d 金%-4d 人口%2d/%2d 祭司%s 瞪羚活%d 已探%4d | %s | %s"
            %(W.frame,W.Wood,W.Meat,W.Gold,len(W.farmers)+len(W.armies),ai.humanMax(),pz,
              ai.liveGazelleNum(),len(W.explored),
              ' '.join('%s:%d'%(k,v) for k,v in sorted(roles.items())),
              ' '.join(bl)))

def main():
    W=World('../new-aoe3.0.7h/map1.njust')
    ai=AI(W); ai._civ=2      # config.json DefaultCivilization=2 工具时代
    ai.FIX_C1B = (len(sys.argv)>2 and sys.argv[2]=='fix')
    ai.MAP=infoShare(W); ai.getBaseInfo()
    for b in W.buildings: b['maxblood']=350
    print("=== 初始 ===")
    print("市中心(%d,%d) 谷仓(%d,%d) 仓库(%d,%d) 箭塔(%d,%d) 人口上限=%d 农民=%d 祭司=%s"
          %(ai.cdr,ai.cur,ai.granaryDR,ai.granaryUR,ai.oriStockDR,ai.oriStockUR,ai.tdr,ai.tur,
            ai.humanMax(),len(W.farmers),(W.armies[0]['dr'],W.armies[0]['ur'])))
    print("浆果丛 %d 处, 瞪羚 %d 只, 金矿 %d 处, 树 %d 棵"
          %(sum(1 for r in W.eres_list if r['Type']=='BUSH' and r['Cnt']>0),
            sum(1 for r in W.eres_list if r['Type']=='GAZELLE' and r['Blood']>0),
            sum(1 for r in W.eres_list if r['Type']=='GOLDORE' and r['Cnt']>0),
            sum(1 for r in W.eres_list if r['Type']=='TREE' and r['Cnt']>0)))
    print()
    N=int(sys.argv[1]) if len(sys.argv)>1 else 3000
    marks=set()
    for i in range(N):
        engine_step(W); updateVision(W); ai.processData()
        if W.frame in (1,2,3,5,10,20,50,100,200,400,800,1200,1600,2000,2500,3000,4000,5000,6000,7000,8000,10000,13500,21000):
            marks.add(W.frame)
    print("=== 事件流 ===")
    for e in ai.ev: print(" ",e)
    print()
    print("=== 关键快照 ===")
    # 重跑一次带快照
    W2=World('../new-aoe3.0.7h/map1.njust'); ai2=AI(W2); ai2._civ=2
    ai2.FIX_C1B=ai.FIX_C1B
    ai2.MAP=infoShare(W2); ai2.getBaseInfo()
    for b in W2.buildings: b['maxblood']=350
    for i in range(N):
        engine_step(W2); updateVision(W2); ai2.processData()
        if W2.frame in (50,100,300,600,1000,1500,2000,2500,3000):
            print(" ",snapshot(ai2,W2))
    print()
    print("=== 祭司移动指令统计(横跳检测) ===")
    pm=ai.priestMoves
    print("  总指令数 %d, 覆盖 %d 帧, 平均每 %.1f 帧 1 条"%(len(pm),N,len(pm)/max(1,N)*1))
    from collections import Counter
    c=Counter((pm[i][1],pm[i][2]) for i in range(len(pm)))
    print("  不同目标格个数: %d (若远小于指令数=每格被反复重发=横跳)"%len(c))
    print("  最高频目标 Top5:",c.most_common(5))
    gaps=[pm[i+1][0]-pm[i][0] for i in range(len(pm)-1)]
    if gaps:
        gaps.sort()
        print("  相邻指令间隔: 最小%d 中位%d 最大%d 帧"%(gaps[0],gaps[len(gaps)//2],gaps[-1]))
    print("  badFrontier 拉黑表大小:",len(ai.badFrontier))
    a=ai.army(ai.priestSN)
    print("  祭司最终位置:",(a['dr'],a['ur']) if a else None," 探索中" if ai.priestExploring else "已回家")
    print("  gazelleState=",ai.gazelleState,"(0未派 1到位中 2猎杀 3建仓库 4完成)")
    print("  市场门 marketGate=",getattr(ai,'marketGate',None)," gateLog=",
          getattr(ai,'_gateLog',None))
    print("  最终建筑:",[b['Type'] for b in W.buildings])
    print("  最终人口: 农民%d 军队%d 上限%d"%(len(W.farmers),len(W.armies),ai.humanMax()))
    print("  最终资源: 木%d 肉%d 金%d"%(W.Wood,W.Meat,W.Gold))

if __name__=='__main__': main()
