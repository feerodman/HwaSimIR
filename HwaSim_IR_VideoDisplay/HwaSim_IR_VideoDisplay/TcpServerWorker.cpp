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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <chrono>
#include <cstring>
#include "CommonData.h"
#include "../../HwaSim_IR/HwaSim_IR/Common/TcpVideoPacketV3.h"

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

struct ParsedFramePacket
{
	BYHWICD::DisplayC2cObjTrackingData trackingData = {};
	QString annotationJson;
	QByteArray encodedPayload;
	bool hasVideo = false;
	bool hasRealtimeData = false;
	bool hasAnnotation = false;
	int packetVersion = 2;
	quint32 sectionFlags = 0;
	int codecId = HwaSimTcpVideoV3::CodecNone;
	QString payloadCodec = QStringLiteral("none");
	bool keyFrame = false;
	quint64 frameSeq = 0;
	quint64 outputOrdinal = 0;
	qint64 ptsMs = 0;
	int realtimeBytes = 0;
	int annotationBytes = 0;
	int videoBytes = 0;
};

QString codecText(int codecId)
{
	switch (codecId)
	{
	case HwaSimTcpVideoV3::CodecJpeg:
		return QStringLiteral("jpeg");
	case HwaSimTcpVideoV3::CodecH264AnnexB:
		return QStringLiteral("h264_annexb");
	default:
		return QStringLiteral("none");
	}
}

bool parseV2DisplayFrameBody(
	const QByteArray& body,
	ParsedFramePacket& parsed)
{
	parsed = ParsedFramePacket();
	parsed.packetVersion = 2;
	parsed.hasRealtimeData = true;
	parsed.realtimeBytes = static_cast<int>(sizeof(BYHWICD::DisplayC2cObjTrackingData));
	parsed.sectionFlags |= HwaSimTcpVideoV3::HasRealtimeData;
	if (body.size() < 4)
	{
		qWarning() << QStringLiteral("显示帧包体过小");
		return false;
	}

	const quint32 structLen1 = qFromBigEndian<quint32>(body.constData());
	if (structLen1 != sizeof(BYHWICD::DisplayC2cObjTrackingData))
	{
		qWarning() << QStringLiteral("跟踪数据结构体大小不匹配，期望")
			<< sizeof(BYHWICD::DisplayC2cObjTrackingData) << "实际" << structLen1;
		return false;
	}
	if (body.size() < 4 + static_cast<int>(structLen1))
	{
		qWarning() << QStringLiteral("显示帧包体不足以容纳跟踪数据");
		return false;
	}

	memcpy(&parsed.trackingData, body.constData() + 4, structLen1);
	int offset = 4 + static_cast<int>(structLen1);
	if (body.size() < offset + 4)
	{
		qWarning() << QStringLiteral("显示帧缺少第二段长度");
		return false;
	}

	const quint32 structLen2 = qFromBigEndian<quint32>(body.constData() + offset);
	offset += 4;
	if (structLen2 > HwaSimTcpVideoV3::kMaxSectionBytes ||
		body.size() < offset + static_cast<int>(structLen2))
	{
		qWarning() << QStringLiteral("显示帧第二段长度非法:") << structLen2;
		return false;
	}

	const QByteArray segment2(body.constData() + offset, structLen2);
	offset += static_cast<int>(structLen2);
	if (looksLikeJpeg(segment2))
	{
		// 旧 v2：trackingData + JPEG，没有 annotation。
		parsed.encodedPayload = segment2;
	}
	else
	{
		parsed.hasAnnotation = true;
		parsed.annotationBytes = static_cast<int>(structLen2);
		parsed.sectionFlags |= HwaSimTcpVideoV3::HasAnnotation;
		parsed.annotationJson = QString::fromUtf8(segment2);
		if (body.size() < offset + 4)
		{
			qWarning() << QStringLiteral("v2 显示帧缺少视频段长度");
			return false;
		}
		const quint32 structLen3 = qFromBigEndian<quint32>(body.constData() + offset);
		offset += 4;
		if (structLen3 == 0 || structLen3 > HwaSimTcpVideoV3::kMaxSectionBytes ||
			body.size() < offset + static_cast<int>(structLen3))
		{
			qWarning() << QStringLiteral("v2 视频段长度非法:") << structLen3;
			return false;
		}
		parsed.encodedPayload = QByteArray(body.constData() + offset, structLen3);
		offset += static_cast<int>(structLen3);
	}
	if (offset != body.size())
	{
		qWarning() << QStringLiteral("v2 显示帧存在未解析尾部字节:") << (body.size() - offset);
		return false;
	}

	parsed.hasVideo = true;
	parsed.videoBytes = parsed.encodedPayload.size();
	parsed.sectionFlags |= HwaSimTcpVideoV3::HasVideo;
	parsed.codecId = HwaSimTcpVideoV3::CodecJpeg;
	parsed.payloadCodec = QStringLiteral("jpeg");
	if (parsed.hasAnnotation)
	{
		QJsonParseError parseError;
		const QJsonDocument document =
			QJsonDocument::fromJson(parsed.annotationJson.toUtf8(), &parseError);
		if (parseError.error == QJsonParseError::NoError && document.isObject())
		{
			const QJsonObject object = document.object();
			parsed.payloadCodec = object.value(QStringLiteral("payloadCodec"))
				.toString(object.value(QStringLiteral("codec")).toString(QStringLiteral("jpeg")));
			parsed.codecId = parsed.payloadCodec == QStringLiteral("h264_annexb")
				? HwaSimTcpVideoV3::CodecH264AnnexB
				: (parsed.payloadCodec == QStringLiteral("jpeg")
					? HwaSimTcpVideoV3::CodecJpeg
					: HwaSimTcpVideoV3::CodecNone);
			parsed.keyFrame = object.value(QStringLiteral("keyFrame")).toBool(false);
			parsed.ptsMs = static_cast<qint64>(
				object.value(QStringLiteral("ptsMs")).toDouble(0.0));
			parsed.frameSeq = object.value(QStringLiteral("sourceSeq"))
				.toVariant().toULongLong();
			if (parsed.frameSeq == 0)
			{
				parsed.frameSeq = object.value(QStringLiteral("frameSeq"))
					.toVariant().toULongLong();
			}
			parsed.outputOrdinal = object.value(QStringLiteral("outputOrdinal"))
				.toVariant().toULongLong();
			const int declaredBytes =
				object.value(QStringLiteral("encodedBytes")).toInt(parsed.videoBytes);
			if (declaredBytes != parsed.videoBytes)
			{
				qWarning().noquote()
					<< QStringLiteral("[VideoDecoder] encodedBytesMismatch declared=%1 actual=%2")
						.arg(declaredBytes).arg(parsed.videoBytes);
			}
		}
	}
	return true;
}

