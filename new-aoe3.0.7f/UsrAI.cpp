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
#define Unknown -1
#define Ocean -2
#define Open 1




tagInfo info;
int MAP[100][100]{};

int centerSN=-1;
int centerBlockDR=-1;
int centerBlockUR=-1;

pair<int,int> angle1,angle2,angle3,angle4;





int homeSN=-1;
int homeBlockDR=-1;
int homeBlockUR=-1;
int dir_home=-1;// 1-左 2-上 3-右 4-下





int arrowTowerSN=-1;
int arrowTowerBlockDR=-1;
int arrowTowerBlockUR=-1;

int gazelleSN=-1;
int gazelleBlockDR=-1;
int gazelleBlockUR=-1;

int granarySN=-1;
int granaryBlockDR=-1;
int granaryBlockUR=-1;

int stockSN=-1;
int stockBlockDR=-1;
int stockBlockUR=-1;

int priestSN=-1;
int priestBlockDR=-1;
int priestBlockUR=-1;
int priestState=-1;


bool getOnlyOnce=false;

unordered_map<int,bool>resIsgotten;
unordered_map<int,bool>farIsgotten;

//==== 建造调度用 ====
struct ResRec{int type,dr,ur;};
unordered_map<int,ResRec>gathering;   // 正在采集的资源: SN -> 类型/块坐标
unordered_map<int,int>badBuildSpot;   // 建不起来的选址 -> 冷却截止帧




int dx_build[]={-2,-2,-2,0,2,2,2,0};
int dy_build[]={-2,0,2,2,2,0,-2,-2};

int homeBuilderSN=-1;
int homeBuilderDR=-1;
int homeBuilderUR=-1;
int homeBuilderState=-1;



int cnt=0;
int cnt_build=0;
bool stockOnce=false;

// 建造工本轮是否已有建造任务(有的话, 集合循环就不要再派他去采集, 否则
// manageBuild 刚下的"走过去"会被随后的 HumanAction 覆盖, 结果永远不建房)
bool buildBusy=false;

const int goHomeFrame=5250;



