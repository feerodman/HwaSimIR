#pragma once
#include <QObject>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <atomic>
#include <memory>
#include "CommonData.h"
#include "Video/VideoDecoder.h"

class TcpServerWorker : public QObject
{
	Q_OBJECT
public:
	explicit TcpServerWorker(const QString& networkConfigPath = QString(),
		const QString& channel = QStringLiteral("unknown"), int platID = 0, int sensorID = 0,
		QObject* parent = nullptr);
	~TcpServerWorker();
	quint64 receivedFrameCount() const { return m_receivedFrameCount.load(); }

public slots:
	void doWork();
	void stop() { m_stop = true; };

signals:
	// 图像+跟踪数据+标注 JSON；旧格式包会传入空 JSON，界面层负责生成占位记录。
	void dataReceived(
		const QImage& img,
		const BYHWICD::DisplayC2cObjTrackingData& trackingData,
		const QString& annotationJson,
		bool hasVideo,
		bool hasRealtimeData,
		bool hasAnnotation,
		int packetVersion,
		quint32 sectionFlags,
		int codecId,
		bool keyFrame,
		quint64 frameSeq,
		quint64 outputOrdinal,
		qint64 ptsMs,
		qint64 receiveTimeNs,
		double jpegDecodeMs,
		int decodedChannels,
		const QString& imageFormat);
	// 可选：收到初始化命令时通知主线程（参数可根据需要扩展）
	void initCommandReceived(const BYHWICD::InitP2cObjectTrackingCmd& cmd);

	void controlCmdReceived(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
private:
	QByteArray readExactBytes(QTcpSocket* socket, qint64 count);
	// 发送结构体数据（自动添加总长度和结构体长度头）
	bool sendStruct(QTcpSocket* socket, const void* structPtr, quint32 structSize);
	// 从 NetworkConfig.ini 读取网络配置
	void loadConfig(QString& ip, quint16& port);
	void resetVideoDecoders(const QString& reason);
	void logDecoderPerf(const DecodedVideoFrame& decoded, int payloadBytes);

	std::atomic<bool> m_stop{ false };
	std::atomic<quint64> m_receivedFrameCount{ 0 };
	std::atomic<quint64> m_receivedPacketCount{ 0 };
	QString m_networkConfigPath;
	QString m_channel;
	int m_platID = 0;
	int m_sensorID = 0;
	std::unique_ptr<JpegFrameDecoder> m_jpegDecoder;
	std::unique_ptr<H264FfmpegDecoder> m_h264Decoder;
	qint64 m_lastDecoderPerfLogNs = 0;
	double m_decodeMsTotal = 0.0;
	quint64 m_decodeSampleCount = 0;
};
