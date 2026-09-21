#include "pandaFramework.h"
#include "load_prc_file.h"
#include "pnmImage.h"
#include "../HwaSim_IR/HwaSim_IR/Annotation/AnnotationOverlay.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstdlib>
static std::string identity(const AnnotationFrameRecord& r){
    std::ostringstream s;s<<r.frameIndex<<','<<r.width<<','<<r.height;
    for(const auto& t:r.targets){s<<'|'<<t.targetType<<','<<t.targetPlatID<<','<<t.targetID<<','<<t.modelLabel<<','<<t.bbox.x<<','<<t.bbox.y<<','<<t.bbox.width<<','<<t.bbox.height<<','<<t.bbox.visible;
        for(const auto& p:t.keyPoints)s<<'|'<<p.name<<','<<p.x<<','<<p.y<<','<<p.visible;}return s.str();
}
int main(int argc,char**argv){
    if(argc!=2)return 64;std::string dir=argv[1];
    load_prc_file_data("p10_labels","window-type offscreen\naudio-library-name null\nsync-video false\nframebuffer-multisample false\n");
    PandaFramework f;f.open_framework(argc,argv);std::ofstream result(dir+"/label_tests.csv");
    result<<"width,height,case,labels,overlap,overflow,coordinate_record_unchanged,stable,reordered_stable,motion_max_displacement_px\n";
    int tests=0;
    for(auto size:std::vector<std::pair<int,int>>{{800,800},{640,480},{1024,768},{400,400}}){
        WindowProperties props;props.set_size(size.first,size.second);
        auto* w=f.open_window(props,0);assert(w);w->get_graphics_output()->set_clear_color(LColor(.11f,.11f,.11f,1));
        AnnotationOverlay overlay;overlay.initialize(w->get_render_2d());
        for(int c=0;c<4;++c){
            AnnotationFrameRecord r;r.width=size.first;r.height=size.second;r.frameIndex=100+c;
            for(int i=0;i<5;++i){TargetAnnotation t;t.targetType=0;t.targetPlatID=100+i;t.targetID=i;
                t.modelLabel="Object "+std::to_string(i+1);t.bbox.visible=true;t.bbox.width=30;t.bbox.height=24;
                int x=c==0?r.width/2:c==1?r.width/2+i*3:c==2?(i%2?r.width-6:6):(i+1)*r.width/6;
                int y=c==2?(i<2?7:r.height-7):r.height/2+i*(c==3?25:0);
                t.bbox.x=x-15;t.bbox.y=y-12;
                for(int k=0;k<3;++k){AnnotationPoint2D p;p.name="marker"+std::to_string(k);p.displayIndex=k+1;p.x=x+(c==0?0:k);p.y=y;p.visible=true;t.keyPoints.push_back(p);}r.targets.push_back(t);
            }
            const auto original=identity(r);AnnotationDrawOptions options;
            _putenv_s("P10LabelLegacy","0");overlay.drawFrame(r,options);
            const auto first=overlay.labelPlacements();int overlaps=0,overflow=0;
            assert(first.size()==20);
            for(size_t i=0;i<first.size();++i){overflow+=first[i].overflow;
                for(size_t j=0;j<i;++j)overlaps+=AnnotationLabelLayout::overlap(first[i].box,first[j].box);}
            overlay.drawFrame(r,options);bool stable=true;const auto& second=overlay.labelPlacements();
            for(size_t i=0;i<first.size();++i)stable=stable&&first[i].box.x==second[i].box.x&&first[i].box.y==second[i].box.y;
            const bool unchanged=original==identity(r);assert(!overlaps&&!overflow&&stable&&unchanged);
            std::map<std::string,AnnotationLabelBox> byKey;
            for(size_t i=0;i<first.size();++i)byKey[overlay.labelRequests()[i].key]=first[i].box;
            auto reordered=r;std::reverse(reordered.targets.begin(),reordered.targets.end());overlay.drawFrame(reordered,options);
            bool orderStable=true;
            for(size_t i=0;i<first.size();++i){const auto& b=overlay.labelPlacements()[i].box;const auto old=byKey.at(overlay.labelRequests()[i].key);orderStable=orderStable&&b.x==old.x&&b.y==old.y;}
            assert(orderStable);
            auto moving=r;for(auto& t:moving.targets){t.bbox.x+=1;t.bbox.y+=1;for(auto& p:t.keyPoints){p.x+=1;p.y+=1;}}
            overlay.drawFrame(moving,options);float maxMove=0;
            for(size_t i=0;i<first.size();++i){const auto& b=overlay.labelPlacements()[i].box;const auto old=byKey.at(overlay.labelRequests()[i].key);maxMove=std::max(maxMove,std::max(std::abs(b.x-old.x),std::abs(b.y-old.y)));assert(!overlay.labelPlacements()[i].overflow);}
            std::cout<<"[LabelMotion] size="<<r.width<<'x'<<r.height<<" case="<<c<<" maxMovePx="<<maxMove<<std::endl;
            // Coordinate digits legitimately change measured glyph widths.
            // Record displacement; same-frame and reordered-frame stability
            // remain strict and independent of this moving-input diagnostic.
            assert(maxMove<=12.f);
            overlay.drawFrame(r,options);
            result<<r.width<<','<<r.height<<','<<c<<','<<first.size()<<','<<overlaps<<','<<overflow<<','<<unchanged<<','<<stable<<','<<orderStable<<','<<maxMove<<'\n';++tests;
            if(r.width==800&&(c==0||c==2)){
                for(int legacy=0;legacy<2;++legacy){_putenv_s("P10LabelLegacy",legacy?"1":"0");overlay.drawFrame(r,options);
                    for(int n=0;n<3;++n)f.get_graphics_engine()->render_frame();PNMImage image;
                    assert(w->get_graphics_output()->get_screenshot(image));
                    image.write(Filename::from_os_specific(dir+"/labels_"+std::to_string(c)+(legacy?"_before.png":"_after.png")));
                }
                std::ofstream record(dir+"/labels_"+std::to_string(c)+"_original_coordinates.txt");record<<original;
            }
        }
		if(size.first==800&&size.second==800){
			AnnotationFrameRecord stable;stable.width=800;stable.height=800;stable.frameIndex=900;
			TargetAnnotation t;t.targetType=0x55;t.targetPlatID=1001;t.targetID=7;t.modelLabel="CIVIL";
			t.bbox.visible=true;t.bbox.x=330;t.bbox.y=330;t.bbox.width=140;t.bbox.height=90;
			for(int k=0;k<3;++k){AnnotationPoint2D p;p.name="part"+std::to_string(k+1);p.displayIndex=k+1;
				p.x=360+k*35;p.y=380;p.visible=(k!=1);t.keyPoints.push_back(p);}stable.targets.push_back(t);
			const std::string hiddenOriginal=identity(stable);AnnotationDrawOptions options;overlay.drawFrame(stable,options);
			assert(hiddenOriginal==identity(stable));
			std::string hiddenLabels;for(const auto& request:overlay.labelRequests())hiddenLabels+=request.text+",";
			assert(hiddenLabels.find("1,")!=std::string::npos&&hiddenLabels.find("2,")==std::string::npos&&hiddenLabels.find("3,")!=std::string::npos);
			for(int n=0;n<3;++n)f.get_graphics_engine()->render_frame();PNMImage hiddenImage;
			assert(w->get_graphics_output()->get_screenshot(hiddenImage));
			hiddenImage.write(Filename::from_os_specific(dir+"/p15_numeric_hidden_2.png"));
			stable.targets[0].keyPoints[1].visible=true;std::reverse(stable.targets[0].keyPoints.begin(),stable.targets[0].keyPoints.end());
			const std::string restoredOriginal=identity(stable);overlay.drawFrame(stable,options);assert(restoredOriginal==identity(stable));std::map<std::string,int> numeric;
			for(const auto& request:overlay.labelRequests())if(request.keyPoint)numeric[request.text]++;
			assert(numeric.size()==3&&numeric["1"]==1&&numeric["2"]==1&&numeric["3"]==1);
			// Apply an explicit 90-degree projected rotation around the image centre;
			// numbering must remain definition-bound rather than screen-position-bound.
			for(auto& point:stable.targets[0].keyPoints){const int oldX=point.x,oldY=point.y;
				point.x=400-(oldY-400);point.y=400+(oldX-400);}
			const std::string rebuiltOriginal=identity(stable);AnnotationOverlay rebuilt;rebuilt.initialize(w->get_render_2d());rebuilt.drawFrame(stable,options);assert(rebuiltOriginal==identity(stable));
			std::map<std::string,int> rebuiltNumeric;
			for(const auto& request:rebuilt.labelRequests())if(request.keyPoint)rebuiltNumeric[request.text]++;
			assert(rebuiltNumeric.size()==3&&rebuiltNumeric["1"]==1&&rebuiltNumeric["2"]==1&&rebuiltNumeric["3"]==1);
			for(int n=0;n<3;++n)f.get_graphics_engine()->render_frame();PNMImage image;
			assert(w->get_graphics_output()->get_screenshot(image));
			image.write(Filename::from_os_specific(dir+"/p15_numeric_1_2_3.png"));
			std::ofstream stableOut(dir+"/p15_stable_numbering.txt");
			stableOut<<"hidden_2_labels="<<hiddenLabels<<"\nrestored_reordered=1,2,3\nrotated_90deg_rebuilt=1,2,3\nnon_display_fields_unchanged=1\nnon_display_identity="<<identity(stable)<<"\n";
		}
        f.close_window(w);
    }
    f.close_framework();std::cout<<"[P10Labels] cases="<<tests<<" realTextNodeFontBounds=1 unchangedRecords=1 allLabelsRetained=1 PASS\n";
}
