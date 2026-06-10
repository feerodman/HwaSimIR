#pragma once
#include <QObject>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <atomic>
#include "CommonData.h"

class TcpServerWorker : public QObject
{
	Q_OBJECT
public:
	explicit TcpServerWorker(QObject* parent = nullptr);
	~TcpServerWorker();

public slots:
	void doWork();
	void stop() { m_stop = true; };

signals:
	// 图像+跟踪数据+标注 JSON；旧格式包会传入空 JSON，界面层负责生成占位记录。
	void dataReceived(const QImage& img, const BYHWICD::DisplayC2cObjTrackingData& trackingData, const QString& annotationJson);
	// 可选：收到初始化命令时通知主线程（参数可根据需要扩展）
	void initCommandReceived(const BYHWICD::InitP2cObjectTrackingCmd& cmd);

	void controlCmdReceived(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
private:
	QByteArray readExactBytes(QTcpSocket* socket, qint64 count);
	// 发送结构体数据（自动添加总长度和结构体长度头）
	bool sendStruct(QTcpSocket* socket, const void* structPtr, quint32 structSize);
	// 从 NetworkConfig.ini 读取网络配置
	void loadConfig(QString& ip, quint16& port);

	std::atomic<bool> m_stop{ false };
};
