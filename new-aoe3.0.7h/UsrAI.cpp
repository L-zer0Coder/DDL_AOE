#include "UsrAI.h"
#include<set>
#include <iostream>
#include<unordered_map>
#include<list>
#include <cstdlib>



using namespace std;
tagGame tagUsrGame;
ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/
#include <algorithm>
#include<unordered_map>

///地图块标记
#define Unknown -1
#define Ocean -2
#define Open 1




tagInfo info;
int MAP[100][100]{};
//基地信息
int centerSN=-1;
int centerBlockDR=-1;
int centerBlockUR=-1;

//谷仓
int granaryBlockDR=-1;
int granaryBlockUR=-1;
//初始仓库--因为之后猎瞪羚与挖金子都会建自己的仓库 这里的仓库是专门给伐木用的 
int oriStockBlockDR=-1;
int oriStockBlockUR=-1;

int goldStockSN=-1;
int goldStockDR=-1;
int goldStockUR=-1;


//箭塔信息
int arrowTowerSN=-1;
int arrowTowerBlockDR=-1;
int arrowTowerBlockUR=-1;


//猎瞪羚信息
int gazelleState=0;//0-派 1-等 2-打 3-建 4-done
int gazelleSpotDR=-1;// 落脚点
int gazelleSpotUR=-1;
int gazelleHunter1SN=-1;//猎人1
int gazelleHunter2SN=-1;//猎人2
int gazelleTargetSN=-1;//当前被猎对象

//挖金信息 -- 不再用状态机: 挖金照抄"猎瞪羚/浆果"那套, 每次派人都 checkEnv
int goldSpotDR=-1;       // 金矿落脚点(仓库建在这附近)
int goldSpotUR=-1;

//祭司信息
int priestSN=-1;
int priestBlockDR=-1;
int priestBlockUR=-1;
int priestState=-1;
bool priestExploring=true;      // 祭司探索开关
double priestDR0=0,priestUR0=0;   // [AI] 引擎当前认定的目的地细节坐标: 移动途中它就是我们下的目标,
                                  //       拿它和 tgtDR/tgtUR 比对, 就能知道寻路有没有被引擎悄悄改写

//一局只获取一次基本信息
bool getOnlyOnce=false;

//防占用
//每一帧记录好现在工作的村民 防止被其他调走
unordered_map<int,bool>farIsgotten;


//建房点
int dx_home[]={-2,0,2,0};
int dy_home[]={0,2,0,-2};
//专职建房者
int homeBuilderSN=-1;
int homeBuilderState=-1;


const int goHomeFrame=5250;//强制回家帧

// [AI] bushNum / gazelleNum 已删除: 它们是"派过几个人"的累计值, 现改成每帧实时统计 bushNow / gazelleNow
int killGazelle=0;//6 记录猎杀的瞪羚数
bool storageStarted=false;//防止猎人重复建仓库
int farmNum=0;
// [AI] 农田上限: 铜器前 4 块, 铜器后放开到 16 块(只有谷仓/市中心两圈共 16 个方位)
const int FARM_MAX_TOOL=4;
const int FARM_MAX_BRONZE=16;
int farmLimit=FARM_MAX_TOOL;


int phaseNum=20;//铜器之前先限制20上限(19村民+1祭司 6浆果(后期种田) 3伐木 6瞪羚 1建房 3建造)--铜器后再开放5个村民(2个种田，还有三个辅助瞪羚组进行砍树或者挖金子) 


//军事
int marketBlockDR=-1;//市场的位置
int marketBlockUR=-1;

int armyCampBlockDR=-1;//兵营的位置
int armyCampBlockUR=-1;
//靶场挨者兵营 可以考虑记录
unordered_map<int,bool>bushFarmer;//采过浆果的农民: 【种田时优先挑他们】, 但不再禁止他们干别的活（主要是帮建放开）
unordered_map<int,bool>goldFarmer;   //被派去挖金的农民 矿采完自动接下一口, 不闲着
unordered_map<int,bool>woodFarmer;

const int PRIEST_HARNESS=5;   // 祭司活动范围: 离箭塔不超过5格(塔射程7格, 保证一直在保护圈内)
//================= 反攻 =================
int counterState=0;         //  0-前期：抵御三波 1-中期：集结与侦察 2-拉扯 3-大反攻
// ================= [AI] 拉扯 / 反攻 =================
const int RUSH_ROUNDS=5;        // [AI] 拉扯多少轮之后转全面反攻(用户定: 完成5轮, 第6轮反攻)
int  rushRound=0;               // [AI] 已完成的拉扯轮数(视野内敌人"从有到无"记一轮)
bool rallyDone=false;           // [AI] 集结是否已完成(大军已铺开到位)
bool rushHadEnemy=false;        // [AI] 边沿检测用: 上一帧视野内是否有敌人
int  seekTgtDR=-1,seekTgtUR=-1; // [AI] 斥候推进的锁定目标格(绝对坐标, 避免每帧重发)
int  towerTgtSN=-1;             // [AI] 斥候当前攻击的敌方箭塔SN(避免重复下令)
unordered_map<int,bool> rallyArrived;  // [AI] 兵SN -> 是否曾经到过自己的集结格(用于"循环填入"的释放判据)

int factorySN=-1;           // 敌方攻城武器厂
int factoryBlockDR=-1;      // 攻城武器厂坐标
int factoryBlockUR=-1;
int scoutSN=-1;             // 侦察骑兵

//集结锚点
int anchorDR=-1;
int anchorUR=-1;

// ================= 集结 =================
const int RALLY_BACK=15;      // 从敌人位置朝自家退几格
const int RALLY_R=2;         // 集结区半径(2 -> 5x5 = 25 格)
const int RALLY_NEED=8;      // 到齐几个兵算集结完毕
const int SCOUT_LURE_RANGE=6;  // [AI] 斥候拉扯的诱敌距离: 比敌人视野(7-9)略小, 保证敌人看见斥候会追(意见1)
int rallyDR=-1,rallyUR=-1;   // 集结点(区域中心); -1 表示还没定
unordered_map<int,int> rallySlot;   // 兵SN -> 分到的格子(DR*100+UR)

/////////////////////////////////////////////



void UsrAI::processData(){   
    //本帧大管家
    info=getInfo();
    //升级
    centerUpgrade();
    //清空 本帧重新获得劳动力表
    farIsgotten.clear();
    for(auto&r:info.resources){
        if(r.Type!=RESOURCE_GAZELLE)continue;
        for(auto&f:info.farmers){
            if(f.WorkObjectSN==r.SN)farIsgotten[f.SN]=true;
        }
    }
    if(gazelleState!=4){
        if(gazelleHunter1SN!=-1)farIsgotten[gazelleHunter1SN]=true;
        if(gazelleHunter2SN!=-1)farIsgotten[gazelleHunter2SN]=true;
    }
    //获取基本信息&&初始化房屋建造者
    if(!getOnlyOnce)getBaseInfo();
    //更新地图 主要用于判断Open空地
    betterMap();
    //处理祭司探索机制
    priestExplore();
    //针对波次攻击
    waveBattle();
    //处理反攻事宜
    counterAttack();
    //转化敌方攻城武器厂
    CalmAndCrazy();

    

    
    
    storageStarted=false;
    if(info.civilizationStage>=CIVILIZATION_BRONZEAGE)buildGoldStock();

    //处理建造及原材料的获取策略
    manageBuild();   
    //前期士兵训练及部分科技研发
    trainArmy();

    //猎瞪羚状态机
    storageStarted=false;
    huntGazelle();

    //第二猎人帮建第一猎人的仓库
    if(gazelleState==4&&gazelleHunter2SN!=-1){
        for(auto&b:info.buildings){
            if(b.Type!=BUILDING_STOCK)continue;
            if(b.Percent>=100)continue;                          // 只要在建的
            if(max(abs(b.BlockDR-gazelleSpotDR),abs(b.BlockUR-gazelleSpotUR))>6)continue;
            for(auto&f:info.farmers){
                if(f.SN!=gazelleHunter2SN)continue;
                
                if(f.WorkObjectSN==b.SN)continue;        // 已经在建了, 别重发
                HumanAction(f.SN,b.SN);            // 去帮建
                break;
            }
            break;
        }
    }
    

    

    static int assignedFrame=-1;//按帧执行可能有问题 之后找其他方式或者直接删掉该节流
    //当前帧的实时记忆
    unordered_map<int,int>resWorkers;
    unordered_map<int,bool>isResourceSN;
    for(auto&r:info.resources){
        isResourceSN[r.SN]=true;
    }
    for(auto&f:info.farmers){
        //根据有无工人的工作对象是资源来判断 并且用哈希 很快很简洁
        if(isResourceSN.count(f.WorkObjectSN))resWorkers[f.WorkObjectSN]++;
    }

    // [AI] 实时统计"当前在采浆果 / 在采瞪羚肉"的人数, 用来替代只增不减的 bushNum / gazelleNum。
    //   原来那两个是"派过几个人"的累计值: 一到 6 就永久封门(有人转岗也不会回落),
    //   还被当成"建造的前提" -> 差一个人就永远建不出市场。
    unordered_map<int,int>resType;                       // 资源SN -> 资源类型
    for(auto&r:info.resources)resType[r.SN]=r.Type;
    int bushNow=0,gazelleNow=0;
    for(auto&f:info.farmers){
        if(f.NowState!=HUMAN_STATE_WORKING&&f.NowState!=HUMAN_STATE_WALKING)continue;
        auto it=resType.find(f.WorkObjectSN);
        if(it==resType.end())continue;
        if(it->second==RESOURCE_BUSH)bushNow++;
        else if(it->second==RESOURCE_GAZELLE)gazelleNow++;
    }
 
    //最多几个人占用
    auto resMax=[](int type)->int{
        if(type==RESOURCE_TREE)return 1;
        if(type==RESOURCE_BUSH)return 1;
        return 1; //留接口
    };

    for(auto&r:info.resources){
        // [AI] 花名册已满 6 人就不再补人 —— 否则有人采空转岗后 bushNow 下降,
        //      会补第 7 个人进花名册(用户要求花名册恒为开局那 6 个人)
        if(r.Type==RESOURCE_BUSH&&(bushNow>=6||(int)bushFarmer.size()>=6))continue;
        if(r.Type==RESOURCE_GAZELLE&&gazelleNow>=6)continue;  // [AI] 同上

        //节流
        if(info.GameFrame-assignedFrame<19)break; //考虑删除

        if(r.Type==RESOURCE_BUSH||(r.Type==RESOURCE_GAZELLE&&gazelleState==4)){
            if(r.Type==RESOURCE_GAZELLE&&r.Blood>0)continue; //只采集不打猎
            if(r.Cnt<=0)continue; //没资源了不采集
            if(resWorkers[r.SN]>=resMax(r.Type))continue; //每个资源固定人数采集
            int need=((r.Type==RESOURCE_BUSH)?BUILDING_GRANARY:BUILDING_STOCK);
            int needWood=((r.Type==RESOURCE_BUSH)?BUILD_GRANARY_WOOD:BUILD_STOCK_WOOD);
            //其实一般不需要再建了 这里是鲁棒性 但可以考虑删除
            int fSN=findFarmer(r.BlockDR,r.BlockUR);          // 换掉"列表第一个空闲" -> 取离资源最近的
            if(fSN==-1)continue;
            
            int bySN=-1;
            int st=checkEnv(r.Type,bySN);
            if(r.Type==RESOURCE_GAZELLE)st=2; //因为默认会自己建
            if(st==2){
                HumanAction(fSN,r.SN);
                if(r.Type==RESOURCE_BUSH){
                    bushFarmer[fSN]=true;
                }
            }
            else if(st==1)HumanAction(fSN,bySN);
            else{
                int ox=-1,oy=-1;
                if(info.Wood>=needWood&&!storageStarted&&findBuildSpot(r.BlockDR,r.BlockUR,3,2,5,ox,oy)){
                    HumanBuild(fSN,need,ox,oy);
                    storageStarted=true; //一般来说只建一次 做限制
                }
                else{
                    HumanAction(fSN,r.SN);
                    if(r.Type==RESOURCE_BUSH){
                        bushFarmer[fSN]=true;
                    }
                }
            }
            farIsgotten[fSN]=true;
            assignedFrame=info.GameFrame;
            
        }
    }

    
    int woodNow=0; //本帧实时木工数
    unordered_map<int,bool>isTree;
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_TREE)isTree[r.SN]=true;
    }
    for(auto&f:info.farmers){
        if(f.NowState!=HUMAN_STATE_WORKING&&f.NowState!=HUMAN_STATE_WALKING)continue;
        if(isTree.count(f.WorkObjectSN))woodNow++;
    }
    
    int woodWant=5;
    if(haveBuilding(BUILDING_COLLAGE))woodWant=4;
    for(int i=woodNow;i<woodWant;i++){
        int id=assignWoodcutter();          // 补到 woodWant 个
        if(id==-1)break;        // 没树/没人就停, 不会空转
    }
    
    // ---- 升铜器后挖金: 补到 3 个人(一口井空了自动补下一口) ----
    
    
    if(info.civilizationStage>=CIVILIZATION_BRONZEAGE){
        int goldNow=0;                                     // 本帧实时金工数
        unordered_map<int,bool>isGold;
        for(auto&r:info.resources){ 
            if(r.Type==RESOURCE_GOLD)isGold[r.SN]=true; 
        }
        for(auto&f:info.farmers){
            if(f.NowState!=HUMAN_STATE_WORKING&&f.NowState!=HUMAN_STATE_WALKING)continue;
            if(isGold.count(f.WorkObjectSN))goldNow++;
        }
        
        int goldWant=2;
        if(haveBuilding(BUILDING_COLLAGE))goldWant=3;
        for(int i=goldNow;i<goldWant;i++){                 // 人数上限=goldWant
            if(assignGoldMiner()==-1)break;
        }
    }
    // ---- 挖金的农民: 手头那口矿没了 -> 立刻接下一口(手上还拿着金子的先去交) ----
    if(info.civilizationStage>=CIVILIZATION_BRONZEAGE){
        unordered_map<int,bool>liveGold;
        for(auto&r:info.resources){ 
            if(r.Type==RESOURCE_GOLD&&r.Cnt>0)liveGold[r.SN]=true; 
        }
                                     
        for(auto&f:info.farmers){
            
            if(!goldFarmer.count(f.SN))continue;             // 只管被派去挖金的人
            if(liveGold.count(f.WorkObjectSN))continue;       // 还在挖 -> 不动
            if(f.ResourceSort!=-1)continue;                   // 手上还有金子(去交) -> 先交
            if(f.NowState!=HUMAN_STATE_IDLE)continue;         // 忙 -> 不打扰
            if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
            int tSN=-1,best=1e18;                            // 找离他最近、还没人占的金矿
            for(auto&r:info.resources){
                if(r.Type!=RESOURCE_GOLD||r.Cnt<=0)continue;
                bool busy=false;
                for(auto&f:info.farmers){ 
                    if(f.WorkObjectSN==r.SN){
                        busy=true;
                        break;
                    } 
                }
                if(busy)continue;
                int dd=max(abs(r.BlockDR-f.BlockDR),abs(r.BlockUR-f.BlockUR));
                if(dd<best){
                    best=dd;
                    tSN=r.SN;
                }
            }
            if(tSN!=-1){
                HumanAction(f.SN,tSN);
                farIsgotten[f.SN]=true;
                
            }
        }
    }

    
   
    

}

