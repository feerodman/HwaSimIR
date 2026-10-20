// 验证生产侧面筛选：被排除球不得与视口内前向射线相交。
#include "../HwaSim_IR/HwaSim_IR/IR/IRCloudPrefixCulling.h"
#include "perspectiveLens.h"
#include <random>
#include <iostream>
int main(){
    std::mt19937 random(20261018);std::uniform_real_distribution<float> unit(-1,1);
    int spheres=0,rejected=0,raysChecked=0,falseNegatives=0;
    for(float fov:{.1f,2.f,20.f,80.f,150.f}){
        PT(PerspectiveLens) lens=new PerspectiveLens();lens->set_fov(fov);lens->set_near_far(.1,10000);
        std::array<LVector3f,4> corners;const LPoint2f screen[]={LPoint2f(-1,-1),LPoint2f(1,-1),LPoint2f(1,1),LPoint2f(-1,1)};
        for(int i=0;i<4;++i){LPoint3f n,f;lens->extrude(screen[i],n,f);corners[i]=f;}
        const auto planes=IRPrefixViewPlanes(corners);
        for(int i=0;i<2000;++i){
            LPoint3f center(unit(random)*1000,unit(random)*1000,unit(random)*1000);float radius=(unit(random)+1)*100;
            ++spheres;
            if(IRPrefixSphereIntersectsView(center,radius,planes))continue;
            ++rejected;
            for(int j=0;j<256;++j){
                LPoint3f n,f;lens->extrude(LPoint2f(unit(random),unit(random)),n,f);LVector3f d=f;d.normalize();
                float forward=std::max(0.f,d.dot(center));++raysChecked;
                if((d*forward-center).length_squared()<radius*radius)++falseNegatives;
            }
        }
        // 入云和侧面边界不可被筛掉；不用near/far约束前缀。
        for(const auto& plane:planes){
            if(!IRPrefixSphereIntersectsView(LPoint3f(-plane*10),10,planes))++falseNegatives;
        }
        if(!IRPrefixSphereIntersectsView(LPoint3f(0),1,planes))++falseNegatives;
    }
    std::cout<<"{\"spheres\":"<<spheres<<",\"rejected\":"<<rejected<<",\"raysChecked\":"<<raysChecked<<",\"falseNegatives\":"<<falseNegatives<<",\"result\":\""<<(falseNegatives?"FAIL":"PASS")<<"\"}\n";
    return falseNegatives?2:0;
}
