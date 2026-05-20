#ifndef MATH_ALGORITHM_H
#define MATH_ALGORITHM_H

//#include "eigen3/Eigen/Dense"
#include "common_data.h"
#include "cmath.h"


//using namespace Eigen;
using namespace ICD;

#define PI_Mi 3.14159265358979311600

// #############################################################################################################
// 坐标转换


//inline CartesianCoordinate LLA2ECEF(Position coSelf) {

//    CartesianCoordinate tmp;

//    // Transform to radian
//    double lat = coSelf.lat * PI_Mi / 180;
//    double lon = coSelf.lon * PI_Mi / 180;
//    double alt = coSelf.alt;

//    // 基准椭球体长半径a
//    double a = 6378137.0;
//    // 基准椭球体极扁率f
//    double f = 1/298.257223563;
//    // 偏心率的平方e2
//    double e2 = f * ( 2 - f );
//    // 基准椭球体卯酉圆曲率半径N
//    double N = a / sqrt( 1 - e2 * sin(lat) * sin(lon) );

//    tmp.x = ( N + alt ) * cos(lat) * cos(lon);
//    tmp.y = ( N + alt ) * cos(lat) * sin(lon);
//    tmp.z = ( N * ( 1 - e2 ) + alt ) * sin(lat);

//    return tmp;
//}
inline double toRadian(double dAngle)
{
    double PI =3.1415926535898;
    return dAngle * PI / 180.0;
}
inline double toDegree(double dRadian)
{
    double PI =3.1415926535898;
    return dRadian * 180.0 / PI;
}

inline CartesianCoordinate LLA2ECEF(Position coSelf) {

    CartesianCoordinate tmp;

    // Transform to radian
    double lat = coSelf.lat * PI_Mi / 180;
    double lon = coSelf.lon * PI_Mi / 180;
    double alt = coSelf.alt;

    // 基准椭球体长半径a
    double a = 6378137.0;
    // 基准椭球体极扁率f
    double f_inverse = 298.257223563;
    //椭球短半轴
    double b = a - a / f_inverse;
    const double e = sqrt(a * a - b * b) / a;
    // 基准椭球体卯酉圆曲率半径N
    double N = a / sqrt( 1 - e * e * sin(lat) * sin(lat) );

    tmp.x = ( N + alt ) * cos(lat) * cos(lon);
    tmp.y = ( N + alt ) * cos(lat) * sin(lon);
    tmp.z = ( N * ( 1 - e * e ) + alt ) * sin(lat);

    return tmp;
}

inline Position ECEF2LLA(CartesianCoordinate coSelf) {

    Position tmp;

    // Transform to radian
    double x = coSelf.x;
    double y = coSelf.y;
    double z = coSelf.z;

    double curB = 0;
    double N = 0;
    double calB = atan2(z,sqrt(x*x + y*y));

    double r2d = 180/PI_Mi;
    double epsilon = pow(0.1,15);

    int counter = 0;

    // 基准椭球体长半径a
    double a = 6378137.0;
    // 基准椭球体极扁率f
    double f_inverse = 298.257223563;
    //椭球短半轴
    double b = a - a / f_inverse;
    const double e = sqrt(a * a - b * b) / a;

    while(abs(curB - calB)*r2d > epsilon && counter < 25){
        curB = calB;
        N = a/sqrt(1 - e * e * sin(curB) * sin(curB));
        calB = atan2(z + N * e * e * sin(curB), sqrt(x*x +y*y));
        counter++;
    }
    tmp.lon = atan2(y,x)*r2d;
    tmp.lat = curB * r2d;
    tmp.alt = z / sin(curB) - N * (1- e*e);


    return tmp;
}

///ecefSelf:ENU坐标原点的ecef坐标，coSelf：ENU坐标原点的经纬高，ecefTarget：目标的ECEF坐标
inline CartesianCoordinate ECEF2ENU(CartesianCoordinate ecefSelf, Position coSelf, CartesianCoordinate ecefTarget) {

    // Transform
    double latOrg = coSelf.lat * PI_Mi / 180;
    double lonOrg = coSelf.lon * PI_Mi / 180;
    double altOrg = coSelf.alt;

    double deltaX = ecefTarget.x - ecefSelf.x;
    double deltaY = ecefTarget.y - ecefSelf.y;
    double deltaZ = ecefTarget.z - ecefSelf.z;

    CartesianCoordinate tmp;

    tmp.x = -sin(lonOrg) * deltaX + cos(lonOrg) * deltaY;
    tmp.y = -sin(latOrg) * cos(lonOrg) * deltaX - sin(latOrg) * sin(lonOrg) * deltaY + cos(latOrg) * deltaZ;
    tmp.z =  cos(latOrg) * cos(lonOrg) * deltaX + cos(latOrg) * sin(lonOrg) * deltaY + sin(latOrg) * deltaZ;

    return tmp;
}