//是否拥有某建筑
bool UsrAI::haveBuilding(int type){
    for(auto&b:info.buildings){
        if(b.Type==type)return true;
    }
    return false;
}
//时代升级
void UsrAI::centerUpgrade(){
    if(info.civilizationStage==CIVILIZATION_BRONZEAGE)return;
    //铜器升级事宜
    int centerState=-1;
    for(auto&b:info.buildings){
        if(b.Type==BUILDING_CENTER)centerState=b.Project;
    }
    //考虑前置建筑 食物条件以及建筑物状态
    if(haveBuilding(BUILDING_MARKET)&&haveBuilding(BUILDING_RANGE)&&info.Meat>=BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD){
        if(centerState==ACT_NULL)
            BuildingAction(centerSN,BUILDING_CENTER_UPGRADE);
    }
    
    if(info.civilizationStage==CIVILIZATION_BRONZEAGE){
        phaseNum=25; //放宽村民人口
        farmLimit=FARM_MAX_BRONZE; //放宽农田种植
    }
}

//本函数用于实时获得祭司的信息
void UsrAI::getPriest(){
    for(auto&a:info.armies){
        if(a.Sort==AT_PRIEST){
            priestSN=a.SN;
            priestBlockDR=a.BlockDR;
            priestBlockUR=a.BlockUR;
            priestState=a.NowState;
            // priestDR0=a.DR0;      // [AI] 记录引擎认定的目的地, 供锁定判据③用
            // priestUR0=a.UR0;
            break;
        }
    }
}
//获取基本信息：基地、房屋聚集地、箭塔地址
void UsrAI::getBaseInfo(){
    //获取祭司信息
    getPriest();
    for(auto&b:info.buildings){
        //基地
        if(b.Type==BUILDING_CENTER){
            centerSN=b.SN;
            centerBlockDR=b.BlockDR;   
            centerBlockUR=b.BlockUR;
        }
        if(b.Type==BUILDING_GRANARY){
            granaryBlockDR=b.BlockDR;
            granaryBlockUR=b.BlockUR;
        }
        if(b.Type==BUILDING_STOCK){
            oriStockBlockDR=b.BlockDR;
            oriStockBlockUR=b.BlockUR;
        }
        //箭塔
        if(b.Type==BUILDING_ARROWTOWER){
            arrowTowerSN=b.SN;
            arrowTowerBlockDR=b.BlockDR;
            arrowTowerBlockUR=b.BlockUR;
        }
        
    }
    //获取唯一的房屋建造者 -- 这里写开主要是完成初始化
    for(auto&f:info.farmers){
        if(homeBuilderSN==-1&&f.NowState==HUMAN_STATE_IDLE){
            homeBuilderSN=f.SN;
            gethomeBuilder();
            break;
        }
    }
    //标记 以后不要再进来了
    getOnlyOnce=true;
}
//更新地图
void UsrAI::betterMap(){
    //扫整张图
    //思路 先赋值地块类型 再安装资源与建筑

    //地块类型
    for(int dr=0;dr<100;dr++){
        for(int ur=0;ur<100;ur++){
            if((*info.theMap)[dr][ur].type==MAPPATTERN_UNKNOWN)MAP[dr][ur]=Unknown;//未知区域
            else if((*info.theMap)[dr][ur].type==MAPPATTERN_OCEAN)MAP[dr][ur]=Ocean;//海洋
            else MAP[dr][ur]=Open;//空地
        }
    }
    //资源类型
    for(auto&r:info.resources){
        if(r.Type!=RESOURCE_EMPTY)MAP[r.BlockDR][r.BlockUR]=r.Type+100; //资源+100偏移 主要是防止与其他常量值冲突
    }
    //建筑
    for(auto&b:info.buildings){
        int sz=3;
        if(b.Type==BUILDING_HOME||b.Type==BUILDING_ARROWTOWER)sz=2;
        int dr=b.BlockDR;
        int ur=b.BlockUR;
        for(int i=0;i<sz;i++){
            for(int j=0;j<sz;j++)MAP[dr+i][ur+j]=b.Type+1000; //建筑+1000偏移 主要是防止与其他常量值冲突
        }
    }
    //敌方建筑
    for(auto&eb:info.enemy_buildings){
        int sz=3;
        if(eb.Type==BUILDING_HOME||eb.Type==BUILDING_ARROWTOWER)sz=2;
        int dr=eb.BlockDR;
        int ur=eb.BlockUR;
        for(int i=0;i<sz;i++){
            for(int j=0;j<sz;j++)MAP[dr+i][ur+j]=eb.Type+2000; //建筑+2000偏移 主要是防止与其他常量值冲突 同时与己方建筑区分
        }
    }
}

//==================== 祭司探路 ====================
// 阶段0: 探自己所在的角(以市镇中心为界, 只扫"中心->地图角"那半张图)
// 阶段1: 以地图中心(50,50)为圆心, 半径40起一圈圈向里收(只在环带里找边界点)
// 另外三个角不探。视野内出现敌人/猛兽立即躲避。
// 说明: 边界点 = 已探明陆地(Open) 且 周围2格内有未知区(Unknown)

// 当前已探明的活瞪羚数量。
const int gazelleWantNum=6;     // 祭司探路/开猎前要凑够的"已探明活瞪羚"数量
int liveGazelleNum(){
    int n=0;
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_GAZELLE&&r.Blood>0)n++;
    }
    return n;
}
//是否近战
bool isMeleeSort(int sort){
    return sort==AT_CLUBMAN||sort==AT_SWORDSMAN||sort==AT_IMPROVED
        ||sort==AT_HOPLITE||sort==AT_BROADSWORDSMAN||sort==AT_CAVALRY
        ||sort==AT_CHARIOT||sort==AT_SCOUT;
}
//是否远程
bool isArcherSort(int sort){
    return sort==AT_BOWMAN||sort==AT_SLINGER
        ||sort==AT_CHARIOT_ARCHER||sort==AT_COMPOSITE_BOWMAN;
}

//本函数主要实现祭司的探索与自主避障功能 回归的前提是当前视野有六只活瞪羚以及金矿地
void UsrAI::priestExplore(){
    getPriest(); //每次进来获取祭司信息
    //如果处于非探索阶段则直接退出
    if(!priestExploring)return;

    static int phase=-1;                 // -1-先逛基地周边 0-自己角 1-绕中心 2-结束
    static int radius=40;                // 阶段1当前半径
    static int curDR=-1,curUR=-1;        // 当前目标格

    const int DANGER_ESCAPE=64;          // 敌人8格内视为危险（64为平方）
    

    //几个轻量lambda函数
    //是否 已探明
    auto usable=[&](int dr,int ur)->bool{
        return (dr>=0&&dr<100&&ur>=0&&ur<100)&&MAP[dr][ur]==Open;
    };
    //是否 边界点: 自己去得, 且周围2格内可走且有未知区
    //祭司探索还是尽量走已知路 并且是迷雾的边界 直接点迷雾走容易掉水里
    auto frontier=[&](int dr,int ur)->bool{
        if(!usable(dr,ur))return false;
        for(int dx=-2;dx<=2;dx++){
            for(int dy=-2;dy<=2;dy++){
                if(MAP[dr+dx][ur+dy]==Unknown)return true;
            }
        }
        return false;
    };

    auto dangerNear=[&](int dr,int ur)->bool{
        //避敌军
        for(auto&ea:info.enemy_armies){
            int dx=ea.BlockDR-dr, dy=ea.BlockUR-ur;
            if(dx*dx+dy*dy<DANGER_ESCAPE)return true;
        }
        return false;
    };

    //开始局部避障
    //---------- 1. 找最近的危险 ----------
    int ex=-1,ey=-1,dist=1e18; //分别记录 危险DR 危险UR 危险距离
    for(auto&ea:info.enemy_armies){
        int dx=ea.BlockDR-priestBlockDR, dy=ea.BlockUR-priestBlockUR;
        int d=dx*dx+dy*dy;
        if(d<dist){ //以此筛选出最近
            dist=d;
            ex=ea.BlockDR;
            ey=ea.BlockUR;
        }
    }
    //---------- 2. 有危险就跑: 8个方向里挑最背离危险、且可走的一格, 撤6格 ----------
    if(ex!=-1&&dist<DANGER_ESCAPE&&priestExploring){ //存在这么一个危险 并且达到避障极限距离 并且此时处于探索状态
    
        //周围八格
        int dirx[8]={1,1,0,-1,-1,-1,0,1};
        int diry[8]={0,1,1,1,0,-1,-1,-1};
        int tdr=-1,tur=-1; //targetdr/ur
        double ddd=-1e18; //这次要比较大的 所以取负数
        for(int k=0;k<8;k++){
            int nx=priestBlockDR+dirx[k]*6;
            int ny=priestBlockUR+diry[k]*6;
            if(!usable(nx,ny))continue;                 // 局部避障: 只往能走的格子躲
            if(dangerNear(nx,ny))continue;              // 不往另一堆危险里躲
            int d=(nx-ex)*(nx-ex)+(ny-ey)*(ny-ey);   // 候选格离危险(敌人)多远          
            if(d>ddd){
                ddd=d;
                tdr=nx;
                tur=ny;
            } 
        }
        
        static int dodgeDR=-1,dodgeUR=-1;
        if(tdr!=-1&&(tdr!=dodgeDR||tur!=dodgeUR)){
            dodgeDR=tdr;
            dodgeUR=tur;
            HumanMove(priestSN,tdr*BLOCKSIDELENGTH,tur*BLOCKSIDELENGTH);
        }
        return;
    }
    //---------- 2. 到点回家(优先级高于躲避) ----------
    
    static bool goldSeenOnce=false;                  // 见过一次金矿就记下(属于探索任务)
    if(!goldSeenOnce){
        for(auto&r:info.resources){
            if(r.Type==RESOURCE_GOLD){
                goldSeenOnce=true;
                break;
            }
        }
    }

    if((liveGazelleNum()>=gazelleWantNum&&goldSeenOnce)||info.GameFrame>=goHomeFrame){
        // if(info.GameFrame>=goHomeFrame)goldSeenOnce=true;  // 兜底触发时同步置位, 避免后续偏好探索段重复进入
        static int homeDR=-1,homeUR=-1;              // 落脚点(箭塔四邻)
        // [AI] 目标锁定用: 已经发给祭司的目标(和上面的 dodgeDR 同一个套路)
        static int moveDR=-1,moveUR=-1;
        // 还没到家
        if(abs(priestBlockDR-arrowTowerBlockDR)>PRIEST_HARNESS||abs(priestBlockUR-arrowTowerBlockUR)>PRIEST_HARNESS){
            if(homeDR==-1){                          // 挑箭塔四邻当落脚点
                int dx4[4]={0,1,0,-1};
                int dy4[4]={1,0,-1,0};
                for(int k=0;k<4;k++){
                    int nr=arrowTowerBlockDR+dx4[k], nu=arrowTowerBlockUR+dy4[k];
                    if(!usable(nr,nu))continue;
                    homeDR=nr;
                    homeUR=nu;
                    break;
                }
            }
            // [AI] 目标锁定: 落脚点只挑一次, 目标没变就不重复发令(理由同躲兵段)
            if(homeDR!=-1&&(homeDR!=moveDR||homeUR!=moveUR)){
                moveDR=homeDR;moveUR=homeUR;
                HumanMove(priestSN,homeDR*BLOCKSIDELENGTH,homeUR*BLOCKSIDELENGTH);
            }
            return;
        }
        // 到家了 -> 不再探索
        priestExploring=false;
        return;
    }

    
    
    //偏好探索
    
    if(liveGazelleNum()<gazelleWantNum||!goldSeenOnce){   // [AI] 变量随回家段一起改名(goldSeen -> goldSeenOnce)
        static int seekDR=-1,seekUR=-1; //准备追的、看到的瞪羚的坐标

        int gdr=-1,gur=-1,gd=1e18; //gazelledr/ur/distance
        //偏好探索 还没探到六只瞪羚
        
        for(auto&r:info.resources){
            bool isGaz=(r.Type==RESOURCE_GAZELLE);
            bool isGold=(r.Type==RESOURCE_GOLD&&!goldSeenOnce);
            if(!isGaz&&!isGold)continue;
            
            if(isGaz&&r.Blood<=0)continue;
            if(r.Cnt<=0)continue;
            int dx=r.BlockDR-priestBlockDR;
            int dy=r.BlockUR-priestBlockUR; //拿目标资源坐标
            int d=dx*dx+dy*dy; //计算坐标平方
            if(d<=36)continue;             // 已经贴着它了(6格内), 换下一只
            if(d>1600)continue;            // 太远的先别追(免得隔着海去够), 交给常规探索
            if(d<gd){   //找最近的一个
                gd=d;
                gdr=r.BlockDR;
                gur=r.BlockUR;
            }
        }

        int ox=-1,oy=-1; //output
        if(gdr!=-1){ //有目标
            int dx4[4]={0,1,0,-1};
            int dy4[4]={1,0,-1,0};
            for(int k=0;k<4;k++){                             // 先找它四邻的落脚点
                int nd=gdr+dx4[k], nu=gur+dy4[k];   //neardr ur
                if(!usable(nd,nu))continue;
                if(dangerNear(nd,nu))continue;
                ox=nd;
                oy=nu;
                break;
            }
        }

        if(ox==-1){
            seekDR=-1;
            seekUR=-1;              // 暂时没目标可追
        }
        else if(ox!=seekDR||oy!=seekUR){                      // 换了新的落脚点 -> 计时并发一次指令
            seekDR=ox;
            seekUR=oy;
            HumanMove(priestSN,ox*BLOCKSIDELENGTH,oy*BLOCKSIDELENGTH);

            return;
        }
        else return;                                           // 正在去这只瞪羚的路上, 别打断
    }
    

    //---------- 4. 选目标 ----------
    int bd=-1,bu=-1;              // dist 用上面(危险搜索那段)已经声明的那个, 别重复声明
    static int cx=-1;
    static int cy=-1;   //center
    if(phase==-1){
        //分别以主要建筑物为中心进行绕圈
        if(cx==-1){
            cx=centerBlockDR;
            cy=centerBlockUR;
        }
        else if(cx==centerBlockDR){
            cx=granaryBlockDR;
            cy=granaryBlockUR;
        }
        else if(cx==granaryBlockDR){
            cx=oriStockBlockDR;
            cy=oriStockBlockUR;
        }
        bd=-1;
        bu=-1;
        dist=1e18; //每次刷新
        for(int dr=2;dr<98;dr++){
            for(int ur=2;ur<98;ur++){
                int d=max(abs(dr-cx),abs(ur-cy));
                if(d>30)continue;      //先把这些建筑周围30格探掉                    
                if(!frontier(dr,ur))continue;
                if(dangerNear(dr,ur))continue;
                int dx=dr-priestBlockDR, dy=ur-priestBlockUR;
                int d2=dx*dx+dy*dy;
                if(d2<dist){ //找最近的格子
                    dist=d2;
                    bd=dr;
                    bu=ur;
                }
            }
        }
        if(bd==-1){                                        // 基地周边探完了 -> 进正常流程
            phase=0; //状态转移
        }
    }
    else if(phase==0){
        // 自己那一角: 市中心 → 地图角 的半张图
        int drMin=(centerBlockDR<50?2:centerBlockDR);
        int drMax=(centerBlockDR<50?centerBlockDR:97);
        int urMin=(centerBlockUR<50?2:centerBlockUR);
        int urMax=(centerBlockUR<50?centerBlockUR:97);
        for(int dr=drMin;dr<=drMax;dr++){
            for(int ur=urMin;ur<=urMax;ur++){
                if(!frontier(dr,ur))continue;
                
                if(dangerNear(dr,ur))continue;
                int dx=dr-priestBlockDR, dy=ur-priestBlockUR;
                int d=dx*dx+dy*dy;                        // 离祭司越近越优先
                if(d<dist){
                    dist=d; 
                    bd=dr;
                    bu=ur;
                }
            }
        }
        if(bd==-1){    //bd没赋值 这一角探完 → 转绕中心
            phase=1;
            radius=40;
        }
    }
    else if(phase==1){
        // 绕中心: 半径从大到小, 在每条环带(|切比雪夫距离 - r| <= 1)里找最近的边界点
        for(int r=radius;r>=4;r--){       // 步长1: 消灭 34-38/26-30/18-22 
            bd=-1;
            bu=-1;
            dist=1e18;
            for(int dr=2;dr<98;dr++){
                for(int ur=2;ur<98;ur++){
                    int adx=abs(dr-50),ady=abs(ur-50); //与中心比较
                    int d=(adx>ady?adx:ady);
                    if(d<r-1||d>r+1)continue;   //形成环带
                    if(!frontier(dr,ur))continue;
                    if(dangerNear(dr,ur))continue;
                    int dx=dr-priestBlockDR, dy=ur-priestBlockUR;
                    int d1=dx*dx+dy*dy;
                    if(d1<dist){
                        dist=d1;
                        bd=dr;
                        bu=ur;
                    }
                }
            }
            if(bd!=-1){radius=r;break;}                    // 这条环带还有得探
        }
        if(bd==-1){                                        // 绕完了
            if(liveGazelleNum()<gazelleWantNum){
                radius=40;                                  // 还没凑够 6 只 -> 重新绕, 别停
            }
            else{
                phase=2;
            }
        }
    }

    if(bd==-1)return;                                      // 阶段2: 探完, 不再动作
    // [AI] 目标锁定: 这一段最需要它 —— bd 是"离祭司最近的迷雾边界点",
    //     祭司自己每走一格 bd 就可能变, 不锁住的话每帧都在换目标,
    //     路径每帧被清空 -> 祭司在原地无限横跳(实机日志: 每 8 帧换一个目标)。
    static int tgtDR=-1,tgtUR=-1;
    if(bd!=tgtDR||bu!=tgtUR){
        tgtDR=bd;tgtUR=bu;
        HumanMove(priestSN,bd*BLOCKSIDELENGTH,bu*BLOCKSIDELENGTH);
    }
    return;
}
// 造兵: 兵营先升级战斧, 升完出 2 个斧兵; 靶场出 2 个弓箭手; 造完集合到箭塔下
void UsrAI::trainArmy(){
    static bool compTech=false,logistics=false; //复合弓 后勤 （科技）

    for(auto&b:info.buildings){
        if(b.Percent<100||b.Project!=ACT_NULL)continue;
        if(b.Type==BUILDING_ARMYCAMP){
            if(!logistics&&info.Meat>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD&&info.Gold>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD){
                BuildingAction(b.SN,BUILDING_ARMYCAMP_RESEARCH_LOGISTICS);
                logistics=true;       // 第三波后立刻研后勤(兵营0.5人口)
            }
        }
        
        else if(b.Type==BUILDING_RANGE){
            if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&!compTech){
                // 
                if(info.Meat>=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD&&info.Wood>=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD){
                    BuildingAction(b.SN,BUILDING_RANGE_UPGRADE_COMPOSITE_BOW);
                    compTech=true;
                    
                }
            }
            if(compTech&&info.Meat>=BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD&&info.Gold>=BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD
                &&info.Human_Num<46)
                BuildingAction(b.SN,BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN);
        }
        else if(b.Type==BUILDING_COLLAGE&&info.Human_Num<46){        // 学院好了就出方阵兵, 支援第二波
            BuildingAction(b.SN,BUILDING_COLLAGE_CREATE_HOPLITE);
        }
    }
    return;
    

    
}
// void UsrAI::trainArmy(){
//     static bool clubUp=false,broadTech=false,compTech=false,logistics=false; //战斧 阔剑 复合弓 后勤 （科技）
    