void UsrAI::processData()
{   
    
    
    info=getInfo();
    if(!getOnlyOnce)getBaseInfo();
    BuildingAction(centerSN,BUILDING_CENTER_CREATEFARMER);
    betterMap();
    
    priestExplore();


    for(auto&f:info.farmers){
        if(f.NowState==HUMAN_STATE_IDLE&&farIsgotten.find(f.SN)==farIsgotten.end()&&homeBuilderSN==-1){
            homeBuilderSN=f.SN;
        }
    }
    
    manageBuild();   // 统一建造: 房屋 + 资源配套仓库/谷仓 + 兵营/市场/靶场/马厩
    
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_BUSH||r.Type==RESOURCE_GAZELLE){
            if(resIsgotten.find(r.SN)!=resIsgotten.end())continue;
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN&&(buildBusy||homeBuilderState!=HUMAN_STATE_IDLE))continue;
                if(f.NowState!=HUMAN_STATE_IDLE||farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                HumanAction(f.SN,r.SN);
                resIsgotten[r.SN]=true;
                farIsgotten[f.SN]=true;
                gathering[r.SN]=ResRec{r.Type,r.BlockDR,r.BlockUR};
                break;
            }   
        }
    }
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_TREE){
            if(resIsgotten.find(r.SN)!=resIsgotten.end())continue;
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN&&(buildBusy||homeBuilderState!=HUMAN_STATE_IDLE))continue;
                if(f.NowState!=HUMAN_STATE_IDLE||farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                HumanAction(f.SN,r.SN);
                resIsgotten[r.SN]=true;
                farIsgotten[f.SN]=true;
                gathering[r.SN]=ResRec{r.Type,r.BlockDR,r.BlockUR};
                break;
            }
        }
    }
    checkWorkState();
    

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
    for(auto&a:info.armies){
        if(a.Sort==AT_PRIEST){
            priestSN=a.SN;
            priestBlockDR=a.BlockDR;
            priestBlockUR=a.BlockUR;
            break;
        }
    }
    for(auto&b:info.buildings){
        if(b.Type==BUILDING_CENTER){
            centerSN=b.SN;
            centerBlockDR=b.BlockDR;   
            centerBlockUR=b.BlockUR;
            int dx1=centerBlockDR-25;
            int dx2=centerBlockDR+25;
            int dy1=centerBlockUR-25;
            int dy2=centerBlockUR+25;
            dx1=(dx1<0?0:dx1);
            dx2=(dx2>=100?99:dx2);
            dy1=(dy1<0?0:dy1);
            dy2=(dy2>=100?99:dy2);
            angle1={dx1,dy1};
            angle2={dx1,dy2};
            angle3={dx2,dy2};
            angle4={dx2,dy1};
        }
        if(b.Type==BUILDING_HOME){
            homeSN=b.SN;
            homeBlockDR=b.BlockDR;
            homeBlockUR=b.BlockUR;
        }
        if(b.Type==BUILDING_ARROWTOWER){
            arrowTowerSN=b.SN;
            arrowTowerBlockDR=b.BlockDR;
            arrowTowerBlockUR=b.BlockUR;
        }
        if(b.Type==BUILDING_GRANARY){
            granarySN=b.SN;
            granaryBlockDR=b.BlockDR;
            granaryBlockUR=b.BlockUR;
            BuildingAction(b.SN,BUILDING_GRANARY_ARROWTOWER);
        }
        if(b.Type==BUILDING_STOCK){
            stockSN=b.SN;
            stockBlockDR=b.BlockDR;
            stockBlockUR=b.BlockUR;
        }
    }
    if(centerBlockDR<50){
        if(centerBlockUR<50)dir_home=1;
        else dir_home=2;
    }
    else{
        if(centerBlockUR>50)dir_home=3;
        else dir_home=4;
    }


    for(auto&r:info.resources){
        if(r.Type==RESOURCE_GAZELLE){
            gazelleSN=r.SN;
            gazelleBlockDR=r.BlockDR;
            gazelleBlockUR=r.BlockUR;
            break;
        }
    }
    // 注: 以前这里开局就盲目在瞪羚旁边 HumanBuild 一个仓库, 也不检查空地,
    //     已删掉, 改由 manageBuild() 按"采集点附近有没有落点"来判断和选址。
    
    getOnlyOnce=true;
}
//更新地图
void UsrAI::betterMap(){
    for(int dr=0;dr<100;dr++){
        for(int ur=0;ur<100;ur++){
            if((*info.theMap)[dr][ur].type==MAPPATTERN_UNKNOWN)MAP[dr][ur]=Unknown;
            else if((*info.theMap)[dr][ur].type==MAPPATTERN_OCEAN)MAP[dr][ur]=Ocean;
            else MAP[dr][ur]=Open;
        }
    }
    for(auto&r:info.resources){
        if(r.Type!=RESOURCE_EMPTY)MAP[r.BlockDR][r.BlockUR]=r.Type+100;
    }
    for(auto&b:info.buildings){
        int sz=3;
        if(b.Type==BUILDING_HOME||b.Type==BUILDING_ARROWTOWER)sz=2;
        int dr=b.BlockDR;
        int ur=b.BlockUR;
        for(int i=0;i<sz;i++){
            for(int j=0;j<sz;j++)MAP[dr+i][ur+j]=b.Type+1000;
        }
        
    }
}

