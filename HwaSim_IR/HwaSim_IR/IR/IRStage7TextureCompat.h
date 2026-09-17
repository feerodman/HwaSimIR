#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace IRStage7TextureCompat
{
// Panda stores four-component unsigned-byte RAM images in BGRA order.  Some
// deployed Panda3D builds predate the 1.10.9 fix that made set_ram_image_as()
// convert every page of a 3-D texture.  Convert the complete volume explicitly
// and hand the native bytes to set_ram_image() so R=density and G=appearance
// remain unchanged on every platform.
template <typename ByteContainer>
std::vector<unsigned char> RgbaToPandaBgra(const ByteContainer& rgba)
{
	if ((rgba.size() % 4u) != 0u)
	{
		throw std::invalid_argument("RGBA volume byte count is not divisible by four");
	}

	std::vector<unsigned char> bgra(rgba.size());
	for (std::size_t offset = 0; offset < rgba.size(); offset += 4u)
	{
		bgra[offset] = rgba[offset + 2u];
		bgra[offset + 1u] = rgba[offset + 1u];
		bgra[offset + 2u] = rgba[offset];
		bgra[offset + 3u] = rgba[offset + 3u];
	}
	return bgra;
}
}
