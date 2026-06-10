#include "HwaSim_IR_VideoDisplay.h"
#include <QtWidgets/QApplication>
#include <QTextCodec>
#include <QtGlobal>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5 运行时统一使用 UTF-8，避免中文界面和日志在不同系统代码页下乱码。
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif
    QApplication app(argc, argv);
    HwaSim_IR_VideoDisplay window;
    window.show();
    return app.exec();
}
