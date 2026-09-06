#include <QCoreApplication>
#include <QDebug>
#include "DdsRuntime.h"
#include "demo.h"
#include "demo2.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    const QByteArray qosFile = (QCoreApplication::applicationDirPath() +
        "/Config/ZRDDS_PROTOCOL_QOS.xml").toLocal8Bit();
    if (!DdsRuntime::init(qosFile.constData()))
        return 1;

    int result = 0;
    {
        // 公共 Factory 已准备好；必须先 ZR，后 HwaSimIR 业务 Participant。
        demo d;
        demo2 d2;
        if (!d.isReady() || !d2.initDds())
        {
            qCritical() << "DDS business initialization failed";
            result = 2;
        }
        else
        {
            // HwaSimIR 的 Reset/Init/Start/Realtime/Stop 在各自业务位置调用，
            // 不在这里自动执行完整回合。原 demo 的发送调用保持不变。
            result = a.exec();
        }
    } // 先 d2 的 Reader/Participant，再 d 的原 ZR Participant。
    if (!DdsRuntime::shutdown())
        result = 3;
    return result;
}