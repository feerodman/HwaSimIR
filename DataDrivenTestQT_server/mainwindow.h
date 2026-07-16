//#pragma once
// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
//#define _USE_MATH_DEFINES
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>
#include "CommonData.h"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <QVector>
#include "ICD/common_data.h"

using namespace ICD;

class HeadlessLineEdit : public QObject
{
public:
	explicit HeadlessLineEdit(const QString& text = QString(), QObject* parent = nullptr)
		: QObject(parent), m_text(text) {}

	QString text() const { return m_text; }
	void setText(const QString& text) { m_text = text; }

private:
	QString m_text;
};

class HeadlessLabel : public QObject
{
public:
	explicit HeadlessLabel(const QString& text = QString(), QObject* parent = nullptr)
		: QObject(parent), m_text(text) {}

	QString text() const { return m_text; }
	void setText(const QString& text) { m_text = text; }
	void setStyleSheet(const QString&) {}

private:
	QString m_text;
};

class HeadlessButton : public QObject
{
public:
	explicit HeadlessButton(QObject* parent = nullptr) : QObject(parent) {}

	void setEnabled(bool enabled) { m_enabled = enabled; }
	bool isEnabled() const { return m_enabled; }

private:
	bool m_enabled = true;
};

class MainWindow : public QObject
{
	Q_OBJECT

public:
	explicit MainWindow(QObject *parent = nullptr);
	~MainWindow();
	void setH264EnabledForTest(bool enabled) { m_h264Enabled = enabled; }
	void configurePhase4cAeroMachTest(bool enabled, double altitudeKm, double mach);

	private slots:
	void onResetButtonClicked();
	void onInitButtonClicked();
	void onStartButtonClicked();
	void onStopButtonClicked();
	void onSendRealTimeData();
	void updatePosition();
	void onAutoExecuteStep();

private:
	void setupUI();
	void loadNetworkConfig();
	void setupUDP();
	void sendControlCommand(int command);
	void sendInitCommand();
	void sendRealTimeData();
	void applyPhase4cAeroMachOverride(BYHWICD::DisplayC2cObjTrackingData& data) const;
	void logAeroSpeedSend(const BYHWICD::DisplayC2cObjTrackingData& data) const;
	void scheduleNextRealTimeFrame();
	int targetVideoFps() const;

	bool step(BYHWICD::CartesianCoordinate& plane_pos, BYHWICD::Euler& plane_att,
		BYHWICD::CartesianCoordinate& missile_pos, BYHWICD::Euler& missile_att);
	void initStepSimData();
	void readData(QString tmp);

	QString m_udpLocalIp;
	quint16 m_udpLocalPort = 9999;
	QString m_udpRemoteIp;
	quint16 m_udpRemotePort = 8888;

	// Headless state fields replacing the old UI widgets.
	HeadlessLineEdit *m_localIpEdit = nullptr;
	HeadlessLineEdit *m_localPortEdit = nullptr;
	HeadlessLineEdit *m_remoteIpEdit = nullptr;
	HeadlessLineEdit *m_remotePortEdit = nullptr;
	HeadlessLineEdit *m_platIDEdit = nullptr;
	HeadlessLineEdit *m_sensorIDEdit = nullptr;
	HeadlessLineEdit *m_currentRoundEdit = nullptr;
	HeadlessLineEdit *m_roundCutEdit = nullptr;

	HeadlessLineEdit *m_latEdit = nullptr;
	HeadlessLineEdit *m_lonEdit = nullptr;
	HeadlessLineEdit *m_altEdit = nullptr;
	HeadlessLineEdit *m_yawEdit = nullptr;
	HeadlessLineEdit *m_pitchEdit = nullptr;
	HeadlessLineEdit *m_rollEdit = nullptr;
	HeadlessLineEdit *m_latEditTarget = nullptr;
	HeadlessLineEdit *m_lonEditTarget = nullptr;
	HeadlessLineEdit *m_altEditTarget = nullptr;
	HeadlessLineEdit *m_yawEditTarget = nullptr;
	HeadlessLineEdit *m_pitchEditTarget = nullptr;
	HeadlessLineEdit *m_rollEditTarget = nullptr;
	HeadlessLineEdit *m_collisionTime = nullptr;
	HeadlessLineEdit *m_speed = nullptr;
	HeadlessLineEdit *m_timeStep = nullptr;
	HeadlessLineEdit *m_fovHEdit = nullptr;
	HeadlessLineEdit *m_fovVEdit = nullptr;
	HeadlessLineEdit *m_targetTypeEdit = nullptr;
	HeadlessLineEdit *m_videoFpsEdit = nullptr;

	HeadlessButton *m_resetButton = nullptr;
	HeadlessButton *m_initButton = nullptr;
	HeadlessButton *m_startButton = nullptr;
	HeadlessButton *m_stopButton = nullptr;

	HeadlessLabel *m_statusLabel = nullptr;
	HeadlessLabel *m_lastSentLabel = nullptr;
	HeadlessLabel *m_lastReceivedLabel = nullptr;

	// UDP Socket
	QUdpSocket *m_udpSocket = nullptr;
	QTimer *m_realTimeTimer = nullptr;
	QTimer *m_autoCommandTimer = nullptr;
	int m_autoCommandStep = 0;
	QElapsedTimer m_sendClock;
	bool m_isRealtimeSending = false;
	quint64 m_sentFrameCount = 0;
	quint64 m_sendDeadlineIndex = 0;
	qint64 m_lastSendPerfLogNs = 0;
	quint64 m_lastSendPerfFrameCount = 0;
	int m_uiUpdateEveryFrames = 12;
	int m_targetVideoFps = 60;
	bool m_h264Enabled = false;
	bool m_phase4cAeroMachMode = false;
	double m_phase4cAltitudeKm = 10.0;
	double m_phase4cMach = 1.0;
	double m_phase4cSpeedMps = 0.0;
	double m_phase4cSpeedKmh = 0.0;

	// Current Position
	double m_currentLat;
	double m_currentLon;
	double m_currentAlt;
	double m_currentYaw;
	double m_currentPitch;
	double m_currentRoll;

	bool is_collided = false;   // 是否已相撞
    double current_time = 0.0;  // 当前模拟时间（秒）
    int time_step = 25;     // 每次调用的时间步长（豪秒）
	double collision_time;      // 预计相撞时间（秒）

								// 飞机参数（可配置）
	BYHWICD::CartesianCoordinate plane_init_pos;  // 飞机初始位置
	BYHWICD::Euler plane_init_attitude;          // 飞机初始姿态（固定不变）
	double plane_speed_y;               // 飞机沿y轴的速度（米/秒）

										// 导弹参数
	BYHWICD::CartesianCoordinate missile_init_pos; // 导弹初始位置
	BYHWICD::Euler missile_init_attitude;               // 导弹当前姿态
	//double missile_z;                     // 导弹z轴固定高度（和飞机一致）
	double parabola_k;                    // 抛物线参数（控制x方向运动）
	BYHWICD::CartesianCoordinate m_currPlane_pos, m_currMissile_pos;
	BYHWICD::Euler m_currPlane_att, m_currMissile_att;
	double m_fovH = 5.0, m_fovV = 5.0;
	int m_targetType;

    //讀取文件中的位置
    QVector<realtimeInfo> realTimeData;
    int dataNum = 1;
    double adddate = 1.0;

};

#endif // MAINWINDOW_H
