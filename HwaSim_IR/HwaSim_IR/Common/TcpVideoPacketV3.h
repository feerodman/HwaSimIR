#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace HwaSimTcpVideoV3
{
constexpr std::uint32_t kMagic = 0x48575633U; // "HWV3"
constexpr std::uint16_t kVersion = 3U;
constexpr std::uint16_t kHeaderBytes = 56U;
constexpr std::uint32_t kKnownSectionFlags = 0x00000007U;
constexpr std::uint32_t kMaxSectionBytes = 50U * 1024U * 1024U;

enum SectionFlag : std::uint32_t
{
	HasRealtimeData = 1U << 0,
	HasAnnotation = 1U << 1,
	HasVideo = 1U << 2
};

enum CodecId : std::uint8_t
{
	CodecNone = 0,
	CodecJpeg = 1,
	CodecH264AnnexB = 2
};

struct Header
{
	std::uint32_t sectionFlags = 0;
	std::uint8_t codecId = CodecNone;
	bool keyFrame = false;
	std::uint64_t frameSeq = 0;
	std::uint64_t outputOrdinal = 0;
	std::int64_t ptsMs = 0;
	std::uint32_t realtimeBytes = 0;
	std::uint32_t annotationBytes = 0;
	std::uint32_t videoBytes = 0;
};

inline void WriteU16(std::uint8_t* output, std::uint16_t value)
{
	output[0] = static_cast<std::uint8_t>((value >> 8) & 0xffU);
	output[1] = static_cast<std::uint8_t>(value & 0xffU);
}

inline void WriteU32(std::uint8_t* output, std::uint32_t value)
{
	output[0] = static_cast<std::uint8_t>((value >> 24) & 0xffU);
	output[1] = static_cast<std::uint8_t>((value >> 16) & 0xffU);
	output[2] = static_cast<std::uint8_t>((value >> 8) & 0xffU);
	output[3] = static_cast<std::uint8_t>(value & 0xffU);
}

inline void WriteU64(std::uint8_t* output, std::uint64_t value)
{
	for (int index = 7; index >= 0; --index)
	{
		output[7 - index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xffU);
	}
}

inline std::uint16_t ReadU16(const std::uint8_t* input)
{
	return static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(input[0]) << 8) |
		static_cast<std::uint16_t>(input[1]));
}

inline std::uint32_t ReadU32(const std::uint8_t* input)
{
	return (static_cast<std::uint32_t>(input[0]) << 24) |
		(static_cast<std::uint32_t>(input[1]) << 16) |
		(static_cast<std::uint32_t>(input[2]) << 8) |
		static_cast<std::uint32_t>(input[3]);
}

inline std::uint64_t ReadU64(const std::uint8_t* input)
{
	std::uint64_t value = 0;
	for (int index = 0; index < 8; ++index)
	{
		value = (value << 8) | static_cast<std::uint64_t>(input[index]);
	}
	return value;
}

inline std::array<std::uint8_t, kHeaderBytes> EncodeHeader(const Header& header)
{
	std::array<std::uint8_t, kHeaderBytes> bytes = {};
	WriteU32(bytes.data() + 0, kMagic);
	WriteU16(bytes.data() + 4, kVersion);
	WriteU16(bytes.data() + 6, kHeaderBytes);
	WriteU32(bytes.data() + 8, header.sectionFlags);
	bytes[12] = header.codecId;
	bytes[13] = header.keyFrame ? 1U : 0U;
	WriteU16(bytes.data() + 14, 0U);
	WriteU64(bytes.data() + 16, header.frameSeq);
	WriteU64(bytes.data() + 24, header.outputOrdinal);
	WriteU64(bytes.data() + 32, static_cast<std::uint64_t>(header.ptsMs));
	WriteU32(bytes.data() + 40, header.realtimeBytes);
	WriteU32(bytes.data() + 44, header.annotationBytes);
	WriteU32(bytes.data() + 48, header.videoBytes);
	WriteU32(bytes.data() + 52, 0U);
	return bytes;
}

inline bool DecodeHeader(
	const std::uint8_t* bytes,
	std::size_t size,
	Header& header,
	const char** error)
{
	if (error)
	{
		*error = nullptr;
	}
	if (!bytes || size < kHeaderBytes)
	{
		if (error) *error = "v3_header_truncated";
		return false;
	}
	if (ReadU32(bytes + 0) != kMagic)
	{
		if (error) *error = "v3_magic_mismatch";
		return false;
	}
	if (ReadU16(bytes + 4) != kVersion)
	{
		if (error) *error = "v3_version_unsupported";
		return false;
	}
	if (ReadU16(bytes + 6) != kHeaderBytes)
	{
		if (error) *error = "v3_header_size_mismatch";
		return false;
	}

	header.sectionFlags = ReadU32(bytes + 8);
	header.codecId = bytes[12];
	header.keyFrame = bytes[13] != 0;
	header.frameSeq = ReadU64(bytes + 16);
	header.outputOrdinal = ReadU64(bytes + 24);
	header.ptsMs = static_cast<std::int64_t>(ReadU64(bytes + 32));
	header.realtimeBytes = ReadU32(bytes + 40);
	header.annotationBytes = ReadU32(bytes + 44);
	header.videoBytes = ReadU32(bytes + 48);

	if ((header.sectionFlags & ~kKnownSectionFlags) != 0)
	{
		if (error) *error = "v3_unknown_section_flags";
		return false;
	}
	if (header.realtimeBytes > kMaxSectionBytes ||
		header.annotationBytes > kMaxSectionBytes ||
		header.videoBytes > kMaxSectionBytes)
	{
		if (error) *error = "v3_section_too_large";
		return false;
	}
	if (((header.sectionFlags & HasRealtimeData) != 0) != (header.realtimeBytes != 0) ||
		((header.sectionFlags & HasAnnotation) != 0) != (header.annotationBytes != 0) ||
		((header.sectionFlags & HasVideo) != 0) != (header.videoBytes != 0))
	{
		if (error) *error = "v3_flag_length_mismatch";
		return false;
	}
	if ((header.sectionFlags & HasVideo) == 0)
	{
		if (header.codecId != CodecNone || header.keyFrame)
		{
			if (error) *error = "v3_video_metadata_without_video";
			return false;
		}
	}
	else if (header.codecId != CodecJpeg && header.codecId != CodecH264AnnexB)
	{
		if (error) *error = "v3_video_codec_unsupported";
		return false;
	}
	return true;
}

inline std::uint64_t PayloadBytes(const Header& header)
{
	return static_cast<std::uint64_t>(header.realtimeBytes) +
		static_cast<std::uint64_t>(header.annotationBytes) +
		static_cast<std::uint64_t>(header.videoBytes);
}
}
