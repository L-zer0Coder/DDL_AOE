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

    for(auto&f:info.farmers){
        if(homeBuilderSN==-1&&f.NowState==HUMAN_STATE_IDLE){
            homeBuilderSN=f.SN;
            gethomeBuilder();
        }
    }

    
    
    manageBuild();   
    
    huntGazelle();
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
    

    storageStarted=false;

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
        

        if(want!=-1&&info.Wood>=cost){
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE)continue;
                if(farIsgotten.find(f.SN)!=farIsgotten.end())continue;

                int ox=-1,oy=-1;
                if(findBuildSpot(bd,bu,3,3,6,ox,oy)){
                    HumanBuild(f.SN,want,ox,oy);
                    farIsgotten[f.SN]=true;             // 本帧别再被采集循环抢走
                    if(want==BUILDING_ARMYCAMP){        // 记住兵营位置给靶场用
                        armyCampBlockDR=ox;
                        armyCampBlockUR=oy;
                    }
                }
                break;
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