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

int centerSN=-1;
int centerBlockDR=-1;
int centerBlockUR=-1;


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

int gazelleState=0;//0-派 1-等 2-打 3-建 4-done
int gazelleSpotDR=-1;
int gazelleSpotUR=-1;
int gazelleHunter1SN=-1;
int gazelleHunter2SN=-1;
int gazelleTargetSN=-1;


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
bool priestExploring=true;      // 祭司是否还在探索阶段: 回到箭塔底下才置 false, 之后遇到敌人不再躲

bool getOnlyOnce=false;

unordered_map<int,bool>resIsgotten;
unordered_map<int,bool>farIsgotten;







int dx_home[]={-2,0,2,0};
int dy_home[]={0,2,0,-2};

int homeBuilderSN=-1;
int homeBuilderDR=-1;
int homeBuilderUR=-1;
int homeBuilderState=-1;



int cnt=0;
int cnt_build=0;
bool stockOnce=false;



const int goHomeFrame=5250;

int bushNum=0;//6
int gazelleNum=0;//6
int killGazelle=0;//6
int woodNum=0;
bool storageStarted=false;//防止猎人重复建仓库

int phaseNum=20;//铜器之前先限制20
bool phaseChange=false;
////////////////////////////////////////////
//军事
int marketBlockDR=-1;
int marketBlockUR=-1;

int armyCampBlockDR=-1;
int armyCampBlockUR=-1;


/////////////////////////////////////////////

void UsrAI::processData()
{   
    
    info=getInfo();
    //升级
    if(info.civilizationStage!=CIVILIZATION_BRONZEAGE){
        bool haveMarket=false;
        bool haveRange=false;
        int centerState=-1;
        for(auto&b:info.buildings){
            if(b.Type==BUILDING_MARKET)haveMarket=true;
            if(b.Type==BUILDING_RANGE)haveRange=true;
            if(b.Type==BUILDING_CENTER)centerState=b.Project;
        }
        if(haveMarket&&haveRange&&info.Meat>=BUILDING_CENTER_UPGRADE_BRONZEAGE_FOOD){
            if(centerState==ACT_NULL)
                BuildingAction(centerSN,BUILDING_CENTER_UPGRADE);
        }
    }
    if(!phaseChange){
        if(info.civilizationStage==CIVILIZATION_BRONZEAGE){
            phaseNum=25;
            phaseChange=true;
        }
    }

    farIsgotten.clear();
    if(!getOnlyOnce)getBaseInfo();
    
    betterMap();
    
    priestExplore();

    waveBattle();
    //获取唯一的房屋建造者
    for(auto&f:info.farmers){
        if(homeBuilderSN==-1&&f.NowState==HUMAN_STATE_IDLE){
            homeBuilderSN=f.SN;
            gethomeBuilder();
        }
    }

    
    //处理建造
    manageBuild();   

    storageStarted=false;
    huntGazelle();

    //第二猎人帮建
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
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE||farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                
                int bySN=-1;
                int st=checkEnv(r.Type,bySN);
                if(r.Type==RESOURCE_GAZELLE)st=2;
                if(st==2){ //已有 直接建
                    HumanAction(f.SN,r.SN); //这里f.SN 后期要改为距离最近的村民 搞个函数替代一下
                    resIsgotten[r.SN]=true;
                    farIsgotten[f.SN]=true;
                    if(r.Type==RESOURCE_BUSH)bushNum++;
                    if(r.Type==RESOURCE_GAZELLE)gazelleNum++;
                    
                    
                }
                else if(st==1)HumanAction(f.SN,bySN); //有没建好的 帮建
                else{ //没有
                    int ox=-1,oy=-1;
                    if(info.Wood>=needWood&&!storageStarted&&findBuildSpot(r.BlockDR,r.BlockUR,3,2,5,ox,oy)){
                        HumanBuild(f.SN,need,ox,oy);
                        storageStarted=true;
                    }
                    else{
                        //这里直接采是有点问题的 但是因为基本上不会被触发 可以忽略
                        HumanAction(f.SN,r.SN);
                        resIsgotten[r.SN]=true;
                        if(r.Type==RESOURCE_BUSH)bushNum++;
                        if(r.Type==RESOURCE_GAZELLE)gazelleNum++;
                        
                    }
                }
                farIsgotten[f.SN]=true;
                assignedFrame=info.GameFrame;
                break;
            }   
        }
    }

    {
        int woodNow=0;//本帧实时木工数
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

    //这个好像已经没用了
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_GAZELLE){
            gazelleSN=r.SN;
            gazelleBlockDR=r.BlockDR;
            gazelleBlockUR=r.BlockUR;
            break;
        }
    }
   
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
        if(r.Type!=RESOURCE_EMPTY)MAP[r.BlockDR][r.BlockUR]=r.Type+100;//资源+100偏移
    }
    for(auto&b:info.buildings){
        int sz=3;
        if(b.Type==BUILDING_HOME||b.Type==BUILDING_ARROWTOWER)sz=2;
        int dr=b.BlockDR;
        int ur=b.BlockUR;
        for(int i=0;i<sz;i++){
            for(int j=0;j<sz;j++)MAP[dr+i][ur+j]=b.Type+1000;//建筑+1000偏移
        }
        
    }
}