//     static int armyTechId=-1,armyTechType=0;   // 兵营科技: 待确认的指令id / 1=战斧 2=阔剑 3=后勤
//     static int rangeTechId=-1;                 // 靶场科技: 待确认的指令id(复合弓)
//     if(armyTechId>=0){
//         if(info.ins_ret.count(armyTechId)&&info.ins_ret[armyTechId]==ACTION_SUCCESS){
//             if(armyTechType==1)clubUp=true;
//             else if(armyTechType==2)broadTech=true;
//             else if(armyTechType==3)logistics=true;
//         }
//         armyTechId=-1;armyTechType=0;      // 无回执(被每帧条数上限丢弃)也清空 -> 下帧重试本科技
//     }
//     if(rangeTechId>=0){
//         if(info.ins_ret.count(rangeTechId)&&info.ins_ret[rangeTechId]==ACTION_SUCCESS)compTech=true;
//         rangeTechId=-1;
//     }

//     // [AI] 造兵/科技不中断: 原来整段被 if(counterState<=1) 包着, 进入集结/拉扯/反攻后就不再造兵。
//     //      用户明确要求"市镇中心仍然有机会就造兵进行补充", 所以这里放开这道门, 改为一直生效
//     //      (建筑空闲、资源够、人口没满时才真正发得出去, 下面的判断本来就已经保证了)。
//     {
//         int club=0,bow=0,scoutNum=0;
//         for(auto&a:info.armies){
//             if(a.Sort==AT_CLUBMAN)club++;
//             else if(a.Sort==AT_BOWMAN)bow++;
           
//         }
//         // [AI] 科技保证金: 先把"下一步要研究的科技"所需资源扣出来, 再决定造不造兵。
//         //      否则靶场一有 40肉20金就出复合弓 -> 科技要的 180肉100木永远攒不齐
//         //      (用户反馈: 资源都被生产兵种吃掉了)。只在"建好且空闲"的建筑上算, 正在研究的不会重复计入。
//         int rsMeat=0,rsGold=0,rsWood=0;
//         for(auto&b:info.buildings){
//             if(b.Percent<100||b.Project!=ACT_NULL)continue;
//             if(b.Type==BUILDING_ARMYCAMP){
//                 if(!clubUp){ rsMeat+=BUILDING_ARMYCAMP_UPGRADE_CLUBMAN_FOOD; }
//                 else if(!broadTech){ rsMeat+=BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_FOOD; rsGold+=BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_GOLD; }
//                 else if(!logistics){ rsMeat+=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD; rsGold+=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD; }
//             }
//             if(b.Type==BUILDING_RANGE&&!compTech){
//                 rsMeat+=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD;
//                 rsWood+=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD;
//             }
//         }
//         for(auto&b:info.buildings){
//             if(b.Percent<100||b.Project!=ACT_NULL)continue;
//             if(b.Type==BUILDING_ARMYCAMP){
//                 //先升科技
//                 if(!clubUp&&armyTechId<0&&info.Meat>=BUILDING_ARMYCAMP_UPGRADE_CLUBMAN_FOOD){
//                     armyTechId=BuildingAction(b.SN,BUILDING_ARMYCAMP_UPGRADE_CLUBMAN);
//                     armyTechType=1;
//                 }
//                 else if(clubUp&&!broadTech&&armyTechId<0&&info.civilizationStage>=CIVILIZATION_BRONZEAGE&&
//                         info.Meat>=BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_FOOD&&info.Gold>=BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_GOLD){
//                     armyTechId=BuildingAction(b.SN,BUILDING_ARMYCAMP_UPGRADE_BROADSWORD);
//                     armyTechType=2;   // 前期就升阔剑科技
//                 }
                
//                 else if(broadTech&&!logistics&&armyTechId<0&&info.Meat>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD&&info.Gold>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD){
//                     armyTechId=BuildingAction(b.SN,BUILDING_ARMYCAMP_RESEARCH_LOGISTICS);
//                     armyTechType=3;       // 第三波后立刻研后勤(兵营0.5人口)
//                 }
//                 else if(club<2&&info.Meat>=BUILDING_ARMYCAMP_CREATE_CLUBMAN_FOOD+rsMeat){
//                     BuildingAction(b.SN,BUILDING_ARMYCAMP_CREATE_CLUBMAN);
//                     club++;
//                 }
//             }
//             if(b.Type==BUILDING_COLLAGE){        // 学院好了就出方阵兵, 支援第二波
//                 // [AI] 原来这里不查资源也不查科技保证金, 每帧都发 -> 把肉/金吃光。补上保证金。
//                 // [AI] 再补人口预留: 方阵兵/复合弓原来都没有人口上限, 一直造到 Human_MaxNum,
//                 //      结果连 1 个给斥候的名额都留不出来(见下面 BUILDING_STABLE 分支的说明)。
//                 if(info.Meat>=BUILDING_COLLAGE_CREATE_HOPLITE_FOOD+rsMeat&&
//                    info.Gold>=BUILDING_COLLAGE_CREATE_HOPLITE_GOLD+rsGold&&
//                    info.Human_Num<info.Human_MaxNum-1)
//                     BuildingAction(b.SN,BUILDING_COLLAGE_CREATE_HOPLITE);
//             }
//             if(b.Type==BUILDING_RANGE){
//                 if(!compTech&&bow<2&&info.Meat>=BUILDING_RANGE_CREATE_BOWMAN_FOOD+rsMeat&&info.Wood>=BUILDING_RANGE_CREATE_BOWMAN_WOOD+rsWood){
//                     BuildingAction(b.SN,BUILDING_RANGE_CREATE_BOWMAN);
//                     bow++;
//                 }
//                 if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&!compTech&&rangeTechId<0){
//                     // 
//                     if(info.Meat>=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD&&info.Wood>=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD){
//                         rangeTechId=BuildingAction(b.SN,BUILDING_RANGE_UPGRADE_COMPOSITE_BOW);
                        
//                     }
//                 }
//                 // [AI] 留 1 个名额给斥候(理由同上面的方阵兵分支)
//                 if(compTech&&info.Meat>=BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_FOOD+rsMeat&&info.Gold>=BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN_GOLD+rsGold
//                    &&info.Human_Num<info.Human_MaxNum-1)
//                     BuildingAction(b.SN,BUILDING_RANGE_CREATE_COMPOSITE_BOWMAN);
//             }
//             // [AI] ===== 造斥候: 从马厩直接训, 不再依赖 counterState =====
//             //  原来造斥候只有 manageScout() 里那几行, 而 manageScout() 只有 counterState>=2 才会被调用;
//             //  counterState 1->2 的门却是 Human_Num>45 —— 等跨过去时人口早被复合弓/方阵兵顶满,
//             //  BuildingAction 返回 ACTION_INVALID_BUILDACT_MAXHUMAN, 斥候永远造不出来,
//             //  scoutSN 恒为 -1, 于是 case 2 一直卡在"派斥候探路"那一段, 永远进不了 case 3。
//             //  现在马厩一建好就试训, 不再看 counterState。
//             if(b.Type==BUILDING_STABLE&&scoutNum<1&&info.Meat>=BUILDING_STABLE_CREATE_SCOUT_FOOD){
//                 BuildingAction(b.SN,BUILDING_STABLE_CREATE_SCOUT);
//             }
//         }
//         return;
//     }

    
// }

//获取唯一的房屋建造者
void UsrAI::gethomeBuilder(){
    for(auto&f:info.farmers){
        if(f.SN==homeBuilderSN){
            homeBuilderState=f.NowState;
        }
    }
}

//==================== 建造小工具 ====================

// 在(bd,bu)附近找一个 size×size 的空地(返回左下角块坐标)
// 条件: 全是已探明陆地(Open, 说明没资源没建筑) + 高度一致且不是斜坡 + 没被拉黑
bool UsrAI::spotBusy(int dr,int ur,int size){
    for(auto&f:info.farmers){
        if(f.BlockDR>=dr&&f.BlockDR<dr+size&&f.BlockUR>=ur&&f.BlockUR<ur+size)return true;
    }
    for(auto&a:info.armies){
        if(a.BlockDR>=dr&&a.BlockDR<dr+size&&a.BlockUR>=ur&&a.BlockUR<ur+size)return true;
    }
    return false;
}



bool UsrAI::findBuildSpot(int bd,int bu,int size,int minR,int maxR,int &ox,int &oy){
    ox=-1;oy=-1;
    //一个圆周找建筑

    for(int dr=bd-maxR;dr<=bd+maxR;dr++){
        for(int ur=bu-maxR;ur<=bu+maxR;ur++){
            if(dr<0||dr>=100||ur<0||ur>=100)continue;
            // int adx=abs(dr-bd),ady=abs(ur-bu);
            // int d=(adx>ady?adx:ady);
            // if(d<minR)continue;                        // 别贴着基准点盖, 留出通道
            
            
            bool canbuild=true;
            for(int i=0;i<size;i++){
                for(int j=0;j<size;j++){
                    if(MAP[dr+i][ur+j]!=Open){
                        canbuild=false;
                        break;
                    }
                }
                if(!canbuild)break;
            }
            if(canbuild&&!spotBusy(dr,ur,size)){
                ox=dr,oy=ur;
                return true;
            }
        }
        
    }
    return ox!=-1;
}