bool parseV3DisplayFrameBody(
	const QByteArray& body,
	ParsedFramePacket& parsed)
{
	parsed = ParsedFramePacket();
	HwaSimTcpVideoV3::Header header;
	const char* decodeError = nullptr;
	if (!HwaSimTcpVideoV3::DecodeHeader(
		reinterpret_cast<const std::uint8_t*>(body.constData()),
		static_cast<std::size_t>(body.size()),
		header,
		&decodeError))
	{
		qWarning().noquote() << QStringLiteral("[TcpFramePacketRx] packetVersion=3 parseFailed=1 reason=%1")
			.arg(QString::fromLatin1(decodeError ? decodeError : "unknown"));
		return false;
	}
	const std::uint64_t expectedBodyBytes =
		HwaSimTcpVideoV3::kHeaderBytes +
		HwaSimTcpVideoV3::PayloadBytes(header);
	if (expectedBodyBytes != static_cast<std::uint64_t>(body.size()))
	{
		qWarning().noquote() << QStringLiteral("[TcpFramePacketRx] packetVersion=3 parseFailed=1 reason=body_length_mismatch expected=%1 actual=%2")
			.arg(expectedBodyBytes).arg(body.size());
		return false;
	}

	parsed.packetVersion = 3;
	parsed.sectionFlags = header.sectionFlags;
	parsed.codecId = header.codecId;
	parsed.payloadCodec = codecText(header.codecId);
	parsed.keyFrame = header.keyFrame;
	parsed.frameSeq = header.frameSeq;
	parsed.outputOrdinal = header.outputOrdinal;
	parsed.ptsMs = header.ptsMs;
	parsed.realtimeBytes = static_cast<int>(header.realtimeBytes);
	parsed.annotationBytes = static_cast<int>(header.annotationBytes);
	parsed.videoBytes = static_cast<int>(header.videoBytes);
	parsed.hasRealtimeData =
		(header.sectionFlags & HwaSimTcpVideoV3::HasRealtimeData) != 0;
	parsed.hasAnnotation =
		(header.sectionFlags & HwaSimTcpVideoV3::HasAnnotation) != 0;
	parsed.hasVideo =
		(header.sectionFlags & HwaSimTcpVideoV3::HasVideo) != 0;

	int offset = HwaSimTcpVideoV3::kHeaderBytes;
	if (parsed.hasRealtimeData)
	{
		if (header.realtimeBytes != sizeof(BYHWICD::DisplayC2cObjTrackingData))
		{
			qWarning().noquote() << QStringLiteral("[TcpFramePacketRx] packetVersion=3 parseFailed=1 reason=realtime_size_mismatch expected=%1 actual=%2")
				.arg(sizeof(BYHWICD::DisplayC2cObjTrackingData))
				.arg(header.realtimeBytes);
			return false;
		}
		memcpy(
			&parsed.trackingData,
			body.constData() + offset,
			sizeof(BYHWICD::DisplayC2cObjTrackingData));
		offset += parsed.realtimeBytes;
	}
	if (parsed.hasAnnotation)
	{
		parsed.annotationJson =
			QString::fromUtf8(body.constData() + offset, parsed.annotationBytes);
		offset += parsed.annotationBytes;
	}
	if (parsed.hasVideo)
	{
		parsed.encodedPayload =
			QByteArray(body.constData() + offset, parsed.videoBytes);
		offset += parsed.videoBytes;
	}
	return offset == body.size();
}

