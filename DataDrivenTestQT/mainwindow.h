//#pragma once
//
//#include <QtWidgets/QMainWindow>
//#include "ui_MainWindow.h"
//
//class MainWindow : public QMainWindow
//{
//    Q_OBJECT
//
//public:
//    MainWindow(QWidget *parent = Q_NULLPTR);
//
//private:
//    Ui::MainWindowClass ui;
//};


// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
//#define _USE_MATH_DEFINES
#include <QMainWindow>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QString>
#include "CommonData.h"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <QVector>
#include "ICD/common_data.h"

using namespace ICD;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();
	void setH264EnabledForTest(bool enabled) { m_h264Enabled = enabled; }

	private slots:
	void onResetButtonClicked();
	void onInitButtonClicked();
	void onStartButtonClicked();
	void onStopButtonClicked();
	void onSendRealTimeData();
	void updatePosition();

private:
	void setupUI();
	void loadNetworkConfig();
	void setupUDP();
	void sendControlCommand(int command);
	void sendInitCommand();
	void sendRealTimeData();
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

	// UI Components
	QGroupBox *m_configGroup;
	QGroupBox *m_controlGroup;
	QGroupBox *m_realTimeDataGroup;
	QGroupBox *m_statusGroup;

	QLineEdit *m_localIpEdit;
	QLineEdit *m_localPortEdit;
	QLineEdit *m_remoteIpEdit;
	QLineEdit *m_remotePortEdit;
	QLineEdit *m_platIDEdit;
	QLineEdit *m_sensorIDEdit;
	QLineEdit *m_currentRoundEdit;
	QLineEdit *m_roundCutEdit;

	QLineEdit *m_latEdit;
	QLineEdit *m_lonEdit;
	QLineEdit *m_altEdit;
	QLineEdit *m_yawEdit;
	QLineEdit *m_pitchEdit;
	QLineEdit *m_rollEdit;
	QLineEdit *m_latEditTarget;
	QLineEdit *m_lonEditTarget;
	QLineEdit *m_altEditTarget;
	QLineEdit *m_yawEditTarget;
	QLineEdit *m_pitchEditTarget;
	QLineEdit *m_rollEditTarget;
	QLineEdit *m_collisionTime;
	QLineEdit *m_speed;
	QLineEdit *m_timeStep;
	QLineEdit *m_fovHEdit;
	QLineEdit *m_fovVEdit;
	QLineEdit *m_targetTypeEdit;
	QLineEdit *m_videoFpsEdit;


	


	QPushButton *m_resetButton;
	QPushButton *m_initButton;
	QPushButton *m_startButton;
	QPushButton *m_stopButton;

	QLabel *m_statusLabel;
	QLabel *m_lastSentLabel;
	QLabel *m_lastReceivedLabel;

	// UDP Socket
	QUdpSocket *m_udpSocket;
	QTimer *m_realTimeTimer;
	QElapsedTimer m_sendClock;
	bool m_isRealtimeSending = false;
	quint64 m_sentFrameCount = 0;
	quint64 m_sendDeadlineIndex = 0;
	qint64 m_lastSendPerfLogNs = 0;
	quint64 m_lastSendPerfFrameCount = 0;
	int m_uiUpdateEveryFrames = 12;
	int m_targetVideoFps = 60;
	bool m_h264Enabled = false;

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
