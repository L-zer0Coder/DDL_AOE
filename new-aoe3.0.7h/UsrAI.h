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
    
    void manageBuild();
    int checkEnv(int type,int& byBuildingSN);
    bool findBuildSpot(int bd,int bu,int size,int minR,int maxR,int &ox,int &oy);
    
    int assignWoodcutter();//返回派出去的村民SN
    int reassignWoodcutter(int fSN); // [AI] 给指定的闲下来的伐木工换一棵树(只挑树不挑人), 返回树SN, -1=没树
    int assignGoldMiner();   // 派一个空闲村民去挖金(优先有仓库的金矿), 返回SN
    bool buildGoldStock();   // [AI] 采金前先在旁边建仓库(仿 huntGazelle), 派出去过指令返回true
    bool spotBusy(int dr,int ur,int size);

    void waveBattle();
    void counterAttack();  // 反攻: 护送祭司转化敌方攻城武器厂
    bool hasUnfinishedBuilding(int type);
    void huntGazelle();
    void trainArmy();      // 铜器后造兵: 靶场出弓箭手, 兵营出阔剑兵, 造完集合到箭塔下
    int findFarmer(int bd,int bu);
    int findGazelle(int bd,int bu,int& gazelleDR,int& gazelleUR,int maxR=1e18);


    bool haveBuilding(int type);
    void centerUpgrade();
    void pickRallyPoint();   // 定集结点(从敌人位置朝自家退几格)
    bool rallyArmy();        // 铺开集结: 每帧最多派一个兵, 已分格的不重发
    void rallyPriest();      // 祭司去集结区里"离敌最远"的那一格
    bool rallyEnough();      // 集结完毕判据(到位兵数 >= RALLY_NEED)
    // ---- 反攻四函数(取自 v3.2) ----
    void manageScout(int mode);// 斥候专职拉扯: 0/1/2 推进找厂, 看见敌人撤回, 3 打敌箭塔
    void priestFollow();       // 祭司始终跟在大军身边(5 格内)
    void priestConvert();      // 祭司转化视野内血最厚的敌人
    void armyAttack();         // 大军推进: 优先打远程兵护祭司

    void CalmAndCrazy();
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