///ecefSelf:ENU坐标原点的ecef坐标，coSelf：ENU坐标原点的经纬高，enuTarget：目标的ENU坐标
inline CartesianCoordinate ENU2ECEF(CartesianCoordinate ecefSelf, Position coSelf, CartesianCoordinate enuTarget) {

    // Transform
    double latOrg = coSelf.lat * PI_Mi / 180;
    double lonOrg = coSelf.lon * PI_Mi / 180;
//    double altOrg = coSelf.alt;

    double e = enuTarget.x;
    double n = enuTarget.y;
    double u = enuTarget.z;

    CartesianCoordinate tmp;

    double deltaX = -sin(lonOrg) * e - sin(latOrg) * cos(lonOrg) * n + cos(latOrg) * cos(lonOrg) * u;
    double deltaY = cos(lonOrg) * e - sin(latOrg) * sin(lonOrg) * n + cos(latOrg) * sin (lonOrg) * u;
    double deltaZ = cos(latOrg) * n + sin(latOrg) * u;

    tmp.x = deltaX + ecefSelf.x;
    tmp.y = deltaY + ecefSelf.y;
    tmp.z = deltaZ + ecefSelf.z;

    return tmp;
}

inline CartesianCoordinate ENU2BodyCoordinate2(CartesianCoordinate enu, Euler eulerSelf) {
    // Transform
    double yaw   = eulerSelf.yaw;
    double pitch = eulerSelf.pitch;
//    double roll  = eulerSelf.roll;

    CartesianCoordinate tmp;
    CartesianCoordinate tmp1;
    CartesianCoordinate tmp2;

    CartesianCoordinate ned;

    ned.x = enu.y;
    ned.y = enu.x;
    ned.z = -enu.z;

    double t1 = yaw  * PI_Mi / 180;
    tmp.x = cos(t1) * ned.x + sin(t1) * ned.y;
    tmp.y = -sin(t1) * ned.x + cos(t1) * ned.y;
    tmp.z = ned.z;

    double t2 = pitch * PI_Mi / 180;
    tmp1.x = cos(t2) * tmp.x - sin(t2) * tmp.z;
    tmp1.y = tmp.y;
    tmp1.z = sin(t2) * tmp.x + cos(t2) * tmp.z;

//    double t3 = roll * PI_Mi / 180;
//    tmp2.x = tmp1.x;
//    tmp2.y = cos(t3) * tmp1.y + sin(t3) * tmp1.z;
//    tmp2.z = -sin(t3) * tmp1.y + cos(t3) * tmp1.z;

    return tmp1;
}


inline CartesianCoordinate ENU2BodyCoordinate(CartesianCoordinate enu, Euler eulerSelf) {
    // Transform
    double yaw   = eulerSelf.yaw;
    double pitch = eulerSelf.pitch;
    double roll  = eulerSelf.roll;

    CartesianCoordinate tmp;
    CartesianCoordinate tmp1;
    CartesianCoordinate tmp2;

    CartesianCoordinate ned;

    ned.x = enu.y;
    ned.y = enu.x;
    ned.z = -enu.z;

    double t1 = yaw  * PI_Mi / 180;
    tmp.x = cos(t1) * ned.x + sin(t1) * ned.y;
    tmp.y = -sin(t1) * ned.x + cos(t1) * ned.y;
    tmp.z = ned.z;

    double t2 = pitch * PI_Mi / 180;
    tmp1.x = cos(t2) * tmp.x - sin(t2) * tmp.z;
    tmp1.y = tmp.y;
    tmp1.z = sin(t2) * tmp.x + cos(t2) * tmp.z;

    double t3 = roll * PI_Mi / 180;
    tmp2.x = tmp1.x;
    tmp2.y = cos(t3) * tmp1.y + sin(t3) * tmp1.z;
    tmp2.z = -sin(t3) * tmp1.y + cos(t3) * tmp1.z;

    return tmp2;
}