void UsrAI::manageBuild(){
    //获取当前房屋建造者的信息
    gethomeBuilder();                    
    //当前可容纳最大人数
    int maxNum=info.Human_MaxNum;
    //当前已有人数
    double haveNum=info.Human_Num;
    //距离人满还有多少人
    int spaceNum=maxNum-haveNum;
    
    

    // [AI] 市场开建门: 我方仓库数(开局1个 + 猎人建1个)都到位才开建市场(150木),
    //      免得市场抢走猎人建仓库(120木)的木头。兜底: 打猎阶段已结束(gazelleState==4)也放行, 防死锁。
    int stockNum=0;
    for(auto&b:info.buildings){
        if(b.Type==BUILDING_STOCK&&b.Percent>=100)stockNum++;
    }
    bool marketGate=(stockNum>=2||gazelleState==4);
    

    // // [AI] 金矿木头保证金: 铜器后 goldSpot 那口矿旁还没仓库时, 农田不许把这 120 木花掉
    // //      (木头逐帧累积, 农田每块吃 75, 不设限就永远到不了 120)
    // //      (注意: 金矿仓库已经建够 GOLD_STOCK_MAX 个时不再压着农田, 否则农田永远没木头)
    // int goldReserve=0;
    // if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&goldSpotDR!=-1){
    //     int gb=-1;
        
    //     if(checkEnv(RESOURCE_GOLD,gb)==0)goldReserve=BUILD_STOCK_WOOD;
    // }

    //造人开关
    //进行一个优化 减少无效指令
    if(haveNum<phaseNum){
        int centerProject=ACT_NULL;
        for(auto&b:info.buildings){
            if(b.SN!=centerSN)continue;
            centerProject=b.Project;
            break;
        }
        if(centerProject==ACT_NULL)
            BuildingAction(centerSN,BUILDING_CENTER_CREATEFARMER);
    }
    // 农民修箭塔(保证前三波箭塔可以有效吸引仇恨)
    for(auto&b:info.buildings){
        if(b.Type!=BUILDING_ARROWTOWER)continue;
        if(b.Blood>=b.MaxBlood)continue;                    // 满血不用修
        bool busy=false;
        for(auto&f:info.farmers){                 //目前有无人在修
            if(f.WorkObjectSN==b.SN){
                busy=true;
                break;
            }
        }
        if(busy)continue;                                   // 已经有人在修
        int fSN=findFarmer(b.BlockDR,b.BlockUR);    //找最近农民
        if(fSN!=-1){
            HumanAction(fSN,b.SN);
            farIsgotten[fSN]=true;
        }   // 农民对己方建筑=修理
        break;
    }

    //建房子
    if(spaceNum<=3&&maxNum<50){
        for(auto&b:info.buildings){
            if(b.Type==BUILDING_HOME&&info.Wood>=BUILD_HOUSE_WOOD&&homeBuilderState==HUMAN_STATE_IDLE){
                int cornerDR=b.BlockDR; //当前房屋的角落坐标
                int cornerUR=b.BlockUR;
                //沿四个方向建
                for(int i=0;i<4;i++){
                    int nDR=cornerDR+dx_home[i];
                    int nUR=cornerUR+dy_home[i];
                    bool canBuild=true;
                    for(int i=nUR;i<=nUR+1;i++){
                        for(int j=nDR;j<=nDR+1;j++){
                            if(i<0||i>=100||j<0||j>=100||MAP[j][i]!=Open){
                                //这一刻说明这个方向不能建 可以直接出去了
                                canBuild=false;
                                break;
                            }
                        }
                        if(!canBuild)break;
                    }
                    if(canBuild){
                        HumanBuild(homeBuilderSN,BUILDING_HOME,nDR,nUR);
                        break;
                        //开始建之后 建造者状态已不属于IDLE 所以也不会再进入循环导致重复发指令
                    }
                }
            }
        }
    }
    
        
    //农田
    //周围八个方向 中间留宽2格的通道 
    int fsDR[]={-5,0,5,0,-5,5,5,-5};
    int fsDU[]={0,5,0,-5,5,5,-5,-5};

    //科技研发
    static bool tech[3]{}; //0 木材 1 动物 2 金矿
    if(haveBuilding(BUILDING_MARKET)){
        for(auto&b:info.buildings){
            if(b.Type!=BUILDING_MARKET)continue;
            if(b.Project!=ACT_NULL)continue;
            if(!tech[0]&&info.Wood>=BUILDING_MARKET_WOOD_UPGRADE_WOOD&&info.Meat>=BUILDING_MARKET_WOOD_UPGRADE_FOOD){
                BuildingAction(b.SN,BUILDING_MARKET_WOOD_UPGRADE);
                tech[0]=true;
            }
            else if(info.civilizationStage==CIVILIZATION_BRONZEAGE&&!tech[1]&&info.Meat>=BUILDING_MARKET_GOLD_UPGRADE_FOOD&&info.Wood>=BUILDING_MARKET_GOLD_UPGRADE_WOOD){
                BuildingAction(b.SN,BUILDING_MARKET_GOLD_UPGRADE);
                tech[1]=true;
            }
            else if(info.civilizationStage==CIVILIZATION_BRONZEAGE&&!tech[2]&&info.Meat>=BUILDING_MARKET_FARM_UPGRADE_FOOD&&info.Wood>=BUILDING_MARKET_FARM_UPGRADE_WOOD){
                BuildingAction(b.SN,BUILDING_MARKET_FARM_UPGRADE);
                tech[2]=true;
            }

        }
    }
   
    // if(woodFarmer.size()>=2){
    //     // ================= 市场科技: 按顺序排队, 每帧最多发一条 =================
    //     // [AI] 原来三个科技写成并列 if -> 同帧同时成立时会给同一个市场下三条指令,
    //     //      引擎按 SN 去重只留最后一条(农田升级), 被顶掉的那条 tech[i] 却已经置位
    //     //      -> 金矿采集这类科技永久不再研发。这里改成状态机 + ins_ret 回看结果。
    //     static int marketTech=0;        // 0=木材加工 1=金矿采集 2=农田升级 3=全部走完
    //     static int marketTechId=-1;     // 上一条科技指令 id(-1=没有待确认的)
    //     if(marketTechId>=0){
    //         // 有回执: 成功 -> 留在本档(这几个是二级链, 允许再发一次研第二级);
    //         //          失败(含"已达上限/时机不合法") -> 本档走完, 进下一档。
    //         // 无回执(指令被引擎"每帧条数上限"丢弃, 不会写回执) -> 不推进, 下帧重发本档。
    //         // [AI] 只在"明确不可用"(解锁/已达上限/重复)时才推进到下一档;
    //         //      资源不足(ACTION_INVALID_RESOURCE)这类临时失败 -> 不推进, 下帧重试本档
    //         if(info.ins_ret.count(marketTechId)&&
    //            info.ins_ret[marketTechId]!=ACTION_SUCCESS&&
    //            info.ins_ret[marketTechId]!=ACTION_INVALID_RESOURCE)marketTech++;
    //         marketTechId=-1;
    //     }
    //     if(marketTech<3&&marketTechId<0&&haveBuilding(BUILDING_MARKET)){
    //         for(auto&b:info.buildings){
    //             if(b.Type!=BUILDING_MARKET)continue;
    //             if(b.Percent<100)continue;                   // 市场还没盖好 -> 不能研
    //             if(b.Project!=ACT_NULL)continue;             // 市场正忙 -> 这帧不发
    //             if(marketTech==0&&info.Wood>=BUILDING_MARKET_WOOD_UPGRADE_WOOD&&info.Meat>=BUILDING_MARKET_WOOD_UPGRADE_FOOD)
    //                 marketTechId=BuildingAction(b.SN,BUILDING_MARKET_WOOD_UPGRADE);
    //             else if(marketTech==1&&info.civilizationStage==CIVILIZATION_BRONZEAGE&&
    //                     info.Meat>=BUILDING_MARKET_GOLD_UPGRADE_FOOD&&info.Wood>=BUILDING_MARKET_GOLD_UPGRADE_WOOD)
    //                 marketTechId=BuildingAction(b.SN,BUILDING_MARKET_GOLD_UPGRADE);
    //             else if(marketTech==2&&info.civilizationStage==CIVILIZATION_BRONZEAGE&&
    //                     info.Meat>=BUILDING_MARKET_FARM_UPGRADE_FOOD&&info.Wood>=BUILDING_MARKET_FARM_UPGRADE_WOOD)
    //                 marketTechId=BuildingAction(b.SN,BUILDING_MARKET_FARM_UPGRADE);
    //             break;
    //         }
    //     }
        //有无在建的
        int buildingSN=-1; //农田SN 
        for(auto&b:info.buildings){
            if(b.Percent>=100)continue;
            if(b.Type!=BUILDING_MARKET&&b.Type!=BUILDING_ARMYCAMP&&b.Type!=BUILDING_RANGE&&b.Type!=BUILDING_STABLE&&b.Type!=BUILDING_COLLAGE)continue;
            buildingSN=b.SN;
            break;
        }
        //有在建的
        if(buildingSN!=-1){
            
            for(auto&f:info.farmers){
                
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue; //非空闲不建
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                if(bushFarmer.count(f.SN))continue; 
                if(f.WorkObjectSN==buildingSN)continue; //已在建 防重复发指令
                HumanAction(f.SN,buildingSN);
                farIsgotten[f.SN]=true;
                
            }
            
        }
        //无在建的
        
        int want=-1,cost=0;//要什么 花多少
        int bd=centerBlockDR,bu=centerBlockUR;          // 默认以市镇中心为基准找空地
        
        //市场 以市镇中心为基准
       
        
        
        if(!haveBuilding(BUILDING_MARKET)){
            if(marketGate){                              // [AI] 仓库数>=2 或 打猎阶段已结束 才开建市场
                want=BUILDING_MARKET;
                cost=BUILD_MARKET_WOOD;
            }
        }
        //兵营
        else if(!haveBuilding(BUILDING_ARMYCAMP)){
            want=BUILDING_ARMYCAMP;
            cost=BUILD_ARMYCAMP_WOOD;
            if(marketBlockDR!=-1){                   //兵营挨着市场建
                bd=marketBlockDR;
                bu=marketBlockUR;
            }
        }
        //靶场
        else if(!haveBuilding(BUILDING_RANGE)){
            want=BUILDING_RANGE;
            cost=BUILD_RANGE_WOOD;
            if(armyCampBlockDR!=-1){                    // 靶场挨着兵营建
                bd=armyCampBlockDR;
                bu=armyCampBlockUR;
            }
        }
        //马厩(工具时代, 前置兵营)
        else if(info.civilizationStage>=CIVILIZATION_TOOLAGE&&!haveBuilding(BUILDING_STABLE)){
            want=BUILDING_STABLE;
            cost=BUILD_STABLE_WOOD;
            if(armyCampBlockDR!=-1){                    // 马厩也挨着兵营建
                bd=armyCampBlockDR;
                bu=armyCampBlockUR;
            }
        }
        //学院(铜器时代, 前置马厩)
        else if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&!haveBuilding(BUILDING_COLLAGE)&&haveBuilding(BUILDING_STABLE)){
            want=BUILDING_COLLAGE;
            cost=BUILD_COLLAGE_WOOD;
        }
        // //木材不够 -- 空闲的人都去砍树
        // // [AI] 原写法把"闲下来的伐木工"直接从 woodFarmer 里 erase 掉, 有两个问题:
        // //   1) 下面那行补人 if 要求 want!=-1 && info.Wood<cost, 所以木材够用时
        // //      被 erase 掉的人永远不会被重新派活 -> 一直闲着, 名册也永远不恢复;
        // //   2) woodFarmer 语义应是"谁是伐木工"的持续身份, 不是"本帧谁在砍树"的快照。
        // // 改成: 发现某人闲下来, 就立刻给他换一棵别的树; 只有没树可砍了才注销身份。
        // // 收集要处理的人, 遍历结束后再统一 erase, 避免迭代器失效(UB)。
        // vector<int> woodOut;                              // 闲下来 -> 换树; 换不到 -> 注销
        // for(auto &wf:woodFarmer){
        //     for(auto&f:info.farmers){
        //         if(f.SN!=wf.first)continue;
        //         if(f.NowState==HUMAN_STATE_IDLE){                // 闲下来了
        //             if(reassignWoodcutter(f.SN)==-1)woodOut.push_back(f.SN); // 没树可砍才注销
        //         }
        //         break;
        //     }
        // }
        // for(int sn:woodOut)woodFarmer.erase(sn);
        

        // if(want!=-1&&info.Wood<cost&&woodFarmer.size()<5)assignWoodcutter();

        if(buildingSN!=-1)want=-1;                       // [AI] 已有建筑在建 -> 不开新的(原来靠上面那句 return 挡住)
        if(want!=-1&&info.Wood>=cost){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                
               

                static const int DX[2][8]={
                    {-3,0,3,0,-3,3,3,-3},
                    {-6,0,6,0,-6,6,6,-6}
                };
                static const int DY[2][8]={
                    {0,3,0,-3,3,3,-3,-3},
                    {0,6,0,-6,6,6,-6,-6}
                };

                int row=haveBuilding(BUILDING_MARKET)?0:1; //主要是没有市场要建市场 市场是以市镇中心为中心的 为了不堵塞 建造寻找的半径会大一点
                int dx[8]{};
                int dy[8]{};
                for(int i=0;i<8;++i){
                    dx[i]=DX[row][i];
                    dy[i]=DY[row][i];
                }

                for(int k=0;k<8;k++){
                    int dr=bd+dx[k],du=bu+dy[k];
                    if(dr<0||dr>=100||du<0||du>=100)continue;
                    bool ok=true;
                    for(int i=0;i<3&&ok;i++){
                        for(int j=0;j<3;j++){
                            if(MAP[dr+i][du+j]!=Open){
                                ok=false;
                                break;
                            }
                        }
                    }
                    if(!ok)continue;
                    if(spotBusy(dr,du,3))continue; //看看有没有人站在这 一般来说用不到
                    
                    HumanBuild(f.SN,want,dr,du);
                    farIsgotten[f.SN]=true;
                    //一层层传递
                    if(want==BUILDING_MARKET){
                        marketBlockDR=dr;
                        marketBlockUR=du;
                    }
                    if(want==BUILDING_ARMYCAMP){
                        armyCampBlockDR=dr;
                        armyCampBlockUR=du;
                    }
                    break;
                }
                break;
                
            }
        }
        
    
    if(haveBuilding(BUILDING_MARKET)&&granaryBlockDR!=-1&&info.Wood>=BUILD_FARM_WOOD){
        farmNum=0;
        for(auto&b:info.buildings){
            // [AI] F1: 判据由 b.Cnt>0 改为 b.Percent>=100 —— Cnt 是"已收获的粮食存量"
            //      (Core.cpp:612 -> Resource.h:20 get_Cnt), 收割时才增加。
            //      用它当"这块田存在吗"会让"刚建好/正在种/未收割"的田全部隐形,
            //      于是浆果采完的人找不到田 -> 站着不种田。现在只看"盖好没盖好"。
            if(b.Type==BUILDING_FARM&&b.Percent>=100)farmNum++;
        }
        if(farmNum<farmLimit){                            // [AI] 铜器前 4 块, 铜器后 16 块
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN||f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                if(!canFarm(f.SN))continue;               // [AI] 种田三阶段规则(见 canFarm)

                int spotDR[16],spotUR[16];//前8-农田 后8-市镇中心
                for(int q=0;q<8;q++){
                    spotDR[q]=granaryBlockDR+fsDR[q];
                    spotUR[q]=granaryBlockUR+fsDU[q];
                }
                for(int q=0;q<8;q++){
                    spotDR[8+q]=centerBlockDR+fsDR[q];
                    spotUR[8+q]=centerBlockUR+fsDU[q]; 
                }
                for(int q=0;q<16;q++){
                    int dr=spotDR[q], du=spotUR[q];
                    if(dr<0||du<0||dr+3>=100||du+3>=100)continue;
                    bool ok=true;
                    for(int i=0;i<3&&ok;i++)
                        for(int j=0;j<3;j++)
                            if(MAP[dr+i][du+j]!=Open){
                                ok=false;
                                break;
                            }
                    if(!ok)continue;
                    if(spotBusy(dr,du,3))continue;
                    HumanBuild(f.SN,BUILDING_FARM,dr,du);
                    farIsgotten[f.SN]=true;
                    break;
                }
                break;
            }
        }
    }
    
    
    unordered_map<int,bool>liveBush;                     // 还有货的浆果丛
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_BUSH&&r.Cnt>0)liveBush[r.SN]=true;
    }
    
    for(auto&f:info.farmers){
        
        if(!bushFarmer.count(f.SN))continue;
        if(liveBush.count(f.WorkObjectSN)){
            if(f.NowState==HUMAN_STATE_IDLE&&f.ResourceSort==-1){
                HumanAction(f.SN,f.WorkObjectSN);
            }
            continue;
        }
        if(f.ResourceSort!=-1)continue;                  // 手上有货(还没交) -> 先去交
        // [AI] H1: 原来是 if(f.NowState!=HUMAN_STATE_IDLE)continue; 也就是"忙就一律不派活"。
        //      但引擎 infoShare(Core.cpp:696-709) 只要农夫还挂着 WorkObjectSN 就把 NowState 判成
        //      WORKING —— 人在田里干活时 NowState 永远是 WORKING, 于是下一帧被这句跳过,
        //      永远走不到下面的"找田" -> 能种上第一块田, 但永远不会续种。
        //      改成: 只在"他已经挂在某块田上"时才跳过, 其余情况(WORKING 在别处/IDLE/WALKING)都允许续派。
        if(f.NowState!=HUMAN_STATE_IDLE){                 // 还在干活 -> 判他是不是已经在田上
            bool onFarm=false;                            // 已经在田上 -> 不打扰
            for(auto&b:info.buildings){
                if(b.Type==BUILDING_FARM&&b.SN==f.WorkObjectSN){onFarm=true;break;}
            }
            if(onFarm)continue;
        }
        if(f.SN==homeBuilderSN)continue;
        if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;

        int freeFarm=-1;                                 // 1) 有现成空田 -> 去种
        for(auto&b:info.buildings){
            // [AI] F1: 去掉 b.Cnt<=0 —— Cnt 是已收获的粮食存量, 没收割时就是 0,
            //      原来这行会让"刚建好/正在种/粮还没收"的田整块隐形, 浆果采完的人因此找不到田。
            if(b.Type!=BUILDING_FARM||b.Percent<100)continue;
            bool busy=false;
            for(auto&f:info.farmers){
                if(f.WorkObjectSN==b.SN){
                    busy=true;
                    break;
                } 
            }
            if(!busy){
                freeFarm=b.SN;
                break;
            }
        }
        if(freeFarm!=-1){
            HumanAction(f.SN,freeFarm);
            farIsgotten[f.SN]=true;

            continue;
        }

        //重新更新农田数量
        farmNum=0;
        for(auto&b:info.buildings){
            // [AI] F1: 同上, 农田计数改看"盖好没盖好", 不看存量, 否则收割完 farmNum 归零会放开上限狂盖田
            if(b.Type==BUILDING_FARM&&b.Percent>=100)farmNum++;
        }
        if(farmNum>=farmLimit||info.Wood<BUILD_FARM_WOOD)continue;   // [AI] 上限同上 + 金矿木头保证金
        for(int q=0;q<8;q++){
            int dr=granaryBlockDR+fsDR[q], du=granaryBlockUR+fsDU[q];
            if(dr<0||du<0||dr+3>=100||du+3>=100)continue;
            bool ok=true;
            for(int i=0;i<3&&ok;i++)
                for(int j=0;j<3;j++)
                    if(MAP[dr+i][du+j]!=Open){ok=false;break;}
            if(!ok)continue;
            if(spotBusy(dr,du,3))continue;
            HumanBuild(f.SN,BUILDING_FARM,dr,du);
            farIsgotten[f.SN]=true;

            break;
        }
    }
    
    // ---- 建好但没人种的农田 -> 派人去种 ----
    
    for(auto&b:info.buildings){

        if(b.Type!=BUILDING_FARM||b.Percent<100)continue;
        // [AI] F1: 删掉原来的 if(b.Cnt<=0)continue; —— 新盖的田 Cnt 必然是 0(粮是收割才加进去的),
        //      保留这行等于"田刚盖好就不派人", 与本段"派活"的意图正好相反。
        bool busy=false;
        for(auto&f:info.farmers){
            if(f.WorkObjectSN==b.SN){
                busy=true;
                break;
            }
        }
        if(busy)continue;
        //到下面说明这块地没人种
        int pick=-1;
        // [AI] 种田优先挑"采过浆果的人" —— 哪怕他现在在砍树/挖金, 也调过来种田(种田优先级最高);
        //      只有手上扛着货(正在去交货)的不打断, 免得丢资源。
        for(auto&f:info.farmers){
            if(f.SN==homeBuilderSN)continue;
            if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
            
            if(!bushFarmer.count(f.SN))continue;
            if(!bushDone(f.SN))continue;                 // [AI] 必须是"浆果已采完且空闲"才调(原来不问状态直接把人从砍树/挖金调走)
            pick=f.SN;
            break;
        }
        if(pick!=-1){
            HumanAction(pick,b.SN);
            farIsgotten[pick]=true;
            
            continue;                                   // 这块田有人了, 换下一块
        }
        //到这里说明pick还是-1
        for(auto&f:info.farmers){
            if(f.SN==homeBuilderSN||f.NowState!=HUMAN_STATE_IDLE)continue;
            if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
            if(!canFarm(f.SN))continue;                     // [AI] 铜器前不放闲人; 铜器后花名册全员都在田上才放
            HumanAction(f.SN,b.SN);                         // 农民对农田 = 去种/收
            farIsgotten[f.SN]=true;

            break;
        }
    }
    // ---- 兜底: 还闲着的农民, 按 空田 -> 金矿 -> 树 的顺序派活, 不许站着不动 ----
    
    for(auto&f:info.farmers){
        
        if(f.SN==homeBuilderSN)continue;
        if(f.NowState!=HUMAN_STATE_IDLE)continue;
        if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
        
        bool done=false;
        
        if(canFarm(f.SN))
        for(auto&b:info.buildings){                       // ① 有空田就去种
            // [AI] F1: 同上, 去掉 b.Cnt<=0, 否则兜底段也看不见刚盖好/没收割的田
            if(b.Type!=BUILDING_FARM||b.Percent<100)continue;
            bool busy=false;
            for(auto&f:info.farmers){ 
                if(f.WorkObjectSN==b.SN){
                    busy=true;
                    break;
                } 
            }
            if(busy)continue;
            HumanAction(f.SN,b.SN);
            // [AI] 原: bushFarmer[f.SN]=true; (把任意闲人也收编进花名册 -> 名册膨胀超过 6 人) 已去掉。
            //      花名册只由 processData 派浆果工时登记, 保证恒为开局那 6 个人。
            farIsgotten[f.SN]=true;
            
            done=true;
            break;
        }
        if(done)continue;
        // [AI] 金工上限 3 人: 原来这里会把所有闲人都吸去挖金
        if(info.civilizationStage>=CIVILIZATION_BRONZEAGE){                  // ② 没田就去挖金
            int goldNow=0;                                        // 本帧在挖/去挖的金工数
            unordered_map<int,bool>isGold;
            for(auto&r:info.resources){
                if(r.Type==RESOURCE_GOLD)isGold[r.SN]=true;
            }
            for(auto&f2:info.farmers){
                if(f2.NowState!=HUMAN_STATE_WORKING&&f2.NowState!=HUMAN_STATE_WALKING)continue;
                if(isGold.count(f2.WorkObjectSN))goldNow++;
            }
            if(goldNow>=3)continue;                               // 已够 5 人 -> 这个闲人留给种田/砍树
            int tSN=-1,dist=1e18;
            for(auto&r:info.resources){
                if(r.Type!=RESOURCE_GOLD||r.Cnt<=0)continue;
                bool busy=false;
                for(auto&f:info.farmers){ 
                    if(f.WorkObjectSN==r.SN){
                        busy=true;
                        break;
                    } 
                }
                if(busy)continue;
                int dd=max(abs(r.BlockDR-f.BlockDR),abs(r.BlockUR-f.BlockUR));
                if(dd<dist){
                    dist=dd;
                    tSN=r.SN;
                }
            }
            if(tSN!=-1){
                HumanAction(f.SN,tSN);
                goldFarmer[f.SN]=true;
                farIsgotten[f.SN]=true;

                continue;
            }
        }
    }
}


