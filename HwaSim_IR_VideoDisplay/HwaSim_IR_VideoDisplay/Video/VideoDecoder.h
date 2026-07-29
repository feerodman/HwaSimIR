#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QtGlobal>

struct DecodedVideoFrame
{
	QImage image;
	QString payloadCodec;
	QString decoderName;
	bool keyFrame = false;
	qint64 ptsMs = 0;
	double decodeMs = 0.0;
	int decodedChannels = 0;
	QString imageFormat;
};

class IVideoDecoder
{
public:
	virtual ~IVideoDecoder() = default;
	virtual bool decode(
		const QByteArray& payload,
		bool keyFrame,
		qint64 ptsMs,
		DecodedVideoFrame& decoded,
		QString& error) = 0;
	virtual void reset(const QString& reason) = 0;
	virtual bool isAvailable() const = 0;
	virtual QString name() const = 0;
};

class JpegFrameDecoder final : public IVideoDecoder
{
public:
	bool decode(
		const QByteArray& payload,
		bool keyFrame,
		qint64 ptsMs,
		DecodedVideoFrame& decoded,
		QString& error) override;
	void reset(const QString& reason) override;
	bool isAvailable() const override { return true; }
	QString name() const override { return QStringLiteral("qt_qimage_jpeg"); }
};

class H264FfmpegDecoder final : public IVideoDecoder
{
public:
	H264FfmpegDecoder();
	~H264FfmpegDecoder() override;
	bool decode(
		const QByteArray& payload,
		bool keyFrame,
		qint64 ptsMs,
		DecodedVideoFrame& decoded,
		QString& error) override;
	void reset(const QString& reason) override;
	bool isAvailable() const override;
	QString name() const override;
	bool waitingForIdr() const;

private:
	struct Impl;
	Impl* m_impl;
};

