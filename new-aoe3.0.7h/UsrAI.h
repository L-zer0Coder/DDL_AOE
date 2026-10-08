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
    void Resort();
    void getPriest();
    void gethomeBuilder();
    void priestExplore();
    
    void manageBuild();
    int checkEnv(int type,int& byBuildingSN);
    bool bushDone(int fSN);          // [AI] 花名册成员是否"浆果已采完且空闲"
    bool canFarm(int fSN);           // [AI] 这个农民现在能不能被派去农田(三阶段规则)
    bool spotBusy(int dr,int ur,int size);
    bool findBuildSpot(int bd,int bu,int size,int minR,int maxR,int &ox,int &oy);
    
    int assignWoodcutter();//返回派出去的村民SN
    int assignGoldMiner();   // 派一个空闲村民去挖金(优先有仓库的金矿), 返回SN

    void waveBattle();
    void counterAttack();  // 反攻: 护送祭司转化敌方攻城武器厂
    void huntGazelle();
    void trainArmy();      // 铜器后造兵: 靶场出弓箭手, 兵营出阔剑兵, 造完集合到箭塔下
    int findFarmer(int bd,int bu);
    int findGazelle(int bd,int bu,int& gazelleDR,int& gazelleUR,int maxR=1e18);


    bool haveBuilding(int type);
    void centerUpgrade();

    void CalmAndCrazy();

    
    void manageScout();            // 斥候: 找敌 / 贴身勾引 / 带回集结区
    void armyAttack();             // 部队: 退/打/进 (贴脸退, 射程内打, 其余朝行军点走)
    void attackTowers();           // 厂区: 分头拆塔, 塔没了就当肉盾
    void priestConvert();         // 祭司转化: 优先转"当前生命值最高"的敌人
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
