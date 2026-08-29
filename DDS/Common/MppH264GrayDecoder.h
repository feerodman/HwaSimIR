#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct MppDecodedGrayFrame
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> gray8;
};

class MppH264GrayDecoder
{
public:
    MppH264GrayDecoder();
    ~MppH264GrayDecoder();

    bool initialize(std::string& error);
    bool pushAccessUnit(const std::uint8_t* data, std::size_t size,
        std::vector<MppDecodedGrayFrame>& output, std::string& error);
    bool flush(std::vector<MppDecodedGrayFrame>& output, std::string& error);
    void shutdown();

    bool initialized() const;
    int width() const;
    int height() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