inline CartesianCoordinate BodyCoordinate2ENU(CartesianCoordinate body, Euler eulerSelf) {
    // Transform
    double yaw   = eulerSelf.yaw;
    double pitch = eulerSelf.pitch;
    double roll  = eulerSelf.roll;

    CartesianCoordinate tmp;
    CartesianCoordinate tmp1;
    CartesianCoordinate enu;

    CartesianCoordinate ned;

    //从机体坐标转换成ned
    double t3 = roll * PI_Mi / 180;
    tmp.x = body.x;
    tmp.y = cos(t3) * body.y - sin(t3) * body.z;
    tmp.z = sin(t3) * body.y + cos(t3) * body.z;

    double t2 = pitch * PI_Mi / 180;
    tmp1.x = cos(t2) * tmp.x + sin(t2) * tmp.z;
    tmp1.y = tmp.y;
    tmp1.z = -sin(t2) * tmp.x + cos(t2) * tmp.z;

    double t1 = yaw  * PI_Mi / 180;
    ned.x = cos(t1) * tmp1.x - sin(t1) * tmp1.y;
    ned.y = sin(t1) * tmp1.x + cos(t1) * tmp1.y;
    ned.z = tmp1.z;

    //ned转成enu
    enu.x = ned.y;
    enu.y = ned.x;
    enu.z = -ned.z;

    return enu;
}


// 输入：我方位置、我方姿态、目标位置
// 输出：目标位置在我方机体坐标系下的位置(前右下)
inline CartesianCoordinate LLA2BodyCoordinate(Position coSelf, Euler eulerSelf, Position coTarget) {

    CartesianCoordinate ecefSelf = LLA2ECEF(coSelf);

    CartesianCoordinate ecefTarget = LLA2ECEF(coTarget);

    CartesianCoordinate enu = ECEF2ENU(ecefSelf, coSelf, ecefTarget);

    CartesianCoordinate bodyTarget = ENU2BodyCoordinate(enu, eulerSelf);

    return bodyTarget;
}


// 输入：我方位置、我方姿态、目标位置
// 输出：目标位置在我方机体坐标系下的位置(前右下),不算滚转
inline CartesianCoordinate LLA2BodyCoordinate2(Position coSelf, Euler eulerSelf, Position coTarget) {

    CartesianCoordinate ecefSelf = LLA2ECEF(coSelf);

    CartesianCoordinate ecefTarget = LLA2ECEF(coTarget);

    CartesianCoordinate enu = ECEF2ENU(ecefSelf, coSelf, ecefTarget);

    CartesianCoordinate bodyTarget = ENU2BodyCoordinate2(enu, eulerSelf);

    return bodyTarget;
}

// 输入：我方位置、我方姿态、目标位置
// 输出：目标位置在我方机体坐标系下的位置(前右下)
inline Position BodyCoordinate2LLA(Position coSelf, Euler eulerSelf, CartesianCoordinate bodyTarget) {

    CartesianCoordinate enuTarget = BodyCoordinate2ENU(bodyTarget, eulerSelf);

    CartesianCoordinate ecefSelf = LLA2ECEF(coSelf);

    CartesianCoordinate ecefTarget = ENU2ECEF(ecefSelf, coSelf, enuTarget);

    Position llaTarget = ECEF2LLA(ecefTarget);

    return llaTarget;
}

// 输入：我方位置、我方姿态、目标位置
// 输出：目标位置在东北天（原点：0,0,0）坐标系下的位置
inline CartesianCoordinate LLA2ENUOrg(Position coTarget) {
    Position coSelf;
    coSelf.alt = 0;
    coSelf.lat = 0;
    coSelf.lon = 0;
    CartesianCoordinate ecefSelf = LLA2ECEF(coSelf);
    CartesianCoordinate ecefTarget = LLA2ECEF(coTarget);

    CartesianCoordinate enu = ECEF2ENU(ecefSelf, coSelf, ecefTarget);

    return enu;
}

// 输入：我方位置、我方姿态、目标位置
// 输出：目标位置在东北天（原点：0,0,0）坐标系下的位置
inline CartesianCoordinate LLA2ENU2(Position coSelf, Position coTarget) {
    CartesianCoordinate ecefSelf = LLA2ECEF(coSelf);
    CartesianCoordinate ecefTarget = LLA2ECEF(coTarget);

    CartesianCoordinate enu = ECEF2ENU(ecefSelf, coSelf, ecefTarget);

    return enu;
}

inline CartesianCoordinate rotateAboutZAxis(CartesianCoordinate origin, double degree) {
    // 旋转z轴
    CartesianCoordinate tmp;
    double t1 = ( degree ) * PI_Mi / 180;
    tmp.x = cos(t1) * origin.x + sin(t1) * origin.y;
    tmp.y = -sin(t1) * origin.x + cos(t1) * origin.y;
    tmp.z = origin.z;
    return tmp;
}

inline CartesianCoordinate rotateAboutYAxis(CartesianCoordinate origin, double degree) {
    // 旋转y轴
    CartesianCoordinate tmp;
    double t2 = -degree * PI_Mi / 180;
    tmp.x = cos(t2) * origin.x - sin(t2) * origin.z;
    tmp.y = origin.y;
    tmp.z = sin(t2) * origin.x + cos(t2) * origin.z;
    return tmp;
}