//==================== 祭司探路 ====================
// 阶段0: 探自己所在的角(以市镇中心为界, 只扫"中心->地图角"那半张图)
// 阶段1: 以地图中心(50,50)为圆心, 半径40起一圈圈向里收(只在环带里找边界点)
// 另外三个角不探。视野内出现敌人/猛兽立即躲避。
// 说明: 边界点 = 已探明陆地(Open) 且 周围2格内有未知区(Unknown)
void UsrAI::priestExplore(){
    getPriest();
    if(priestSN==-1)return;

    static int phase=0;                  // 0-自己角 1-绕中心 2-结束
    static int radius=40;                // 阶段1当前半径
    static int curDR=-1,curUR=-1;        // 当前目标格(用于判超时/拉黑)
    static int lastMoveFrame=-1;         // 上次下移动指令的帧
    static int lastDodgeFrame=-1000;     // 上次躲避的帧
    static unordered_map<int,int> bad;   // 格子key -> 冷却截止帧(走不到的格子)

    const int DANGER_R2=64;              // 敌人/猛兽 8 格内视为危险
    const int MOVE_TIMEOUT=300;          // 目标走不到的超时(帧)
    const int BAD_COOL=1500;             // 走不到的格子拉黑时长(帧)
    const int DODGE_COOL=80;             // 躲避冷却(帧)

    // 只有"已探明陆地"才允许站上去(海里/未知区/资源格/建筑格一律不去)
    auto usable=[&](int dr,int ur)->bool{
        if(dr<2||dr>=98||ur<2||ur>=98)return false;
        return MAP[dr][ur]==Open;
    };
    // 是否边界点: 自己去得, 且周围2格内有未知区(去了才能多看到东西)
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
    auto isBad=[&](int dr,int ur)->bool{
        auto it=bad.find(dr*100+ur);
        return it!=bad.end()&&info.GameFrame<it->second;
    };
    // 该点附近有没有危险(选点时避开, 属于局部避障的第一层)
    auto dangerNear=[&](int dr,int ur)->bool{
        for(auto&a:info.enemy_armies){
            int dx=a.BlockDR-dr, dy=a.BlockUR-ur;
            if(dx*dx+dy*dy<DANGER_R2)return true;
        }
        for(auto&f:info.enemy_farmers){
            int dx=f.BlockDR-dr, dy=f.BlockUR-ur;
            if(dx*dx+dy*dy<DANGER_R2)return true;
        }
        for(auto&r:info.resources){
            if((r.Type==RESOURCE_LION||r.Type==RESOURCE_ELEPHANT)&&r.Blood>0){
                int dx=r.BlockDR-dr, dy=r.BlockUR-ur;
                if(dx*dx+dy*dy<DANGER_R2)return true;
            }
        }
        return false;
    };

    //---------- 1. 找最近的危险(视野内的敌兵/敌农/狮子/大象) ----------
    int ex=-1,ey=-1,bestD2=1<<30;
    for(auto&a:info.enemy_armies){
        int dx=a.BlockDR-priestBlockDR, dy=a.BlockUR-priestBlockUR;
        int d2=dx*dx+dy*dy;
        if(d2<bestD2){bestD2=d2;ex=a.BlockDR;ey=a.BlockUR;}
    }
    for(auto&f:info.enemy_farmers){
        int dx=f.BlockDR-priestBlockDR, dy=f.BlockUR-priestBlockUR;
        int d2=dx*dx+dy*dy;
        if(d2<bestD2){bestD2=d2;ex=f.BlockDR;ey=f.BlockUR;}
    }
    for(auto&r:info.resources){
        if((r.Type==RESOURCE_LION||r.Type==RESOURCE_ELEPHANT)&&r.Blood>0){
            int dx=r.BlockDR-priestBlockDR, dy=r.BlockUR-priestBlockUR;
            int d2=dx*dx+dy*dy;
            if(d2<bestD2){bestD2=d2;ex=r.BlockDR;ey=r.BlockUR;}
        }
    }

    //---------- 2. 有危险就跑: 8个方向里挑最背离危险、且可走的一格, 撤6格 ----------
    if(ex!=-1&&bestD2<DANGER_R2){
        if(info.GameFrame-lastDodgeFrame>=DODGE_COOL){
            int vx=priestBlockDR-ex, vy=priestBlockUR-ey;
            if(vx==0&&vy==0)vx=1;
            int dirx[8]={1,1,0,-1,-1,-1,0,1};
            int diry[8]={0,1,1,1,0,-1,-1,-1};
            int tdr=-1,tur=-1;
            double bestScore=-1e18;
            for(int k=0;k<8;k++){
                int nx=priestBlockDR+dirx[k]*6;
                int ny=priestBlockUR+diry[k]*6;
                if(!usable(nx,ny))continue;                 // 局部避障: 只往能走的格子躲
                if(dangerNear(nx,ny))continue;              // 不往另一堆危险里躲
                double score=dirx[k]*vx+diry[k]*vy;         // 越背离危险越好
                if(score>bestScore){bestScore=score;tdr=nx;tur=ny;}
            }
            if(tdr!=-1){
                if(curDR!=-1)bad[curDR*100+curUR]=info.GameFrame+BAD_COOL;
                curDR=-1;curUR=-1;
                HumanMove(priestSN,tdr*BLOCKSIDELENGTH,tur*BLOCKSIDELENGTH);
                lastDodgeFrame=info.GameFrame;
                lastMoveFrame=info.GameFrame;
                DebugText("priest: dodge");
            }
        }
        return;                                            // 危险期间不推进探索
    }

    //---------- 3. 走路中不打断; 超时(走不到)则拉黑该格, 重新选 ----------
    if(priestState!=HUMAN_STATE_IDLE){
        if(lastMoveFrame!=-1&&info.GameFrame-lastMoveFrame>MOVE_TIMEOUT){
            if(curDR!=-1)bad[curDR*100+curUR]=info.GameFrame+BAD_COOL;
            curDR=-1;curUR=-1;
            lastMoveFrame=-1;
        }
        else{
            return;
        }
    }
    lastMoveFrame=-1;

    //---------- 4. 选目标 ----------
    int bd=-1,bu=-1,bv=1<<30;

    if(phase==0){
        // 自己那一角: 市中心 → 地图角 的半张图
        int drMin=(centerBlockDR<50?2:centerBlockDR);
        int drMax=(centerBlockDR<50?centerBlockDR:97);
        int urMin=(centerBlockUR<50?2:centerBlockUR);
        int urMax=(centerBlockUR<50?centerBlockUR:97);
        for(int dr=drMin;dr<=drMax;dr++){
            for(int ur=urMin;ur<=urMax;ur++){
                if(!frontier(dr,ur))continue;
                if(isBad(dr,ur))continue;
                if(dangerNear(dr,ur))continue;
                int dx=dr-priestBlockDR, dy=ur-priestBlockUR;
                int d2=dx*dx+dy*dy;                        // 离祭司越近越优先
                if(d2<bv){bv=d2;bd=dr;bu=ur;}
            }
        }
        if(bd==-1){                                        // 这一角探完 → 转绕中心
            phase=1;
            radius=40;
            DebugText("priest: phase0 done");
        }
    }
    else if(phase==1){
        // 绕中心: 半径从大到小, 在每条环带(|切比雪夫距离 - r| <= 1)里找最近的边界点
        for(int r=radius;r>=8;r-=8){
            bd=-1;bu=-1;bv=1<<30;
            for(int dr=2;dr<98;dr++){
                for(int ur=2;ur<98;ur++){
                    int adx=abs(dr-50),ady=abs(ur-50);
                    int d=(adx>ady?adx:ady);
                    if(d<r-1||d>r+1)continue;
                    if(!frontier(dr,ur))continue;
                    if(isBad(dr,ur))continue;
                    if(dangerNear(dr,ur))continue;
                    int dx=dr-priestBlockDR, dy=ur-priestBlockUR;
                    int d2=dx*dx+dy*dy;
                    if(d2<bv){bv=d2;bd=dr;bu=ur;}
                }
            }
            if(bd!=-1){radius=r;break;}                    // 这条环带还有得探
        }
        if(bd==-1){                                        // 绕完了, 不探另外三个角
            phase=2;
            DebugText("priest: explore done");
        }
    }

    if(bd==-1)return;                                      // 阶段2: 探完, 不再动作
    HumanMove(priestSN,bd*BLOCKSIDELENGTH,bu*BLOCKSIDELENGTH);
    lastMoveFrame=info.GameFrame;
    curDR=bd;
    curUR=bu;
}