//找离bd bu最近的一个空闲村民
int UsrAI::findFarmer(int bd,int bu){
    int bestSN=-1,bestD=1e18;
    for(auto&f:info.farmers){
        if(f.SN==homeBuilderSN)continue;
        if(f.NowState!=HUMAN_STATE_IDLE)continue;
        if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
        // [AI] 原: if(bushFarmer.count(f.SN))continue; (采浆果的人不干别的) -> 限制已解除
        int d=max(abs(f.BlockDR-bd),abs(f.BlockUR-bu));
        if(d<bestD){
            bestD=d;
            bestSN=f.SN;
        }
    }
    return bestSN;
}
// 派"一个"空闲农民去砍树
// 规则: 以【仓库】为中心 —— 外层遍历仓库, 对每个仓库找离它最近的、还能砍的树,
//       在所有 (仓库,树) 组合里挑距离最小的一对; 再派离那棵树最近的空闲农民过去。
// 调用一次只派一个人(不带人数上限), 想控制人数就在调用方数着调几次;
// 返回: 派出去的农民SN; 没树可砍 / 没空闲农民可派 -> 返回 -1
int UsrAI::assignWoodcutter(){
    // if(woodFarmer.size()>=3)return-1;
    // ---------- ① 以仓库为中心找最近的树 ----------
    int bestSN=-1,bestDR=-1,bestUR=-1,bestD=1e18;
    for(auto&b:info.buildings){                                  // 外层: 仓库
        if(b.Type!=BUILDING_STOCK)continue;
        if(b.Percent<100)continue;                               // 还没建好的不算锚点
        for(auto&r:info.resources){                              // 内层: 树
            if(r.Type!=RESOURCE_TREE)continue;
            if(r.Cnt<=0)continue;                                // 砍光了
            
            // 树必须有一面是空地(能站人), 否则是林子深处, 人挤进去就卡住
            bool reach=false;
            int dx4[4]={0,1,0,-1},dy4[4]={1,0,-1,0};
            for(int k=0;k<4;k++){
                int nr=r.BlockDR+dx4[k],nu=r.BlockUR+dy4[k];
                if(nr<0||nr>=100||nu<0||nu>=100)continue;
                if(MAP[nr][nu]==Open){
                    reach=true;
                    break;
                }
            }
            if(!reach)continue; //这棵树周围不可达 换另一棵树
            
            if(farIsgotten.find(r.SN)!=farIsgotten.end())continue; // 本帧已经给这棵树派过人了 用这个是因为这个每帧都会清空
            bool busy=false;                                     // 这棵树已经被别人占着了吗
            for(auto&f:info.farmers){
                if(f.WorkObjectSN==r.SN){
                    busy=true;
                    break;
                }
            }
            if(busy)continue;
            int d=max(abs(b.BlockDR-r.BlockDR),abs(b.BlockUR-r.BlockUR));  // 仓库->树
            if(d<bestD){
                bestD=d;
                bestSN=r.SN;
                bestDR=r.BlockDR;
                bestUR=r.BlockUR;
            }
        }
    }

    if(bestSN==-1)return -1;                                     // 树都在忙 / 树砍光了 / 还没有仓库

    // ---------- ② 挑离这棵树最近的空闲农民 ----------
    int fSN=-1,best=1e18;
    for(auto&f:info.farmers){
        if(f.SN==homeBuilderSN)continue;
        if(f.NowState!=HUMAN_STATE_IDLE)continue;
        if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
        if(bushFarmer.count(f.SN))continue; 
        int d=max(abs(f.BlockDR-bestDR),abs(f.BlockUR-bestUR));
        if(d<best){
            best=d;
            fSN=f.SN;
        }
    }
    if(fSN==-1)return -1;                                        // 没有空闲的人可派

    // ---------- ③ 派去砍 ----------
    HumanAction(fSN,bestSN);
    farIsgotten[bestSN]=true;
    woodFarmer[fSN]=true;
    farIsgotten[fSN]=true;
    return fSN;
}
// [AI] 新增: 给"指定的伐木工"重新找一棵树并派活(不挑人, 只挑树)。
// 用途: manageBuild 的清理循环发现某个 woodFarmer 闲下来时, 立刻给他换一棵别的树,
//       而不是把他从名册里 erase 掉 —— 否则木材够用时那行补人 if 不执行, 他就永远闲着。
// 与 assignWoodcutter 共用同一套选树规则(仓库锚点 + 四邻可达 + 本帧未派 + 无人占用)。
// 返回: 派出去的树SN; 没树可砍 -> -1
int UsrAI::reassignWoodcutter(int fSN){
    if(fSN==-1)return -1;
    int bestSN=-1,bestDR=-1,bestUR=-1,bestD=1e18;
    for(auto&b:info.buildings){                                  // 外层: 仓库
        if(b.Type!=BUILDING_STOCK)continue;
        if(b.Percent<100)continue;                               // 还没建好的不算锚点
        for(auto&r:info.resources){                              // 内层: 树
            if(r.Type!=RESOURCE_TREE)continue;
            if(r.Cnt<=0)continue;                                // 砍光了
            bool reach=false;                                     // 树必须有一面是空地(能站人)
            int dx4[4]={0,1,0,-1},dy4[4]={1,0,-1,0};
            for(int k=0;k<4;k++){
                int nr=r.BlockDR+dx4[k],nu=r.BlockUR+dy4[k];
                if(nr<0||nr>=100||nu<0||nu>=100)continue;
                if(MAP[nr][nu]==Open){reach=true;break;}
            }
            if(!reach)continue;
            if(farIsgotten.find(r.SN)!=farIsgotten.end())continue; // 本帧已经派过这棵树
            
            bool busy=false;                                     // 这棵树被别人占着了吗
            for(auto&f:info.farmers){
                if(f.SN==fSN)continue;                          // 排除他自己
                if(f.WorkObjectSN==r.SN){
                    busy=true;
                    break;
                }
            }
            if(busy)continue;
            int d=max(abs(b.BlockDR-r.BlockDR),abs(b.BlockUR-r.BlockUR));  // 仓库->树
            if(d<bestD){
                bestD=d;
                bestSN=r.SN;
                bestDR=r.BlockDR;
                bestUR=r.BlockUR;
            }
        }
    }
    if(bestSN==-1)return -1;                                     // 没树可砍了
    HumanAction(fSN,bestSN);
    farIsgotten[bestSN]=true;
    farIsgotten[fSN]=true;
    return bestSN;
}
// 采金前先造仓库 —— 写法对齐 huntGazelle 的 gazelleState==3 段(用户: "完全按瞪羚那的逻辑来"):
//   checkEnv(RESOURCE_GOLD) 判有没有仓库 -> 有(2=已建好 / 1=在建)就不用管, 交给 assignGoldMiner 派人去采;
//   没有(0)就先 assignWoodcutter 凑木头, 再 findBuildSpot 在矿边 HumanBuild 一个仓库。
// 为什么不需要"仓库数量上限"和"矿挖空自动换矿":
//   checkEnv 是**全局**判据 —— 任意一口金矿旁有仓库就返回非 0, 所以天然只会建一个仓库,
//   换矿建第二个仓库这件事根本不会发生, 那两层逻辑是纯冗余。
// 返回: 这一帧下过建造指令返回 true
bool UsrAI::buildGoldStock(){
    int bySN=-1;
    if(checkEnv(RESOURCE_GOLD,bySN)!=0)return false;      // ① 已有仓库(或在建) -> 这一环结束

    if(goldSpotDR==-1){                                    // ② 还没定落脚点 -> 找一口离市中心最近、没人占的金矿
        int best=1e18;
        for(auto&r:info.resources){
            if(r.Type!=RESOURCE_GOLD||r.Cnt<=0)continue;
            bool busy=false;
            for(auto&f:info.farmers){
                if(f.WorkObjectSN==r.SN){
                    busy=true;
                    break;
                }
            }
            if(busy)continue;
            int d=max(abs(r.BlockDR-centerBlockDR),abs(r.BlockUR-centerBlockUR));
            if(d<best){
                best=d;
                goldSpotDR=r.BlockDR;
                goldSpotUR=r.BlockUR;
            }
        }
        if(goldSpotDR==-1)return false;                    // 一口金矿都没有
    }

    if(info.Wood<BUILD_STOCK_WOOD){
        assignWoodcutter();
        return false;
    }   // ③ 木不够 -> 先砍树(和瞪羚一样)
    if(storageStarted)return false;                        // 本帧已经下过建造指令了
    int ox=-1,oy=-1;
    if(!findBuildSpot(goldSpotDR,goldSpotUR,3,2,4,ox,oy))return false;  // 找不到空地 -> 下帧再试
    int fSN=findFarmer(goldSpotDR,goldSpotUR);
    if(fSN==-1)return false;
    HumanBuild(fSN,BUILDING_STOCK,ox,oy);
    
    farIsgotten[fSN]=true;
    return true;
}
int UsrAI::assignGoldMiner(){
    int bestSN=-1,bestDR=-1,bestUR=-1,bestD=1e18;
    for(auto&b:info.buildings){                       // 外层: 仓库
        if(b.Type!=BUILDING_STOCK)continue;
        if(b.Percent<100)continue;                    // 还没建好的不算锚点
        for(auto&r:info.resources){                   // 内层: 金矿
            if(r.Type!=RESOURCE_GOLD)continue;
            if(r.Cnt<=0)continue;                     // 挖光了
            if(farIsgotten.find(r.SN)!=farIsgotten.end())continue;
            bool busy=false;
            for(auto&f:info.farmers){
                if(f.WorkObjectSN==r.SN){busy=true;break;}
            }
            if(busy)continue;
            int d=max(abs(b.BlockDR-r.BlockDR),abs(b.BlockUR-r.BlockUR));  // 仓库->金矿
            if(d<bestD){
                bestD=d;
                bestSN=r.SN;
                bestDR=r.BlockDR;
                bestUR=r.BlockUR;
            }
        }
    }
    if(bestSN==-1){                                   // 一个仓库都没配上 -> 退回"最近的口就采"
        for(auto&r:info.resources){
            if(r.Type!=RESOURCE_GOLD||r.Cnt<=0)continue;
            if(farIsgotten.find(r.SN)!=farIsgotten.end())continue;
            bool busy=false;
            for(auto&f:info.farmers){ 
                if(f.WorkObjectSN==r.SN){
                    busy=true;
                    break;
                }
            }
            if(busy)continue;
            bestSN=r.SN;bestDR=r.BlockDR;bestUR=r.BlockUR;
            break;
        }
        if(bestSN==-1)return -1;
    }

    int fSN=-1,best=1e18;                             // 派离这口井最近的空闲农民
    for(auto&f:info.farmers){
        if(f.SN==homeBuilderSN)continue;
        if(f.NowState!=HUMAN_STATE_IDLE)continue;
        if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
        int d=max(abs(f.BlockDR-bestDR),abs(f.BlockUR-bestUR));
        if(d<best){
            best=d;
            fSN=f.SN;
        }
    }
    if(fSN==-1)return -1;

    HumanAction(fSN,bestSN);
    farIsgotten[bestSN]=true;
    farIsgotten[fSN]=true;
    goldFarmer[fSN]=true;        // 记住他, 矿采完自动给他接下一口
    return fSN;
}

//该函数可以获得离bd bu最近的一个gazelle 并且获得其SN DR UR
int UsrAI::findGazelle(int bd,int bu,int& gazelleDR,int& gazelleUR,int maxR){
    int bestSN=-1;
    int bestD=1e18;
    for(auto&r:info.resources){
        if(r.Type!=RESOURCE_GAZELLE)continue;
        if(r.Blood<=0)continue;
        int d=max(abs(r.BlockDR-bd),abs(r.BlockUR-bu));
        if(d>maxR)continue;
        if(d<bestD){
            bestD=d;
            bestSN=r.SN;
            gazelleDR=r.BlockDR;
            gazelleUR=r.BlockUR;
        }
    }
    return bestSN;
}
///////////////////////////////////////
//检查sn是否已到dr ur（一格容错）
bool farmerAt(int sn,int dr,int ur){
    for(auto&f:info.farmers){
        if(f.SN!=sn)continue;
        if(abs(f.BlockDR-dr)<=1&&abs(f.BlockUR-ur)<=1)return true;
        return false;
    }
    return false;
}
//看当前的瞪羚是否还存活（或还未找-1）
bool isLiveGazelle(int sn){
    for(auto&r:info.resources){
        if(r.SN!=sn)continue;
        return r.Type==RESOURCE_GAZELLE&&r.Blood>0; 
    }
    return false;
}
/////////////////////////////////////////////////
void UsrAI::huntGazelle(){
    if(gazelleState==4)return;
    if(gazelleState==0){
        if(liveGazelleNum()<gazelleWantNum)return; //没达到要求就不进
        
        int gazelleDR=-1,gazelleUR=-1;
        int nearSN=findGazelle(centerBlockDR,centerBlockUR,gazelleDR,gazelleUR);
        if(nearSN==-1)return;
        gazelleSpotDR=gazelleDR;
        gazelleSpotUR=gazelleUR;
        


        int dx4[4]={0,1,0,-1};
        int dy4[4]={1,0,-1,0};
        for(int i=0;i<4;i++){
            int nr=gazelleDR+dx4[i];
            int nu=gazelleUR+dy4[i];
            if(nr<0||nr>=100||nu<0||nu>=100)continue;
            if(MAP[nr][nu]!=Open)continue;
            gazelleSpotDR=nr;
            gazelleSpotUR=nu;
            break;
        } //刷新落脚点

        //招募猎人
        if(gazelleHunter1SN==-1)gazelleHunter1SN=findFarmer(gazelleSpotDR,gazelleSpotUR);
        if(gazelleHunter1SN==-1)return;
        farIsgotten[gazelleHunter1SN]=true;

        if(gazelleHunter2SN==-1)gazelleHunter2SN=findFarmer(gazelleSpotDR,gazelleSpotUR);
        if(gazelleHunter2SN==-1)return;
        farIsgotten[gazelleHunter2SN]=true;

        HumanMove(gazelleHunter1SN,gazelleSpotDR*BLOCKSIDELENGTH,gazelleSpotUR*BLOCKSIDELENGTH);
        HumanMove(gazelleHunter2SN,(gazelleSpotDR-1)*BLOCKSIDELENGTH,gazelleSpotUR*BLOCKSIDELENGTH);
        gazelleState=1;
        //0阶段结束 进入1阶段
        return;

    }

    if(gazelleState==1){
        farIsgotten[gazelleHunter1SN]=true; //这样就不用再给每个派人逻辑判断是否时gazelleHunter了
        farIsgotten[gazelleHunter2SN]=true;
        if(!farmerAt(gazelleHunter1SN,gazelleSpotDR,gazelleSpotUR))return;
        if(!farmerAt(gazelleHunter2SN,gazelleSpotDR,gazelleSpotUR))return; 
        
        gazelleState=2;
    }
    if(gazelleState==2){
        farIsgotten[gazelleHunter1SN]=true;
        farIsgotten[gazelleHunter2SN]=true;

        if(!isLiveGazelle(gazelleTargetSN)){
            int gazelleDR=-1,gazelleUR=-1;
            gazelleTargetSN=findGazelle(gazelleSpotDR,gazelleSpotUR,gazelleDR,gazelleUR,20);
            
            
            if(gazelleTargetSN==-1||killGazelle>=6){          // 一只活瞪羚都没有了||
                gazelleState=3;
                return;
                
            }
            HumanAction(gazelleHunter1SN,gazelleTargetSN);
            HumanAction(gazelleHunter2SN,gazelleTargetSN);
            killGazelle++;
        }
        return;
    }
        
    
    if(gazelleState==3){
        int byBuildingSN=-1;
        if(checkEnv(RESOURCE_GAZELLE,byBuildingSN)==2){
            gazelleState=4;
            return;
        }
        if(info.Wood<BUILD_STOCK_WOOD){
            // gazelleState=4; //不够就再等等
            assignWoodcutter();
            return;
        }
        int ox=-1,oy=-1; //output_x/y
        if(findBuildSpot(gazelleSpotDR,gazelleSpotUR,3,2,4,ox,oy)){ //minR已经没作用了
            HumanBuild(gazelleHunter1SN,BUILDING_STOCK,ox,oy);
            storageStarted=true;
            farIsgotten[gazelleHunter1SN]=true;
            
            
            
            gazelleState=4;
            return;
            
        }
        gazelleState=4;
        return;
    }
}

