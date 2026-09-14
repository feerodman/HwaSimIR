#pragma once
#include <QByteArray>
#include <QString>
#include "../../../DDS/Protocol/FrameProductV2.h"
#if defined(HWASIM_HAS_FFMPEG)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
}
#endif

// Remux the received access unit. No image resize, second lossy encode, or
// invented source sequence. Capture timestamps define the MP4 presentation
// timeline; original producer media PTS remains in the per-frame sidecar.
class RecordingMuxer {
public:
    qint64 lastPtsUs=0, originNs=0;
    QString error;
#if defined(HWASIM_HAS_FFMPEG)
    AVFormatContext* format=nullptr;
    AVStream* stream=nullptr;
    bool header=false;
    bool close(){
        bool ok=true;
        if(format){
            if(header&&av_write_trailer(format)<0){error="write_trailer_failed";ok=false;}
            if(format->pb&&avio_closep(&format->pb)<0){error="close_video_file_failed";ok=false;}
            avformat_free_context(format);format=nullptr;header=false;
        }
        return ok;
    }
    ~RecordingMuxer(){close();}
    bool open(const QString& path,const QByteArray& au,int width,int height,int fps) {
        const auto* bytes=reinterpret_cast<const std::uint8_t*>(au.constData());
        std::vector<std::uint8_t> extra;
        for(std::size_t i=0;i<static_cast<std::size_t>(au.size());) {
            auto prefix=HwaFrameV2::startCode(bytes,au.size(),i);if(!prefix){++i;continue;}
            auto begin=i+prefix,end=begin+1;
            while(end<static_cast<std::size_t>(au.size())&&!HwaFrameV2::startCode(bytes,au.size(),end))++end;
            if(begin<static_cast<std::size_t>(au.size())&&((bytes[begin]&31)==7||(bytes[begin]&31)==8))extra.insert(extra.end(),bytes+i,bytes+end);
            i=end;
        }
        if(extra.empty()){error="missing_SPS_PPS";return false;}
        if(avformat_alloc_output_context2(&format,nullptr,"mp4",path.toUtf8().constData())<0||!format){error="allocate_muxer";return false;}
        stream=avformat_new_stream(format,nullptr);if(!stream)return false;
        stream->time_base=AVRational{1,1000000};stream->avg_frame_rate=AVRational{fps>0?fps:60,1};
        auto* p=stream->codecpar;p->codec_type=AVMEDIA_TYPE_VIDEO;p->codec_id=AV_CODEC_ID_H264;p->width=width;p->height=height;
        p->extradata=static_cast<std::uint8_t*>(av_mallocz(extra.size()+AV_INPUT_BUFFER_PADDING_SIZE));if(!p->extradata)return false;
        std::memcpy(p->extradata,extra.data(),extra.size());p->extradata_size=static_cast<int>(extra.size());
        if(avio_open(&format->pb,path.toUtf8().constData(),AVIO_FLAG_WRITE)<0){error="open_video_file";return false;}
        if(avformat_write_header(format,nullptr)<0){error="write_header";return false;}header=true;return true;
    }
    bool write(const QByteArray& au,bool key,qint64 captureNs,int fps) {
        if(!header)return false;
        if(!originNs)originNs=captureNs;
        const qint64 proposed=captureNs>0?(captureNs-originNs)/1000:lastPtsUs+1000000/(fps>0?fps:60);
        // Capture timestamps are monotonic. A regression is a file error, not
        // permission to change the producer's frame identity or duplicate it.
        if(proposed<lastPtsUs){error="capture_time_regression";return false;}
        lastPtsUs=proposed;
        AVPacket* packet=av_packet_alloc();if(!packet)return false;
        if(av_new_packet(packet,au.size())<0){av_packet_free(&packet);return false;}
        std::memcpy(packet->data,au.constData(),au.size());packet->stream_index=stream->index;
        packet->pts=proposed;packet->dts=proposed;packet->duration=1000000/(fps>0?fps:60);
        if(key)packet->flags|=AV_PKT_FLAG_KEY;
        av_packet_rescale_ts(packet,AVRational{1,1000000},stream->time_base);
        const int result=av_interleaved_write_frame(format,packet);av_packet_free(&packet);
        if(result<0){error="write_video_packet";return false;}return true;
    }
#else
    bool close(){return true;}
    bool open(const QString&,const QByteArray&,int,int,int){error="avformat_unavailable";return false;}
    bool write(const QByteArray&,bool,qint64,int){return false;}
#endif
};