void UsrAI::checkWorkState(){
    for(auto&f:info.farmers){
        if(farIsgotten.find(f.SN)!=farIsgotten.end()&&f.NowState!=HUMAN_STATE_WORKING){
            farIsgotten.erase(f.SN);
        }
    }
}
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
// 是否已经有某类建筑(onlyFinished=true 表示只算建成的)
bool UsrAI::hasBuilding(int type,bool onlyFinished){
    for(auto&b:info.buildings){
        if(b.Type!=type)continue;
        if(onlyFinished&&b.Percent<100)continue;
        return true;
    }
    return false;
}
// 是否有某类建筑正在施工(避免同一类连建两个)
bool UsrAI::hasUnfinishedBuilding(int type){
    for(auto&b:info.buildings){
        if(b.Type==type&&b.Percent<100)return true;
    }
    return false;
}
// (dr,ur)附近 range 格内有没有"已建成"的某类建筑
bool UsrAI::nearFinishedBuilding(int type,int dr,int ur,int range){
    for(auto&b:info.buildings){
        if(b.Type!=type)continue;
        if(b.Percent<100)continue;
        int dx=abs(b.BlockDR-dr),dy=abs(b.BlockUR-ur);
        if((dx>dy?dx:dy)<=range)return true;
    }
    return false;
}
// 在(bd,bu)附近找一个 size×size 的空地(返回左下角块坐标)
// 条件: 全是已探明陆地(Open, 说明没资源没建筑) + 高度一致且不是斜坡 + 没被拉黑
bool UsrAI::findBuildSpot(int bd,int bu,int size,int minR,int maxR,int &ox,int &oy){
    ox=-1;oy=-1;
    int bestD=1<<30;
    for(int dr=bd-maxR;dr<=bd+maxR;dr++){
        for(int ur=bu-maxR;ur<=bu+maxR;ur++){
            if(dr<2||dr+size-1>=98||ur<2||ur+size-1>=98)continue;
            int adx=abs(dr-bd),ady=abs(ur-bu);
            int d=(adx>ady?adx:ady);
            if(d<minR)continue;                        // 别贴着基准点盖, 留出通道
            auto bit=badBuildSpot.find(dr*100+ur);
            if(bit!=badBuildSpot.end()&&info.GameFrame<bit->second)continue;
            bool ok=true;
            int h=-2;
            for(int i=0;i<size&&ok;i++){
                for(int j=0;j<size;j++){
                    if(MAP[dr+i][ur+j]!=Open){ok=false;break;}
                    int hh=(*info.theMap)[dr+i][ur+j].height;
                    if(hh<0){ok=false;break;}          // 斜坡或海
                    if(h==-2)h=hh;
                    else if(hh!=h){ok=false;break;}    // 高度不一致建不了
                }
            }
            if(!ok)continue;
            if(d<bestD){bestD=d;ox=dr;oy=ur;}          // 取离基准点最近的
        }
    }
    return ox!=-1;
}