inline CartesianCoordinate rotateAboutXAxis(CartesianCoordinate origin, double degree) {
    // 旋转x轴
    CartesianCoordinate tmp;
    double t3 = ( 180 + degree ) * PI_Mi / 180;
    tmp.x = origin.x;
    tmp.y = cos(t3) * origin.y + sin(t3) * origin.z;
    tmp.z = -sin(t3) * origin.y + cos(t3) * origin.z;
    return tmp;
}

inline CartesianCoordinate translation(CartesianCoordinate origin, CartesianCoordinate delta) {
    // 平移
    CartesianCoordinate tmp;
    tmp.x = origin.x - delta.x;
    tmp.y = origin.y - delta.y;
    tmp.z = origin.z - delta.z;
    return tmp;
}

inline double comLLADistance(Position Pos_a, Position Pos_b)
{
    //将b的坐标换到a的enu坐标系下

//    Position Pos_a;
//    Position Pos_b;
//    Pos_a.alt = a.alt;
//    Pos_a.lon = a.lon;
//    Pos_a.lat = a.lat;

//    Pos_b.alt = b.alt;
//    Pos_b.lon = b.lon;
//    Pos_b.lat = b.lat;

    CartesianCoordinate ECEF_a = LLA2ECEF(Pos_a);

    CartesianCoordinate ECEF_b = LLA2ECEF(Pos_b);

    CartesianCoordinate enu = ECEF2ENU(ECEF_a, Pos_a, ECEF_b);
    double distance = sqrt(enu.x * enu.x + enu.y * enu.y + enu.z * enu.z);
    return distance;
}


//void readAllData(QString tmp)
//{

//    QFile file(tmp);
//    QByteArray fileValueTmp;

//    realtimeInfo data;
////    QVector<realtimeInfo> coeOpaPgVector;
//    if(file.open(QIODevice::ReadOnly))
//    {
//        //读取所有数据
//        fileValueTmp=file.readAll();
//        //将QByteArray转换为QString
//        QString QStringTmp(fileValueTmp);
//        //将QString的内容用\r\n进行切分，存入QStringList
////        QStringList list = QStringTmp.split("\r\n");
//        QStringList list = QStringTmp.split("\n");

//        //迭代器代替for循环，不用计算循环的次数
//        //QList的迭代器，定义如下，对list进行迭代
//        QListIterator<QString> i(list);

//        //将QStringList的内容放至coeRis
//       //将\r\n获得的数据进行拆分，存入QStringList
//        QStringList list1;
//        int index=0;

//        while (i.hasNext())
//        {
//            list1 = i.next().split(",");
//            if(list1.length()==59 && index != 0)
//            {
//                data.distance = list1[1].toDouble();
//                //redplat
//                data.platPos.lat = list1[2].toDouble();
//                data.platPos.lon = list1[3].toDouble();
//                data.platPos.alt = list1[4].toDouble();
//                data.platPos.yaw = list1[5].toDouble();
//                data.platPos.pitch = list1[6].toDouble();
//                data.platPos.roll = list1[7].toDouble();
//                data.platPos.speed = 1000/*list1[8].toDouble()*/;

//                //mission
//                data.targetPos.lat = list1[10].toDouble();
//                data.targetPos.lon = list1[11].toDouble();
//                data.targetPos.alt = list1[12].toDouble();
//                data.targetPos.yaw = list1[13].toDouble();
//                data.targetPos.pitch = list1[14].toDouble();
//                data.targetPos.roll = list1[15].toDouble();
//                data.targetPos.speed = list1[16].toDouble();
//                data.target2plat[0] = list1[30].toDouble();
//                data.target2plat[1] = list1[31].toDouble();
//                switch (list1[17].toInt()) {
//                case 0:{
//                    data.targetType = 0x22;
//                    break;
//                }
//                case 1:{
//                    data.targetType = 0x33;
//                    break;
//                }
//                default:{
//                    qDebug()<<"unknown target type!";
//                    break;
//                }
//                }
////                data.viewValid = list1[53].toInt();
//                data.viewValid = 1;
//                data.damageFlag = list1[29].toInt();
//                data.strikeFlag = list1[28].toInt();

//                realTimeData.push_back(data);
//            }
//            else
//            {
//                qDebug()<<"OpaPg data error,the line number is"<<index;
//            }
//            index++;
//        }
//    }
//    else
//    {
//        qDebug()<<"open OpaPg file error";
//    }
//    qDebug()<<u8"数据已导入！";
//}
// #############################################################################################################

#endif // MATH_ALGORITHM_H
