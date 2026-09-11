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
                    int d=max(abs(dr-50),abs(ur-50));
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
