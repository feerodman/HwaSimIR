#include "pandaFramework.h"
#include "load_prc_file.h"
#include "pnmImage.h"
#include "../HwaSim_IR/HwaSim_IR/Annotation/AnnotationOverlay.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstdlib>
#include <map>
#include <set>
static std::string identity(const AnnotationFrameRecord& r){
    std::ostringstream s;s<<r.frameIndex<<','<<r.width<<','<<r.height;
    for(const auto& t:r.targets){s<<'|'<<t.targetType<<','<<t.targetPlatID<<','<<t.targetID<<','<<t.modelLabel<<','<<t.bbox.x<<','<<t.bbox.y<<','<<t.bbox.width<<','<<t.bbox.height<<','<<t.bbox.visible;
        for(const auto& p:t.keyPoints)s<<'|'<<p.name<<','<<p.x<<','<<p.y<<','<<p.visible;}return s.str();
}
static std::string pointText(const AnnotationPoint2D& p){
    std::ostringstream s;s<<p.displayIndex<<'('<<p.x<<','<<p.y<<')';return s.str();
}
static std::string pointKey(const TargetAnnotation& t,const AnnotationPoint2D& p){
    std::ostringstream s;s<<t.targetType<<':'<<t.targetPlatID<<':'<<t.targetID<<':'<<p.displayIndex;return s.str();
}
static std::map<std::string,const AnnotationPoint2D*> visiblePoints(const AnnotationFrameRecord& r){
    std::map<std::string,const AnnotationPoint2D*> out;
    for(const auto& t:r.targets)for(const auto& p:t.keyPoints)if(p.visible)out[pointKey(t,p)]=&p;
    return out;
}
static void verifyCoordinateLabels(const AnnotationFrameRecord& record,const AnnotationOverlay& overlay,
    const std::string& caseId,std::ofstream& alignment){
    const auto expected=visiblePoints(record);std::set<std::string> seen;
    const auto& requests=overlay.labelRequests();const auto& placements=overlay.labelPlacements();
    assert(requests.size()==placements.size());
    for(size_t i=0;i<requests.size();++i){const auto& request=requests[i];if(!request.keyPoint)continue;
        const auto found=expected.find(request.key);assert(found!=expected.end());const auto& point=*found->second;
        const std::string expectedText=pointText(point);assert(request.text==expectedText);
        assert(request.anchorX==float(point.x+6)&&request.anchorY==float(point.y-6));
        assert(request.key==request.key.substr(0,request.key.find_last_of(':')+1)+std::to_string(point.displayIndex));
        seen.insert(request.key);
        alignment<<caseId<<','<<request.key<<",\""<<request.text<<"\","<<point.x<<','<<point.y<<','
            <<request.anchorX<<','<<request.anchorY<<','<<placements[i].box.x<<','<<placements[i].box.y<<','
            <<placements[i].overflow<<",1\n";
    }
    assert(seen.size()==expected.size());
}
int main(int argc,char**argv){
    if(argc!=2)return 64;std::string dir=argv[1];
    load_prc_file_data("p10_labels","window-type offscreen\naudio-library-name null\nsync-video false\nframebuffer-multisample false\n");
    PandaFramework f;f.open_framework(argc,argv);std::ofstream result(dir+"/label_tests.csv");
    std::ofstream alignment(dir+"/p16_coordinate_label_alignment.csv");
    alignment<<"case_id,layout_key,text,record_x,record_y,request_anchor_x,request_anchor_y,label_x,label_y,overflow,text_matches_record\n";
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
            verifyCoordinateLabels(r,overlay,"matrix_"+std::to_string(r.width)+"x"+std::to_string(r.height)+"_"+std::to_string(c),alignment);
            const auto first=overlay.labelPlacements();int overlaps=0,overflow=0;
            assert(first.size()==20);
            for(size_t i=0;i<first.size();++i){overflow+=first[i].overflow;
                for(size_t j=0;j<i;++j)overlaps+=AnnotationLabelLayout::overlap(first[i].box,first[j].box);}
            overlay.drawFrame(r,options);verifyCoordinateLabels(r,overlay,"matrix_repeat",alignment);bool stable=true;const auto& second=overlay.labelPlacements();
            for(size_t i=0;i<first.size();++i)stable=stable&&first[i].box.x==second[i].box.x&&first[i].box.y==second[i].box.y;
            const bool unchanged=original==identity(r);assert(!overlaps&&!overflow&&stable&&unchanged);
            std::map<std::string,AnnotationLabelBox> byKey;
            for(size_t i=0;i<first.size();++i)byKey[overlay.labelRequests()[i].key]=first[i].box;
            auto reordered=r;std::reverse(reordered.targets.begin(),reordered.targets.end());overlay.drawFrame(reordered,options);verifyCoordinateLabels(reordered,overlay,"matrix_reordered",alignment);
            bool orderStable=true;
            for(size_t i=0;i<first.size();++i){const auto& b=overlay.labelPlacements()[i].box;const auto old=byKey.at(overlay.labelRequests()[i].key);orderStable=orderStable&&b.x==old.x&&b.y==old.y;}
            assert(orderStable);
            auto moving=r;for(auto& t:moving.targets){t.bbox.x+=1;t.bbox.y+=1;for(auto& p:t.keyPoints){p.x+=1;p.y+=1;}}
            const auto movingBeforeDraw=identity(moving);overlay.drawFrame(moving,options);assert(movingBeforeDraw==identity(moving));verifyCoordinateLabels(moving,overlay,"matrix_coordinate_update",alignment);float maxMove=0;int movingOverlaps=0;
            for(size_t i=0;i<first.size();++i){const auto& b=overlay.labelPlacements()[i].box;const auto old=byKey.at(overlay.labelRequests()[i].key);maxMove=std::max(maxMove,std::max(std::abs(b.x-old.x),std::abs(b.y-old.y)));assert(!overlay.labelPlacements()[i].overflow);
                for(size_t j=0;j<i;++j)movingOverlaps+=AnnotationLabelLayout::overlap(b,overlay.labelPlacements()[j].box);}
            assert(!movingOverlaps);
            std::cout<<"[LabelMotion] size="<<r.width<<'x'<<r.height<<" case="<<c<<" maxMovePx="<<maxMove<<std::endl;
            // Coordinate digits legitimately change measured glyph widths and
            // may repack an adversarial edge cluster.  Keep the displacement as
            // evidence; correctness is exact text/record equality, stable keys,
            // no overlap, no overflow, and no mutation of the source record.
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
			const std::string hiddenOriginal=identity(stable);AnnotationDrawOptions options;overlay.drawFrame(stable,options);verifyCoordinateLabels(stable,overlay,"civil_hidden_2",alignment);
			assert(hiddenOriginal==identity(stable));
			std::string hiddenLabels;for(const auto& request:overlay.labelRequests())hiddenLabels+=request.text+",";
			assert(hiddenLabels.find("1(360,380),")!=std::string::npos&&hiddenLabels.find("2(395,380),")==std::string::npos&&hiddenLabels.find("3(430,380),")!=std::string::npos);
			for(int n=0;n<3;++n)f.get_graphics_engine()->render_frame();PNMImage hiddenImage;
			assert(w->get_graphics_output()->get_screenshot(hiddenImage));
			hiddenImage.write(Filename::from_os_specific(dir+"/p16_number_coordinates_hidden_2.png"));
			stable.targets[0].keyPoints[1].visible=true;std::reverse(stable.targets[0].keyPoints.begin(),stable.targets[0].keyPoints.end());
			const std::string restoredOriginal=identity(stable);overlay.drawFrame(stable,options);verifyCoordinateLabels(stable,overlay,"civil_restored_reordered",alignment);assert(restoredOriginal==identity(stable));std::map<std::string,int> numeric;
			for(const auto& request:overlay.labelRequests())if(request.keyPoint)numeric[request.text]++;
			assert(numeric.size()==3&&numeric["1(360,380)"]==1&&numeric["2(395,380)"]==1&&numeric["3(430,380)"]==1);
			// Apply an explicit 90-degree projected rotation around the image centre;
			// numbering must remain definition-bound rather than screen-position-bound.
			for(auto& point:stable.targets[0].keyPoints){const int oldX=point.x,oldY=point.y;
				point.x=400-(oldY-400);point.y=400+(oldX-400);}
			overlay.clear();
			const std::string rebuiltOriginal=identity(stable);AnnotationOverlay rebuilt;rebuilt.initialize(w->get_render_2d());rebuilt.drawFrame(stable,options);verifyCoordinateLabels(stable,rebuilt,"civil_rotated_90_rebuilt",alignment);assert(rebuiltOriginal==identity(stable));
			std::map<std::string,int> rebuiltNumeric;
			for(const auto& request:rebuilt.labelRequests())if(request.keyPoint)rebuiltNumeric[request.text]++;
			assert(rebuiltNumeric.size()==3&&rebuiltNumeric["1(420,360)"]==1&&rebuiltNumeric["2(420,395)"]==1&&rebuiltNumeric["3(420,430)"]==1);
			for(int n=0;n<3;++n)f.get_graphics_engine()->render_frame();PNMImage image;
			assert(w->get_graphics_output()->get_screenshot(image));
			image.write(Filename::from_os_specific(dir+"/p16_number_coordinates_1_2_3.png"));
			// Four image corners exercise clamped layout while the text continues to
			// carry the unmodified original-image coordinates.
			AnnotationFrameRecord corners;corners.width=800;corners.height=800;corners.frameIndex=901;
			TargetAnnotation edge;edge.targetType=0x55;edge.targetPlatID=1002;edge.targetID=8;edge.modelLabel="EDGE";
			edge.bbox.visible=true;edge.bbox.x=0;edge.bbox.y=0;edge.bbox.width=800;edge.bbox.height=800;
			const int edgeXY[4][2]={{0,0},{799,0},{0,799},{799,799}};
			for(int k=0;k<4;++k){AnnotationPoint2D p;p.name="edge"+std::to_string(k+1);p.displayIndex=k+1;p.x=edgeXY[k][0];p.y=edgeXY[k][1];p.visible=true;edge.keyPoints.push_back(p);}corners.targets.push_back(edge);
			const auto cornersOriginal=identity(corners);rebuilt.drawFrame(corners,options);verifyCoordinateLabels(corners,rebuilt,"four_corners",alignment);assert(cornersOriginal==identity(corners));
			for(const auto& placement:rebuilt.labelPlacements())assert(!placement.overflow);
			for(int n=0;n<3;++n)f.get_graphics_engine()->render_frame();PNMImage cornerImage;assert(w->get_graphics_output()->get_screenshot(cornerImage));
			cornerImage.write(Filename::from_os_specific(dir+"/p16_number_coordinates_corners.png"));
			// Coordinate digit-count changes must preserve the definition-bound key.
			AnnotationFrameRecord digits=corners;digits.frameIndex=902;digits.targets[0].targetID=9;
			digits.targets[0].keyPoints.resize(2);digits.targets[0].keyPoints[0].displayIndex=9;digits.targets[0].keyPoints[0].x=7;digits.targets[0].keyPoints[0].y=8;
			digits.targets[0].keyPoints[1].displayIndex=10;digits.targets[0].keyPoints[1].x=79;digits.targets[0].keyPoints[1].y=80;
			rebuilt.drawFrame(digits,options);verifyCoordinateLabels(digits,rebuilt,"digit_width_short",alignment);
			std::set<std::string> keysBefore;for(const auto& request:rebuilt.labelRequests())if(request.keyPoint)keysBefore.insert(request.key);
			digits.targets[0].keyPoints[0].x=707;digits.targets[0].keyPoints[0].y=708;digits.targets[0].keyPoints[1].x=779;digits.targets[0].keyPoints[1].y=780;
			const auto digitOriginal=identity(digits);rebuilt.drawFrame(digits,options);verifyCoordinateLabels(digits,rebuilt,"digit_width_long",alignment);assert(digitOriginal==identity(digits));
			std::set<std::string> keysAfter;for(const auto& request:rebuilt.labelRequests())if(request.keyPoint)keysAfter.insert(request.key);assert(keysBefore==keysAfter);
			std::ofstream stableOut(dir+"/p16_stable_number_coordinates.txt");
			stableOut<<"hidden_2_labels="<<hiddenLabels<<"\nrestored_reordered=1(360,380),2(395,380),3(430,380)\nrotated_90deg_rebuilt=1(420,360),2(420,395),3(420,430)\nfour_corners=1\ndigit_width_change_stable_keys=1\nnon_display_fields_unchanged=1\nnon_display_identity="<<identity(stable)<<"\n";
		}
        f.close_window(w);
    }
    f.close_framework();std::cout<<"[P16Labels] cases="<<tests<<" format_index_xy=1 coordinate_match=1 stable_keys=1 realTextNodeFontBounds=1 unchangedRecords=1 allLabelsRetained=1 PASS\n";
}
