#include "VideoDecoder.h"

#include <QElapsedTimer>

bool JpegFrameDecoder::decode(
	const QByteArray& payload,
	bool keyFrame,
	qint64 ptsMs,
	DecodedVideoFrame& decoded,
	QString& error)
{
	QElapsedTimer timer;
	timer.start();
	QImage image;
	if (!image.loadFromData(payload, "JPEG"))
	{
		error = QStringLiteral("jpeg_decode_failed");
		return false;
	}
	decoded.image = image;
	decoded.payloadCodec = QStringLiteral("jpeg");
	decoded.decoderName = name();
	decoded.keyFrame = keyFrame;
	decoded.ptsMs = ptsMs;
	decoded.decodeMs = static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
	decoded.decodedChannels = image.isGrayscale() ? 1 : 3;
	decoded.imageFormat = image.isGrayscale() ? QStringLiteral("grayscale") : QStringLiteral("rgb");
	error.clear();
	return true;
}

void JpegFrameDecoder::reset(const QString& reason)
{
	Q_UNUSED(reason);
}