bool decodeVideoSection(
	const ParsedFramePacket& parsed,
	JpegFrameDecoder& jpegDecoder,
	H264FfmpegDecoder& h264Decoder,
	DecodedVideoFrame& decoded)
{
	if (!parsed.hasVideo)
	{
		return true;
	}
	QString decodeError;
	IVideoDecoder* decoder = &jpegDecoder;
	if (parsed.codecId == HwaSimTcpVideoV3::CodecH264AnnexB)
	{
		decoder = &h264Decoder;
		if (!h264Decoder.isAvailable())
		{
			qWarning().noquote() << QStringLiteral("[CodecFallback] receiverCannotDecode=1 payloadCodec=h264_annexb reason=ffmpeg_sdk_not_compiled");
			return false;
		}
	}
	else if (parsed.codecId != HwaSimTcpVideoV3::CodecJpeg)
	{
		qWarning().noquote() << QStringLiteral("[VideoDecoder] unsupportedPayloadCodec=%1")
			.arg(parsed.payloadCodec);
		return false;
	}
	if (!decoder->decode(
		parsed.encodedPayload,
		parsed.keyFrame,
		parsed.ptsMs,
		decoded,
		decodeError))
	{
		if (decodeError != QStringLiteral("waiting_for_decodable_idr") && decodeError != QStringLiteral("decoder_needs_more_data"))
			qWarning().noquote() << QStringLiteral("[VideoDecoder] decodeFailed=1 payloadCodec=%1 reason=%2")
				.arg(parsed.payloadCodec, decodeError);
		return false;
	}
	return true;
}
}

TcpServerWorker::TcpServerWorker(const QString& networkConfigPath,
	const QString& channel, int platID, int sensorID, QObject* parent)
	: QObject(parent), m_networkConfigPath(networkConfigPath),
	m_channel(channel), m_platID(platID), m_sensorID(sensorID)
{
	m_jpegDecoder.reset(new JpegFrameDecoder());
	m_h264Decoder.reset(new H264FfmpegDecoder());
	qDebug().noquote() << QStringLiteral("[VideoDecoder] channel=%1 platID=%2 sensorID=%3 pid=%4 jpegAvailable=1 h264Available=%5 h264Decoder=%6")
		.arg(m_channel).arg(m_platID).arg(m_sensorID).arg(QCoreApplication::applicationPid())
		.arg(m_h264Decoder->isAvailable() ? 1 : 0)
		.arg(m_h264Decoder->name());
}
TcpServerWorker::~TcpServerWorker() = default;

void TcpServerWorker::resetVideoDecoders(const QString& reason)
{
	qDebug().noquote() << QStringLiteral("[VideoDecoder] channel=%1 platID=%2 sensorID=%3 pid=%4 reset=1 reason=%5")
		.arg(m_channel).arg(m_platID).arg(m_sensorID).arg(QCoreApplication::applicationPid()).arg(reason);
	m_jpegDecoder->reset(reason);
	m_h264Decoder->reset(reason);
	m_lastDecoderPerfLogNs = 0;
	m_decodeMsTotal = 0.0;
	m_decodeSampleCount = 0;
}

