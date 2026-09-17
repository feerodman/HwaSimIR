#include "IR/IRStage7TextureCompat.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
std::uint64_t Fnv1a64(const std::vector<unsigned char>& bytes)
{
	std::uint64_t value = 1469598103934665603ULL;
	for (const unsigned char byte : bytes)
	{
		value ^= byte;
		value *= 1099511628211ULL;
	}
	return value;
}
}

int main()
{
	const std::vector<unsigned char> rgba = {
		0x11, 0x22, 0x33, 0x44,
		0xaa, 0xbb, 0xcc, 0xdd,
	};
	const std::vector<unsigned char> expected = {
		0x33, 0x22, 0x11, 0x44,
		0xcc, 0xbb, 0xaa, 0xdd,
	};
	const std::vector<unsigned char> bgra = IRStage7TextureCompat::RgbaToPandaBgra(rgba);
	if (bgra != expected)
	{
		std::cerr << "[P11Stage7TextureCompat][FAIL] reason=unexpected_channel_mapping\n";
		return 1;
	}
	if (IRStage7TextureCompat::RgbaToPandaBgra(bgra) != rgba)
	{
		std::cerr << "[P11Stage7TextureCompat][FAIL] reason=conversion_not_involutive\n";
		return 2;
	}
	bool rejected = false;
	try
	{
		(void)IRStage7TextureCompat::RgbaToPandaBgra(std::vector<unsigned char>{1, 2, 3});
	}
	catch (const std::invalid_argument&)
	{
		rejected = true;
	}
	if (!rejected)
	{
		std::cerr << "[P11Stage7TextureCompat][FAIL] reason=invalid_size_accepted\n";
		return 3;
	}

	std::cout << "[P11Stage7TextureCompat] result=PASS"
		<< " sourceOrder=RGBA ramOrder=BGRA"
		<< " inputFNV64=" << std::hex << Fnv1a64(rgba)
		<< " outputFNV64=" << Fnv1a64(bgra) << std::dec
		<< " invariant=only_R_B_swapped_G_A_preserved"
		<< std::endl;
	return 0;
}
