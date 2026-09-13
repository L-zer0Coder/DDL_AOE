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

//==================== 打大象(集体风筝) 全局状态 ====================
// 流程: 探明活象 -> 给市镇中心加3个造人名额 -> 3人集合 -> 抱团集体风筝 -> 就地建仓库 -> 采肉
int elephantState=0;                     // 状态机: 0-探明等 1-集结 2-风筝 3-建仓库 4-采肉
int elephantTargetSN=-1;                 // 当前锁定的大象 SN
int elephantSpotDR=-1,elephantSpotUR=-1; // 大象位置(集合落脚点/建仓库基准点)
int elephantHunterSN[3]={-1,-1,-1};      // 3个猎人的 SN
int elephantExtra=0;                     // 发现象后给市镇中心加的额外造人名额(只加一次)
int elephantKiteCmd=-1;                  // 上帧发的动作: 1-打 2-撤, 防抖(动作没变就不重发指令)

int priestSN=-1;
int priestBlockDR=-1;
int priestBlockUR=-1;
int priestState=-1;


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
            phaseNum=51;
            phaseChange=true;
        }
    }

    farIsgotten.clear();
    if(!getOnlyOnce)getBaseInfo();
    
    betterMap();
    
    priestExplore();

    waveBattle();
    for(auto&f:info.farmers){
        if(homeBuilderSN==-1&&f.NowState==HUMAN_STATE_IDLE){
            homeBuilderSN=f.SN;
            gethomeBuilder();
        }
    }

    
    
    manageBuild();   
    storageStarted=false;
    huntGazelle();
    huntElephant();            // 打大象(集体风筝): 探明活象后加3个名额, 3人抱团拉扯
    if(gazelleState==4&&gazelleHunter2SN!=-1){
        for(auto&b:info.buildings){
            if(b.Type!=BUILDING_STOCK)continue;
            if(b.Percent>=100)continue;                                        // 只要在建的
            if(max(abs(b.BlockDR-gazelleSpotDR),abs(b.BlockUR-gazelleSpotUR))>6)continue;
            for(auto&f:info.farmers){
                if(f.SN!=gazelleHunter2SN)continue;
                
                if(f.WorkObjectSN==b.SN)continue;                              // 已经在建了, 别重发
                HumanAction(f.SN,b.SN);                                        // 去帮建
                break;
            }
            break;
        }
    }
    

    

    static int assignedFrame=-1;
    unordered_map<int,int>resWorkers;
    unordered_map<int,bool>isResourceSN;
    for(auto&r:info.resources){
        isResourceSN[r.SN]=true;
    }
    for(auto&f:info.farmers){
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
        if(info.GameFrame-assignedFrame<19)break;

        if(r.Type==RESOURCE_BUSH||(r.Type==RESOURCE_GAZELLE&&gazelleState==4)){
            if(r.Type==RESOURCE_GAZELLE&&r.Blood>0)continue; 
            if(r.Cnt<=0)continue;
            if(resWorkers[r.SN]>=resMax(r.Type))continue;
            int need=((r.Type==RESOURCE_BUSH)?BUILDING_GRANARY:BUILDING_STOCK);
            int needWood=((r.Type==RESOURCE_BUSH)?BUILD_GRANARY_WOOD:BUILD_STOCK_WOOD);
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE||farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                
                int bySN=-1;
                int st=checkEnv(r.Type,bySN);

                if(st==2){
                    HumanAction(f.SN,r.SN);//这里f.SN 后期要改为距离最近的村民 搞个函数替代一下
                    resIsgotten[r.SN]=true;
                    farIsgotten[f.SN]=true;
                    if(r.Type==RESOURCE_BUSH)bushNum++;
                    if(r.Type==RESOURCE_GAZELLE)gazelleNum++;
                    
                    
                }
                else if(st==1)HumanAction(f.SN,bySN);
                else{
                    int ox=-1,oy=-1;
                    if(info.Wood>=needWood&&!storageStarted&&findBuildSpot(r.BlockDR,r.BlockUR,3,2,5,ox,oy)){
                        HumanBuild(f.SN,need,ox,oy);
                        storageStarted=true;
                    }
                    else{
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
            if(isTree.count(f.WorkObjectSN)){
                woodNum++;
                woodNow++;
            }
        }
        struct TreeCan{
            int sn,dr,ur,dist;
        };
        vector<TreeCan>can;
        for(auto&r:info.resources){
            if(r.Type!=RESOURCE_TREE)continue;
            if(r.Cnt<=0)continue;
            if(resWorkers[r.SN]>=resMax(RESOURCE_TREE))continue;
            int bestD=1e18;
            for(auto&b:info.buildings){
                if(b.Type!=BUILDING_STOCK&&b.Type!=BUILDING_CENTER)continue;
                int dx=abs(b.BlockDR-r.BlockDR);
                int dy=abs(b.BlockUR-r.BlockUR);
                int d=max(dx,dy);
                
                
                if(d<bestD)bestD=d;
            }
            can.push_back(TreeCan{r.SN,r.BlockDR,r.BlockUR,bestD});
        }
        sort(can.begin(),can.end(),
            [](const TreeCan&a,const TreeCan&b){return a.dist<b.dist;});
        for(auto&t:can){
            if(woodNow>=3)break;//先预设3人
            int fSN=findFarmer(t.dr,t.ur);
            
            if(fSN==-1)break;

            int bySN=-1;
            int st=checkEnv(RESOURCE_TREE,bySN);

            if(st==2){
                HumanAction(fSN,t.sn);
                resIsgotten[t.sn]=true;
                
                woodNum++;
                woodNow++;
            }
            else if(st==1)HumanAction(fSN,bySN);
            else{
                int ox=-1,oy=-1;
                if(info.Wood>=BUILD_STOCK_WOOD&&!storageStarted&&findBuildSpot(t.dr,t.ur,3,2,10,ox,oy)){
                    HumanBuild(fSN,BUILDING_STOCK,ox,oy);
                    storageStarted=true;
                }
                else{
                    HumanAction(fSN,t.sn);
                    resIsgotten[t.sn]=true;
                    
                    woodNum++;
                    woodNow++;
                }
            }
            farIsgotten[fSN]=true;
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
    const int BAD_COOL=1500;             // 走不到的格子拉黑时长(帧)
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

    //---------- 3.2 到点回家: goHomeFrame 之后回箭塔底下待命 ----------
    // 只在"闲着"时才动(走路中上面已经 return 了); 到了箭塔旁边就待命, 不再探索/不再追瞪羚。
    if(info.GameFrame>=goHomeFrame&&arrowTowerBlockDR!=-1){
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
                if(d2<gD2){gD2=d2;gdr=r.BlockDR;gur=r.BlockUR;}
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

// 是否有某类建筑正在施工(避免同一类连建两个)
bool UsrAI::hasUnfinishedBuilding(int type){
    for(auto&b:info.buildings){
        if(b.Type==type&&b.Percent<100)return true;
    }
    return false;
}

// 在(bd,bu)附近找一个 size×size 的空地(返回左下角块坐标)
// 条件: 全是已探明陆地(Open, 说明没资源没建筑) + 高度一致且不是斜坡 + 没被拉黑
bool UsrAI::findBuildSpot(int bd,int bu,int size,int minR,int maxR,int &ox,int &oy){
    ox=-1;oy=-1;
    
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
            if(canbuild){
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
    if(haveNum<phaseNum+elephantExtra)BuildingAction(centerSN,BUILDING_CENTER_CREATEFARMER);   // +elephantExtra: 发现大象后多造3个人
    
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
    int fsDR[]={-3,-3,-3, 0, 0, 3, 3, 3, -4,-4,-4, 0, 0, 4, 4, 4};
    int fsDU[]={-3, 0, 3,-3, 3,-3, 0, 3, -4, 0, 4,-4, 4,-4, 0, 4};

    if(bushNum>=6&&gazelleNum>=6&&woodNum>=3){
        //有在建的
        int buildingSN=-1;
        for(auto&b:info.buildings){
            if(b.Percent>=100)continue;
            if(b.Type!=BUILDING_MARKET&&b.Type!=BUILDING_ARMYCAMP&&b.Type!=BUILDING_RANGE)continue;
            buildingSN=b.SN;
            break;
        }
        if(buildingSN!=-1){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                if(f.WorkObjectSN==buildingSN)continue;
                HumanAction(f.SN,buildingSN);
                farIsgotten[f.SN]=true;
                
            }
            return;
        }
        //无在建的
        
        int want=-1,cost=0;
        int bd=centerBlockDR,bu=centerBlockUR;          // 默认以市镇中心为基准找空地
        
        //市场
        if(!hasType(BUILDING_MARKET)){
            want=BUILDING_MARKET;
            cost=BUILD_MARKET_WOOD;
        }
        //兵营
        else if(!hasType(BUILDING_ARMYCAMP)){
            want=BUILDING_ARMYCAMP;
            cost=BUILD_ARMYCAMP_WOOD;
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
        
        //★1 木材不够 -> 空闲的人全部去砍树
        if(want!=-1&&info.Wood<cost){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                int tSN=-1,bestD=1e18;
                for(auto&r:info.resources){
                    if(r.Type!=RESOURCE_TREE||r.Cnt<=0)continue;
                    int d=max(abs(r.BlockDR-f.BlockDR),abs(r.BlockUR-f.BlockUR));
                    if(d<bestD){bestD=d;tSN=r.SN;}
                }
                if(tSN==-1)break;
                HumanAction(f.SN,tSN);
                farIsgotten[f.SN]=true;
            }
        }

        if(want!=-1&&info.Wood>=cost){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                int dx4[]{-3,0,3,0};
                int dy4[]{0,3,0,-3};
                for(int k=0;k<4;k++){
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
            if(b.Type==BUILDING_FARM)farmNum++;
        }
        if(farmNum<8){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN||f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;

                for(int q=0;q<16;q++){
                    int dr=granaryBlockDR+fsDR[q], du=granaryBlockUR+fsDU[q];
                    if(dr<0||du<0||dr+3>100||du+3>100)continue;
                    bool ok=true;
                    for(int i=0;i<3&&ok;i++)
                        for(int j=0;j<3;j++)
                            if(MAP[dr+i][du+j]!=Open){
                                ok=false;
                                break;
                            }
                    if(!ok)continue;
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
        bool busy=false;
        for(auto&f:info.farmers){
            if(f.WorkObjectSN==b.SN){
                busy=true;
                break;
            }
        }
        if(busy)continue;
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
    //====================================================================================
    // [AI修改] state 4 之后的"防卡解救"逻辑
    // 病因: 猎人杀完瞪羚时站在尸体堆里, 之后被派去建仓库(state 3)或帮建(processData 里
    //       gazelleState==4 那段)时, 目标地基的路容易被尸体/彼此挡住, 内核算不出路径,
    //       单位就一直停在 HUMAN_STATE_WALKING 却原地不动(内核不会把状态退回 IDLE)。
    //       而所有调度(通用资源循环/findFarmer/manageBuild/huntElephant)都只认
    //       HUMAN_STATE_IDLE, WALKING 的一律跳过 -> 没有任何代码能再把他们捞出来, 永久卡死。
    // 对策: 死盯这两个猎人, 一旦 WALKING 且位置连续 150 帧没动, 就强制改派:
    //       优先采身边最近的瞪羚尸体(采肉会清掉尸体, 路自然就通了),
    //       没有尸体可采就 HumanMove 回市中心; 人一重新动起来变 IDLE, 通用调度自动接走。
    // 150 是"判卡帧数": 嫌救得慢就调小, 怕误伤正常长途赶路的就调大。
    //====================================================================================
    if(gazelleState==4){
        static int lastDR[2]={-1,-1},lastUR[2]={-1,-1};   // 两个猎人上一帧的位置
        static int still[2]={0,0};                        // 两个猎人"位置没动"的连续帧数
        int hs[2]={gazelleHunter1SN,gazelleHunter2SN};
        for(int i=0;i<2;i++){
            if(hs[i]==-1)continue;
            for(auto&f:info.farmers){
                if(f.SN!=hs[i])continue;
                // 不在走路(闲着/在干活)就不算卡, 清零计数并记录当前位置
                if(f.NowState!=HUMAN_STATE_WALKING){still[i]=0;lastDR[i]=f.BlockDR;lastUR[i]=f.BlockUR;break;}
                if(f.BlockDR==lastDR[i]&&f.BlockUR==lastUR[i]){
                    // WALKING 但位置跟上一帧一样 -> 没挪窝
                    if(++still[i]>150){                   // 连续 150 帧没动 -> 判定卡死
                        int bestSN=-1,bestD=1e18;
                        for(auto&r:info.resources){        // 找最近的死瞪羚去采肉(顺便清出一条路)
                            if(r.Type!=RESOURCE_GAZELLE||r.Blood>0||r.Cnt<=0)continue;
                            int d=max(abs(r.BlockDR-f.BlockDR),abs(r.BlockUR-f.BlockUR));
                            if(d<bestD){bestD=d;bestSN=r.SN;}
                        }
                        if(bestSN!=-1)HumanAction(hs[i],bestSN);
                        else HumanMove(hs[i],centerBlockDR*BLOCKSIDELENGTH,centerBlockUR*BLOCKSIDELENGTH);
                        still[i]=0;
                    }
                }else{still[i]=0;}                        // 位置变了, 在正常赶路, 重新计数
                lastDR[i]=f.BlockDR;lastUR[i]=f.BlockUR;
                break;
            }
        }
        return;
    }
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
        }
        if(gazelleHunter1SN==-1)gazelleHunter1SN=findFarmer(gazelleSpotDR,gazelleSpotUR);
        if(gazelleHunter1SN==-1)return;
        farIsgotten[gazelleHunter1SN]=true;

        if(gazelleHunter2SN==-1)gazelleHunter2SN=findFarmer(gazelleSpotDR,gazelleSpotUR);
        if(gazelleHunter2SN==-1)return;
        farIsgotten[gazelleHunter2SN]=true;

        HumanMove(gazelleHunter1SN,gazelleSpotDR*BLOCKSIDELENGTH,gazelleSpotUR*BLOCKSIDELENGTH);
        HumanMove(gazelleHunter2SN,(gazelleSpotDR-1)*BLOCKSIDELENGTH,gazelleSpotUR*BLOCKSIDELENGTH);
        gazelleState=1;
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
            gazelleState=4;
            return;
        }
        int ox=-1,oy=-1;
        if(findBuildSpot(gazelleSpotDR,gazelleSpotUR,3,2,4,ox,oy)){
            
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
//==================== 打大象(集体风筝) ====================
// 跟 huntGazelle 一个骨架, 状态机 0~4; 唯一的新逻辑在 state 2 的集体风筝:
//   3人抱团, 象贴上来了(<=2格) 一起往背对方向撤5格, 拉开了(>=4格) 一起贴上去打,
//   中间 2~4 格是滞回区, 保持上一帧动作不发指令, 防止在边界来回抖。
void UsrAI::huntElephant(){
    // 人一旦入队就锁死, 别让通用资源调度把他们抢走(跟 huntGazelle 一个思路)
    if(elephantState>=1){
        for(int i=0;i<3;i++)
            if(elephantHunterSN[i]!=-1)farIsgotten[elephantHunterSN[i]]=true;
    }

    // ---------- 0: 地图上有活象 -> 加3个造人名额, 进集结 ----------
    if(elephantState==0){
        int esn=-1,edr=-1,eur=-1,bestD=1e18;
        for(auto&r:info.resources){                       // 找离市中心最近的活象
            if(r.Type!=RESOURCE_ELEPHANT||r.Blood<=0)continue;
            int d=max(abs(r.BlockDR-centerBlockDR),abs(r.BlockUR-centerBlockUR));
            if(d<bestD){bestD=d;esn=r.SN;edr=r.BlockDR;eur=r.BlockUR;}
        }
        if(esn==-1)return;                                // 还没探明活象, 继续等
        elephantTargetSN=esn;
        // [AI修复] 集合点不能直接用"象所在的那一格"!
        //   象在 MAP 里被标成资源格(非 Open, 不可走), 拿它当集合点等于永远走不到:
        //   farmerAt 永远为 false -> 每帧重发移动指令 -> 单位被钉死 + 日志刷屏。
        //   照 huntGazelle 的做法, 取象四邻的一个可走格当集合点。
        int sdr=edr,sur=eur;
        int dx4[4]={0,1,0,-1};
        int dy4[4]={1,0,-1,0};
        for(int k=0;k<4;k++){
            int nr=edr+dx4[k], nu=eur+dy4[k];
            if(nr<0||nr>=100||nu<0||nu>=100)continue;
            if(MAP[nr][nu]!=Open)continue;
            sdr=nr;sur=nu;
            break;
        }
        elephantSpotDR=sdr;
        elephantSpotUR=sur;
        if(elephantExtra==0)elephantExtra=3;              // 名额只加一次, 打完一圈回来不再加
        elephantState=1;
        return;
    }

    // ---------- 1: 凑3个空闲农民, 派去象旁边集合 ----------
    if(elephantState==1){
        bool alive=false;                                 // 目标象还在不在(死了就重新找)
        for(auto&r:info.resources){
            if(r.SN==elephantTargetSN&&r.Blood>0){alive=true;break;}
        }
        if(!alive){elephantState=0;elephantTargetSN=-1;return;}
        for(int i=0;i<3;i++){                             // 猎人死了就清掉, 下面重新挑
            if(elephantHunterSN[i]==-1)continue;
            bool a2=false;
            for(auto&f:info.farmers)if(f.SN==elephantHunterSN[i]){a2=true;break;}
            if(!a2)elephantHunterSN[i]=-1;
        }
        for(int i=0;i<3;i++){
            if(elephantHunterSN[i]!=-1)continue;
            int sn=findFarmer(elephantSpotDR,elephantSpotUR);   // 找最近的空闲农民
            if(sn==-1)return;                             // 还没凑够3个, 等下一帧(新农民在生产)
            elephantHunterSN[i]=sn;
            farIsgotten[sn]=true;                         // 立刻标记, 否则同一帧会挑到同一个人
        }
        bool allAt=true;                                  // 3人都到象旁边了吗
        for(int i=0;i<3;i++)
            if(!farmerAt(elephantHunterSN[i],elephantSpotDR,elephantSpotUR)){allAt=false;break;}
        if(allAt){elephantState=2;elephantKiteCmd=-1;return;}
        // [AI修复] 集合移动绝对不能每帧重发!
        //   每帧重发 HumanMove, 内核会反复 suspendRelation 把路径清掉, 单位被钉在原地,
        //   状态却一直停在 WALKING -> 既走不动、也没人敢接手(调度只认 IDLE), 还每帧刷屏。
        //   规则: 同一个人、同一个集合点, MOVE_TIMEOUT 帧内只发一次; 集合点变了才立刻重发。
        static int moveFrame[3]={-1,-1,-1};
        static int moveDR[3]={-1,-1,-1},moveUR[3]={-1,-1,-1};
        const int MOVE_TIMEOUT=300;
        for(int i=0;i<3;i++){
            if(elephantHunterSN[i]==-1){moveFrame[i]=-1;moveDR[i]=-1;moveUR[i]=-1;continue;}
            if(farmerAt(elephantHunterSN[i],elephantSpotDR,elephantSpotUR)){      // 到位了 -> 清计时
                moveFrame[i]=-1;moveDR[i]=-1;moveUR[i]=-1;
                continue;
            }
            if(moveDR[i]!=elephantSpotDR||moveUR[i]!=elephantSpotUR){              // 集合点变了 -> 允许立即重发
                moveDR[i]=elephantSpotDR;moveUR[i]=elephantSpotUR;moveFrame[i]=-1;
            }
            if(moveFrame[i]!=-1&&info.GameFrame-moveFrame[i]<MOVE_TIMEOUT)continue; // 刚发过, 别打断他赶路
            HumanMove(elephantHunterSN[i],elephantSpotDR*BLOCKSIDELENGTH,elephantSpotUR*BLOCKSIDELENGTH);
            moveFrame[i]=info.GameFrame;
        }
        return;
    }

    // ---------- 2: 3人抱团集体风筝 ----------
    if(elephantState==2){
        bool alive=false;
        for(auto&r:info.resources){
            if(r.SN==elephantTargetSN&&r.Blood>0){alive=true;break;}
        }
        if(!alive){elephantState=3;elephantKiteCmd=-1;return;}  // 象死了 -> 去建仓库

        int edr=-1,eur=-1;                                // 象的实时位置
        for(auto&r:info.resources){
            if(r.SN==elephantTargetSN){edr=r.BlockDR;eur=r.BlockUR;break;}
        }
        if(edr==-1){elephantState=3;return;}

        int cx=0,cy=0,cnt=0;                              // 3人站位中心
        for(auto&f:info.farmers){
            for(int i=0;i<3;i++)
                if(f.SN==elephantHunterSN[i]){cx+=f.BlockDR;cy+=f.BlockUR;cnt++;}
        }
        if(cnt==0){elephantState=0;elephantTargetSN=-1;return;}  // 人全死了, 重来
        cx/=cnt;cy/=cnt;

        int dx=cx-edr, dy=cy-eur;                         // 象 -> 人中心 的向量
        int d=max(abs(dx),abs(dy));

        int cmd=0;                                        // 1-打 2-撤 0-保持(滞回区)
        if(d<=2)cmd=2;                                    // 象贴上来了 -> 撤
        else if(d>=4)cmd=1;                               // 拉开了 -> 打
        if(cmd==0||cmd==elephantKiteCmd)return;           // 滞回区 或 动作没变 -> 不重发指令

        if(cmd==2){
            // 背对方向撤5格; 3人同一个目标点, 队形不散("尽量站在一块")
            int stepX=(dx>0?5:(dx<0?-5:0));
            int stepY=(dy>0?5:(dy<0?-5:0));
            int tx=cx+stepX, ty=cy+stepY;
            if(tx<1)tx=1; if(tx>98)tx=98;
            if(ty<1)ty=1; if(ty>98)ty=98;
            for(int i=0;i<3;i++)
                HumanMove(elephantHunterSN[i],tx*BLOCKSIDELENGTH,ty*BLOCKSIDELENGTH);
        }else{
            // 拉开了, 3人一起贴上去打
            for(int i=0;i<3;i++)
                HumanAction(elephantHunterSN[i],elephantTargetSN);
        }
        elephantKiteCmd=cmd;
        return;
    }

    // ---------- 3: 就地建仓库 ----------
    if(elephantState==3){
        int byBuildingSN=-1;
        if(checkEnv(RESOURCE_ELEPHANT,byBuildingSN)==2){elephantState=4;return;}  // 附近已有仓库, 直接采
        if(info.Wood>=BUILD_STOCK_WOOD){
            int ox=-1,oy=-1;
            if(findBuildSpot(elephantSpotDR,elephantSpotUR,3,2,4,ox,oy))
                HumanBuild(elephantHunterSN[0],BUILDING_STOCK,ox,oy);   // 1号猎人建
        }
        elephantState=4;
        return;
    }

    // ---------- 4: 帮建仓库 / 采肉, 采完自动找下一头 ----------
    if(elephantState==4){
        for(auto&b:info.buildings){                       // 有在建仓库 -> 3人帮建
            if(b.Type!=BUILDING_STOCK||b.Percent>=100)continue;
            if(max(abs(b.BlockDR-elephantSpotDR),abs(b.BlockUR-elephantSpotUR))>6)continue;
            for(int i=0;i<3;i++)
                for(auto&f:info.farmers){
                    if(f.SN==elephantHunterSN[i]&&f.WorkObjectSN!=b.SN)
                        HumanAction(f.SN,b.SN);
                }
            return;
        }
        bool found=false;                                 // 采附近的死象
        for(auto&r:info.resources){
            if(r.Type!=RESOURCE_ELEPHANT||r.Blood>0||r.Cnt<=0)continue;
            if(max(abs(r.BlockDR-elephantSpotDR),abs(r.BlockUR-elephantSpotUR))>10)continue;
            for(int i=0;i<3;i++)
                for(auto&f:info.farmers){
                    if(f.SN==elephantHunterSN[i]&&f.WorkObjectSN!=r.SN)
                        HumanAction(f.SN,r.SN);
                }
            found=true;
            break;
        }
        if(!found){                                       // 这片采完了 -> 找下一头活象继续
            elephantState=0;
            elephantTargetSN=-1;
            elephantKiteCmd=-1;                           // 3个猎人保留, 继续用, 不换人
        }
    }
}

void UsrAI::waveBattle(){
    if(info.enemy_armies.empty())return;

    //====================================================================================
    // [AI修改] 祭司/箭塔"目标锁定"机制
    // 病因: info.enemy_armies 每帧被引擎打乱顺序, 原来"取列表第1/第2个近战"会让
    //       priestTarget / towerPick 每帧都变 -> 祭司和箭塔每帧换目标 ->
    //       箭塔没把任何一个近战"持续"打 -> 两个近战都按 enemyai 的优先级跑去打祭司。
    // 敌方索敌(enemyai.cpp FindWaveTargetByPriority, 第1137行)写死的优先级:
    //       谁在打我 > 玩家祭司 > 玩家农民
    //   所以: 祭司转化近战A, A必然反击祭司(改不了, 那本来就是转化目标);
    //         箭塔只要"持续"输出近战B, B才会反击箭塔(把仇恨从祭司身上抢走)。
    //====================================================================================
    static int priestLock=-1;   // [AI修改] 祭司锁定的近战(转化目标), 死了/没了才换
    static int towerLock=-1;    // [AI修改] 箭塔锁定的近战(持续输出目标), 死了/跑出射程才换

    // 箭塔位置和它当前的攻击目标
    int towerDR=-1,towerUR=-1,towerProject=-1;
    for(auto&b:info.buildings){
        if(b.SN!=arrowTowerSN)continue;
        towerDR=b.BlockDR;towerUR=b.BlockUR;towerProject=b.Project;
        break;
    }
    // 这个近战还活着吗
    auto aliveMelee=[&](int sn)->bool{
        if(sn==-1)return false;
        for(auto&e:info.enemy_armies)
            if(e.SN==sn&&isMeleeSort(e.Sort))return true;
        return false;
    };
    // 离(x,y)最近的近战, 可排除一个 SN(避免祭司和箭塔抢同一个)
    auto nearestMelee=[&](int x,int y,int exclude)->int{
        int best=-1,bestD=1e18;
        for(auto&e:info.enemy_armies){
            if(!isMeleeSort(e.Sort)||e.SN==exclude)continue;
            int d=max(abs(e.BlockDR-x),abs(e.BlockUR-y));
            if(d<bestD){bestD=d;best=e.SN;}
        }
        return best;
    };

    // ---- [AI修改] 祭司: 回家后才转化; 锁住一个近战, 不每帧换 ----
    bool priestAtHome=(arrowTowerBlockDR!=-1)
        &&abs(priestBlockDR-arrowTowerBlockDR)<=2
        &&abs(priestBlockUR-arrowTowerBlockUR)<=2;
    if(priestSN!=-1&&info.GameFrame>=goHomeFrame&&priestAtHome){
        if(!aliveMelee(priestLock)||priestLock==towerLock)
            priestLock=nearestMelee(priestBlockDR,priestBlockUR,towerLock);   // 重新锁, 别跟箭塔抢同一个
        if(priestLock!=-1){
            for(auto&a:info.armies){
                if(a.SN!=priestSN)continue;
                if(a.ConvertCooldown<=0&&a.WorkObjectSN!=priestLock)HumanAction(priestSN,priestLock);
                break;
            }
        }
    }

    // ---- [AI修改] 箭塔: 锁一个近战"持续"打, 把它的仇恨从祭司身上抢走 ----
    if(arrowTowerSN!=-1&&towerDR!=-1){
        const int r2=DIS_ARROWTOWER*DIS_ARROWTOWER;         // 射程平方(7格)
        auto inRange=[&](int sn)->bool{                     // 这个近战还在箭塔射程内吗
            for(auto&e:info.enemy_armies){
                if(e.SN!=sn)continue;
                int dx=e.BlockDR-towerDR, dy=e.BlockUR-towerUR;
                return dx*dx+dy*dy<=r2;
            }
            return false;
        };
        if(!aliveMelee(towerLock)||towerLock==priestLock||!inRange(towerLock))
            towerLock=nearestMelee(towerDR,towerUR,priestLock);               // 重新锁, 别跟祭司抢同一个
        if(towerLock!=-1&&inRange(towerLock)&&towerProject!=towerLock)
            HumanAction(arrowTowerSN,towerLock);
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
        if(r.Cnt<=0);
        for(auto&b:info.buildings){
            if(b.Type!=need&&b.Type!=BUILDING_CENTER)continue;
            int d=max(abs(b.BlockDR-r.BlockDR),abs(b.BlockUR-r.BlockUR));
            if(d>radius)continue;
            if(b.Percent>=100)return 2;//建好
            if(byBuildingSN==-1)byBuildingSN=b.SN;
        }
    }
    return (byBuildingSN!=-1)?1:0; //不等于-1说明周围有
    
}