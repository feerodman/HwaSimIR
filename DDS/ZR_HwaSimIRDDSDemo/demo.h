#ifndef DEMO_H
#define DEMO_H
#include "servtoproxy_publiser.h"
#include "servtoproxy_subdcriber.h"
#include <memory>

class demo
{
public:
    demo();

    void sendinf();
    std::shared_ptr<ServToProxy_subscriber> track_data_recv;
    ServToProxy_publiser *servToProxy__writer;
public slots:
    void getinf(const ServToProxy *data, uint did, const QString &topicName);
    void callBakePro();
};

#endif // DEMO_H
