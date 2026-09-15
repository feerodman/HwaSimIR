#pragma once
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
// Explicit generated-geometry diagnostic; no equipment semantic calibration.
#include "Annotation/AnnotationManager.h"
#include "geomVertexWriter.h"
#include "geomVertexFormat.h"
#include "geomTriangles.h"
#include "geomNode.h"
#include "shader.h"
#include "InputAuditV1.h"
#include <cstdint>
#include <cstdlib>
#include <cmath>
class P8OrdinaryAnnotationScene {
    NodePath root; std::vector<TargetPlatformData> objects; AnnotationManager manager; int count=0;
    NodePath geometry(const NodePath& parent,const char* name,bool sphere,float gray,int shape=0) {
        PT(GeomVertexData) data=new GeomVertexData(name,GeomVertexFormat::get_v3(),Geom::UH_static);
        GeomVertexWriter v(data,"vertex");PT(GeomTriangles) t=new GeomTriangles(Geom::UH_static);
        const bool aa=std::getenv("P10Graphics")!=nullptr;
        if(aa&&shape>=3){
            const int sides=shape==3?8:96;
            for(int j=0;j<=sides;++j){float a=float(j*6.283185307179586/sides);v.add_data3f(std::cos(a),std::sin(a),-1);v.add_data3f(std::cos(a),std::sin(a),1);}
            for(int j=0;j<sides;++j){int a=2*j;t->add_vertices(a,a+1,a+2);t->add_vertices(a+1,a+3,a+2);}
        }
        else if(!sphere){v.add_data3f(-1,0,-1);v.add_data3f(1,0,-1);v.add_data3f(1,0,1);v.add_data3f(-1,0,1);t->add_vertices(0,1,2);t->add_vertices(0,2,3);}
        else {
            const int rings=aa?(shape==2?4:48):16,columns=aa?(shape==2?8:96):32;
            for(int r=0;r<=rings;++r)for(int s=0;s<=columns;++s){double a=3.141592653589793*r/rings,b=6.283185307179586*s/columns;v.add_data3f(float(std::sin(a)*std::cos(b)),float(std::sin(a)*std::sin(b)),float(std::cos(a)));}
            for(int r=0;r<rings;++r)for(int s=0;s<columns;++s){int a=r*(columns+1)+s,b=a+columns+1;t->add_vertices(a,b,a+1);t->add_vertices(a+1,b,b+1);}
        }
        t->close_primitive();PT(Geom) g=new Geom(data);g->add_primitive(t);PT(GeomNode) n=new GeomNode(name);n->add_geom(g);
        NodePath p=parent.attach_new_node(n);p.set_two_sided(true);p.set_shader_input("u_test_gray",LVecBase2f(gray,0));return p;
    }
public:
    static int requestedCount(){static const int n=std::atoi(HwaInputAuditV1::environment("P8OrdinaryAnnotations").c_str());return n==1||n==2||n==5?n:0;}
    void clear(){if(!root.is_empty())root.remove_node();root=NodePath();objects.clear();manager.clear();count=0;}
    bool update(const NodePath& render,NodePath camera,Lens* lens,const BYHWICD::DisplayC2cObjTrackingData& input,std::uint64_t seq,int width,int height) {
        int requested=requestedCount();if(!requested||!lens)return false;
        if(root.is_empty()||root.get_parent()!=render||requested!=count){
            clear();count=requested;root=render.attach_new_node("P8OrdinaryGeneratedScene");root.set_pos(0,0,100);
#ifdef _WIN32
            const std::string version="#version 130\n";
#else
            const std::string version="#version 300 es\nprecision highp float;\n";
#endif
            root.set_shader(Shader::make(Shader::SL_GLSL,version+"uniform mat4 p3d_ModelViewProjectionMatrix;in vec4 p3d_Vertex;void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}",version+"uniform vec2 u_test_gray;out vec4 fragColor;void main(){fragColor=vec4(vec3(u_test_gray.x),1);}"),1000);
            if(std::getenv("P10Graphics")){
                root.set_shader(Shader::make(Shader::SL_GLSL,version+"uniform mat4 p3d_ModelViewProjectionMatrix;in vec4 p3d_Vertex;out vec3 localPosition;void main(){localPosition=p3d_Vertex.xyz;gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}",version+"uniform vec2 u_test_gray;uniform vec2 u_test_pattern;in vec3 localPosition;out vec4 fragColor;void main(){float g=u_test_gray.x;if(u_test_pattern.x>0.5)g=mix(.22,.8,mod(floor((localPosition.x+1.)*8.)+floor((localPosition.z+1.)*8.),2.));fragColor=vec4(vec3(g),1);}"),1000);
                root.set_shader_input("u_test_pattern",LVecBase2f(0,0));
                for(int j=0;j<2;++j){auto line=geometry(root,"P10_ThinDiagonal",false,.8f);line.set_pos(j?4.8f:-4.8f,20,0);line.set_scale(.008f,1,1.8f);line.set_r(j?31.f:17.f);}
                std::cout<<"[P10Graphics] source=generated_geometry static_input=1 shapes=checker_diagonal_plate,sphere_96x48,sphere_8x4,cylinder_8,cylinder_96,thin_diagonals spectralUnits=common_scaled_gray temperatureChanges=0\n";
            }
            auto bg=geometry(root,"SyntheticBackground",false,.15f);bg.set_pos(0,40,0);bg.set_scale(30);
            for(int i=0;i<count;++i){
                TargetPlatformData o={};o.isExist=true;o.type=static_cast<PLATFORM_TYPE>(-1);o.nodePath=root.attach_new_node("SyntheticObject"+std::to_string(i));
                auto body=geometry(o.nodePath,"SyntheticBody",i==1||(std::getenv("P10Graphics")&&i==2),.45f+.07f*i,i);
                if(std::getenv("P10Graphics")&&i==0)body.set_shader_input("u_test_pattern",LVecBase2f(1,0));
                auto m=geometry(o.nodePath,"SyntheticMarkerA",false,1.f);m.set_pos(0,-1.01f,0);m.set_scale(.055f);
                m=geometry(o.nodePath,"SyntheticMarkerB",false,.95f);m.set_pos(.5f,-1.01f,.5f);m.set_scale(.055f);objects.push_back(o);
            }
            manager.initialize(root.attach_new_node("P8DiagnosticOverlay"));
            manager.loadProfileFromCandidates({"Config/Diagnostics/p8_ordinary_annotations.json"},"Config/Diagnostics/p8_ordinary_annotations.json","explicit_P8_fixture");
            AnnotationRuntimeOptions options;options.bboxMarginPx=0;options.minBBoxSizePx=1;options.surfaceSnapEnabled=false;options.surfaceKeyPointEnabled=false;options.quietPerfMode=true;
            manager.applyRuntimeOptions(options);manager.setEnabled(true);
            std::cout<<"[P8OrdinaryAnnotations] count="<<count<<" source=generated_geometry normalWorldAcceptance=0 origin=top_left size="<<width<<'x'<<height<<" fovDeg=35 markerSemantics=synthetic_only\n";
        }
        camera.set_pos(render,0,0,100);camera.set_hpr(render,0,0,0);lens->set_fov(35,35);
        double seconds=std::fmod(input.time,60000.0)/1000.0;int phase=int(seconds/2.0)%4;
        for(int i=0;i<count;++i){
            objects[i].targetState=input.targetState[i];objects[i].platID=input.targetState[i].targetPlatID;objects[i].isExist=i<input.targetNumValid;
            float x=(i-(count-1)*.5f)*2.3f,z=(i%2==0?-.8f:1.f),y=20.f;
            if(phase==1)x+=float((seconds-2.0*std::floor(seconds/2.0))*6.0);
            if(phase==2&&i<2){x=0;z=0;y=i?23.f:19.f;}
            if(std::getenv("P10Graphics")){x=(i%3-1)*3.3f;z=i<3?1.7f:-1.7f;y=20.f;objects[i].nodePath.set_r(float(i==0?19:13));}
            objects[i].nodePath.set_pos(x,y,z);
            if(!std::getenv("P10Graphics")&&phase==3&&i==count-1)objects[i].targetState.viewValid=false;
            if(objects[i].isExist)objects[i].nodePath.show();else objects[i].nodePath.hide();
        }
        manager.updateFrame(seq,input.time,input.sensorID,width,height,objects,render,camera,lens,false,true);return true;
    }
    AnnotationFrameRecord record() const {return manager.latestRecord();}
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif
