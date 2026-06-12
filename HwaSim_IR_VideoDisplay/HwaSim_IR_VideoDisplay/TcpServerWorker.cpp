#include "TcpServerWorker.h"
#include <QDataStream>
#include <QDebug>
#include <QtEndian>
#include <QtGlobal>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QElapsedTimer>
#include <chrono>
#include <cstring>
#include "CommonData.h"

//#pragma execution_character_set("utf-8")

static int regMetaType1 = qRegisterMetaType<BYHWICD::DisplayC2cObjTrackingData>("BYHWICD::DisplayC2cObjTrackingData");
static int regMetaType2 = qRegisterMetaType<BYHWICD::InitP2cObjectTrackingCmd>("BYHWICD::InitP2cObjectTrackingCmd");
static int regMetaType3 = qRegisterMetaType<BYHWICD::ControlP2cX1ObjTrackingCmd>("BYHWICD::ControlP2cX1ObjTrackingCmd");

namespace
{
qint64 wallTimeNs()
{
	return static_cast<qint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
}

bool looksLikeJpeg(const QByteArray& data)
{
	return data.size() >= 2 &&
		static_cast<unsigned char>(data[0]) == 0xFF &&
		static_cast<unsigned char>(data[1]) == 0xD8;
}

bool parseDisplayFrameBody(
	const QByteArray& body,
	BYHWICD::DisplayC2cObjTrackingData& trackingData,
	QImage& image,
	QString& annotationJson,
	double& jpegDecodeMs)
{
	if (body.size() < 4)
	{
		qWarning() << "显示帧包体过小";
		return false;
	}

	quint32 structLen1 = qFromBigEndian<quint32>(body.constData());
	if (structLen1 != sizeof(BYHWICD::DisplayC2cObjTrackingData))
	{
		qWarning() << "跟踪数据结构体大小不匹配，期望" << sizeof(BYHWICD::DisplayC2cObjTrackingData) << "实际" << structLen1;
		return false;
	}
	if (body.size() < 4 + static_cast<int>(structLen1))
	{
		qWarning() << "显示帧包体不足以容纳跟踪数据";
		return false;
	}

	memcpy(&trackingData, body.constData() + 4, structLen1);
	int offset = 4 + static_cast<int>(structLen1);
	if (body.size() < offset + 4)
	{
		qWarning() << "显示帧缺少第二段长度";
		return false;
	}

	const quint32 structLen2 = qFromBigEndian<quint32>(body.constData() + offset);
	offset += 4;
	if (structLen2 > 50 * 1024 * 1024 || body.size() < offset + static_cast<int>(structLen2))
	{
		qWarning() << "显示帧第二段长度非法:" << structLen2;
		return false;
	}

	QByteArray segment2(body.constData() + offset, structLen2);
	offset += static_cast<int>(structLen2);

	QByteArray jpegData;
	if (looksLikeJpeg(segment2))
	{
		// 旧格式：trackingData + JPEG，未携带标注 JSON。
		annotationJson.clear();
		jpegData = segment2;
	}
	else
	{
		// 新格式：trackingData + annotationJson + JPEG。
		annotationJson = QString::fromUtf8(segment2);
		if (body.size() < offset + 4)
		{
			qWarning() << "新显示帧缺少 JPEG 段长度";
			return false;
		}
		const quint32 structLen3 = qFromBigEndian<quint32>(body.constData() + offset);
		offset += 4;
		if (structLen3 == 0 || structLen3 > 50 * 1024 * 1024 || body.size() < offset + static_cast<int>(structLen3))
		{
			qWarning() << "JPEG 段长度非法:" << structLen3;
			return false;
		}
		jpegData = QByteArray(body.constData() + offset, structLen3);
	}

	QElapsedTimer decodeTimer;
	decodeTimer.start();
	const bool decodeOk = image.loadFromData(jpegData, "JPEG");
	jpegDecodeMs = static_cast<double>(decodeTimer.nsecsElapsed()) / 1.0e6;
	if (!decodeOk)
	{
		qWarning() << "JPEG解码失败";
		return false;
	}
	return true;
}
}

TcpServerWorker::TcpServerWorker(QObject* parent) : QObject(parent) {}
TcpServerWorker::~TcpServerWorker() = default;

void TcpServerWorker::loadConfig(QString& ip, quint16& port)
{
	// 默认值
	ip = "0.0.0.0";
	port = 5555;

	QString configPath = QCoreApplication::applicationDirPath() + "/NetworkConfig.ini";
	if (!QFile::exists(configPath)) {
		qWarning() << "配置文件不存在:" << configPath << "，使用默认值" << ip << port;
		return;
	}

	QSettings settings(configPath, QSettings::IniFormat);
	settings.beginGroup("Network");
	ip = settings.value("ip", ip).toString();
	port = static_cast<quint16>(settings.value("port", port).toUInt());
	settings.endGroup();

	qDebug() << "加载网络配置: IP =" << ip << "端口 =" << port;
}

QByteArray TcpServerWorker::readExactBytes(QTcpSocket* socket, qint64 count)
{
	QByteArray data;
	while (!m_stop && data.size() < count) {
		if (socket->bytesAvailable() <= 0) {
			if (!socket->waitForReadyRead(3000)) {
				if (socket->state() != QAbstractSocket::ConnectedState)
					return QByteArray();
				continue;
			}
		}
		QByteArray chunk = socket->read(count - data.size());
		if (chunk.isEmpty())
			return QByteArray();
		data.append(chunk);
	}
	return data;
}

bool TcpServerWorker::sendStruct(QTcpSocket* socket, const void* structPtr, quint32 structSize)
{
	quint32 totalLen = 4 + 4 + structSize;
	quint32 netTotalLen = qToBigEndian(totalLen);
	quint32 netStructSize = qToBigEndian(structSize);
	QByteArray buffer;
	buffer.append(reinterpret_cast<const char*>(&netTotalLen), 4);
	buffer.append(reinterpret_cast<const char*>(&netStructSize), 4);
	buffer.append(reinterpret_cast<const char*>(structPtr), structSize);
	qint64 sent = socket->write(buffer);
	socket->flush(); // 确保数据发出
	return (sent == buffer.size());
}

void TcpServerWorker::doWork()
{
	// 从配置文件读取 IP 和端口
	QString ip;
	quint16 port;
	loadConfig(ip, port);

	QTcpServer server;
	if (!server.listen(QHostAddress(ip), port)) {
		qWarning() << "监听失败，无法开启端口" << port << ":" << server.errorString();
		return;
	}
	qDebug() << "服务器启动成功，监听" << ip << "端口" << port << "...";

	while (!m_stop) {
		if (!server.waitForNewConnection(1000)) {
			continue;
		}

		QTcpSocket* client = server.nextPendingConnection();
		qDebug() << "客户端已连接:" << client->peerAddress().toString();

		while (!m_stop && client->state() == QAbstractSocket::ConnectedState) {
			// ----- 读取总包长度（4字节，网络序）-----
			QByteArray totalLenRaw = readExactBytes(client, 4);
			if (totalLenRaw.isEmpty()) {
				qWarning() << "客户端断开或读取总长度失败";
				break;
			}
			quint32 totalLen = qFromBigEndian<quint32>(totalLenRaw.constData());
			if (totalLen < 8 || totalLen > 50 * 1024 * 1024) {
				qWarning() << "非法总包长度:" << totalLen;
				break;
			}

			// 兼容最老的纯 JPEG 包：4 字节 JPEG 长度 + JPEG 数据。
			if (client->bytesAvailable() < 2 && !client->waitForReadyRead(3000)) {
				qWarning() << "等待包体起始字节超时";
				break;
			}
			if (looksLikeJpeg(client->peek(2))) {
				QByteArray jpegData = readExactBytes(client, totalLen);
				const qint64 receiveTimeNs = wallTimeNs();
				QImage img;
				QElapsedTimer decodeTimer;
				decodeTimer.start();
				if (!img.loadFromData(jpegData, "JPEG")) {
					qWarning() << "旧 JPEG 包解码失败";
					continue;
				}
				const double jpegDecodeMs = static_cast<double>(decodeTimer.nsecsElapsed()) / 1.0e6;
				BYHWICD::DisplayC2cObjTrackingData trackingData;
				memset(&trackingData, 0, sizeof(trackingData));
				trackingData.flag = 0x38;
				++m_receivedFrameCount;
				emit dataReceived(img, trackingData, QString(), receiveTimeNs, jpegDecodeMs);
				continue;
			}

			// ----- 读取包体（总长度 - 4）-----
			QByteArray body = readExactBytes(client, totalLen - 4);
			if (body.isEmpty()) {
				qWarning() << "读取包体失败";
				break;
			}
			const qint64 receiveTimeNs = wallTimeNs();

			// ----- 解析第一个结构体长度 -----
			if (body.size() < 4) {
				qWarning() << "包体过小";
				break;
			}
			quint32 structLen1 = qFromBigEndian<quint32>(body.constData());
			if (structLen1 < 4 || structLen1 > 1024 * 1024) {
				qWarning() << "非法结构体长度:" << structLen1;
				break;
			}
                if (body.size() < 4 + static_cast<int>(structLen1)) {
				qWarning() << "包体不足以容纳第一个结构体";
				break;
			}

			const char* structData1 = body.constData() + 4;
			int flag = 0;
			memcpy(&flag, structData1, sizeof(int));

			// ---------- 处理初始化命令（单结构体） ----------
			if (flag == 0x36) {
				if (structLen1 != sizeof(BYHWICD::InitP2cObjectTrackingCmd)) {
					qWarning() << "初始化命令结构体大小不匹配";
					break;
				}
				BYHWICD::InitP2cObjectTrackingCmd initCmd;
				memcpy(&initCmd, structData1, structLen1);
				qDebug() << "收到初始化命令，platID=" << initCmd.platID << "sensorID=" << initCmd.sensorID;
				emit initCommandReceived(initCmd);

				//BYHWICD::InitAckC2pObjectTrackingCmd ack;
				//ack.flag = 0x37;
				//ack.JB = initCmd.JB;
				//ack.platID = initCmd.platID;
				//ack.sensorID = initCmd.sensorID;
				//ack.trackingReady = true;
				//if (!sendStruct(client, &ack, sizeof(ack))) {
				//	qWarning() << "发送初始化应答失败";
				//	break;
				//}
				//qDebug() << "已发送初始化应答";
			}
			// ---------- 处理控制命令（单结构体） ----------
			else if (flag == 0x41) {
				if (structLen1 != sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)) {
					qWarning() << "控制命令结构体大小不匹配";
					break;
				}
				BYHWICD::ControlP2cX1ObjTrackingCmd cmd;
				memcpy(&cmd, structData1, structLen1);
				qDebug() << "收到控制命令，simCommand=" << cmd.simCommand;
				emit controlCmdReceived(cmd);
			}
			// ---------- 处理实时成像数据（双结构体：跟踪数据 + JPEG图像） ----------
			else if (flag == 0x38) {
				BYHWICD::DisplayC2cObjTrackingData trackingData;
				QImage img;
				QString annotationJson;
				double jpegDecodeMs = 0.0;
				if (!parseDisplayFrameBody(body, trackingData, img, annotationJson, jpegDecodeMs)) {
					continue;
				}
				++m_receivedFrameCount;
				emit dataReceived(img, trackingData, annotationJson, receiveTimeNs, jpegDecodeMs);
			}
			else {
				qWarning() << "未知的 flag:" << flag << "，忽略当前包";
				// 不中断连接，等待下一个包
				continue;
			}
		}

		client->deleteLater();
		qDebug() << "客户端断开，等待新的连接...\n";
	}

	server.close();
	qDebug() << "TCP 服务器线程退出";
}