void TcpServerWorker::logDecoderPerf(const DecodedVideoFrame& decoded, int payloadBytes)
{
	++m_decodeSampleCount;
	m_decodeMsTotal += decoded.decodeMs;
	const qint64 nowNs = wallTimeNs();
	if (m_decodeSampleCount <= 3 || (m_decodeSampleCount % 120) == 0 ||
		nowNs - m_lastDecoderPerfLogNs >= 2000000000LL)
	{
		qDebug().noquote() << QStringLiteral("[VideoDecoder] channel=%1 platID=%2 sensorID=%3 pid=%4 frame=%5 payloadCodec=%6 decoderName=%7 decodeMs=%8 payloadBytes=%9 keyFrame=%10 decodeMsAvg=%11 waitingForIdr=%12")
			.arg(m_channel).arg(m_platID).arg(m_sensorID).arg(QCoreApplication::applicationPid())
			.arg(m_decodeSampleCount)
			.arg(decoded.payloadCodec)
			.arg(decoded.decoderName)
			.arg(decoded.decodeMs, 0, 'f', 3)
			.arg(payloadBytes)
			.arg(decoded.keyFrame ? 1 : 0)
			.arg(m_decodeMsTotal / static_cast<double>(m_decodeSampleCount), 0, 'f', 3)
			.arg(m_h264Decoder->waitingForIdr() ? 1 : 0);
		if (decoded.payloadCodec == QStringLiteral("h264_annexb"))
		{
			qDebug().noquote() << QStringLiteral("[H264Perf] channel=%1 platID=%2 sensorID=%3 pid=%4 decodeMs=%5 payloadBytes=%6 keyFrame=%7")
				.arg(m_channel).arg(m_platID).arg(m_sensorID).arg(QCoreApplication::applicationPid())
				.arg(decoded.decodeMs, 0, 'f', 3)
				.arg(payloadBytes)
				.arg(decoded.keyFrame ? 1 : 0);
		}
		m_lastDecoderPerfLogNs = nowNs;
	}
}

