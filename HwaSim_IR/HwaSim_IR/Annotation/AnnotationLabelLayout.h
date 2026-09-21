#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

// Presentation-only rectangles measured by the actual TextNode font. No change
// to AnnotationFrameRecord, projected geometry, identities or saved coordinates.
struct AnnotationLabelBox { float x=0,y=0,w=0,h=0; };
struct AnnotationLabelRequest {
    std::string key,text; float anchorX=0,anchorY=0,w=0,h=0; bool keyPoint=false;
};
struct AnnotationLabelPlacement {
    AnnotationLabelBox box; int candidate=-1; bool overflow=false;
};
class AnnotationLabelLayout {
    struct Prior {AnnotationLabelBox box;float anchorX=0,anchorY=0;};
    std::map<std::string,Prior> previous;
public:
    static bool overlap(const AnnotationLabelBox& a,const AnnotationLabelBox& b){
        return a.x<b.x+b.w+2 && a.x+a.w+2>b.x && a.y<b.y+b.h+2 && a.y+a.h+2>b.y;
    }
    void reset(){previous.clear();}
    std::vector<AnnotationLabelPlacement> place(const std::vector<AnnotationLabelRequest>& requests,int width,int height){
        std::vector<AnnotationLabelPlacement> out(requests.size());std::vector<size_t> order;
        for(size_t i=0;i<requests.size();++i)order.push_back(i);
        std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return requests[a].key<requests[b].key;});
        std::vector<AnnotationLabelBox> occupied;std::map<std::string,Prior> next;
        for(auto i:order){
            const auto& r=requests[i];std::vector<AnnotationLabelBox> candidates;
            auto add=[&](float x,float y){AnnotationLabelBox b;
                b.x=std::max(2.f,std::min(float(width)-r.w-2,x));b.y=std::max(2.f,std::min(float(height)-r.h-2,y));
                b.w=r.w;b.h=r.h;candidates.push_back(b);};
            auto old=previous.find(r.key);float preferredX=r.anchorX,preferredY=r.anchorY;
            if(old!=previous.end()){
                preferredX=old->second.box.x+r.anchorX-old->second.anchorX;
                preferredY=old->second.box.y+r.anchorY-old->second.anchorY;
                add(preferredX,preferredY);
                // Local alternatives absorb glyph-width changes without sending
                // an edge label to the opposite explanation column.
                for(int radius=4;radius<=32;radius+=4)
                    for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)
                        if(x||y)add(preferredX+x*radius,preferredY+y*radius);
            }
            // Stable small columns on both sides, followed by top/bottom.
            for(int row=0;row<12;++row){float dy=(row%2?1.f:-1.f)*float((row+1)/2)*(r.h+4);
                add(r.anchorX+9,r.anchorY-r.h-6+dy);add(r.anchorX-r.w-9,r.anchorY-r.h-6+dy);}
            add(r.anchorX-r.w*.5f,r.anchorY-r.h-10);add(r.anchorX-r.w*.5f,r.anchorY+10);
            // Fixed edge explanation columns if the neighbourhood is full.
            for(float y=3;y+r.h<float(height)-2;y+=r.h+4){add(3,y);add(float(width)-r.w-3,y);}
            // Additional fixed columns for adversarial coincident multi-object labels.
            for(float x=3;x+r.w<float(width)-2;x+=r.w+4)
                for(float y=3;y+r.h<float(height)-2;y+=r.h+4)add(x,y);
            auto fits=[&](int n){const auto& b=candidates[n];
                if(b.x+b.w>width-1||b.y+b.h>height-1)return false;
                for(const auto& p:occupied)if(overlap(b,p))return false;return true;};
            int chosen=-1;float best=1.e30f;
            for(size_t n=0;n<candidates.size();++n)if(fits(int(n))){
                if(old==previous.end()){chosen=int(n);break;}
                const auto& b=candidates[n];const float dx=b.x-preferredX,dy=b.y-preferredY,score=dx*dx+dy*dy;
                if(score<best){chosen=int(n);best=score;}
                if(score==0)break;
            }
            // Impossible packing is explicit; keep all required labels, never truncate.
            if(chosen<0){chosen=0;out[i].overflow=true;}
            out[i].box=candidates[chosen];out[i].candidate=chosen;occupied.push_back(out[i].box);
            Prior p;p.box=out[i].box;p.anchorX=r.anchorX;p.anchorY=r.anchorY;next[r.key]=p;
        }
        previous.swap(next);return out;
    }
};
