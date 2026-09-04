//#include "mainwindow.h"
#include <QCoreApplication>
#include "demo.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    demo d;
//    MainWindow w;
//    w.show();

//    ServToProxy_RX::ProcessDataCallBack callBackStatus_OD = std::bind(recvServToProxy, this, std::placeholders::_1, std::placeholders::_2,std::placeholders::_3);
//    od_guide_recv.reset(new PHOTO_ELECTRIC_GUIDANCE_PARAM_subscriber(1, callBackStatus_OD, "PHOTO_ELECTRIC_GUIDANCE_PARAM"));






    return a.exec();
}


//inline void recvServToProxy(){

//}