void TcpServerWorker::loadConfig(QString& ip, quint16& port)
{
	// 默认值
	ip = "0.0.0.0";
	port = 5555;

	QString configPath = m_networkConfigPath.trimmed();
	if (configPath.isEmpty())
	{
		configPath = QCoreApplication::applicationDirPath() + "/NetworkConfig.ini";
	}
	if (!QFile::exists(configPath)) {
		qWarning() << QStringLiteral("配置文件不存在:") << configPath << QStringLiteral("，使用默认值") << ip << port;
		return;
	}

	QSettings settings(configPath, QSettings::IniFormat);
	settings.beginGroup("Network");
	ip = settings.value("ip", ip).toString();
	port = static_cast<quint16>(settings.value("port", port).toUInt());
	settings.endGroup();

	qDebug() << QStringLiteral("LoadNetworkConfig: IP =") << ip << QStringLiteral("port =") << port;
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
		qWarning() << QStringLiteral("监听失败，无法开启端口") << port << ":" << server.errorString();
		return;
	}
	qDebug() << QStringLiteral("ServerRunSuccess，监听") << ip << QStringLiteral("port") << port << "...";

	while (!m_stop) {
		if (!server.waitForNewConnection(1000)) {
			continue;
		}

		QTcpSocket* client = server.nextPendingConnection();
		resetVideoDecoders(QStringLiteral("tcp_connected"));
		qDebug() << QStringLiteral("clientConnection:") << client->peerAddress().toString();

		while (!m_stop && client->state() == QAbstractSocket::ConnectedState) {
			// ----- 读取总包长度（4字节，网络序）-----
			QByteArray totalLenRaw = readExactBytes(client, 4);
			if (totalLenRaw.isEmpty()) {
				qWarning() << QStringLiteral("1客户端断开或读取总长度失败");
				break;
			}
			quint32 totalLen = qFromBigEndian<quint32>(totalLenRaw.constData());
			if (totalLen < 8 || totalLen > 50 * 1024 * 1024) {
				qWarning() << QStringLiteral("2非法总包长度:") << totalLen;
				break;
			}

			// 兼容最老的纯 JPEG 包：4 字节 JPEG 长度 + JPEG 数据。
			if (client->bytesAvailable() < 2 && !client->waitForReadyRead(3000)) {
				qWarning() << QStringLiteral("3等待包体起始字节超时");
				break;
			}
			if (looksLikeJpeg(client->peek(2))) {
				QByteArray jpegData = readExactBytes(client, totalLen);
				const qint64 receiveTimeNs = wallTimeNs();
				DecodedVideoFrame decoded;
				QString decodeError;
				if (!m_jpegDecoder->decode(jpegData, false, 0, decoded, decodeError)) {
					qWarning() << QStringLiteral("4旧 JPEG 包解码失败");
					continue;
				}
				BYHWICD::DisplayC2cObjTrackingData trackingData;
				memset(&trackingData, 0, sizeof(trackingData));
				++m_receivedPacketCount;
				++m_receivedFrameCount;
				logDecoderPerf(decoded, jpegData.size());
				emit dataReceived(
					decoded.image,
					trackingData,
					QString(),
					true,
					false,
					false,
					1,
					HwaSimTcpVideoV3::HasVideo,
					HwaSimTcpVideoV3::CodecJpeg,
					false,
					0,
					0,
					0,
					receiveTimeNs,
					decoded.decodeMs,
					decoded.decodedChannels,
					decoded.imageFormat);
				continue;
			}

			// ----- 读取包体（总长度 - 4）-----
			QByteArray body = readExactBytes(client, totalLen - 4);
			if (body.isEmpty()) {
				qWarning() << QStringLiteral("5读取包体失败");
				break;
			}
			const qint64 receiveTimeNs = wallTimeNs();

			if (body.size() >= HwaSimTcpVideoV3::kHeaderBytes &&
				HwaSimTcpVideoV3::ReadU32(
					reinterpret_cast<const std::uint8_t*>(body.constData())) ==
					HwaSimTcpVideoV3::kMagic)
			{
				ParsedFramePacket parsed;
				if (!parseV3DisplayFrameBody(body, parsed))
				{
					continue;
				}
				const quint64 packetCount = ++m_receivedPacketCount;
				if (parsed.keyFrame || packetCount <= 3 || (packetCount % 120) == 0)
				{
					qInfo().noquote() << QStringLiteral("[TcpFramePacketRx] channel=%1 platID=%2 sensorID=%3 pid=%4 packetVersion=%5 flags=0x%6 codec=%7 keyFrame=%8 realtimeBytes=%9 annotationBytes=%10 videoBytes=%11 frameSeq=%12 outputOrdinal=%13 ptsMs=%14 hasVideo=%15 hasAnnotation=%16 hasRealtimeData=%17")
						.arg(m_channel).arg(m_platID).arg(m_sensorID)
						.arg(QCoreApplication::applicationPid())
						.arg(parsed.packetVersion)
						.arg(parsed.sectionFlags, 0, 16)
						.arg(parsed.payloadCodec)
						.arg(parsed.keyFrame ? 1 : 0)
						.arg(parsed.realtimeBytes)
						.arg(parsed.annotationBytes)
						.arg(parsed.videoBytes)
						.arg(parsed.frameSeq)
						.arg(parsed.outputOrdinal)
						.arg(parsed.ptsMs)
						.arg(parsed.hasVideo ? 1 : 0)
						.arg(parsed.hasAnnotation ? 1 : 0)
						.arg(parsed.hasRealtimeData ? 1 : 0);
				}
				DecodedVideoFrame decoded;
				if (!decodeVideoSection(
					parsed,
					*m_jpegDecoder,
					*m_h264Decoder,
					decoded))
				{
					continue;
				}
				if (parsed.hasVideo)
				{
					++m_receivedFrameCount;
					logDecoderPerf(decoded, parsed.videoBytes);
				}
				emit dataReceived(
					decoded.image,
					parsed.trackingData,
					parsed.annotationJson,
					parsed.hasVideo,
					parsed.hasRealtimeData,
					parsed.hasAnnotation,
					parsed.packetVersion,
					parsed.sectionFlags,
					parsed.codecId,
					parsed.keyFrame,
					parsed.frameSeq,
					parsed.outputOrdinal,
					parsed.ptsMs,
					receiveTimeNs,
					parsed.hasVideo ? decoded.decodeMs : 0.0,
					parsed.hasVideo ? decoded.decodedChannels : 0,
					parsed.hasVideo ? decoded.imageFormat : QStringLiteral("none"));
				continue;
			}

			// ----- 解析第一个结构体长度 -----
			if (body.size() < 4) {
				qWarning() << QStringLiteral("6包体过小");
				break;
			}
			quint32 structLen1 = qFromBigEndian<quint32>(body.constData());
			if (structLen1 < 4 || structLen1 > 1024 * 1024) {
				qWarning() << QStringLiteral("7非法结构体长度:") << structLen1;
				break;
			}
                if (body.size() < 4 + static_cast<int>(structLen1)) {
				qWarning() << QStringLiteral("8包体不足以容纳第一个结构体");
				break;
			}

			const char* structData1 = body.constData() + 4;
			int flag = 0;
			memcpy(&flag, structData1, sizeof(int));

			// ---------- 处理初始化命令（单结构体） ----------
			if (flag == 0x36) {
				if (structLen1 != sizeof(BYHWICD::InitP2cObjectTrackingCmd)) {
					qWarning() << QStringLiteral("9初始化命令结构体大小不匹配");
					break;
				}
				BYHWICD::InitP2cObjectTrackingCmd initCmd;
				memcpy(&initCmd, structData1, structLen1);
				resetVideoDecoders(QStringLiteral("initialization"));
				qDebug() << QStringLiteral("initCmd，platID=") << initCmd.platID << "sensorID=" << initCmd.sensorID;
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
					qWarning() << QStringLiteral("0控制命令结构体大小不匹配");
					break;
				}
				BYHWICD::ControlP2cX1ObjTrackingCmd cmd;
				memcpy(&cmd, structData1, structLen1);
				resetVideoDecoders(QStringLiteral("control_or_round_change"));
				qDebug() << QStringLiteral("ControlCmd，simCommand=") << cmd.simCommand;
				emit controlCmdReceived(cmd);
			}
			// ---------- 处理实时成像数据（双结构体：跟踪数据 + JPEG图像） ----------
			else if (flag == 0x38) {
				ParsedFramePacket parsed;
				DecodedVideoFrame decoded;
				if (!parseV2DisplayFrameBody(body, parsed) ||
					!decodeVideoSection(
						parsed,
						*m_jpegDecoder,
						*m_h264Decoder,
						decoded)) {
					continue;
				}
				const quint64 packetCount = ++m_receivedPacketCount;
				++m_receivedFrameCount;
				logDecoderPerf(decoded, parsed.videoBytes);
				if (parsed.keyFrame || packetCount <= 3 || (packetCount % 120) == 0)
				{
					qInfo().noquote() << QStringLiteral("[TcpFramePacketRx] channel=%1 platID=%2 sensorID=%3 pid=%4 packetVersion=2 flags=0x%5 codec=%6 keyFrame=%7 realtimeBytes=%8 annotationBytes=%9 videoBytes=%10 frameSeq=%11 outputOrdinal=%12 ptsMs=%13")
						.arg(m_channel).arg(m_platID).arg(m_sensorID)
						.arg(QCoreApplication::applicationPid())
						.arg(parsed.sectionFlags, 0, 16)
						.arg(parsed.payloadCodec)
						.arg(parsed.keyFrame ? 1 : 0)
						.arg(parsed.realtimeBytes)
						.arg(parsed.annotationBytes)
						.arg(parsed.videoBytes)
						.arg(parsed.frameSeq)
						.arg(parsed.outputOrdinal)
						.arg(parsed.ptsMs);
				}
				emit dataReceived(
					decoded.image,
					parsed.trackingData,
					parsed.annotationJson,
					true,
					true,
					parsed.hasAnnotation,
					2,
					parsed.sectionFlags,
					parsed.codecId,
					parsed.keyFrame,
					parsed.frameSeq,
					parsed.outputOrdinal,
					parsed.ptsMs,
					receiveTimeNs,
					decoded.decodeMs,
					decoded.decodedChannels,
					decoded.imageFormat);
			}
			else {
				qWarning() << QStringLiteral("未知的 flag:") << flag << QStringLiteral("，忽略当前包");
				// 不中断连接，等待下一个包
				continue;
			}
		}

		resetVideoDecoders(QStringLiteral("tcp_disconnected"));
		client->deleteLater();
		qDebug() << QStringLiteral("客户端断开，等待新的连接...\n");
	}

	server.close();
	qDebug() << QStringLiteral("TCP 服务器线程退出");
}
