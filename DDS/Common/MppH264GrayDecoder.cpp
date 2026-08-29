#include "MppH264GrayDecoder.h"

#if defined(HWASIMIR_HAS_RKMPP)
extern "C" {
#include <rk_mpi.h>
#include <mpp_buffer.h>
#include <mpp_frame.h>
#include <mpp_packet.h>
}
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

struct MppH264GrayDecoder::Impl
{
    bool ready = false;
    int width = 0;
    int height = 0;
#if defined(HWASIMIR_HAS_RKMPP)
    MppCtx context = nullptr;
    MppApi* api = nullptr;

    bool collect(std::vector<MppDecodedGrayFrame>& output, bool waitForEos,
        bool& eosSeen, std::string& error)
    {
        int idlePolls = 0;
        const int maxIdlePolls = waitForEos ? 2000 : 2;
        while (idlePolls < maxIdlePolls)
        {
            MppFrame frame = nullptr;
            const MPP_RET result = api->decode_get_frame(context, &frame);
            if (result != MPP_OK)
            {
                error = "MPP decode_get_frame failed code=" + std::to_string(result);
                return false;
            }
            if (!frame)
            {
                ++idlePolls;
                if (waitForEos) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            idlePolls = 0;
            if (mpp_frame_get_info_change(frame))
            {
                width = static_cast<int>(mpp_frame_get_width(frame));
                height = static_cast<int>(mpp_frame_get_height(frame));
                api->control(context, MPP_DEC_SET_INFO_CHANGE_READY, nullptr);
                mpp_frame_deinit(&frame);
                continue;
            }
            if (mpp_frame_get_eos(frame))
            {
                eosSeen = true;
                mpp_frame_deinit(&frame);
                break;
            }
            if (mpp_frame_get_errinfo(frame) || mpp_frame_get_discard(frame))
            {
                error = "MPP returned corrupt/discarded frame";
                mpp_frame_deinit(&frame);
                return false;
            }
            MppBuffer buffer = mpp_frame_get_buffer(frame);
            const std::uint8_t* source = buffer
                ? static_cast<const std::uint8_t*>(mpp_buffer_get_ptr(buffer)) : nullptr;
            const int frameWidth = static_cast<int>(mpp_frame_get_width(frame));
            const int frameHeight = static_cast<int>(mpp_frame_get_height(frame));
            const int stride = static_cast<int>(mpp_frame_get_hor_stride(frame));
            if (!source || frameWidth <= 0 || frameHeight <= 0 || stride < frameWidth)
            {
                error = "MPP returned invalid NV12 frame geometry/buffer";
                mpp_frame_deinit(&frame);
                return false;
            }
            MppDecodedGrayFrame decoded;
            decoded.width = frameWidth;
            decoded.height = frameHeight;
            decoded.gray8.resize(static_cast<std::size_t>(frameWidth) * frameHeight);
            for (int y = 0; y < frameHeight; ++y)
                std::memcpy(decoded.gray8.data() + static_cast<std::size_t>(y) * frameWidth,
                    source + static_cast<std::size_t>(y) * stride,
                    static_cast<std::size_t>(frameWidth));
            width = frameWidth;
            height = frameHeight;
            output.push_back(std::move(decoded));
            mpp_frame_deinit(&frame);
        }
        if (waitForEos && !eosSeen)
        {
            error = "MPP EOS drain timeout";
            return false;
        }
        return true;
    }
#endif
};

MppH264GrayDecoder::MppH264GrayDecoder() : m_impl(new Impl()) {}
MppH264GrayDecoder::~MppH264GrayDecoder() { shutdown(); }

bool MppH264GrayDecoder::initialize(std::string& error)
{
    if (m_impl->ready) return true;
#if !defined(HWASIMIR_HAS_RKMPP)
    error = "binary was built without real RKMPP support";
    return false;
#else
    MPP_RET result = mpp_create(&m_impl->context, &m_impl->api);
    if (result != MPP_OK)
    {
        error = "mpp_create failed code=" + std::to_string(result);
        return false;
    }
    RK_U32 split = 1;
    m_impl->api->control(m_impl->context, MPP_DEC_SET_PARSER_SPLIT_MODE, &split);
    result = mpp_init(m_impl->context, MPP_CTX_DEC, MPP_VIDEO_CodingAVC);
    if (result != MPP_OK)
    {
        error = "mpp_init AVC decoder failed code=" + std::to_string(result);
        shutdown();
        return false;
    }
    MppFrameFormat format = MPP_FMT_YUV420SP;
    m_impl->api->control(m_impl->context, MPP_DEC_SET_OUTPUT_FORMAT, &format);
    RK_U32 immediate = 1;
    m_impl->api->control(m_impl->context, MPP_DEC_SET_IMMEDIATE_OUT, &immediate);
    RK_S64 outputTimeout = 0;
    m_impl->api->control(m_impl->context, MPP_SET_OUTPUT_TIMEOUT, &outputTimeout);
    m_impl->ready = true;
    return true;
#endif
}

bool MppH264GrayDecoder::pushAccessUnit(const std::uint8_t* data, std::size_t size,
    std::vector<MppDecodedGrayFrame>& output, std::string& error)
{
#if !defined(HWASIMIR_HAS_RKMPP)
    (void)data; (void)size; (void)output;
    error = "binary was built without real RKMPP support";
    return false;
#else
    if (!m_impl->ready && !initialize(error)) return false;
    if (!data || size == 0) { error = "empty H264 AU"; return false; }
    for (int attempt = 0; attempt < 2000; ++attempt)
    {
        MppPacket packet = nullptr;
        MPP_RET result = mpp_packet_init(&packet, const_cast<std::uint8_t*>(data), size);
        if (result != MPP_OK)
        {
            error = "mpp_packet_init failed code=" + std::to_string(result);
            return false;
        }
        result = m_impl->api->decode_put_packet(m_impl->context, packet);
        mpp_packet_deinit(&packet);
        if (result == MPP_OK)
        {
            bool eos = false;
            return m_impl->collect(output, false, eos, error);
        }
        bool eos = false;
        if (!m_impl->collect(output, false, eos, error)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    error = "MPP decoder input remained full for 2000 ms";
    return false;
#endif
}

bool MppH264GrayDecoder::flush(std::vector<MppDecodedGrayFrame>& output, std::string& error)
{
#if !defined(HWASIMIR_HAS_RKMPP)
    (void)output; error = "binary was built without real RKMPP support"; return false;
#else
    if (!m_impl->ready) return true;
    MppPacket packet = nullptr;
    MPP_RET result = mpp_packet_init(&packet, nullptr, 0);
    if (result != MPP_OK) { error = "MPP EOS packet init failed"; return false; }
    mpp_packet_set_eos(packet);
    result = m_impl->api->decode_put_packet(m_impl->context, packet);
    mpp_packet_deinit(&packet);
    if (result != MPP_OK)
    {
        error = "MPP EOS put failed code=" + std::to_string(result);
        return false;
    }
    bool eos = false;
    return m_impl->collect(output, true, eos, error);
#endif
}

void MppH264GrayDecoder::shutdown()
{
#if defined(HWASIMIR_HAS_RKMPP)
    if (m_impl->context)
    {
        if (m_impl->api) m_impl->api->reset(m_impl->context);
        mpp_destroy(m_impl->context);
    }
    m_impl->context = nullptr;
    m_impl->api = nullptr;
#endif
    m_impl->ready = false;
    m_impl->width = 0;
    m_impl->height = 0;
}

bool MppH264GrayDecoder::initialized() const { return m_impl->ready; }
int MppH264GrayDecoder::width() const { return m_impl->width; }
int MppH264GrayDecoder::height() const { return m_impl->height; }
