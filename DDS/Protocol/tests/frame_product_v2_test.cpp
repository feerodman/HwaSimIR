#include "../FrameProductV2.h"
#include <cassert>
#include <iostream>
int main() {
    HwaFrameV2::Product p;p.session="test-session";p.channel="precise";p.generation=2;p.run=3;
    p.frameSeq=91;p.sourceSeq=44;p.ptsMs=1500;p.annotation="{\"targets\":[]}";
    p.realtime.platLoc.lat=40.25;p.realtime.targetState[4].targetID=77;
    p.realtime.weaponState.offsetAng[1]=-3.25;
    const auto raw=HwaFrameV2::serialize(p);
    // Independent golden prefix: version 2, LE string length 12, ASCII session.
    const unsigned char golden[]={2,0,0,0,12,0,0,0,'t','e','s','t','-','s','e','s','s','i','o','n'};
    assert(std::memcmp(raw.data(),golden,sizeof(golden))==0);
    std::vector<std::uint8_t> au={0,0,0,1,9,0xf0,0,0,0,1,0x67,0x64,0x80,0,0,1,0x65,0x88,0x80};
    HwaFrameV2::insert(au,p);HwaFrameV2::Product q;
    assert(HwaFrameV2::extract(au.data(),au.size(),q));
    assert(q.frameSeq==91&&q.sourceSeq==44&&q.generation==2&&q.run==3&&q.ptsMs==1500);
    assert(q.realtime.platLoc.lat==40.25&&q.realtime.targetState[4].targetID==77);
    assert(q.realtime.weaponState.offsetAng[1]==-3.25&&q.annotation==p.annotation);
    for(std::size_t n=0;n<raw.size();++n)assert(!HwaFrameV2::deserialize(raw.data(),n,q));
    HwaFrameV2::insert(au,p);assert(!HwaFrameV2::extract(au.data(),au.size(),q));
    std::vector<std::uint8_t> legacy={0,0,1,0x65,0x80};assert(!HwaFrameV2::extract(legacy.data(),legacy.size(),q));
    p.annotation=std::string(8192,'\0');p.annotation[256]=1;p.annotation[4096]=3;
    au=legacy;HwaFrameV2::insert(au,p);assert(HwaFrameV2::extract(au.data(),au.size(),q));
    assert(q.annotation==p.annotation);
    std::cout<<"FrameProductV2 PASS golden, identity, snapshot, SEI escaping, truncation, duplicate, legacy\n";
}
