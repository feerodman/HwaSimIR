#ifndef COMMON_DATA_H
#define COMMON_DATA_H
#pragma pack(1)
#include <cstring>
#define WHITE2CONTROL_PORT 6001    // 仿真平台发送到控制程序的端口
#define CONTROL2WHITE_PORT 6002    // 控制程序发送到仿真平台的端口
#define JSBSIM_PORT        5090    // JSBSIM端口
#define COCKPIT_PORT      4090    // 座舱采集端口
#define INSM_PORT           1020    // 惯导端口

#define CONTROL2STZK_PORT 6003    // 控制程序发送到仿真平台的端口

#define REVCONTROL2STZK_PORT 6004    // 控制程序接收端口

namespace ICD {

//直角坐标系
struct CartesianCoordinate
{
    double x;  //米
    double y;  //米
    double z;  //米
};

//位置信息
struct Position
{
    ///纬度：北纬为正
    double lat;
    ///经度：东经为正
    double lon;
    double alt;     //海拔：米
};

//速度信息
struct SpeedENU
{
    double SpeedE;     //目标东向速度
    double SpeedN;     //目标北向速度
    double SpeedU;     //目标天向速度
};

//加速度信息
struct AccelerENU
{
    double AccelerE;     //目标东向加速度
    double AccelerN;     //目标北向加速度
    double AccelerU;     //目标天向加速度
};

//姿态信息
struct Euler
{
    double yaw;     //航向：0-360度，顺时针   //马建平相遇的定义为航向：±180度，左转为正
    double pitch;   //俯仰：±90度，上仰为正   //马建平相遇的定义俯仰：±90度，抬头为正
    double roll;    //滚转：±180度，右倾为正  //马建平相遇的定义滚转：±180度，右倾为正
};

//位置姿态信息
struct PosAtti : public Position, public Euler
{
};

//空间状态信息
struct SpatialState: public Position, public Euler
{
    double speed;   //km/h
};

//带aoa sa 的空间状态信息

struct SpaAoaState
{
    SpatialState spatial;
    ///攻角
    double aoa;
    ///侧滑角
    double sa;
};



//方位角和俯仰角
struct relAngular{
    double pitch;
    double azimuth;
};

struct realtimeInfo{
    double distance;
    Position platPos;
    Euler platEul;
    double platSpeed;
    Position tarPos;
    Euler tarEul;
    double tarSpeed;
    int targetType;
    relAngular target2plat;
    bool viewValid;
    bool damageFlag;
    bool strikeFlag;
    realtimeInfo(){
        memset(this,0,sizeof (realtimeInfo));
    }

};

}

#pragma pack()
#endif
