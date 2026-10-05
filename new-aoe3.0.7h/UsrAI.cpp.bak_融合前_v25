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

//初始房子信息
int homeSN=-1;
int homeBlockDR=-1;
int homeBlockUR=-1;
//谷仓
int granaryBlockDR=-1;
int granaryBlockUR=-1;
//初始仓库--因为之后猎瞪羚与挖金子都会建自己的仓库 这里的仓库是专门给伐木用的 
int oriStockBlockDR=-1;
int oriStockBlockUR=-1;

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

//祭司信息
int priestSN=-1;
int priestBlockDR=-1;
int priestBlockUR=-1;
int priestState=-1;
bool priestExploring=true;      // 祭司探索开关
unordered_map<int,bool> badFrontier;  // 探索时"试过但走不到"的边界点(状态驱动, 不用帧数)

//一局只获取一次基本信息
bool getOnlyOnce=false;

//防占用
unordered_map<int,bool>resIsgotten;
unordered_map<int,bool>farIsgotten;


//建房点
int dx_home[]={-2,0,2,0};
int dy_home[]={0,2,0,-2};
//专职建房者
int homeBuilderSN=-1;
int homeBuilderDR=-1;
int homeBuilderUR=-1;
int homeBuilderState=-1;


const int goHomeFrame=5250;//考虑优化或删除（帧判断有误差）

int bushNum=0;//6 记录被采的浆果数
int gazelleNum=0;//6 记录被采的瞪羚数
int killGazelle=0;//6 记录猎杀的瞪羚数
int woodNum=0; //记录被砍的树数
bool storageStarted=false;//防止猎人重复建仓库
int farmNum=0;



int phaseNum=20;//铜器之前先限制20上限(19村民+1祭司 6浆果(后期种田) 3伐木 6瞪羚 1建房 3建造)--铜器后再开放5个村民(2个种田，还有三个辅助瞪羚组进行砍树或者挖金子) 
bool phaseChange=false;//用于判断铜器前后策略的开关

//军事
int marketBlockDR=-1;//市场的位置
int marketBlockUR=-1;

int armyCampBlockDR=-1;//兵营的位置
int armyCampBlockUR=-1;
//靶场挨者兵营 可以考虑记录
unordered_map<int,bool>bushFarmer;//采浆果的农民 空闲后优先种田 不参与砍树
unordered_map<int,bool>goldFarmer;   //被派去挖金的农民 矿采完自动接下一口, 不闲着

int stableBlockDR=-1;//马厩位置
int stableBlockUR=-1;

const int PRIEST_HARNESS=5;   // 祭司活动范围: 离箭塔不超过5格(塔射程7格, 保证一直在保护圈内)
//================= 反攻 =================
int counterState=0;         //  0-前期：抵御三波 1-中期：集结与侦察 2-后期：反攻
int factorySN=-1;           // 敌方攻城武器厂
int factoryBlockDR=-1;      // 攻城武器厂坐标
int factoryBlockUR=-1;
int scoutSN=-1;             // 侦察骑兵

int victoryDR=-1;
int victoryUR=-1;

// ================= 集结 =================
const int RALLY_BACK=6;      // 从敌人位置朝自家退几格
const int RALLY_R=2;         // 集结区半径(2 -> 5x5 = 25 格)
const int RALLY_NEED=8;      // 到齐几个兵算集结完毕
int rallyDR=-1,rallyUR=-1;   // 集结点(区域中心); -1 表示还没定
unordered_map<int,int> rallySlot;   // 兵SN -> 分到的格子(DR*100+UR)

/////////////////////////////////////////////

