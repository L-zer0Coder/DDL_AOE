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



int dx_build[]={-3,-3,-3,0,3,3,3,0};
int dy_build[]={-3,0,3,3,3,0,-3,-3};

int homeBuilderSN=-1;
bool builderGotten=false;

int cnt=0;
int cnt_build=0;
bool stockOnce=false;

const int goHomeFrame=5250;


int id=-1;
void UsrAI::processData()
{   
    
    farIsgotten.clear();
    info=getInfo();
    if(!getOnlyOnce)getBaseInfo();
    BuildingAction(centerSN,BUILDING_CENTER_CREATEFARMER);
    betterMap();
    // for(int dr=0;dr<100;dr++){
    //     for(int ur=0;ur<100;ur++){
    //         if(MAP[dr][ur]==Unknown)HumanMove(priestSN,dr*BLOCKSIDELENGTH,ur*BLOCKSIDELENGTH);
    //         break;
    //     }
    // }
    //HumanMove(priestSN,1300,1300);
    priestExplore();


    for(auto&f:info.farmers){
        if(f.NowState==HUMAN_STATE_IDLE&&farIsgotten.find(f.SN)==farIsgotten.end()&&homeBuilderSN==-1){
            homeBuilderSN=f.SN;
        }
    }
    int maxNum=info.Human_MaxNum;
    double haveNum=info.Human_Num;
    int spaceNum=maxNum-haveNum;
    if(spaceNum<=3){
        int dr=homeBlockDR+dx_build[cnt_build%8];
        int ur=homeBlockUR+dy_build[cnt_build%8];
        bool undo=false;
        for(auto&b:info.buildings){
            if(b.Type==BUILDING_HOME&&b.Percent<100)undo=true;
            
        }
        if(!undo)id=HumanBuild(homeBuilderSN,BUILDING_HOME,dr,ur);
        if(MAP[dr][ur]!=Open||info.ins_ret[id])cnt++;
        if(!info.ins_ret[id]){
            homeBlockDR=dr;
            homeBlockUR=ur;
        }
    }
    for(auto&b:info.buildings){
        if(b.Type==BUILDING_HOME){
            if(b.Percent<100){
                HumanAction(homeBuilderSN,b.SN);
            }
        }
    }
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_BUSH||r.Type==RESOURCE_GAZELLE){
            if(resIsgotten.find(r.SN)!=resIsgotten.end())continue;
            for(auto&f:info.farmers){
                if(f.SN==homeBuilderSN)continue;
                if(f.NowState!=HUMAN_STATE_IDLE||farIsgotten.find(f.SN)!=farIsgotten.end())continue;
                HumanAction(f.SN,r.SN);
                resIsgotten[r.SN]=true;
                farIsgotten[f.SN]=true;
                break;
            }   
        }
    }
    
    // for(auto&r:info.resources){
    //     if(r.Type==RESOURCE_GAZELLE){
    //         for(auto&f:info.farmers){
    //             if(f.NowState!=HUMAN_STATE_IDLE||farIsgotten.find(f.SN)!=farIsgotten.end())continue;
    //             HumanBuild(f.SN,BUILDING_STOCK,gazelleBlockDR+3,gazelleBlockUR+3);
                
    //         }   
    //     }
    // }
    

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
    for(auto&r:info.resources){
        if(r.Type==RESOURCE_GAZELLE){
            gazelleSN=r.SN;
            gazelleBlockDR=r.BlockDR;
            gazelleBlockUR=r.BlockUR;
            break;
        }
    }
    if(gazelleSN!=-1){
        for(auto&f:info.farmers){
            HumanBuild(f.SN,BUILDING_STOCK,gazelleBlockDR-3,gazelleBlockUR-3);
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



void UsrAI::priestExplore(){
    
    static int startFrame=-1;
    getPriest();
    
    if(priestState!=HUMAN_STATE_IDLE)return;
    if(startFrame!=-1&&info.GameFrame-startFrame<50)return;
    int dirDR=(priestBlockDR>50?-1:1);
    int dirUR=(priestBlockUR>50?-1:1);

    for(int i=10;i>=0;i--){
        int desDR=priestBlockDR+dirDR*i;
        int desUR=priestBlockUR+dirUR*i;
        if(desDR<0||desDR>=100||desUR<0||desUR>=100)continue;
        if(MAP[desDR][desUR]==RESOURCE_TREE+100||MAP[desDR][desUR]==Ocean){
            for(int i=5;i<50;i++){
                for(int j=5;j<50;j++){
                    int dx[]={-15,-15,-15,0,15,15,15,0};
                    int dy[]={-15,0,15,15,15,0,-15,-15};
                    for(int k1=0;k1<8;k1++){
                        for(int k2=0;k2<8;k2++){
                            if(MAP[50+dx[k1]][50+dy[k2]]==Open){
                                
                                HumanMove(priestSN,(50+dx[k1])*BLOCKSIDELENGTH,(50+dy[k2])*BLOCKSIDELENGTH);
                                startFrame=info.GameFrame;
                                return;
                            }
                        }
                    }
                    
                }
            }
            
            
            
        }
        if(MAP[desDR][desUR]!=Open)continue;
        HumanMove(priestSN,desDR*BLOCKSIDELENGTH,desUR*BLOCKSIDELENGTH);
        startFrame=info.GameFrame;
        break;
    }
}