//==================== 祭司探路 ====================
// 阶段0: 探自己所在的角(以市镇中心为界, 只扫"中心->地图角"那半张图)
// 阶段1: 以地图中心(50,50)为圆心, 半径40起一圈圈向里收(只在环带里找边界点)
// 另外三个角不探。视野内出现敌人/猛兽立即躲避。
// 说明: 边界点 = 已探明陆地(Open) 且 周围2格内有未知区(Unknown)
// 当前"已探明"的活瞪羚数量。
// 引擎只要某格被探索过一次, 就会把格上的动物一直报进 info.resources, 所以这个数就是"地图上已知还活着的瞪羚"。
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
    const int BAD_COOL=1000;             // 走不到的格子拉黑时长(帧)
    const int DODGE_COOL=80;             // 躲避冷却(帧)

    // 只有"已探明陆地"才允许站上去(海里/未知区/资源格/建筑格一律不去)
    auto usable=[&](int dr,int ur)->bool{
        if(dr<0||dr>=100||ur<0||ur>=100)return false;
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
            if((r.Type==RESOURCE_LION)&&r.Blood>0){
                int dx=r.BlockDR-dr, dy=r.BlockUR-ur;
                if(dx*dx+dy*dy<DANGER_R2)return true;
            }
        }
        return false;
    };

    //---------- 1. 找最近的危险(视野内的敌兵/敌农/狮子) ----------
    int ex=-1,ey=-1,bestD2=1e18;
    for(auto&a:info.enemy_armies){
        int dx=a.BlockDR-priestBlockDR, dy=a.BlockUR-priestBlockUR;
        int d2=dx*dx+dy*dy;
        if(d2<bestD2){
            bestD2=d2;
            ex=a.BlockDR;
            ey=a.BlockUR;
        }
    }
    for(auto&f:info.enemy_farmers){
        int dx=f.BlockDR-priestBlockDR, dy=f.BlockUR-priestBlockUR;
        int d2=dx*dx+dy*dy;
        if(d2<bestD2){
            bestD2=d2;
            ex=f.BlockDR;
            ey=f.BlockUR;
        }
    }
    for(auto&r:info.resources){
        if((r.Type==RESOURCE_LION)&&r.Blood>0){
            int dx=r.BlockDR-priestBlockDR, dy=r.BlockUR-priestBlockUR;
            int d2=dx*dx+dy*dy;
            if(d2<bestD2){
                bestD2=d2;
                ex=r.BlockDR;
                ey=r.BlockUR;
            }
        }
    }

    //---------- 2. 有危险就跑: 8个方向里挑最背离危险、且可走的一格, 撤6格 ----------
    if(ex!=-1&&bestD2<DANGER_R2&&priestExploring){
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
                if(score>bestScore){
                    bestScore=score;
                    tdr=nx;
                    tur=ny;
                }
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

    //---------- 3.2 到点回家: goHomeFrame 之后回箭塔底下待命 ----------
    // 只在"闲着"时才动(走路中上面已经 return 了); 到了箭塔旁边就待命, 不再探索/不再追瞪羚。
    if((marketBlockDR!=-1||info.GameFrame>=goHomeFrame)&&arrowTowerBlockDR!=-1){
        static int homeDR=-1,homeUR=-1,homeFrame=-1;
        if(abs(priestBlockDR-arrowTowerBlockDR)>2||abs(priestBlockUR-arrowTowerBlockUR)>2){
            if(homeDR==-1){                                  // 还没挑落脚点 -> 挑箭塔四邻
                int dx4[4]={0,1,0,-1};
                int dy4[4]={1,0,-1,0};
                for(int k=0;k<4;k++){
                    int nr=arrowTowerBlockDR+dx4[k], nu=arrowTowerBlockUR+dy4[k];
                    if(!usable(nr,nu))continue;
                    if(isBad(nr,nu))continue;
                    homeDR=nr;homeUR=nu;
                    break;
                }
                if(homeDR!=-1){
                    HumanMove(priestSN,homeDR*BLOCKSIDELENGTH,homeUR*BLOCKSIDELENGTH);
                    lastMoveFrame=info.GameFrame;
                    homeFrame=info.GameFrame;
                }
            }
            else if(info.GameFrame-homeFrame>MOVE_TIMEOUT){  // 走不到 -> 拉黑换点(别再犯"每帧重发"的错)
                bad[homeDR*100+homeUR]=info.GameFrame+BAD_COOL;
                homeDR=-1;homeUR=-1;
            }
            return;
        }
        homeDR=-1;homeUR=-1;
        priestExploring=false;                               // 已到箭塔底下 -> 停止探索状态, 之后遇敌不再躲
        homeDR=-1;homeUR=-1;
        
        return;                                              // 已在箭塔底下 -> 待命
    }

    //---------- 3.5 看见瞪羚但还不够 6 只 -> 先过去把瞪羚照亮 ----------
    // 走近之后祭司 12 格的视野会把周围的瞪羚一起照亮, 凑够 6 只才回去做常规探索。
    // ★ 同一个落脚点只发一次指令; 去了 MOVE_TIMEOUT 帧还没到就拉黑它并放行回常规探索。
    //   否则每帧重发 -> addRelation 反复 suspendRelation(清路径) -> 祭司被钉在原地动不了。
    {
        static int seekDR=-1,seekUR=-1,seekFrame=-1;

        int gdr=-1,gur=-1,gD2=1<<30;
        if(liveGazelleNum()<gazelleWantNum){
            for(auto&r:info.resources){
                if(r.Type!=RESOURCE_GAZELLE)continue;
                if(r.Blood<=0)continue;                       // 只追活的
                int dx=r.BlockDR-priestBlockDR, dy=r.BlockUR-priestBlockUR;
                int d2=dx*dx+dy*dy;
                if(d2<=36)continue;                           // 已经贴着它了(6格内), 换下一只
                if(d2>900)continue;                           // 太远的先别追(免得隔着海去够), 交给常规探索
                if(d2<gD2){
                    gD2=d2;
                    gdr=r.BlockDR;
                    gur=r.BlockUR;
                }
            }
        }

        int ox=-1,oy=-1;
        if(gdr!=-1){
            int dx4[4]={0,1,0,-1};
            int dy4[4]={1,0,-1,0};
            for(int k=0;k<4;k++){                             // 先找它四邻的落脚点
                int nr=gdr+dx4[k], nu=gur+dy4[k];
                if(!usable(nr,nu))continue;
                if(dangerNear(nr,nu))continue;
                if(isBad(nr,nu))continue;
                ox=nr;oy=nu;
                break;
            }
            if(ox==-1){
                for(int k=0;k<4;k++){                         // 四邻都站不上就退两格
                    int nr=gdr+dx4[k]*2, nu=gur+dy4[k]*2;
                    if(!usable(nr,nu))continue;
                    if(dangerNear(nr,nu))continue;
                    if(isBad(nr,nu))continue;
                    ox=nr;oy=nu;
                    break;
                }
            }
        }

        if(ox==-1){
            seekDR=-1;seekUR=-1;seekFrame=-1;                 // 暂时没目标可追
        }
        else if(ox!=seekDR||oy!=seekUR){                      // 换了新的落脚点 -> 计时并发一次指令
            seekDR=ox;seekUR=oy;seekFrame=info.GameFrame;
            HumanMove(priestSN,ox*BLOCKSIDELENGTH,oy*BLOCKSIDELENGTH);
            lastMoveFrame=info.GameFrame;
            curDR=ox;
            curUR=oy;
            return;
        }
        else if(info.GameFrame-seekFrame>MOVE_TIMEOUT){       // 同一个点耗了 300 帧还没到 -> 拉黑它
            bad[ox*100+oy]=info.GameFrame+BAD_COOL;
            seekDR=-1;seekUR=-1;seekFrame=-1;
            curDR=-1;curUR=-1;
            lastMoveFrame=-1;                                 // 不 return, 这一帧就回去做常规探索
        }
        else{
            return;                                           // 正在去这只瞪羚的路上, 别打断
        }
    }

    //---------- 4. 选目标 ----------
    int bd=-1,bu=-1,bv=1e18;

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
                if(d2<bv){
                    bv=d2; //bv到底是啥
                    bd=dr;
                    bu=ur;
                }
            }
        }
        if(bd==-1){    //bd又是啥                                    // 这一角探完 → 转绕中心
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

    // [AI修复] 同一个落脚点只发一次移动指令!
    //   原来这里每帧都发 HumanMove()。那个点要是走不到(被海/山隔开),
    //   内核会反复清路径 -> 祭司钉在原地, 状态还掉回 IDLE ->
    //   第 3 段"走路中不打断"的保护失效 -> 每帧重发 -> 刷屏卡死。
    //   现在: 目标没变就不重发; 同一个点耗过 MOVE_TIMEOUT 还没到就拉黑它, 换下一个。
    static int tgtDR=-1,tgtUR=-1,tgtFrame=-1;
    if(bd!=tgtDR||bu!=tgtUR){                              // 换了新目标 -> 记时, 发一次
        tgtDR=bd;tgtUR=bu;tgtFrame=info.GameFrame;
        HumanMove(priestSN,bd*BLOCKSIDELENGTH,bu*BLOCKSIDELENGTH);
        lastMoveFrame=info.GameFrame;
        curDR=bd;
        curUR=bu;
        return;
    }
    if(info.GameFrame-tgtFrame>MOVE_TIMEOUT){              // 同一个点走不到 -> 拉黑换点
        bad[bd*100+bu]=info.GameFrame+BAD_COOL;
        tgtDR=-1;tgtUR=-1;tgtFrame=-1;
        curDR=-1;curUR=-1;
        lastMoveFrame=-1;
    }
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
        if(a.BlockDR>=dr&&a.BlockUR<dr+size&&a.BlockUR>=ur&&a.BlockUR<ur+size)return true;
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
    // DebugText(QString("bush=%1 gaz=%2 wood=%3").arg(bushNum).arg(gazelleNum).arg(woodNum));
    
    bool alive=false;
    for(auto&f:info.farmers){
        if(f.SN==homeBuilderSN){
            alive=true;
            break;
        }
    }
    //死了//或者开局未初始化
    if(!alive){
        for(auto&f:info.farmers){
            if(f.NowState==HUMAN_STATE_IDLE&&farIsgotten.find(f.SN)==farIsgotten.end()){
                homeBuilderSN=f.SN;
                homeBuilderDR=f.DR;
                homeBuilderUR=f.UR;
                homeBuilderState=f.NowState;
                break;
            }
        }
        
    }

    gethomeBuilder();                    

    int maxNum=info.Human_MaxNum;
    double haveNum=info.Human_Num;
    int spaceNum=maxNum-haveNum;
    
    //造人开关
    if(haveNum<phaseNum)BuildingAction(centerSN,BUILDING_CENTER_CREATEFARMER);
    
    if(spaceNum<=3&&maxNum<50){
        
        for(auto&b:info.buildings){
            
            if(b.Type==BUILDING_HOME&&info.Wood>=BUILD_HOUSE_WOOD&&homeBuilderState==HUMAN_STATE_IDLE){
                int cornerDR=b.BlockDR;
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
    //第一阶段经济调度启动后 着手建造
    auto hasType=[&](int type)->bool{
            for(auto&b:info.buildings){
                if(b.Type==type)return true;
            }
            return false;
        };
    
        
    //农田
    //周围八个方向 中间留宽2格的通道 -- 实操查看是否会同时建造导致卡住
    int fsDR[]={-5,0,5,0,-5,5,5,-5};
    int fsDU[]={0,5,0,-5,5,5,-5,-5};

    if(bushNum>=6&&gazelleNum>=6&&woodNum>=3){
        //有无在建的
        int buildingSN=-1;
        for(auto&b:info.buildings){
            if(b.Percent>=100)continue;
            if(b.Type!=BUILDING_MARKET&&b.Type!=BUILDING_ARMYCAMP&&b.Type!=BUILDING_RANGE)continue;
            buildingSN=b.SN;
            break;
        }
        //有在建的
        if(buildingSN!=-1){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
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
        static bool tech[3]{};//0 木材 1 动物 2 金矿
        if(!tech&&hasType(BUILDING_MARKET)){
            for(auto&b:info.buildings){
                if(b.Type!=BUILDING_MARKET)continue;
                if(b.Project!=ACT_NULL)continue;
                if(!tech[0]&&info.Wood>=BUILDING_MARKET_WOOD_UPGRADE_WOOD&&info.Meat>=BUILDING_MARKET_WOOD_UPGRADE_FOOD){
                    BuildingAction(b.SN,BUILDING_MARKET_WOOD_UPGRADE);
                    tech[0]=true;
                }
                if(!tech[1]&&info.Meat>=BUILDING_MARKET_GOLD_UPGRADE_FOOD&&info.Wood>=BUILDING_MARKET_GOLD_UPGRADE_WOOD){
                    BuildingAction(b.SN,BUILDING_MARKET_GOLD_UPGRADE);
                    tech[1]=true;
                }
                if(!tech[2]&&info.Meat>=BUILDING_MARKET_FARM_UPGRADE_FOOD&&info.Wood>=BUILDING_MARKET_FARM_UPGRADE_WOOD){
                    BuildingAction(b.SN,BUILDING_MARKET_FARM_UPGRADE);
                    tech[2]=true;
                }

            }
        }
        if(!hasType(BUILDING_MARKET)){
            want=BUILDING_MARKET;
            cost=BUILD_MARKET_WOOD;
        }
        //兵营
        else if(!hasType(BUILDING_ARMYCAMP)){
            want=BUILDING_ARMYCAMP;
            cost=BUILD_ARMYCAMP_WOOD;
            if(marketBlockDR!=-1){                   //兵营挨着市场建
                bd=marketBlockDR;
                bu=marketBlockUR;
            }
        }
        //靶场
        else if(!hasType(BUILDING_RANGE)){
            want=BUILDING_RANGE;
            cost=BUILD_RANGE_WOOD;
            if(armyCampBlockDR!=-1){                    // 靶场挨着兵营建
                bd=armyCampBlockDR;
                bu=armyCampBlockUR;
            }
        }
        
        //木材不够 -- 空闲的人都去砍树
        if(want!=-1&&info.Wood<cost)assignWoodcutter();

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

                int row=hasType(BUILDING_MARKET)?0:1;
                int dx4[8]{};
                int dy4[8]{};
                for(int i=0;i<8;++i){
                    dx4[i]=DX[row][i];
                    dy4[i]=DY[row][i];
                }

                for(int k=0;k<8;k++){
                    int dr=bd+dx4[k],du=bu+dy4[k];
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
                    if(spotBusy(dr,du,3))continue;
                    bool farmSlot=false;
                    for(int q=0;q<16;q++){
                        if(dr==granaryBlockDR+fsDR[q]&&du==granaryBlockUR+fsDU[q]){
                            farmSlot=true;
                            break;
                        }
                    }
                    if(farmSlot)continue;
                    HumanBuild(f.SN,want,dr,du);
                    farIsgotten[f.SN]=true;
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
    if(hasType(BUILDING_RANGE)&&granaryBlockDR!=-1&&hasType(BUILDING_MARKET)&&info.Wood>=BUILD_FARM_WOOD){
        int farmNum=0;
        for(auto&b:info.buildings){
            if(b.Type==BUILDING_FARM&&b.Cnt>0)farmNum++;
        }
        if(farmNum<8){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN||f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;

                for(int q=0;q<16;q++){
                    int dr=granaryBlockDR+fsDR[q], du=granaryBlockUR+fsDU[q];
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
        for(auto&f:info.farmers){
            if(f.SN==homeBuilderSN||f.NowState!=HUMAN_STATE_IDLE)continue;
            if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
            HumanAction(f.SN,b.SN);                         // 农民对农田 = 去种/收
            farIsgotten[f.SN]=true;
            break;
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
            if(farIsgotten.find(r.SN)!=farIsgotten.end())continue; // 本帧已经给这棵树派过人了
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
    int fSN=findFarmer(bestDR,bestUR);
    if(fSN==-1)return -1;                                        // 没有空闲的人可派

    // ---------- ③ 派去砍 ----------
    HumanAction(fSN,bestSN);
    resIsgotten[bestSN]=true;
    woodNum++;
    farIsgotten[fSN]=true;
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
        if(liveGazelleNum()<gazelleWantNum)return;
        // bool have=false;
        // for(auto&r:info.resources){
        //     if(r.Type==RESOURCE_GAZELLE&&r.Blood>0){
        //         have=true;
        //         break;
        //     }
        // }
        // if(!have){
        //     gazelleState=4;
        //     return;
        // }
        ///别跟全局定义的gazelleBlockDR搞混了
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
            gazelleTargetSN=findGazelle(gazelleSpotDR,gazelleSpotUR,gazelleDR,gazelleUR,10);
            
            
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
void UsrAI::waveBattle(){
    if(info.enemy_armies.empty())return;
    //波次探测
    // static int wavePhase=1;
    // int edr,edu;
    // for(auto&ea:info.enemy_armies){
    //     if(ea.Sort==AT_CHARIOT){
    //         edr=ea.BlockDR,edu=ea.BlockUR;
    //         int dx=edr-arrowTowerBlockDR,dy=edu-arrowTowerBlockUR;
    //         if(dx*dx+dy*dy<=100)wavePhase=2;
    //         break;
    //     }
    //     if(wavePhase==1&&ea.Sort==AT_STONE_THROWER){
    //         edr=ea.BlockDR,edu=ea.BlockUR;
    //         int dx=edr-arrowTowerBlockDR,dy=edu-arrowTowerBlockUR;
    //         if(dx*dx+dy*dy<=100)wavePhase=3;
    //         break;
    //     }
    // }
    
    // if(wavePhase!=1)goto NEXT;
    // 挑两个近战: 第一个给祭司转化, 第二个给箭塔打
    int priestTarget=-1,towerPick=-1;

    for(auto&e:info.enemy_armies){
        if(!isMeleeSort(e.Sort))continue;
        if(priestTarget==-1)priestTarget=e.SN;
        else{
            towerPick=e.SN;
            break;
        }
    }

    // ---- 继续转化
    bool priestAtHome=abs(priestBlockDR-arrowTowerBlockDR)<=2&&abs(priestBlockUR-arrowTowerBlockUR)<=2;

    if(priestSN!=-1&&priestTarget!=-1&&(marketBlockDR!=-1||info.GameFrame>=goHomeFrame)&&priestAtHome){
        for(auto&a:info.armies){
            if(a.SN!=priestSN)continue;
            if(a.NowState!=HUMAN_STATE_IDLE)break;  
            if(a.ConvertCooldown<=0&&a.WorkObjectSN!=priestTarget)HumanAction(priestSN,priestTarget); //转化另一个人
            break;
        }
    }

    // ---- 箭塔: 另一个近战进射程就打 ----
    if(arrowTowerSN!=-1&&towerPick!=-1){
        int tdr=-1,tdur=-1,project=-1;
        for(auto&b:info.buildings){
            if(b.SN!=arrowTowerSN)continue;
            project=b.Project;                              // 塔当前的攻击目标
            tdr=b.BlockDR;
            tdur=b.BlockUR;
            break;
        }
        const int r2=DIS_ARROWTOWER*DIS_ARROWTOWER;         // 射程平方(7格)
        for(auto&e:info.enemy_armies){
            if(e.SN!=towerPick)continue;
            int dx=e.BlockDR-tdr, dy=e.BlockUR-tdur;
            if(dx*dx+dy*dy<=r2&&project==ACT_NULL){        // 进射程 且 当前没在打它
                HumanAction(arrowTowerSN,towerPick);
            }
            break;
        }
    }

    // ---- 转化过来的兵去打弓兵(除祭司外我们没别的兵, 非祭司即转化来的) ----
    int archerSN=-1;
    for(auto&e:info.enemy_armies){
        if(isArcherSort(e.Sort)){
            archerSN=e.SN;
            break;
        }
    }
    if(archerSN!=-1){
        for(auto&a:info.armies){
            if(a.Sort==AT_PRIEST||a.WorkObjectSN==archerSN)continue; //不是祭司/已经有人打了
            HumanAction(a.SN,archerSN);
        }
    }
// NEXT:
//     if(wavePhase==2){

//     }
}

int UsrAI::checkEnv(int type,int& byBuildingSN){
    byBuildingSN=-1;
    
    int radius=5;
    int need=((type==RESOURCE_BUSH)?BUILDING_GRANARY:BUILDING_STOCK);


    for(auto&r:info.resources){
        if(r.Type!=type)continue;
        if(type==RESOURCE_GAZELLE){
            if(r.Blood>0)continue;
        }
        if(r.Cnt<=0)continue;
        for(auto&b:info.buildings){
            if(b.Type!=need)continue;
            int d=max(abs(b.BlockDR-r.BlockDR),abs(b.BlockUR-r.BlockUR));
            if(d>radius)continue; //以这个资源为中心 链接每个对应的存储点 看有没有存储点是符合要求的
            if(b.Percent>=100)return 2; //建好
            if(byBuildingSN==-1) byBuildingSN=b.SN;
        }
    }
    return (byBuildingSN!=-1)?1:0; //不等于-1说明周围有
    
}