void UsrAI::processData(){   
    //本帧大管家
    info=getInfo();
    //升级
    centerUpgrade();
    //清空 仅用于防止本帧某人被多次调用
    farIsgotten.clear();
    //获取基本信息
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

    

    
    //处理建造
    manageBuild();   

    trainArmy();


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
 
    //最多几个人占用
    auto resMax=[](int type)->int{
        if(type==RESOURCE_TREE)return 1;
        if(type==RESOURCE_BUSH)return 1;
        return 1; //留接口
    };

    for(auto&r:info.resources){
        if(r.Type==RESOURCE_BUSH&&bushNum>=6)continue;
        if(r.Type==RESOURCE_GAZELLE&&gazelleNum>=6)continue;

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
                resIsgotten[r.SN]=true;
                if(r.Type==RESOURCE_BUSH){
                    bushNum++;
                    bushFarmer[fSN]=true;
                }
                if(r.Type==RESOURCE_GAZELLE)gazelleNum++;
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
                    resIsgotten[r.SN]=true;
                    if(r.Type==RESOURCE_BUSH){
                        bushNum++;
                        bushFarmer[fSN]=true;
                    }
                    if(r.Type==RESOURCE_GAZELLE)gazelleNum++;
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
    for(int i=woodNow;i<3;i++){                 // 补到 3 个: 人数上限就写在这个 3
        if(assignWoodcutter()==-1)break;        // 没树/没人就停, 不会空转
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
        for(int i=goldNow;i<3;i++){                        // 人数上限=这个 3
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
void UsrAI::centerUpgrade(){
    //铜器升级事宜
    
    if(info.civilizationStage!=CIVILIZATION_BRONZEAGE){
        int centerState=-1;
        for(auto&b:info.buildings){
            if(b.Type==BUILDING_CENTER)centerState=b.Project;
        }
        //考虑前置建筑
        if(haveBuilding(BUILDING_MARKET)&&haveBuilding(BUILDING_RANGE)&&info.Meat>=BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD){
            if(centerState==ACT_NULL)
                BuildingAction(centerSN,BUILDING_CENTER_UPGRADE);
        }
    }
    if(!phaseChange&&info.civilizationStage==CIVILIZATION_BRONZEAGE){
        phaseNum=25; //放宽村民人口
        phaseChange=true; //状态切换
        
    }
}
void UsrAI::getPriest(){
    for(auto&a:info.armies){
        if(a.Sort==AT_PRIEST){
            priestSN=a.SN;
            priestBlockDR=a.BlockDR;
            priestBlockUR=a.BlockUR;
            priestState=a.NowState;
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
        //房屋
        if(b.Type==BUILDING_HOME){
            homeSN=b.SN;
            homeBlockDR=b.BlockDR;
            homeBlockUR=b.BlockUR;
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
int liveGazelleNum(){
    int n=0;
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_GAZELLE&&r.Blood>0)n++;
    }
    return n;
}
////////
bool isMeleeSort(int sort){
    return sort==AT_CLUBMAN||sort==AT_SWORDSMAN||sort==AT_IMPROVED
        ||sort==AT_HOPLITE||sort==AT_BROADSWORDSMAN||sort==AT_CAVALRY
        ||sort==AT_CHARIOT||sort==AT_SCOUT;
}
bool isArcherSort(int sort){
    return sort==AT_BOWMAN||sort==AT_SLINGER
        ||sort==AT_CHARIOT_ARCHER||sort==AT_COMPOSITE_BOWMAN;
}
///////////
const int gazelleWantNum=6;      // 祭司探路/开猎前要凑够的"已探明活瞪羚"数量

void UsrAI::priestExplore(){
    getPriest(); //每次进来获取祭司信息
    if(!priestExploring)return;
    //本函数主要实现祭司的探索与自主避障功能
    
    
    static int phase=-1;                 // -1-先逛基地周边 0-自己角 1-绕中心 2-结束
    static int radius=40;                // 阶段1当前半径
    static int curDR=-1,curUR=-1;        // 当前目标格(用于判超时/拉黑)
    // static int lastMoveFrame=-1;         // 上次下移动指令的帧
    // static int lastDodgeFrame=-1000;     // 上次躲避的帧
    // static unordered_map<int,int> bad;   // 格子key -> 冷却截止帧(走不到的格子)

    const int DANGER_ESCAPE=64;          // 敌人/猛兽8格内视为危险（64为平方）
    // const int MOVE_TIMEOUT=300;          // 目标走不到的超时(帧)
    // const int BAD_COOL=1000;             // 走不到的格子拉黑时长(帧)
    // const int DODGE_COOL=80;             // 躲避冷却(帧)

    //几个轻量lambda函数
    //是否 已探明
    auto usable=[&](int dr,int ur)->bool{
        return (dr>=0&&dr<100&&ur>=0&&ur<100)&&MAP[dr][ur]==Open;
    };
    //是否 边界点: 自己去得, 且周围2格内有未知区
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

    


    // 是否被拉黑(曾经走不到)
    //本函数需要考虑
    // auto isBad=[&](int dr,int ur)->bool{
    //     auto it=bad.find(dr*100+ur);
    //     return it!=bad.end()&&info.GameFrame<it->second;
    // };

    // 该点附近有没有危险(选点时避开, 属于局部避障的第一层)
    auto dangerNear=[&](int dr,int ur)->bool{
        for(auto&ea:info.enemy_armies){
            int dx=ea.BlockDR-dr, dy=ea.BlockUR-ur;
            if(dx*dx+dy*dy<DANGER_ESCAPE)return true;
        }
        for(auto&ef:info.enemy_farmers){ //一般不会有 但防止遇到了 会被记入对面视野的吧
            int dx=ef.BlockDR-dr, dy=ef.BlockUR-ur;
            if(dx*dx+dy*dy<DANGER_ESCAPE)return true;
        }
        for(auto&r:info.resources){
            if((r.Type==RESOURCE_LION)&&r.Blood>0){
                int dx=r.BlockDR-dr, dy=r.BlockUR-ur;
                if(dx*dx+dy*dy<DANGER_ESCAPE)return true;
            }
        }
        return false;
    };

    //开始局部避障
    //---------- 1. 找最近的危险(视野内的敌兵/敌农/狮子) ----------
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

    for(auto&ef:info.enemy_farmers){
        int dx=ef.BlockDR-priestBlockDR, dy=ef.BlockUR-priestBlockUR;
        int d=dx*dx+dy*dy;
        if(d<dist){
            dist=d;
            ex=ef.BlockDR;
            ey=ef.BlockUR;
        }
    }
    for(auto&r:info.resources){
        if((r.Type==RESOURCE_LION)&&r.Blood>0){
            int dx=r.BlockDR-priestBlockDR, dy=r.BlockUR-priestBlockUR;
            int d=dx*dx+dy*dy;
            if(d<dist){
                dist=d;
                ex=r.BlockDR;
                ey=r.BlockUR;
            }
        }
    }

    //---------- 2. 有危险就跑: 8个方向里挑最背离危险、且可走的一格, 撤6格 ----------
    if(ex!=-1&&dist<DANGER_ESCAPE&&priestExploring){ //存在这么一个危险 并且达到避障极限距离 并且此时处于探索状态
    
        //周围八格
        int dirx[8]={1,1,0,-1,-1,-1,0,1};
        int diry[8]={0,1,1,1,0,-1,-1,-1};
        int tdr=-1,tur=-1; //targetdr/ur
        double dist=-1e18; //这次要比较大的 所以取负数
        for(int k=0;k<8;k++){
            int nx=priestBlockDR+dirx[k]*6;
            int ny=priestBlockUR+diry[k]*6;
            if(!usable(nx,ny))continue;                 // 局部避障: 只往能走的格子躲
            if(dangerNear(nx,ny))continue;              // 不往另一堆危险里躲
            int d=(nx-ex)*(nx-ex)+(ny-ey)*(ny-ey);   // 候选格离危险(敌人)多远          
            if(d>dist){
                dist=d;
                tdr=nx;
                tur=ny;
            } 
        }
        
        // ★挑到了逃跑格就发一次移动指令(原来只算了 tdr/tur 没下令 -> 祭司遇敌不躲)
        //   目标没变就不重发, 免得每帧重发把路径清掉
        static int dodgeDR=-1,dodgeUR=-1;
        if(tdr!=-1&&(tdr!=dodgeDR||tur!=dodgeUR)){
            dodgeDR=tdr;dodgeUR=tur;
            HumanMove(priestSN,tdr*BLOCKSIDELENGTH,tur*BLOCKSIDELENGTH);
        }
        return;                                            // 危险期间不推进探索
    }

    //---------- 3. 走路中不打断; 超时(走不到)则拉黑该格, 重新选 ----------
    

    //---------- 3.2 到点回家: goHomeFrame 之后回箭塔底下待命 ----------
    // 只在"闲着"时才动(走路中上面已经 return 了); 到了箭塔旁边就待命, 不再探索/不再追瞪羚。
    if(marketBlockDR!=-1||info.GameFrame>=goHomeFrame){
        static int homeDR=-1,homeUR=-1; //落脚点
        if(abs(priestBlockDR-arrowTowerBlockDR)>PRIEST_HARNESS||abs(priestBlockUR-arrowTowerBlockUR)>PRIEST_HARNESS){
            if(homeDR==-1){                                  // 还没挑落脚点 -> 挑箭塔四邻
                int dx4[4]={0,1,0,-1};
                int dy4[4]={1,0,-1,0};
                for(int k=0;k<4;k++){
                    int nr=arrowTowerBlockDR+dx4[k], nu=arrowTowerBlockUR+dy4[k];
                    if(!usable(nr,nu))continue;
                   
                    homeDR=nr;homeUR=nu;
                    break;
                } //一般来说不会出现一个能站的点都没有
            }
            // ★挑到了落脚点就发一次"回家"指令(原来漏了这步 -> 祭司永远不动);
            //   已经走起来了(非 IDLE)就不再重发
            if(homeDR!=-1&&priestState==HUMAN_STATE_IDLE){
                HumanMove(priestSN,homeDR*BLOCKSIDELENGTH,homeUR*BLOCKSIDELENGTH);
            }
            return;
        }
        //不再探索
        priestExploring=false;                               
        return;                                             
    }

    //补丁
    //---------- 3.5 看见瞪羚但还不够 6 只 -> 先过去把瞪羚照亮 ----------
    // 走近之后祭司 12 格的视野会把周围的瞪羚一起照亮, 凑够 6 只才回去做常规探索。
    // ★ 同一个落脚点只发一次指令; 去了 MOVE_TIMEOUT 帧还没到就拉黑它并放行回常规探索。
    //   否则每帧重发 -> addRelation 反复 suspendRelation(清路径) -> 祭司被钉在原地动不了。
    if(liveGazelleNum()<gazelleWantNum){
        static int seekDR=-1,seekUR=-1; //准备追的、看到的瞪羚的坐标

        int gdr=-1,gur=-1,gd=1e18; //gazelledr/ur/distance
        //偏好探索 还没探到六只瞪羚
        if(liveGazelleNum()<gazelleWantNum){
            for(auto&r:info.resources){
                if(r.Type!=RESOURCE_GAZELLE) continue;  //不是瞪羚不要
                if(r.Blood<=0)continue;                       // 只追活的
                int dx=r.BlockDR-priestBlockDR;
                int dy=r.BlockUR-priestBlockUR; //拿瞪羚坐标
                int d=dx*dx+dy*dy; //计算坐标平方
                if(d<=36)continue;             // 已经贴着它了(6格内), 换下一只
                if(d>1600)continue;            // 太远的先别追(免得隔着海去够), 交给常规探索
                if(d<gd){   //找最近的一个
                    gd=d;
                    gdr=r.BlockDR;
                    gur=r.BlockUR;
                }
            }
            
        }

        int ox=-1,oy=-1; //output
        if(gdr!=-1){ //有目标瞪羚
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
            curDR=ox;
            curUR=oy;
            return;
        }
        else{
            return;                                           // 正在去这只瞪羚的路上, 别打断
        }
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
                if(badFrontier.count(dr*100+ur))continue;   // 试过走不到的边界点, 别再选
                
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
                if(badFrontier.count(dr*100+ur))continue;   // 试过走不到的边界点, 别再选
                
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
                if(badFrontier.count(dr*100+ur))continue;   // 试过走不到的边界点, 别再选
                    if(badFrontier.count(dr*100+ur))continue;  // 试过走不到的边界点, 别再选
                    
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
            }else{
                phase=2;
            }
        }
    }

    if(bd==-1)return;                                      // 阶段2: 探完, 不再动作

    static int tgtDR=-1,tgtUR=-1;    // 当前正在去的探索点(目标没变就不重发)
    static bool tgtWalked=false;     // 这个目标有没有看到过祭司真的走起来过
    // ★这个目标有没有走到? (全程用引擎给的状态判断, 不含帧数)
    //   看到过它走(WALKING) 之后又停下来了(IDLE) 却不在目标点上 -> 说明这点走不到, 记坏点
    if(tgtDR!=-1&&priestState==HUMAN_STATE_WALKING)tgtWalked=true;
    if(tgtDR!=-1&&tgtWalked&&priestState==HUMAN_STATE_IDLE&&
       (abs(priestBlockDR-tgtDR)>1||abs(priestBlockUR-tgtUR)>1)){
        badFrontier[tgtDR*100+tgtUR]=true;
        tgtDR=-1;tgtUR=-1;tgtWalked=false;
    }

    // ★选到了就近的"未知区域边界点" -> 发一次移动指令
    //   (原来这里少了 HumanMove, 探索目标算出来却从不派出去 -> 祭司整局不去探索)
    //   目标没变就不重发, 免得每帧重发把路径清掉
    if(bd!=tgtDR||bu!=tgtUR){
        tgtDR=bd;tgtUR=bu;tgtWalked=false;
        HumanMove(priestSN,bd*BLOCKSIDELENGTH,bu*BLOCKSIDELENGTH);
    }
}
// 造兵: 兵营先升级战斧, 升完出 2 个斧兵; 靶场出 2 个弓箭手; 造完集合到箭塔下
void UsrAI::trainArmy(){
    static bool clubUp=false,broadTech=false,compTech=false,logistics=false; //战斧 阔剑 复合弓 后勤 （科技）

    // 前中期
    if(counterState<=1){
        int club=0,bow=0;
        for(auto&a:info.armies){
            if(a.Sort==AT_CLUBMAN)club++;
            else if(a.Sort==AT_BOWMAN)bow++;
        }
        for(auto&b:info.buildings){
            if(b.Percent<100||b.Project!=ACT_NULL)continue;
            if(b.Type==BUILDING_ARMYCAMP){
                //先升科技
                if(!clubUp&&info.Meat>=BUILDING_ARMYCAMP_UPGRADE_CLUBMAN_FOOD){
                    BuildingAction(b.SN,BUILDING_ARMYCAMP_UPGRADE_CLUBMAN);
                    clubUp=true;
                }
                else if(clubUp&&!broadTech&&info.civilizationStage>=CIVILIZATION_BRONZEAGE&&
                        info.Meat>=BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_FOOD&&info.Gold>=BUILDING_ARMYCAMP_UPGRADE_BROADSWORD_GOLD){
                    BuildingAction(b.SN,BUILDING_ARMYCAMP_UPGRADE_BROADSWORD);
                    broadTech=true;   // 前期就升阔剑科技
                }
                else if(club<2){
                    BuildingAction(b.SN,BUILDING_ARMYCAMP_CREATE_CLUBMAN);
                    club++;
                }
                else if(broadTech&&!logistics&&info.Meat>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_FOOD&&info.Gold>=BUILDING_ARMYCAMP_RESEARCH_LOGISTICS_GOLD){
                    BuildingAction(b.SN,BUILDING_ARMYCAMP_RESEARCH_LOGISTICS);
                    logistics=true;       // 第三波后立刻研后勤(兵营0.5人口)
                }
            }
            else if(b.Type==BUILDING_COLLAGE){        // 学院好了就出方阵兵, 支援第二波
                BuildingAction(b.SN,BUILDING_COLLAGE_CREATE_HOPLITE);
            }
            else if(b.Type==BUILDING_RANGE){
                if(!compTech&&bow<2){
                    BuildingAction(b.SN,BUILDING_RANGE_CREATE_BOWMAN);
                    bow++;
                }
                if(info.civilizationStage>=CIVILIZATION_BRONZEAGE&&!compTech){
                    // 
                    if(info.Meat>=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_FOOD&&info.Wood>=BUILDING_RANGE_UPGRADE_COMPOSITE_BOW_WOOD){
                        BuildingAction(b.SN,BUILDING_RANGE_UPGRADE_COMPOSITE_BOW);
                        compTech=true;
                        
                    }
                }
            }
        }
        return;
    }

    
}

//获取唯一的房屋建造者
void UsrAI::gethomeBuilder(){
    for(auto&f:info.farmers){
        if(f.SN==homeBuilderSN){
            homeBuilderDR=f.BlockDR;
            homeBuilderUR=f.BlockUR;
            homeBuilderState=f.NowState;
        }
    }
}

//==================== 建造小工具 ====================

// 是否有某类建筑正在施工(避免同一类连建两个)
bool UsrAI::hasUnfinishedBuilding(int type){
    for(auto&b:info.buildings){
        if(b.Type==type&&b.Percent<100)return true;
    }
    return false;
}







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
    // 农民修箭塔
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
    //周围八个方向 中间留宽2格的通道 -- 实操查看是否会同时建造导致卡住
    int fsDR[]={-5,0,5,0,-5,5,5,-5};
    int fsDU[]={0,5,0,-5,5,5,-5,-5};

    if(bushNum>=6&&(gazelleNum>=6||info.GameFrame>=12000)&&woodNum>=3){
        //有无在建的
        int buildingSN=-1;
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
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                if(bushFarmer.count(f.SN))continue;      // 采浆果的人不帮建
                if(f.WorkObjectSN==buildingSN)continue; //已在建 防重复发指令
                HumanAction(f.SN,buildingSN);
                farIsgotten[f.SN]=true;
            }
            return;
        }
        //无在建的
        
        int want=-1,cost=0;//要什么 花多少
        int bd=centerBlockDR,bu=centerBlockUR;          // 默认以市镇中心为基准找空地
        
        //市场 以市镇中心为基准
       
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
        
        if(!haveBuilding(BUILDING_MARKET)){
            want=BUILDING_MARKET;
            cost=BUILD_MARKET_WOOD;
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
            if(stableBlockDR!=-1){                      // 学院挨着马厩建
                bd=stableBlockDR;
                bu=stableBlockUR;
            }
        }
        //木材不够 -- 空闲的人都去砍树
        if(want!=-1&&info.Wood<cost)assignWoodcutter();

        if(want!=-1&&info.Wood>=cost){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                if(bushFarmer.count(f.SN))continue;      // 采浆果的人不盖房
               

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
        
    }
    if(info.Meat<800&&haveBuilding(BUILDING_MARKET)&&granaryBlockDR!=-1&&info.Wood>=BUILD_FARM_WOOD){
        farmNum=0;
        for(auto&b:info.buildings){
            if(b.Type==BUILDING_FARM&&b.Cnt>0)farmNum++;
        }
        if(farmNum<8){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN||f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;

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
            if(f.NowState==HUMAN_STATE_IDLE&&f.ResourceSort==-1)
                HumanAction(f.SN,f.WorkObjectSN);
            continue;
        }
        if(f.ResourceSort!=-1)continue;                  // 手上有货(还没交) -> 先去交
        if(f.NowState!=HUMAN_STATE_IDLE)continue;        // 忙 -> 不打扰
        if(f.SN==homeBuilderSN)continue;
        if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;

        int freeFarm=-1;                                 // 1) 有现成空田 -> 去种
        for(auto&b:info.buildings){
            if(b.Type!=BUILDING_FARM||b.Percent<100||b.Cnt<=0)continue;
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
            if(b.Type==BUILDING_FARM&&b.Cnt>0)farmNum++;
        }
        if(farmNum>=8||info.Wood<BUILD_FARM_WOOD)continue;
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
        if(b.Cnt<=0)continue;
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
        for(auto&f:info.farmers){               // 先挑采过浆果的(他们专供种田)
            if(f.SN==homeBuilderSN||f.NowState!=HUMAN_STATE_IDLE)continue;
            if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
            if(!bushFarmer.count(f.SN))continue;
            pick=f.SN;break;
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
        for(auto&b:info.buildings){                       // ① 有空田就去种
            if(b.Type!=BUILDING_FARM||b.Percent<100||b.Cnt<=0)continue;
            bool busy=false;
            for(auto&f:info.farmers){ 
                if(f.WorkObjectSN==b.SN){
                    busy=true;
                    break;
                } 
            }
            if(busy)continue;
            HumanAction(f.SN,b.SN);
            bushFarmer[f.SN]=true; //收录进浆果队（即后期的种田队）
            farIsgotten[f.SN]=true;
            done=true;
            break;
        }
        if(done)continue;
        if(info.civilizationStage>=CIVILIZATION_BRONZEAGE){          // ② 没田就去挖金
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
        if(bushFarmer.count(f.SN))continue;              // 采浆果的人不干别的
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
            if(!reach)continue;
            
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
        if(bushFarmer.count(f.SN))continue;                  // 采浆果的人留给农田, 不砍树
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
    woodNum++;
    farIsgotten[fSN]=true;
    return fSN;
}
// 派"一个"空闲农民去挖金 —— 和 assignWoodcutter 同套路:
//   外层遍历【仓库】, 内层遍历【金矿】, 在所有组合里取距离最小的一对;
//   再派离那口金矿最近的空闲农民过去。
//   => 这样"有仓库的金矿"天然被优先选, 采完一口会自动接续下一口。
// 返回: 派出去的农民SN; 没金矿 / 没空闲农民 -> -1
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
            if(d<bestD){bestD=d;bestSN=r.SN;bestDR=r.BlockDR;bestUR=r.BlockUR;}
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
        static bool onlyOnce=false;
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
            int stockSN=-1;
            for(auto&f:info.farmers){
                if(f.SN==gazelleHunter1SN)stockSN=f.WorkObjectSN;
            }
            
            
            
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

        case 1:{   // ---- 侦察: 骑兵出去找攻城厂(敌大营在我们出生点的对角) ----
            if(info.Human_Num>=50){
                counterState=2;
                return;
            }
            if(info.Human_Num>=40){
                static bool SeeThem=false;
                if(SeeThem)return;
                if(scoutSN==-1){                                  
                    for(auto&a:info.armies){     // 有现成骑兵就用
                        if(a.Sort==AT_SCOUT){   
                            scoutSN=a.SN;
                            break;
                        } 
                    }
                }
                if(scoutSN==-1){                                   // 没有就造(马厩)
                    for(auto&b:info.buildings){
                        if(b.Type==BUILDING_STABLE&&b.Percent>=100&&b.Project==ACT_NULL&&
                        info.Meat>=BUILDING_STABLE_CREATE_SCOUT_FOOD){
                            BuildingAction(b.SN,BUILDING_STABLE_CREATE_SCOUT);
                            break;
                        }
                    }
                    return;
                }
                
                int tdr=100-centerBlockDR,tur=100-centerBlockUR;   // 敌营方向 = 我们市中心的对角
                
                for(auto&a:info.armies){
                    if(a.SN!=scoutSN)continue;
                    if(a.NowState!=HUMAN_STATE_IDLE)break;
                    HumanMove(a.SN,tdr*BLOCKSIDELENGTH,tur*BLOCKSIDELENGTH);

                    for(auto&ea:info.enemy_armies){
                        if(ea.WorkObjectSN==scoutSN){
                            SeeThem=true;
                            victoryDR=ea.BlockDR;
                            victoryUR=ea.BlockUR;
                        }
                    }
                    break;

                }
                break;
            }
            
            break;
        }

        case 2:{   // ---- 集结 ----
            if(victoryDR<0||victoryUR<0)break;      // ★锚点都没有就先别动(原版会发非法坐标)
            pickRallyPoint();
            rallyArmy();                            // 每帧最多派一个, 不重发
            rallyPriest();                          // 祭司去离敌最远那格(只派一次)
            if(rallyEnough())counterState=3;        // 集结完毕 → 进攻(拉扯, 下一步写)
            break;
        }
        case 3:{   // ---- 进攻 ----
            break;
        }
    }
}

// 定集结点: 敌人位置 -> 朝自家方向退 RALLY_BACK 格 (只做一次, 定完不再动)
void UsrAI::pickRallyPoint(){
    if(rallyDR!=-1)return;                    // 已经定过了 (和你代码里 if(factorySN==-1) 一个套路)
    int eDR=victoryDR,eUR=victoryUR;          // e = enemy: 锚点就是"斥候被咬到的那一格"
    if(eDR<0||eUR<0){                         // 还没有锚点
        eDR=100-centerBlockDR;                // 那就用"我方市中心的对角"当作敌人方向估一个
        eUR=100-centerBlockUR;
    }
    // 朝自家(市中心)退: 我家 DR 比敌人大就往 DR 正方向退, 否则往负方向退
    rallyDR = eDR + (centerBlockDR>eDR ?  RALLY_BACK : -RALLY_BACK);
    rallyUR = eUR + (centerBlockUR>eUR ? RALLY_BACK : -RALLY_BACK);
    rallyDR=max(2,min(97,rallyDR));           // 别退到地图外面
    rallyUR=max(2,min(97,rallyUR));
    // 唯一一个保险: 退回后如果还是离敌厂 22 格以内, 就再朝自家退 10 格
    // (祭司进敌厂 20 格就会被"祭司猎手"锁定追杀, 所以集结区必须站在 20 格外)
    if(factoryBlockDR!=-1&&max(abs(rallyDR-factoryBlockDR),abs(rallyUR-factoryBlockUR))<22){
        rallyDR += (centerBlockDR>rallyDR ?  10 : -10);
        rallyUR += (centerBlockUR>rallyUR ?  10 : -10);
    }
}

// ② 铺开: 每帧最多派一个兵; 已分过格的一律不重发
bool UsrAI::rallyArmy(){
    unordered_map<int,bool> taken;
    for(auto r:rallySlot){          // 清掉死人的槽位
        bool alive=false;
        for(auto&a:info.armies){ 
            if(a.SN==r.first){
                alive=true;
                break;
            } 
        }
        if(!alive){ 
            rallySlot.erase(r.first); 
            continue; 
        }
        //走到这边意味着是活着的 
        taken[r.second]=true;
    }
    for(auto&a:info.armies){
        if(a.SN==priestSN)continue;                 // 祭司单独处理
        if(rallySlot.count(a.SN))continue;          // ★已分过→不重发(状态驱动, 不含帧数)
        int bestKey=-1,bestD=1e9;
        for (int dx = -RALLY_R; dx <= RALLY_R; dx++) {
            for (int dy = -RALLY_R; dy <= RALLY_R; dy++) {
                int dr = rallyDR + dx, ur = rallyUR + dy;
                if (dr < 0 || dr >= 100 || ur < 0 || ur >= 100) continue;
                if (MAP[dr][ur] != Open) continue;
                int key = dr * 100 + ur;
                if (taken.count(key)) continue;
                int dd = max(abs(a.BlockDR - dr), abs(a.BlockUR - ur));
                if (dd < bestD) { 
                    bestD = dd; 
                    bestKey = key; 
                }
            }
        }
        if(bestKey==-1)break;
        rallySlot[a.SN]=bestKey; 
        taken[bestKey]=true;
        HumanMove(a.SN,(bestKey/100)*BLOCKSIDELENGTH,(bestKey%100)*BLOCKSIDELENGTH);
        return true;                                // ★每帧最多派一个
    }
    return false;
}

// ③ 祭司: 区域里挑"离敌人最远"的一格
void UsrAI::rallyPriest(){
    if(priestSN==-1||rallySlot.count(priestSN))return;
    int eDR=victoryDR,eUR=victoryUR;
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
            if(a.BlockDR==kv.second/100&&a.BlockUR==kv.second%100)n++;
            break;
        }
    }
    return n>=RALLY_NEED;
}





void UsrAI::waveBattle(){
    //此函数主要实现反攻前的战斗逻辑
    if(info.enemy_armies.empty())return; //没有检测到兵 自然不用准备战斗
    
    // 挑两个近战: 第一个给祭司转化, 第二个给箭塔打
    // 祭司: 挑"已经在视野里、离自己最近"的敌人(不限兵种); 箭塔: 再挑一个近战
    int priestTarget=-1,towerTarget=-1;//祭司的目标与箭塔的目标

    //箭塔是固定的，作用范围不能扩大
    
    //祭司是可以机动的 所以技能作用范围可以扩大
    const int pRange=(VISION_PRIEST+PRIEST_HARNESS)*(VISION_PRIEST+PRIEST_HARNESS); //反攻前祭司可以响应的范围
    int dist=1e18; //作为最大值参照
    for(auto&ea:info.enemy_armies){                          // 有投石车先转投石车
        if(ea.Sort!=AT_STONE_THROWER)continue;
        int ex=ea.BlockDR-priestBlockDR, ey=ea.BlockUR-priestBlockUR;
        int d=ex*ex+ey*ey;
        if(d<=dist&&d<=pRange){
            dist=d;
            priestTarget=ea.SN;
        }
    }
    if(priestTarget==-1){                                   // 没投石车 -> 取最近的敌人
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
    if(counterState<=1&&priestSN!=-1&&priestTarget!=-1){
        for(auto&a:info.armies){
            if(a.SN!=priestSN)continue;
            if(a.NowState==HUMAN_STATE_ATTACKING)break;          //转化中
            if(a.ConvertCooldown>0)break;                       // 冷却没好 -> 唯一的门
            if(a.WorkObjectSN==priestTarget)break;              // 已经在转它了
            if(priestTarget!=-1){
                HumanAction(priestSN,priestTarget);
                break;
            }
        }
    }
    //处理-箭塔-的目标
    if(arrowTowerSN!=-1){
        // 塔当前锁的是谁? 引擎把这个目标放在建筑的 Project 字段里
        int locked=-1;
        for(auto&b:info.buildings){
            if(b.SN!=arrowTowerSN)continue;
            locked=b.Project;
            break;
        }
        // 当前锁的目标还有效吗: 还在 7 格内, 且它的仇恨不是指着我塔
        bool keep=false;
        for(auto&ea:info.enemy_armies){
            if(ea.SN!=locked)continue;
            int d=max(abs(ea.BlockDR-arrowTowerBlockDR),abs(ea.BlockUR-arrowTowerBlockUR));
            if(d<=DIS_ARROWTOWER&&ea.WorkObjectSN!=arrowTowerSN)keep=true;
            break;
        }
        if(!keep){                       // 无效才重挑+下令(目标没变就不发, 否则每帧重发会让塔打不出箭)
            int best=1e18;
            towerTarget=-1;
            for(auto&ea:info.enemy_armies){
                //找一个最近的、没打自己的敌人
                int arrowD=max(abs(ea.BlockDR-arrowTowerBlockDR),abs(ea.BlockUR-arrowTowerBlockUR));//计算距离
                if(arrowD>DIS_ARROWTOWER)continue; //太远了
                if(ea.WorkObjectSN==arrowTowerSN)continue; //这个人的仇恨已经吸引到了
                if(arrowD<best){ //先记录下来
                    best=arrowD;
                    towerTarget=ea.SN;
                }
            }
            if(towerTarget!=-1)HumanAction(arrowTowerSN,towerTarget);
        }
    }

    // ---- 1v1 牵制: 每个敌方单位至少派 1 个我方单位(祭司除外, 祭司专职转化) ----
    set<int> spare;                                     // 本帧 能派出去的我方单位
    for(auto&a:info.armies){
        if(a.SN==priestSN)continue;     //祭司不要 专职转化
        if(a.WorkObjectSN==-1)spare.insert(a.SN);     // 手上没任务的才算闲置
    }
    for(auto&ea:info.enemy_armies){
        if(counterState<=1){                                   // 只限防守/集结期
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
    if(counterState<=1){
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

int UsrAI::checkEnv(int type,int& byBuildingSN){
    //判断周围有无某建筑 并且通过引用 给出该建筑的SN
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
    if(factorySN!=-1&&counterState==3){
        for(auto&a:info.armies){
            if(a.SN==priestSN&&a.NowState==HUMAN_STATE_IDLE&&a.ConvertCooldown<=0){
                HumanAction(priestSN,factorySN);
                return;
            }
        }
    }
}
