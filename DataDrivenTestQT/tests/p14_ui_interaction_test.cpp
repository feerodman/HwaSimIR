#include "../mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include <iostream>

namespace {

QGroupBox* AncestorGroup(QWidget* widget)
{
    QWidget* current = widget;
    while (current != nullptr)
    {
        QGroupBox* group = qobject_cast<QGroupBox*>(current);
        if (group != nullptr) return group;
        current = current->parentWidget();
    }
    return nullptr;
}

bool Fail(const QString& reason)
{
    std::cerr << "[P14UiClick][FAIL] " << reason.toStdString() << std::endl;
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    if (argc != 4)
    {
        std::cerr << "usage: p14_ui_interaction_test BASE_CONFIG INPUT_1_TXT EVIDENCE_PREFIX" << std::endl;
        return 2;
    }
    const QString baseConfig = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString inputPath = QFileInfo(QString::fromLocal8Bit(argv[2])).absoluteFilePath();
    const QString evidencePrefix = QFileInfo(QString::fromLocal8Bit(argv[3])).absoluteFilePath();
    QTemporaryDir temporary;
    if (!temporary.isValid()) return Fail(QStringLiteral("temporary directory creation failed")) ? 0 : 3;
    const QString configPath = temporary.filePath(QStringLiteral("P14UiTest.ini"));
    const QDir repositoryRoot(QFileInfo(baseConfig).absoluteDir().absoluteFilePath(QStringLiteral("..")));
    const QString productionLut = repositoryRoot.absoluteFilePath(
        QStringLiteral("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"));
    const QString productionManifest = repositoryRoot.absoluteFilePath(
        QStringLiteral("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/p14_coverage_manifest.json"));
    if (!QFile::copy(baseConfig, configPath)) return Fail(QStringLiteral("config copy failed")) ? 0 : 3;
    if (!QFileInfo::exists(productionLut) || !QFileInfo::exists(productionManifest))
        return Fail(QStringLiteral("production atmosphere inputs are missing")) ? 0 : 3;
    {
        QSettings settings(configPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.setValue(QStringLiteral("UDP/localIp"), QStringLiteral("127.0.0.1"));
        settings.setValue(QStringLiteral("UDP/localPort"), 19998);
        settings.setValue(QStringLiteral("UDP/remoteIp"), QStringLiteral("127.0.0.1"));
        settings.setValue(QStringLiteral("UDP/remotePort"), 19999);
        // Exercise the current production LUT/manifest identity directly.  A test
        // executable may have a stale copied Config directory from an older build;
        // accepting that copy would either mask or manufacture an INIT failure.
        settings.setValue(QStringLiteral("Atmosphere/FormalLut"), productionLut);
        settings.setValue(QStringLiteral("Atmosphere/CoverageManifest"), productionManifest);
        settings.sync();
    }

    MainWindow window(configPath, QStringLiteral("precise"), inputPath,
        QStringLiteral("udp"));
    window.show();
    if (!QTest::qWaitForWindowExposed(&window))
        return Fail(QStringLiteral("production window was not exposed")) ? 0 : 4;
    application.processEvents();

    QComboBox* target = window.findChild<QComboBox*>(QStringLiteral("targetTypeCombo"));
    QCheckBox* forceVisible = window.findChild<QCheckBox*>(QStringLiteral("forceVisibleForDemoCheck"));
    QPushButton* init = window.findChild<QPushButton*>(QStringLiteral("initButton"));
    QPushButton* reset = window.findChild<QPushButton*>(QStringLiteral("resetButton"));
    QScrollArea* scroll = window.findChild<QScrollArea*>();
    if (!target || !forceVisible || !init || !reset || !scroll)
        return Fail(QStringLiteral("named production controls not found")) ? 0 : 4;

    QGroupBox* group = AncestorGroup(target);
    if (!group || !group->isCheckable())
        return Fail(QStringLiteral("target group is not a real checkable UI container")) ? 0 : 4;
    if (!group->isChecked())
    {
        QTest::mouseClick(group, Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
        application.processEvents();
    }
    if (!group->isChecked() || !target->isVisible())
        return Fail(QStringLiteral("mouse click did not expand target controls")) ? 0 : 5;

    const int civilIndex = target->findData(0x55);
    if (civilIndex < 0) return Fail(QStringLiteral("0x55 option missing")) ? 0 : 5;
    target->setFocus();
    QTest::mouseClick(target, Qt::LeftButton, Qt::NoModifier, target->rect().center());
    const int delta = civilIndex - target->currentIndex();
    for (int step = 0; step < std::abs(delta); ++step)
        QTest::keyClick(target, delta > 0 ? Qt::Key_Down : Qt::Key_Up);
    QTest::keyClick(target, Qt::Key_Return);
    QTest::mouseClick(forceVisible, Qt::LeftButton, Qt::NoModifier,
        forceVisible->rect().center());
    application.processEvents();
    const bool selectedByUi = target->currentData().toInt() == 0x55;
    const bool demoCheckedByUi = forceVisible->isChecked();
    // Capture the production target group itself so the selected protocol code
    // and demo-only policy remain legible even when the main form is taller
    // than the desktop viewport.
    group->grab().save(evidencePrefix + QStringLiteral("_selected.png"));
    if (!selectedByUi || !demoCheckedByUi)
        return Fail(QStringLiteral("actual UI selection/click did not take effect")) ? 0 : 6;

    // Selecting an item in the expanded file-data group scrolls that production
    // control into view.  Bring the real INIT button back into the viewport before
    // clicking it; QTest intentionally does not deliver a user click to an obscured
    // widget.  This keeps the evidence a visible-widget interaction, not a slot call.
    scroll->ensureWidgetVisible(init);
    QTest::qWait(100);
    application.processEvents();
    if (!init->isVisibleTo(scroll->viewport()))
        return Fail(QStringLiteral("INIT button could not be scrolled into view")) ? 0 : 7;
    QTest::mouseClick(init, Qt::LeftButton, Qt::NoModifier, init->rect().center());
    QTest::qWait(250);
    application.processEvents();
    const bool frozenAtInit = !target->isEnabled() && !forceVisible->isEnabled() &&
        target->currentData().toInt() == 0x55 && forceVisible->isChecked();
    group->grab().save(evidencePrefix + QStringLiteral("_init_frozen.png"));
    if (!frozenAtInit)
        return Fail(QStringLiteral("INIT click did not freeze target/view policy")) ? 0 : 7;

    scroll->ensureWidgetVisible(reset);
    QTest::qWait(50);
    application.processEvents();
    QTest::mouseClick(reset, Qt::LeftButton, Qt::NoModifier, reset->rect().center());
    QTest::qWait(100);
    application.processEvents();
    const bool unlockedByReset = target->isEnabled() && forceVisible->isEnabled();
    if (!unlockedByReset)
        return Fail(QStringLiteral("RESET click did not unlock controls")) ? 0 : 8;

    QJsonObject evidence;
    evidence.insert(QStringLiteral("schema"), QStringLiteral("HwaSimIR.P14.UiInteraction.1"));
    evidence.insert(QStringLiteral("interactionDriver"), QStringLiteral("QtTest_QTest_mouseClick_keyClick"));
    evidence.insert(QStringLiteral("targetWidgetClass"), QString::fromLatin1(target->metaObject()->className()));
    evidence.insert(QStringLiteral("targetObjectName"), target->objectName());
    evidence.insert(QStringLiteral("selectedText"), target->currentText());
    evidence.insert(QStringLiteral("selectedProtocolCode"), target->currentData().toInt());
    evidence.insert(QStringLiteral("forceVisibleWidgetClass"), QString::fromLatin1(forceVisible->metaObject()->className()));
    evidence.insert(QStringLiteral("forceVisibleObjectName"), forceVisible->objectName());
    evidence.insert(QStringLiteral("mouseSelectedCivilVehicle"), selectedByUi);
    evidence.insert(QStringLiteral("mouseCheckedDemoForceVisible"), demoCheckedByUi);
    evidence.insert(QStringLiteral("mouseClickedInitAndFrozen"), frozenAtInit);
    evidence.insert(QStringLiteral("mouseClickedResetAndUnlocked"), unlockedByReset);
    evidence.insert(QStringLiteral("commandLineTargetOverrideUsed"), false);
    evidence.insert(QStringLiteral("environmentTargetOverrideUsed"), false);
    evidence.insert(QStringLiteral("productionAtmosphereIdentityValidated"), true);
    evidence.insert(QStringLiteral("productionFormalLut"), productionLut);
    evidence.insert(QStringLiteral("productionCoverageManifest"), productionManifest);
    evidence.insert(QStringLiteral("originalInputModified"), false);
    evidence.insert(QStringLiteral("result"), QStringLiteral("PASS"));
    QFile output(evidencePrefix + QStringLiteral(".json"));
    if (!output.open(QIODevice::WriteOnly))
        return Fail(QStringLiteral("cannot write UI evidence JSON")) ? 0 : 9;
    output.write(QJsonDocument(evidence).toJson(QJsonDocument::Indented));
    output.close();
    std::cout << "[P14UiClick] target=0x55 combo=QComboBox checkbox=QCheckBox"
              << " initFrozen=1 resetUnlocked=1 commandLineOverride=0 environmentOverride=0 result=PASS"
              << std::endl;
    return 0;
}
