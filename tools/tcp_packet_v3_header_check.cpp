#include "../HwaSim_IR/HwaSim_IR/Common/TcpVideoPacketV3.h"

#include <cstdlib>
#include <iostream>

namespace
{
bool roundTrip(
	std::uint32_t flags,
	std::uint8_t codec,
	bool keyFrame,
	std::uint32_t realtimeBytes,
	std::uint32_t annotationBytes,
	std::uint32_t videoBytes)
{
	HwaSimTcpVideoV3::Header input;
	input.sectionFlags = flags;
	input.codecId = codec;
	input.keyFrame = keyFrame;
	input.frameSeq = 0x0102030405060708ULL;
	input.outputOrdinal = 0x1112131415161718ULL;
	input.ptsMs = 0x2122232425262728LL;
	input.realtimeBytes = realtimeBytes;
	input.annotationBytes = annotationBytes;
	input.videoBytes = videoBytes;
	const auto bytes = HwaSimTcpVideoV3::EncodeHeader(input);

	HwaSimTcpVideoV3::Header output;
	const char* error = nullptr;
	if (!HwaSimTcpVideoV3::DecodeHeader(bytes.data(), bytes.size(), output, &error))
	{
		std::cerr << "round-trip decode failed: "
			<< (error ? error : "unknown") << '\n';
		return false;
	}
	return output.sectionFlags == input.sectionFlags &&
		output.codecId == input.codecId &&
		output.keyFrame == input.keyFrame &&
		output.frameSeq == input.frameSeq &&
		output.outputOrdinal == input.outputOrdinal &&
		output.ptsMs == input.ptsMs &&
		output.realtimeBytes == input.realtimeBytes &&
		output.annotationBytes == input.annotationBytes &&
		output.videoBytes == input.videoBytes;
}
}

int main()
{
	using namespace HwaSimTcpVideoV3;
	bool ok = true;
	ok = roundTrip(HasVideo, CodecJpeg, false, 0, 0, 1234) && ok;
	ok = roundTrip(HasVideo, CodecH264AnnexB, true, 0, 0, 4321) && ok;
	ok = roundTrip(HasVideo | HasAnnotation, CodecH264AnnexB, true, 0, 75, 4321) && ok;
	ok = roundTrip(HasVideo | HasRealtimeData, CodecH264AnnexB, false, 2048, 0, 1234) && ok;
	ok = roundTrip(
		HasVideo | HasAnnotation | HasRealtimeData,
		CodecH264AnnexB,
		false,
		2048,
		75,
		4321) && ok;
	ok = roundTrip(HasAnnotation | HasRealtimeData, CodecNone, false, 2048, 75, 0) && ok;

	Header invalid;
	invalid.sectionFlags = HasVideo;
	invalid.codecId = CodecJpeg;
	invalid.videoBytes = 0;
	auto invalidBytes = EncodeHeader(invalid);
	Header decoded;
	const char* error = nullptr;
	if (DecodeHeader(invalidBytes.data(), invalidBytes.size(), decoded, &error))
	{
		std::cerr << "invalid flag/length combination was accepted\n";
		ok = false;
	}

	std::cout << "[TcpPacketV3HeaderCheck] " << (ok ? "PASS" : "FAIL")
		<< " headerBytes=" << kHeaderBytes
		<< " magic=0x" << std::hex << kMagic << std::dec << '\n';
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
