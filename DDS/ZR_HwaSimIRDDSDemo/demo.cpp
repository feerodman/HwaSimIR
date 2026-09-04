#include "demo.h"
//#include <functional>

demo::demo()
{
//    ServToProxy_RX::ProcessDataCallBack callBackStatus_OD = std::bind(&demo::getinf, this, std::placeholders::_1, std::placeholders::_2,std::placeholders::_3);
//    track_data_recv.reset(new ServToProxy_subscriber(6, callBackStatus_OD, "TOPIC_ABREQ_CMD"));

    servToProxy__writer  = new ServToProxy_publiser(6,"TOPIC_ABREQ_CMD");
    ZRSleep(5000);
    sendinf();
}

void demo::sendinf()
{
    ServToProxy tmp;
    ServToProxyInitialize(&tmp);
    tmp.SysIdx = 0x1000;
    tmp.ValidLen = 5 * sizeof (DDS_Octet);
    tmp.ucData[0] = 'h';
    tmp.ucData[1] = 'e';
    tmp.ucData[2] = 'l';
    tmp.ucData[3] = 'l';
    tmp.ucData[4] = 'o';


    servToProxy__writer->pubData(tmp);
    qDebug()<<"send Data";
}

void demo::getinf(const ServToProxy *data, uint did, const QString &topicName)
{
    qDebug()<<"Receive TRACK DATA Success"<<data->ValidLen;
    qDebug()<<data->ucData[0]<<data->ucData[1]<<data->ucData[2]<<data->ucData[3]<<data->ucData[4];
}

void demo::callBakePro()
{

}
