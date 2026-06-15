#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    int autoSeconds = 0;
    bool h264Enabled = false;
    const QStringList arguments = a.arguments();
    for (const QString& argument : arguments)
    {
        const QString prefix = QStringLiteral("--phase1b-auto-seconds=");
        if (argument.startsWith(prefix))
        {
            autoSeconds = qBound(1, argument.mid(prefix.size()).toInt(), 3600);
        }
        const QString h264Prefix = QStringLiteral("--phase1d-h264=");
        if (argument.startsWith(h264Prefix))
        {
            h264Enabled = argument.mid(h264Prefix.size()).toInt() != 0;
        }
    }
    w.setH264EnabledForTest(h264Enabled);
    if (autoSeconds > 0)
    {
        QTimer::singleShot(500, &w, [&w]() {
            QMetaObject::invokeMethod(&w, "onInitButtonClicked", Qt::DirectConnection);
        });
        QTimer::singleShot(6000, &w, [&w]() {
            QMetaObject::invokeMethod(&w, "onStartButtonClicked", Qt::DirectConnection);
        });
        QTimer::singleShot(6000 + autoSeconds * 1000, &w, [&w]() {
            QMetaObject::invokeMethod(&w, "onStopButtonClicked", Qt::DirectConnection);
        });
        QTimer::singleShot(6500 + autoSeconds * 1000, &a, &QApplication::quit);
    }
    return a.exec();
}
