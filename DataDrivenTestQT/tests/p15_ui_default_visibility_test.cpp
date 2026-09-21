#include "../mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <cstdlib>
#include <iostream>

namespace {

int Fail(const QString& reason)
{
    std::cerr << "[P15UiDefault][FAIL] " << reason.toStdString() << std::endl;
    return 1;
}

QLabel* FindVisibleLabel(QGroupBox* group, const QString& prefix)
{
    const QList<QLabel*> labels = group->findChildren<QLabel*>();
    for (QLabel* label : labels)
    {
        if (label->text().startsWith(prefix) && label->isVisibleTo(group)) return label;
    }
    return nullptr;
}

bool FullyInside(QWidget* child, QWidget* ancestor)
{
    const QRect childRect(child->mapTo(ancestor, QPoint(0, 0)), child->size());
    return ancestor->rect().contains(childRect);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication application(argc, argv);
    if (argc != 4)
    {
        std::cerr << "usage: p15_ui_default_visibility_test BASE_CONFIG INPUT_1_TXT EVIDENCE_PREFIX" << std::endl;
        return 2;
    }

    const QString baseConfig = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString inputPath = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    const QString evidencePrefix = QFileInfo(QString::fromLocal8Bit(argv[3])).absoluteFilePath();
    QTemporaryDir temporary;
    if (!temporary.isValid()) return Fail(QStringLiteral("temporary directory creation failed"));
    const QString configPath = temporary.filePath(QStringLiteral("P15UiTest.ini"));
    const QDir repositoryRoot(QFileInfo(baseConfig).absoluteDir().absoluteFilePath(QStringLiteral("..")));
    const QString productionLut = repositoryRoot.absoluteFilePath(
        QStringLiteral("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"));
    const QString productionManifest = repositoryRoot.absoluteFilePath(
        QStringLiteral("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/p14_coverage_manifest.json"));
    if (!QFile::copy(baseConfig, configPath)) return Fail(QStringLiteral("config copy failed"));
    {
        QSettings settings(configPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.setValue(QStringLiteral("UDP/localIp"), QStringLiteral("127.0.0.1"));
        settings.setValue(QStringLiteral("UDP/localPort"), 20998);
        settings.setValue(QStringLiteral("UDP/remoteIp"), QStringLiteral("127.0.0.1"));
        settings.setValue(QStringLiteral("UDP/remotePort"), 20999);
        settings.setValue(QStringLiteral("Atmosphere/FormalLut"), productionLut);
        settings.setValue(QStringLiteral("Atmosphere/CoverageManifest"), productionManifest);
        settings.sync();
    }

    bool frozen = false;
    bool unlocked = false;
    {
    MainWindow window(configPath, QStringLiteral("precise"), inputPath, QStringLiteral("udp"));
    window.show();
    if (!QTest::qWaitForWindowExposed(&window)) return Fail(QStringLiteral("production window not exposed"));
    application.processEvents();

    QGroupBox* targetGroup = window.findChild<QGroupBox*>(QStringLiteral("testTargetGroup"));
    QGroupBox* previewGroup = window.findChild<QGroupBox*>(QStringLiteral("fileDataPreviewGroup"));
    QComboBox* target = window.findChild<QComboBox*>(QStringLiteral("targetTypeCombo"));
    QCheckBox* forceVisible = window.findChild<QCheckBox*>(QStringLiteral("forceVisibleForDemoCheck"));
    QPushButton* init = window.findChild<QPushButton*>(QStringLiteral("initButton"));
    QPushButton* reset = window.findChild<QPushButton*>(QStringLiteral("resetButton"));
    QPushButton* version = window.findChild<QPushButton*>(QStringLiteral("versionInfoButton"));
    if (!targetGroup || !previewGroup || !target || !forceVisible || !init || !reset || !version)
        return Fail(QStringLiteral("named production controls missing"));
    if (targetGroup->isCheckable() || previewGroup->isChecked())
        return Fail(QStringLiteral("default group states changed"));
    if (!target->isVisibleTo(&window) || !forceVisible->isVisibleTo(&window) ||
        !FindVisibleLabel(targetGroup, QStringLiteral("目标类型")) ||
        !FindVisibleLabel(targetGroup, QStringLiteral("显示策略")))
        return Fail(QStringLiteral("target controls or labels not visible at first exposure"));
    if (!FullyInside(target, targetGroup) || !FullyInside(forceVisible, targetGroup) || target->width() < 240)
        return Fail(QStringLiteral("target controls clipped or too narrow"));

    window.grab().save(evidencePrefix + QStringLiteral("_default.png"));
    const int civilIndex = target->findData(0x55);
    if (civilIndex < 0) return Fail(QStringLiteral("civil 0x55 option missing"));
    target->setFocus();
    QTest::mouseClick(target, Qt::LeftButton, Qt::NoModifier, target->rect().center());
    const int delta = civilIndex - target->currentIndex();
    for (int step = 0; step < std::abs(delta); ++step)
        QTest::keyClick(target, delta > 0 ? Qt::Key_Down : Qt::Key_Up);
    QTest::keyClick(target, Qt::Key_Return);
    application.processEvents();
    if (target->currentData().toInt() != 0x55)
        return Fail(QStringLiteral("civil target selection did not take effect"));
    window.grab().save(evidencePrefix + QStringLiteral("_civil_selected.png"));

    QTest::mouseClick(init, Qt::LeftButton, Qt::NoModifier, init->rect().center());
    QTest::qWait(250);
    application.processEvents();
    frozen = !target->isEnabled() && !forceVisible->isEnabled();
    if (!frozen) return Fail(QStringLiteral("INIT did not freeze target controls"));
    window.grab().save(evidencePrefix + QStringLiteral("_init_frozen.png"));
    QTest::mouseClick(reset, Qt::LeftButton, Qt::NoModifier, reset->rect().center());
    QTest::qWait(100);
    application.processEvents();
    unlocked = target->isEnabled() && forceVisible->isEnabled();
    if (!unlocked) return Fail(QStringLiteral("RESET did not unlock target controls"));

    window.close();
    application.processEvents();
    }
    MainWindow reopened(configPath, QStringLiteral("precise"), inputPath, QStringLiteral("udp"));
    reopened.show();
    if (!QTest::qWaitForWindowExposed(&reopened)) return Fail(QStringLiteral("reopened window not exposed"));
    application.processEvents();
    QComboBox* reopenedTarget = reopened.findChild<QComboBox*>(QStringLiteral("targetTypeCombo"));
    if (!reopenedTarget || !reopenedTarget->isVisibleTo(&reopened))
        return Fail(QStringLiteral("target selector hidden after reopen"));
    reopened.grab().save(evidencePrefix + QStringLiteral("_reopened.png"));

    QJsonObject evidence;
    evidence.insert(QStringLiteral("schema"), QStringLiteral("HwaSimIR.P15.UiDefaultVisibility.1"));
    evidence.insert(QStringLiteral("result"), QStringLiteral("PASS"));
    evidence.insert(QStringLiteral("defaultVisibleWithoutPreviewExpansion"), true);
    evidence.insert(QStringLiteral("previewExpandedByTest"), false);
    evidence.insert(QStringLiteral("childShowCalledByTest"), false);
    evidence.insert(QStringLiteral("layoutModifiedByTest"), false);
    evidence.insert(QStringLiteral("civilTargetSelectedByMouseAndKeys"), true);
    evidence.insert(QStringLiteral("selectedProtocolCode"), 0x55);
    evidence.insert(QStringLiteral("initFrozen"), frozen);
    evidence.insert(QStringLiteral("resetUnlocked"), unlocked);
    evidence.insert(QStringLiteral("reopenVisible"), true);
    evidence.insert(QStringLiteral("logicalWidth"), reopened.width());
    evidence.insert(QStringLiteral("logicalHeight"), reopened.height());
    evidence.insert(QStringLiteral("devicePixelRatio"), reopened.devicePixelRatioF());
    evidence.insert(QStringLiteral("targetWidth"), reopenedTarget->width());
    evidence.insert(QStringLiteral("testEnvironmentTargetOverrideUsed"), false);
    evidence.insert(QStringLiteral("originalInputModified"), false);
    QFile output(evidencePrefix + QStringLiteral(".json"));
    if (!output.open(QIODevice::WriteOnly)) return Fail(QStringLiteral("cannot write evidence JSON"));
    output.write(QJsonDocument(evidence).toJson(QJsonDocument::Indented));
    std::cout << "[P15UiDefault] visible=1 previewExpanded=0 target=0x55 initFrozen=1 resetUnlocked=1 reopenVisible=1 result=PASS" << std::endl;
    return 0;
}
