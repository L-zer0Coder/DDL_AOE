#ifndef USRAI_H
#define USRAI_H

#include "ai.h"
#include <unordered_map>

extern tagGame tagUsrGame;
extern ins UsrIns;
/*##########DO NOT MODIFY THE CODE ABOVE##########*/

class UsrAI:public AI
{
public:
    UsrAI(){this->id=0;}
    ~UsrAI(){}
    void getBaseInfo();
    void betterMap();
    void getPriest();
    void gethomeBuilder();
    void priestExplore();
    void checkWorkState();
    void manageBuild();
    int checkEnv(int type,int& byBuildingSN);
    bool findBuildSpot(int bd,int bu,int size,int minR,int maxR,int &ox,int &oy);
    
    int assignWoodcutter();//返回派出去的村民SN
    int assignGoldMiner();   // 派一个空闲村民去挖金(优先有仓库的金矿), 返回SN
    bool spotBusy(int dr,int ur,int size);

    void waveBattle();
    bool findBuildSpot4(int anchorDR,int anchorUR,int &ox,int &oy);
    bool findFarmSlot(int &ox,int &oy);
    void counterAttack();  // 反攻: 护送祭司转化敌方攻城武器厂
    bool hasUnfinishedBuilding(int type);
    void huntGazelle();
    void trainArmy();      // 铜器后造兵: 靶场出弓箭手, 兵营出阔剑兵, 造完集合到箭塔下
    int findFarmer(int bd,int bu);
    int findGazelle(int bd,int bu,int& gazelleDR,int& gazelleUR,int maxR=1e18);


    bool haveBuilding(int type);
    void centerUpgrade();
    void GOGOGO(int dr,int ur);
private:
    void processData() override;
    tagInfo getInfo(){return tagUsrGame.getInfo();}
    int AddToIns(instruction ins) override
    {
        UsrIns.lock.lock();
        ins.id=UsrIns.g_id;
        UsrIns.g_id++;
        UsrIns.instructions.push(ins);
        UsrIns.lock.unlock();
        return ins.id;
    }
    void clearInsRet() override
    {
        tagUsrGame.clearInsRet();
    }
    /*##########DO NOT MODIFY THE CODE IN THE CLASS##########*/



};

/*##########YOUR CODE BEGINS HERE##########*/




/*##########YOUR CODE ENDS HERE##########*/
#endif // USRAI_H