// ================= 反攻状态机 =================
// 0防守 -> 1侦察(骑兵去敌营对角找攻城厂) -> 2集结(一直造到人口满) -> 3推进(到厂区26格外)
//      -> 4清猎手(祭司点火把5个猎手引出来围杀) -> 5转化(兵挡守军, 祭司贴厂转化=胜利)
// ================= [AI] 斥候统一动作 =================
//  斥候是"探路 / 拉扯 / 找厂 / 打箭塔"的唯一执行者。没有斥候就让马厩造一个, 死了也会重造。
//  mode = 0 探路  : 朝敌方大营方向一段一段推进(视野内出现敌人时由上层记下锚点)
//         1 拉扯  : 视野内出现敌人就退回锚点(集结点), 否则继续前出引诱
//         2 找厂  : 同探路, 一直走到 info.enemy_buildings 里出现攻城厂
//         3 打箭塔: 攻击"离攻城厂最近"的那座敌方箭塔
void UsrAI::manageScout(int mode){
    if(scoutSN==-1){
        for(auto&a:info.armies){
            if(a.Sort==AT_SCOUT){ 
                scoutSN=a.SN; 
                break; 
            }
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
        return;
    }

    // ---- ② 找到斥候本体(找不到说明它已死 -> 下帧重造) ----
    int scoutDr=-1,scoutUr=-1;
    for(auto&a:info.armies){
        if(a.SN!=scoutSN)continue;
        scoutDr=a.BlockDR; 
        scoutUr=a.BlockUR;
        break;
    }
    if(scoutUr==-1){ 
        scoutSN=-1; 
        return; 
    }
    // ---- ⑤ 模式 0/1/2: 朝敌方大营方向推进(直走, 每次朝对角方向推 6 格) ----
    // [AI] 修死循环: 原来是 seekTgtDR=-1 || 走到 || MAP[seekTgt]!=Open 就重发,
    //      而 seekTgt 是"当前位置 + 方向*6" 盲推出来的 —— 若那格是海/未知,
    //      MAP!=Open 恒成立 -> 每帧都重算目标+重发 HumanMove, 寻路被反复重置, 斥候原地不动。
    //      现在: 目标格不可用时沿同一方向逐格向内试探(最多 6 格), 保证目标落在能站的 Open 格;
    //      并且目标没变就不重发(避免同帧重复下令)。
    static int lastSeekDR=-1,lastSeekUR=-1;     // 上一条推进指令的目标(用于"没变就不重发")
    bool tgtOK=(seekTgtDR!=-1&&MAP[seekTgtDR][seekTgtUR]==Open);
    if(!tgtOK||(abs(scoutDr-seekTgtDR)<=1&&abs(scoutUr-seekTgtUR)<=1)){
        int gd=100-centerBlockDR, gu=100-centerBlockUR;   // 敌方大营方向(对角估算)
        int dx=(gd>scoutDr)?1:((gd<scoutDr)?-1:0);
        int dy=(gu>scoutUr)?1:((gu<scoutUr)?-1:0);
        if(dx==0&&dy==0)return;
        int tdr=-1,tur=-1;
        for(int step=6;step>=1;--step){                     // 从 6 格往回试, 取第一个能站的
            int candDR=scoutDr+dx*step, candUR=scoutUr+dy*step;
            if(candDR<2||candDR>97||candUR<2||candUR>97)continue;
            if(MAP[candDR][candUR]==Open){ tdr=candDR; tur=candUR; break; }
        }
        if(tdr==-1)return;                                   // 六个方向全不可站 -> 本帧不动
        seekTgtDR=tdr; seekTgtUR=tur;
    }
    if(seekTgtDR!=lastSeekDR||seekTgtUR!=lastSeekUR){      // 目标变了才下令
        lastSeekDR=seekTgtDR; lastSeekUR=seekTgtUR;
        HumanMove(scoutSN,seekTgtDR*BLOCKSIDELENGTH,seekTgtUR*BLOCKSIDELENGTH);
    }
    // ---- ④ 模式 1/2: 视野内出现敌人 -> 退回锚点(集结点) ----
    bool enemyNear=false;
    for(auto&ea:info.enemy_armies){
        if(max(abs(ea.BlockDR-scoutDr),abs(ea.BlockUR-scoutUr))<=SCOUT_LURE_RANGE){ 
            enemyNear=true; 
            break; 
        } 
    }
    if(mode>=1&&enemyNear&&rallyDR!=-1){
        // [AI] 撤回也加"没变就不重发": 原来每帧都 HumanMove 回集结点, 会反复重置寻路。
        static int backDR=-1,backUR=-1;
        if(scoutDr!=backDR||scoutUr!=backUR){
            backDR=scoutDr; backUR=scoutUr;
            HumanMove(scoutSN,rallyDR*BLOCKSIDELENGTH,rallyUR*BLOCKSIDELENGTH);
        }
        seekTgtDR=-1;
        seekTgtUR=-1;              // 撤回来了 -> 清掉推进目标
        lastSeekDR=-1; lastSeekUR=-1;   // 推进目标也清掉, 下次要推进时重新下令
        return;
    }


    // ---- ③ 模式 3: 打"离攻城厂最近"的那座敌方箭塔 ----
    if(mode==3){
        int bestSN=-1,best=1e18;
        for(auto&eb:info.enemy_buildings){
            if(eb.Type!=BUILDING_ARROWTOWER)continue;
            int d=max(abs(eb.BlockDR-factoryBlockDR),abs(eb.BlockUR-factoryBlockUR));
            if(d<best){ 
                best=d; 
                bestSN=eb.SN; 
            }
        }
        if(bestSN==-1)return;
        if(towerTgtSN==bestSN)return;            // 已经在打同一座 -> 不重发
        HumanAction(scoutSN,bestSN);
        towerTgtSN=bestSN;
        return;
    }

    

    
}

// ================= [AI] 部队攻击 =================
//  优先级: ① 远程兵(复合弓/战车弓/弓箭手) —— 目的是保护祭司; ② 最近的敌人。
//  已在攻击中的兵不重发(重发会清掉攻击进度); 每帧最多派 4 个, 免得超引擎每帧指令上限。
void UsrAI::armyAttack(){
    if(info.enemy_armies.empty())return;
    // if(rallyDR==-1)return;                              // [AI] 集结点未定 -> 不动(防大军擅自行动)
    // const int ARMY_MAX_RANGE=30;                        // [AI] R5(意见5): 大军距集结点>30格不打, 防离集结区
    
    for(auto&a:info.armies){
        
        if(a.SN==priestSN)continue;              // 祭司专职转化
        if(a.SN==scoutSN)continue;               // 斥候专职拉扯
        if(a.WorkObjectSN!=-1)continue;          // 手上已有任务
        int tSN=-1;
        for(auto&ea:info.enemy_armies){          // ① 优先打远程兵
            // if(max(abs(ea.BlockDR-rallyDR),abs(ea.BlockUR-rallyUR))>ARMY_MAX_RANGE)continue; // [AI] R5: 距集结点>30格不打
            if(isArcherSort(ea.Sort)){ 
                tSN=ea.SN; 
                break; 
            }
        }
        if(tSN==-1){                             // ② 没有远程兵 -> 打最近的
            double best=1e18;
            for(auto&ea:info.enemy_armies){
                // if(max(abs(ea.BlockDR-rallyDR),abs(ea.BlockUR-rallyUR))>ARMY_MAX_RANGE)continue; // [AI] R5: 距集结点>30格不打
                double d=calDistance(a.DR,a.UR,ea.DR,ea.UR);
                if(d<best){ 
                    best=d; 
                    tSN=ea.SN; 
                }
            }
        }
        if(tSN!=-1){ 
            HumanAction(a.SN,tSN); 
            
        }
    }
}

// ================= [AI] 祭司转化 =================
//  在"祭司响应范围"内优先转【当前生命值最高】的敌人(削弱敌人 + 增强己方)。
//  门与原来一致: 不在探索 / 非转化中 / 冷却已好 / 目标不是正在转的那个。
void UsrAI::priestConvert(){
    if(priestSN==-1||priestExploring)return;
    const int pRange=(VISION_PRIEST+PRIEST_HARNESS)*(VISION_PRIEST+PRIEST_HARNESS);
    int tSN=-1,bestHp=-1;
    for(auto&ea:info.enemy_armies){
        int dx=ea.BlockDR-priestBlockDR, dy=ea.BlockUR-priestBlockUR;
        if(dx*dx+dy*dy>pRange)continue;          // 只在响应范围内
        if(ea.Blood>bestHp){ 
            bestHp=ea.Blood; 
            tSN=ea.SN; 
        }
    }
    if(tSN==-1)return;
    for(auto&a:info.armies){
        if(a.SN!=priestSN)continue;
        // [AI] 原来判 ==HUMAN_STATE_ATTACKING, 但 NowState 是 infoShare 重算的(Core.cpp:694-710):
        //      目标在"人类"集合里才 ATTACKING, 目标在建筑上就是 WORKING。祭司贴厂转化攻城厂时
        //      一直是 WORKING, 这个门挡不住 -> 会对同一目标反复下令。加 IDLE/WALKING 兜住。
        if(a.NowState!=HUMAN_STATE_IDLE&&a.NowState!=HUMAN_STATE_WALKING)break;   // 忙(转化中)
        if(a.ConvertCooldown>0)break;                 // 冷却没好
        if(a.WorkObjectSN==tSN)break;                 // 已经在转它了
        HumanAction(priestSN,tSN);
        break;
    }
}

// ================= [AI] 祭司跟随 =================
//  用户要求: 祭司"始终躲在大军的后面", 与部队的距离不得超过 PRIEST_HARNESS(5 格)。
//  做法: 找离祭司最近的一个我方战斗单位; 距离超过 5 格就把祭司拉过去(斥候不计入, 它单独在外拉扯)。
void UsrAI::priestFollow(){
    
    // [AI] 新发现A: 祭司忙(转化中/移动中)时不打扰, 否则 HumanMove 会 suspend 旧关系清掉转化进度
    for(auto&a:info.armies){
        if(a.SN!=priestSN)continue;
        if(a.NowState!=HUMAN_STATE_IDLE&&a.NowState!=HUMAN_STATE_WALKING)return;
        break;
    }
    int bestD=1e9,bdr=-1,bur=-1;
    for(auto&a:info.armies){
        if(a.SN==priestSN)continue;      // 不含自己
        if(a.SN==scoutSN)continue;       // 斥候单独在外拉扯, 不算"大军"
        int d=max(abs(a.BlockDR-priestBlockDR),abs(a.BlockUR-priestBlockUR));
        if(d<bestD){ 
            bestD=d; 
            bdr=a.BlockDR; 
            bur=a.BlockUR; 
        }
    }
    if(bdr==-1)return;                   // 没有部队 -> 不动
    if(bestD<=PRIEST_HARNESS)return;     // 已经贴在大军旁边 -> 不动
    HumanMove(priestSN,bdr*BLOCKSIDELENGTH,bur*BLOCKSIDELENGTH);
}

void UsrAI::counterAttack(){
    switch(counterState){

        case 0:{   // ---- 防守: 见过敌投石车(只在第三波出现) 且 现在没可见敌兵 = 三波都清了 -> 转侦察 ----
            static bool seenWave3=false;                       // 纯状态驱动, 不用帧
            for(auto&e:info.enemy_armies){ 
                if(e.Sort==AT_STONE_THROWER)seenWave3=true; 
            }
            if(seenWave3&&info.enemy_armies.empty())counterState=1; //三波已过 到达中期
            break;
        }

        case 1:{  
            if(info.Human_Num>=46){ //如果设定为50 人就满了 造不了斥候
                counterState=2;
                return;
            }
            break;
        }

        case 2:{   
            //完成 RUSH_ROUNDS 轮拉扯之后, 转 case 3 全面反攻。
            if(rushRound>=RUSH_ROUNDS){  //拉扯五轮之后 进入全面反攻
                counterState=3; 
                return; 
            }

            //拉扯
            
            if(anchorDR<0||anchorUR<0){
                manageScout(0);
                if(!info.enemy_armies.empty()){
                    int bestD=1e18;
                    for(auto&ea:info.enemy_armies){
                        int d=max(abs(ea.BlockDR-centerBlockDR),abs(ea.BlockUR-centerBlockUR));
                        //找离市镇中心最近的一个敌军位置作为锚点
                        if(d<bestD){ 
                            bestD=d; 
                            anchorDR=ea.BlockDR; 
                            anchorUR=ea.BlockUR; 
                        }
                    }
                }
                return;
            }

            // ② 锚点有了 -> 定集结点 + 集结(只做一次; 铺开之后大军不再整体移动)
            pickRallyPoint();
            if(!rallyDone){
                rallyArmy();
                rallyPriest();
                if(rallyEnough())rallyDone=true;
                return;
            }

            // ③ 集结完成 -> 斥候前出引诱, 大军在集结区等敌人上门
            manageScout(1);
            priestFollow();          // [AI] 祭司始终跟在大军身边(用户要求: 不脱离部队 5 格)

            // ④ 计轮: 视野内的敌人"从有到无" -> 本轮拉扯结束(用户选的 A 方案: 只看视野)
            if(info.enemy_armies.empty()){
                if(rushHadEnemy){ 
                    rushHadEnemy=false; 
                    rushRound++; 
                }
            }
            else{
                rushHadEnemy=true;
            }
            break;
        }

        case 3:{   // ---- 反攻: 斥候找厂/打箭塔 + 大军推进 + 祭司转化 ----
            // ① 攻城厂还没进视野 -> 斥候继续找(路上会避障; 死了下一帧会自动让马厩重造)
            if(factorySN==-1){
                manageScout(2);
                return;
            }
            // ② 厂找到了 -> 斥候去打"离厂最近"的那座箭塔, 吸引箭塔火力给祭司让路
            manageScout(3);
            // ③ 大军推进: 攻击视野内的敌人(优先远程兵 = 护祭司)
            armyAttack();
            // ④ 祭司转化(优先生命值最高的敌人); 贴厂转化由 CalmAndCrazy 负责
            priestFollow();          // [AI] 先保证祭司不脱离大军
            priestConvert();
            break;
        }
    }
}

// 定集结点: 敌人位置 -> 朝自家方向退 RALLY_BACK 格 (只做一次, 定完不再动)
void UsrAI::pickRallyPoint(){
    if(rallyDR!=-1)return;               // 已经定过了 (和你代码里 if(factorySN==-1) 一个套路)
    //到这里说明还没有定集结点
    int eDR=anchorDR,eUR=anchorUR;          // e = enemy: 锚点就是"斥候被咬到的那一格"
    
    // 朝自家(市中心)退: 我家 DR 比敌人大就往 DR 正方向退, 否则往负方向退
    rallyDR = eDR + (centerBlockDR>eDR ?  RALLY_BACK : -RALLY_BACK);
    rallyUR = eUR + (centerBlockUR>eUR ? RALLY_BACK : -RALLY_BACK);
    rallyDR=max(2,min(97,rallyDR));           // 别退到地图外面
    rallyUR=max(2,min(97,rallyUR));
}

// ② 铺开: 每帧最多派一个兵; 已分过格的一律不重发
bool UsrAI::rallyArmy(){
    unordered_map<int,bool> taken;
    vector<int> deadSlot;                           // [AI] 已阵亡单位的槽位, 收集后统一 erase
    for(auto& r:rallySlot){          // 清掉死人的槽位
        bool alive=false;
        for(auto&a:info.armies){ 
            if(a.SN==r.first){
                alive=true;
                break;
            } 
        }
        if(!alive){ 
            deadSlot.push_back(r.first);            
            continue; 
        }
        //走到这边意味着是活着的 
        taken[r.second]=true;
    }
    for(int& sn:deadSlot)rallySlot.erase(sn);        

    // [AI] 循环填入: 已经在格上待过的兵, 如果后来离开了那一格(被敌人引走/追击),
    //      就把格子释放出来让别人能填 —— 对应策略"军队移动后集结点空出, 可以循环填入"。
    //      注意: 还没到过自己格的兵不释放(他还在路上), 否则刚派出去就被回收。
    vector<int> leftSlot;
    for(auto& r:rallySlot){
        if(r.first==priestSN)continue;              // 祭司单独处理
        for(auto&a:info.armies){
            if(a.SN!=r.first)continue;
            if(abs(a.BlockDR-r.second/100)<=1&&abs(a.BlockUR-r.second%100)<=1){
                rallyArrived[r.first]=true;         // 他确实到过自己的格
            }
            else if(rallyArrived[r.first]){
                leftSlot.push_back(r.first);        // 到过、现在又离开了 -> 释放该格
            }
            break;
        }
    }
    for(int sn:leftSlot)rallySlot.erase(sn);
    for(auto&a:info.armies){
        if(a.SN==priestSN)continue;                 // 祭司单独处理
        if(rallySlot.count(a.SN))continue;          // ★已分过→不重发(状态驱动, 不含帧数)
        
        int ex=(anchorDR!=-1)?anchorDR:(2*centerBlockDR-rallyDR);
        int ey=(anchorUR!=-1)?anchorUR:(2*centerBlockUR-rallyUR);
        bool melee=isMeleeSort(a.Sort);
        int bestKey=-1,bestD=melee?1e9:-1e9;
        for (int dx = -RALLY_R; dx <= RALLY_R; dx++) {
            for (int dy = -RALLY_R; dy <= RALLY_R; dy++) {
                int dr = rallyDR + dx, ur = rallyUR + dy;
                if (dr < 0 || dr >= 100 || ur < 0 || ur >= 100) continue;
                if (MAP[dr][ur] != Open) continue;
                int key = dr * 100 + ur;
                if (taken.count(key)) continue;
                int depth=max(abs(dr-ex),abs(ur-ey));   // 离敌人越近 depth 越小
                int dd=melee?depth:-depth;             // 近战取小(靠前), 远程取大(靠后)
                if(melee){ 
                    if(dd<bestD){
                        bestD=dd;
                        bestKey=key;
                    } 
                }
                else { 
                    if(dd>bestD){
                        bestD=dd;
                        bestKey=key;
                    } 
                }
            }
        }
        if(bestKey==-1)break;
        rallySlot[a.SN]=bestKey; 
        taken[bestKey]=true;
        HumanMove(a.SN,(bestKey/100)*BLOCKSIDELENGTH,(bestKey%100)*BLOCKSIDELENGTH);
        return true;                                
    }
    return false;
}

// ③ 祭司: 区域里挑"离敌人最远"的一格
void UsrAI::rallyPriest(){
    if(priestSN==-1||rallySlot.count(priestSN))return;
    int eDR=anchorDR,eUR=anchorUR;
    int bestKey=-1,bestD=-1;
    for(int dx=-RALLY_R;dx<=RALLY_R;dx++)for(int dy=-RALLY_R;dy<=RALLY_R;dy++){
        int dr=rallyDR+dx,ur=rallyUR+dy;
        if(dr<0||dr>=100||ur<0||ur>=100)continue;
        if(MAP[dr][ur]!=Open)continue;
        int key=dr*100+ur;
        bool used=false;
        for(auto&kv:rallySlot)if(kv.second==key){
            used=true;
            break;
        }

        if(used)continue;
        int dd=max(abs(dr-eDR),abs(ur-eUR));
        if(dd>bestD){
            bestD=dd;
            bestKey=key;
        }
    }
    if(bestKey==-1)return;
    rallySlot[priestSN]=bestKey;
    HumanMove(priestSN,(bestKey/100)*BLOCKSIDELENGTH,(bestKey%100)*BLOCKSIDELENGTH);
}

// ④ 到齐判据
bool UsrAI::rallyEnough(){
    int n=0;
    for(auto&kv:rallySlot){
        if(kv.first==priestSN)continue;
        for(auto&a:info.armies){
            if(a.SN!=kv.first)continue;
            
            if(abs(a.BlockDR-kv.second/100)<=1&&abs(a.BlockUR-kv.second%100)<=1)n++;
            break;
        }
    }
    return n>=RALLY_NEED;
}





void UsrAI::waveBattle(){
    //此函数主要实现反攻前的战斗逻辑
    if(counterState>=1)return;
    if(counterState<1&&info.enemy_armies.empty())return; //没有检测到兵 自然不用准备战斗
    

    int priestTarget=-1,towerTarget=-1;//祭司的目标与箭塔的目标

    //箭塔是固定的，作用范围不能扩大
    
    //祭司是可以机动的 所以技能作用范围可以扩大
    const int pRange=(VISION_PRIEST+PRIEST_HARNESS)*(VISION_PRIEST+PRIEST_HARNESS); //反攻前祭司可以响应的范围
    int dist=1e18; //作为最大值参照
    for(auto&ea:info.enemy_armies){                          // 有投石车先转投石车
        if(counterState==1)break;
        if(ea.Sort!=AT_STONE_THROWER)continue;
        int ex=ea.BlockDR-priestBlockDR, ey=ea.BlockUR-priestBlockUR;
        int d=ex*ex+ey*ey;
        if(d<=dist&&d<=pRange){
            dist=d;
            priestTarget=ea.SN;
        }
    }
    if(priestTarget==-1&&counterState==0){ //没投石车 -> 塔下拉扯时优先转远程兵(用户定)
        //理由: 复合弓/战车弓射程7~10, 站在塔外持续输出, 是塔的主要火力来源;
        //      转掉它等于让箭塔哑火, 同时比转血厚的近战更快(省祭司的 20 秒休整时间)。
        //      优先级: 投石车 > 远程兵 > 最近的敌人(下面的兜底)。
        for(auto&ea:info.enemy_armies){
            if(!isArcherSort(ea.Sort))continue;
            int ex=ea.BlockDR-priestBlockDR, ey=ea.BlockUR-priestBlockUR;
            int d=ex*ex+ey*ey;
            if(d<=dist&&d<=pRange){
                dist=d;
                priestTarget=ea.SN;
                break;
            }
        }
    }
    if(priestTarget==-1&&counterState<1){ //加锁 集结状态不可以主动进攻  反攻阶段作另外逻辑                              // 还没有远程兵 -> 取最近的敌人

        for(auto&ea:info.enemy_armies){
            int ex=ea.BlockDR-priestBlockDR, ey=ea.BlockUR-priestBlockUR;
            int d=ex*ex+ey*ey;
            if(d<=dist){
                dist=d;
                priestTarget=ea.SN;
            }
        }
    }
   //处理-祭司-的目标
    if(counterState<1&&priestTarget!=-1&&!priestExploring){
        for(auto&a:info.armies){
            if(a.SN!=priestSN)continue;
            if(a.NowState!=HUMAN_STATE_IDLE&&a.NowState!=HUMAN_STATE_WALKING)break;  //[AI]忙(转化中); 理由同 priestConvert
            if(a.ConvertCooldown>0)break;                       // 冷却没好 -> 唯一的门
            if(a.WorkObjectSN==priestTarget)break;              // 已经在转它了
            if(priestTarget!=-1){
                HumanAction(priestSN,priestTarget);
                break;
            }
        }
    }
    //处理-箭塔-的目标
    
    int bestD=1e18;
    for(auto&ea:info.enemy_armies){
        //找一个最近的、没打自己的敌人
        int arrowD=max(abs(ea.BlockDR-arrowTowerBlockDR),abs(ea.BlockUR-arrowTowerBlockUR));//计算距离
        if(arrowD>DIS_ARROWTOWER)continue; //太远了
        if(ea.WorkObjectSN==arrowTowerSN)continue; //这个人的仇恨已经吸引到了
        if(arrowD<bestD){ //先记录下来
            bestD=arrowD;
            towerTarget=ea.SN;
        }
    }
    if(towerTarget!=-1)HumanAction(arrowTowerSN,towerTarget);
    
    bool isAllIn=true;
    for(auto&ea:info.enemy_armies){
        int arrowD=max(abs(ea.BlockDR-arrowTowerBlockDR),abs(ea.BlockUR-arrowTowerBlockUR));//计算距离
        if(arrowD>DIS_ARROWTOWER)continue; //太远了
        if(ea.WorkObjectSN!=arrowTowerSN)isAllIn=false;
    }
    if(isAllIn){
        int Blood=1e18;
        int tSN=-1;
        for(auto&ea:info.enemy_armies){
            int arrowD=max(abs(ea.BlockDR-arrowTowerBlockDR),abs(ea.BlockUR-arrowTowerBlockUR));//计算距离
            if(arrowD>DIS_ARROWTOWER)continue; //太远了
            if(ea.Blood<Blood){
                Blood=ea.Blood;
                tSN=ea.SN;
            }
        }
        if(tSN!=-1)HumanAction(arrowTowerSN,towerTarget);
    }


    // ---- 1v1 牵制: 每个敌方单位至少派 1 个我方单位(祭司除外, 祭司专职转化) ----
    set<int> spare;                                     // 本帧 能派出去的我方单位
    for(auto&a:info.armies){
        if(a.SN==priestSN)continue;     //祭司不要 专职转化
        if(a.WorkObjectSN==-1)spare.insert(a.SN);     // 手上没任务的才算闲置
    }
    
    for(auto&ea:info.enemy_armies){
        
        if(counterState<1){                                   // 只限防守/集结期
            //先以箭塔为参照 离得太远的敌人先不管
            int ex=ea.BlockDR-arrowTowerBlockDR,ey=ea.BlockUR-arrowTowerBlockUR;
            if(ex*ex+ey*ey>400)continue;
        }
        
        bool engaged=false;                            //是否有人负责打他
        for(auto&a:info.armies){
            if(a.SN==priestSN||a.WorkObjectSN!=ea.SN)continue;
            engaged=true; //走到这说明不是祭司 是一个工作对象为他的士兵 所以有人在打他
            break;
        }
        if(engaged)continue;               // 有 -> 不动它(重发会清掉攻击进度)

        int pick=-1;double dist=1e18;          // 没有 -> 派最近的闲置兵过去
        for(auto&a:info.armies){
            if(!spare.count(a.SN))continue;      //非闲置兵
            double d=calDistance(a.DR,a.UR,ea.DR,ea.UR);
            if(d<dist){
                dist=d;
                pick=a.SN;
            }
        }
        if(pick!=-1){ //挑到了
            HumanAction(pick,ea.SN);
            spare.erase(pick);
            
        }
    }
    //有空闲兵    
    if(spare.size()!=0){
        bool allAttacked=true; //检查是不是所有兵都有人打
        for(auto&ea:info.enemy_armies){
            bool thisGotten=false; //这个兵是不是有人打 如果所有兵的thisGotten都为true 说明当前战场确实每个人都有人牵扯
            for(auto&a:info.armies){
                if(a.Sort==AT_PRIEST)continue;//祭司不看
                if(spare.count(a.SN))continue;//空闲兵不看
                if(a.WorkObjectSN==ea.SN){
                    thisGotten=true;
                    break;
                }
            }
            if(!thisGotten){
                allAttacked=false;
                break;
            }
        }
        
        //派出剩下的空闲兵
        if(allAttacked){
            
            for(auto&a:info.armies){
                
                if(!spare.count(a.SN))continue;//非空闲兵不看
                int tSN=-1;
                double dist=1e18;
                for(auto&ea:info.enemy_armies){
                    double d=calDistance(a.BlockDR*BLOCKSIDELENGTH,a.BlockUR*BLOCKSIDELENGTH,ea.BlockDR*BLOCKSIDELENGTH,ea.BlockUR*BLOCKSIDELENGTH);
                    if(d<dist){
                        dist=d;
                        tSN=ea.SN;
                    }
                }
                HumanAction(a.SN,tSN);
                spare.erase(a.SN);
                
            }
        }
    }


    // ---- 风筝位: 冷却没好 且 敌人逼近 -> 在"塔保护圈内"挑离敌人最远的一格挪过去 ----
    //祭司走位
    if(counterState<1){
        for(auto&a:info.armies){
            if(a.SN!=priestSN)continue;
            if(a.NowState!=HUMAN_STATE_IDLE)break;
            if(a.ConvertCooldown<=0)break;
            int ex=-1,ey=-1,dist=1e18;
            for(auto&ea:info.enemy_armies){
                int dx=ea.BlockDR-priestBlockDR,dy=ea.BlockUR-priestBlockUR;
                int d=dx*dx+dy*dy;
                if(d<dist){
                    dist=d;
                    ex=ea.BlockDR;
                    ey=ea.BlockUR;
                }
            }
            //有这样的敌人并且离得比较近
            if(ex!=-1&&dist<=64){                               // 8格内才动
                static int kdr=-1,kur=-1;//当前方位
                int ndr=-1,nur=-1,ndist=-1;
                //一个方形范围
                for(int dx=-PRIEST_HARNESS;dx<=PRIEST_HARNESS;dx++){
                    for(int dy=-PRIEST_HARNESS;dy<=PRIEST_HARNESS;dy++){
                        if(max(abs(dx),abs(dy))>PRIEST_HARNESS)continue;
                        int dr=arrowTowerBlockDR+dx;
                        int ur=arrowTowerBlockUR+dy;
                        if(dr<0||dr>=100||ur<0||ur>=100)continue;
                        if(MAP[dr][ur]!=Open)continue;
                        int dd=(dr-ex)*(dr-ex)+(ur-ey)*(ur-ey);
                        if(dd>ndist){
                            ndist=dd;
                            ndr=dr;
                            nur=ur;
                        }
                    }
                }
                if(ndr!=-1&&(ndr!=kdr||nur!=kur)){ //即不在原地
                    kdr=ndr;kur=nur;
                    HumanMove(priestSN,ndr*BLOCKSIDELENGTH,nur*BLOCKSIDELENGTH);
                }
            }
            break;
        }
    }
}
//判断周围有无某建筑 并且通过引用 给出该建筑的SN
int UsrAI::checkEnv(int type,int& byBuildingSN){
    
    //
    byBuildingSN=-1;
    
    int radius=5;
    int need=((type==RESOURCE_BUSH)?BUILDING_GRANARY:BUILDING_STOCK); //根据资源种类判断 应该需求哪种建筑
    

    for(auto&r:info.resources){
        if(r.Type!=type)continue;
        if(type==RESOURCE_GAZELLE){
            if(r.Blood>0)continue;
        }
        if(r.Cnt<=0)continue;
        //一团周围只要有一个有该建筑就行
        for(auto&b:info.buildings){
            if(b.Type!=need)continue; //并非所需
            int d=max(abs(b.BlockDR-r.BlockDR),abs(b.BlockUR-r.BlockUR));
            if(d>radius)continue; //以这个资源为中心 链接每个对应的存储点 看有没有存储点是符合要求的
            if(b.Percent>=100)return 2; //建好
            if(byBuildingSN==-1) byBuildingSN=b.SN;
        }
    }
    return (byBuildingSN!=-1)?1:0; //不等于-1说明周围有
    
}

// 花名册成员是否"浆果已采完": 空闲 + 手上没扛货 + 当前目标不是还有货的浆果丛
// (只用"空闲"会有假阳性: 他可能是刚帮建完/刚运完货才空闲, 不代表浆果采完)
bool UsrAI::bushDone(int fSN){
    for(auto&f:info.farmers){
        if(f.SN!=fSN)continue;
        if(f.NowState!=HUMAN_STATE_IDLE)return false;
        if(f.ResourceSort!=-1)return false;
        for(auto&r:info.resources){
            if(r.Type==RESOURCE_BUSH&&r.SN==f.WorkObjectSN&&r.Cnt>0)return false;
        }
        return true;
    }
    return false;                        // 这个人已经不在农民列表里
}

// 当前有多少花名册成员已经在农田上干活
int UsrAI::farmTeamOnDuty(){
    int n=0;
    for(auto&f:info.farmers){
        if(!bushFarmer.count(f.SN))continue;
        if(f.WorkObjectSN==-1)continue;
        for(auto&b:info.buildings){
            if(b.SN==f.WorkObjectSN&&b.Type==BUILDING_FARM){ n++; break; }
        }
    }
    return n;
}

// 用户定的种田三阶段规则:
//   花名册成员 -> 必须"浆果已采完且空闲"(铜器前后同一把尺)
//   普通闲人   -> 只有【铜器后】且【花名册全员都已在农田上】才允许补缺口(铜器前一律不许种田)
bool UsrAI::canFarm(int fSN){
    //若是浆果队 必须采完浆果才能种田
    if(bushFarmer.count(fSN))return bushDone(fSN);
    //若非浆果队 只能在铜器时代之后才能种田
    if(info.civilizationStage!=CIVILIZATION_BRONZEAGE)return false;
    int need=0;
    for(auto&f:info.farmers){
        if(bushFarmer.count(f.SN))need++;
    }
    if(need>6)need=6;                    // 花名册目标规模就是 6 人(少几个就按实际算, 免得永远凑不满)
    return farmTeamOnDuty()>=need;
}

void UsrAI::CalmAndCrazy(){
    if(factorySN==-1){
        for(auto&b:info.enemy_buildings){
            if(b.Type==BUILDING_SIEGE){
                factorySN=b.SN;
                factoryBlockDR=b.BlockDR;
                factoryBlockUR=b.BlockUR;
                break;
            }
        }
    }
    if(factorySN==-1||counterState<3)return;

    // ---- [AI] 贴厂保护 ----
    //  策略原文: "如果祭司满血, 能抗几轮箭塔射击。但不满血或者箭塔密集,
    //            那还是要靠军队吸引火力, 或者干脆拆掉两三个再进去。"
    //  这里落成两道门:
    //    ① 血量不到一半 -> 先跟住大军(等部队吸火), 不冲;
    //    ② 离厂超过祭司响应范围 -> 先跟着大军推进, 等靠近了再下令
    //       (否则祭司会自己从集结区一路走过去, 进敌厂 20 格就被猎手围杀)。
    for(auto&a:info.armies){
        if(a.SN!=priestSN)continue;
        if(a.NowState!=HUMAN_STATE_IDLE)break;      // 忙(转化中/走路中) -> 不打扰
        if(a.ConvertCooldown>0)break;                // 冷却没好
        
        int d=max(abs(priestBlockDR-factoryBlockDR),abs(priestBlockUR-factoryBlockUR));
        if(d>DIS_PRIEST){ 
            priestFollow(); 
            break; 
        }                             // ② 还太远
        HumanAction(priestSN,factorySN);             // 血够 + 够近 -> 上
        break;
    }
}
