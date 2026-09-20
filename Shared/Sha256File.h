#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace HwaHash {

class Sha256 {
public:
    Sha256() : m_bitCount(0), m_bufferSize(0), m_state{{
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u}} {}

    void update(const unsigned char* data, std::size_t size) {
        for (std::size_t index = 0; index < size; ++index) {
            m_buffer[m_bufferSize++] = data[index];
            if (m_bufferSize == 64) {
                transform(m_buffer.data());
                m_bitCount += 512;
                m_bufferSize = 0;
            }
        }
    }

    std::string finishHex() {
        const std::uint64_t totalBits = m_bitCount + static_cast<std::uint64_t>(m_bufferSize) * 8u;
        m_buffer[m_bufferSize++] = 0x80u;
        if (m_bufferSize > 56) {
            while (m_bufferSize < 64) m_buffer[m_bufferSize++] = 0;
            transform(m_buffer.data());
            m_bufferSize = 0;
        }
        while (m_bufferSize < 56) m_buffer[m_bufferSize++] = 0;
        for (int shift = 56; shift >= 0; shift -= 8)
            m_buffer[m_bufferSize++] = static_cast<unsigned char>((totalBits >> shift) & 0xffu);
        transform(m_buffer.data());
        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (std::uint32_t word : m_state) output << std::setw(8) << word;
        return output.str();
    }

private:
    static std::uint32_t rotateRight(std::uint32_t value, unsigned count) {
        return (value >> count) | (value << (32u - count));
    }

    void transform(const unsigned char* block) {
        static const std::uint32_t constants[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
        std::uint32_t words[64];
        for (int index = 0; index < 16; ++index) {
            const int offset = index * 4;
            words[index] = (static_cast<std::uint32_t>(block[offset]) << 24) |
                (static_cast<std::uint32_t>(block[offset + 1]) << 16) |
                (static_cast<std::uint32_t>(block[offset + 2]) << 8) |
                static_cast<std::uint32_t>(block[offset + 3]);
        }
        for (int index = 16; index < 64; ++index) {
            const std::uint32_t s0 = rotateRight(words[index - 15], 7) ^
                rotateRight(words[index - 15], 18) ^ (words[index - 15] >> 3);
            const std::uint32_t s1 = rotateRight(words[index - 2], 17) ^
                rotateRight(words[index - 2], 19) ^ (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        std::uint32_t a=m_state[0], b=m_state[1], c=m_state[2], d=m_state[3];
        std::uint32_t e=m_state[4], f=m_state[5], g=m_state[6], h=m_state[7];
        for (int index = 0; index < 64; ++index) {
            const std::uint32_t s1 = rotateRight(e,6)^rotateRight(e,11)^rotateRight(e,25);
            const std::uint32_t choose = (e & f) ^ ((~e) & g);
            const std::uint32_t t1 = h + s1 + choose + constants[index] + words[index];
            const std::uint32_t s0 = rotateRight(a,2)^rotateRight(a,13)^rotateRight(a,22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + majority;
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        m_state[0]+=a; m_state[1]+=b; m_state[2]+=c; m_state[3]+=d;
        m_state[4]+=e; m_state[5]+=f; m_state[6]+=g; m_state[7]+=h;
    }

    std::uint64_t m_bitCount;
    std::size_t m_bufferSize;
    std::array<unsigned char, 64> m_buffer{};
    std::array<std::uint32_t, 8> m_state;
};

inline std::string Sha256File(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) throw std::runtime_error("sha256_file_missing: " + path);
    Sha256 hash;
    std::array<unsigned char, 64 * 1024> buffer{};
    while (input.good()) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const std::streamsize count = input.gcount();
        if (count > 0) hash.update(buffer.data(), static_cast<std::size_t>(count));
    }
    if (!input.eof()) throw std::runtime_error("sha256_file_read_failed: " + path);
    return hash.finishHex();
}

} // namespace HwaHash