//==================== 建造调度 ====================
// 一个建造工(homeBuilderSN)串行干这些活:
//   房屋(人口快满) → 采集点配套的谷仓/仓库 → 兵营 → 市场 → 靶场 → 马厩
// 每帧最多下一个指令: 先走过去, 到位再 HumanBuild; 400帧没盖起来就把选址拉黑换地方
void UsrAI::manageBuild(){
    static int tType=-1,tDR=-1,tUR=-1;   // 当前建造任务
    static bool tIssued=false;           // HumanBuild 是否已下达
    static int tFrame=-1;

    buildBusy=false;                     // 先当没活; 真挑到任务再置位

    // ---- 建造工还在不在(死了就换一个) ----
    bool alive=false;
    for(auto&f:info.farmers){
        if(f.SN==homeBuilderSN){alive=true;break;}
    }
    if(!alive){
        homeBuilderSN=-1;
        for(auto&f:info.farmers){
            if(f.NowState==HUMAN_STATE_IDLE){homeBuilderSN=f.SN;break;}
        }
        if(homeBuilderSN==-1)return;
    }
    gethomeBuilder();                    // 刷新 homeBuilderDR/UR/State

    // ---- 老任务: 看盖起来没有 ----
    if(tType!=-1){
        bool appeared=false;
        for(auto&b:info.buildings){
            if(b.Type==tType&&abs(b.BlockDR-tDR)<=1&&abs(b.BlockUR-tUR)<=1){appeared=true;break;}
        }
        if(appeared){
            tType=-1;tIssued=false;tFrame=-1;
        }
        else if(tIssued&&info.GameFrame-tFrame>400){
            badBuildSpot[tDR*100+tUR]=info.GameFrame+6000;   // 建不起来, 这个点先别用了
            tType=-1;tIssued=false;tFrame=-1;
        }
    }

    // ---- 挑新任务(优先级从高到低) ----
    if(tType==-1){
        int wood=info.Wood;
        int ox=-1,oy=-1;
        int cntStock=0,cntGranary=0;
        for(auto&b:info.buildings){
            if(b.Type==BUILDING_STOCK)cntStock++;
            else if(b.Type==BUILDING_GRANARY)cntGranary++;
        }

        // T1 房屋: 人口快满了(每房+4人口)
        if(wood>=BUILD_HOUSE_WOOD&&info.Human_MaxNum-info.Human_Num<=2
           &&!hasUnfinishedBuilding(BUILDING_HOME)
           &&findBuildSpot(centerBlockDR,centerBlockUR,2,4,14,ox,oy)){
            tType=BUILDING_HOME;
        }
        // T2 谷仓: 正在采浆果, 而那里10格内没有谷仓也没有市中心 → 就近补一个
        if(tType==-1&&wood>=BUILD_GRANARY_WOOD&&cntGranary<2&&!hasUnfinishedBuilding(BUILDING_GRANARY)){
            for(auto&kv:gathering){
                if(kv.second.type!=RESOURCE_BUSH)continue;
                int bd=kv.second.dr,bu=kv.second.ur;
                if(nearFinishedBuilding(BUILDING_GRANARY,bd,bu,10))continue;
                if(nearFinishedBuilding(BUILDING_CENTER,bd,bu,10))continue;
                if(findBuildSpot(bd,bu,3,2,10,ox,oy)){tType=BUILDING_GRANARY;break;}
            }
        }
        // T3 仓库: 正在砍树/打猎/挖石, 而那里10格内没有仓库也没有市中心 → 就近补一个
        if(tType==-1&&wood>=BUILD_STOCK_WOOD&&cntStock<4&&!hasUnfinishedBuilding(BUILDING_STOCK)){
            for(auto&kv:gathering){
                int rt=kv.second.type;
                if(!(rt==RESOURCE_TREE||rt==RESOURCE_STONE||rt==RESOURCE_GOLD
                     ||rt==RESOURCE_GAZELLE||rt==RESOURCE_ELEPHANT||rt==RESOURCE_LION))continue;
                int bd=kv.second.dr,bu=kv.second.ur;
                if(nearFinishedBuilding(BUILDING_STOCK,bd,bu,10))continue;
                if(nearFinishedBuilding(BUILDING_CENTER,bd,bu,10))continue;
                if(findBuildSpot(bd,bu,3,2,10,ox,oy)){tType=BUILDING_STOCK;break;}
            }
        }
        // T4 兵营(后面造兵的前置)
        if(tType==-1&&wood>=BUILD_ARMYCAMP_WOOD&&!hasBuilding(BUILDING_ARMYCAMP,false)
           &&findBuildSpot(centerBlockDR,centerBlockUR,3,4,14,ox,oy)){
            tType=BUILDING_ARMYCAMP;
        }
        // T5 市场(前置: 谷仓 + 工具时代; 也是升铜器要的工具时代建筑)
        if(tType==-1&&wood>=BUILD_MARKET_WOOD&&info.civilizationStage>=CIVILIZATION_TOOLAGE
           &&hasBuilding(BUILDING_GRANARY,true)
           &&!hasBuilding(BUILDING_MARKET,false)
           &&findBuildSpot(centerBlockDR,centerBlockUR,3,4,14,ox,oy)){
            tType=BUILDING_MARKET;
        }
        // T6 靶场(前置: 兵营 + 工具时代)
        if(tType==-1&&wood>=BUILD_RANGE_WOOD&&info.civilizationStage>=CIVILIZATION_TOOLAGE
           &&hasBuilding(BUILDING_ARMYCAMP,true)
           &&!hasBuilding(BUILDING_RANGE,false)
           &&findBuildSpot(centerBlockDR,centerBlockUR,3,4,14,ox,oy)){
            tType=BUILDING_RANGE;
        }
        // T7 马厩(前置: 兵营 + 工具时代)
        if(tType==-1&&wood>=BUILD_STABLE_WOOD&&info.civilizationStage>=CIVILIZATION_TOOLAGE
           &&hasBuilding(BUILDING_ARMYCAMP,true)
           &&!hasBuilding(BUILDING_STABLE,false)
           &&findBuildSpot(centerBlockDR,centerBlockUR,3,4,14,ox,oy)){
            tType=BUILDING_STABLE;
        }

        if(tType!=-1){tDR=ox;tUR=oy;tIssued=false;tFrame=-1;}
    }

    buildBusy=(tType!=-1);
    if(!buildBusy)return;

    // ---- 执行: 先走过去(顺带取消他手上的采集), 到了再建 ----
    int dist=abs(homeBuilderDR-tDR)+abs(homeBuilderUR-tUR);
    if(!tIssued){
        if(dist>2){
            if(homeBuilderState!=HUMAN_STATE_WALKING)      // 走路中不重复下指令
                HumanMove(homeBuilderSN,tDR*BLOCKSIDELENGTH,tUR*BLOCKSIDELENGTH);
            return;
        }
        HumanBuild(homeBuilderSN,tType,tDR,tUR);
        tIssued=true;
        tFrame=info.GameFrame;
    }
}