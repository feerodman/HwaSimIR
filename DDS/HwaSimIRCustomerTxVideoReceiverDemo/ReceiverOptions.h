#pragma once
struct Options {
    std::string topic,codec,pixels,out,qos,ffmpeg="ffmpeg";
    int width=0,height=0,fps=60,domain=150,idleMs=0,timeoutMs=0,slowMs=0,callbackBlockMs=0;
    bool display=true,audit=false,countOnly=false;int exitAfterEnds=0;
    std::size_t queueBytes=32*1024*1024;
};
static Options parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;i+=2) {
        if(i+1>=argc)throw std::runtime_error("missing option value");
        const std::string k=argv[i],v=argv[i+1];
        if(k=="--topic")o.topic=v;else if(k=="--codec")o.codec=v;
        else if(k=="--pixel-format")o.pixels=v;else if(k=="--output")o.out=v;
        else if(k=="--qos")o.qos=v;else if(k=="--width")o.width=std::stoi(v);
        else if(k=="--height")o.height=std::stoi(v);else if(k=="--fps")o.fps=std::stoi(v);
        else if(k=="--domain")o.domain=std::stoi(v);else if(k=="--idle-ms")o.idleMs=std::stoi(v);
        else if(k=="--timeout-ms")o.timeoutMs=std::stoi(v);else if(k=="--slow-ms")o.slowMs=std::stoi(v);
        else if(k=="--callback-block-ms")o.callbackBlockMs=std::stoi(v);
        else if(k=="--audit")o.audit=v=="1";
        else if(k=="--count-only")o.countOnly=v=="1";
        else if(k=="--exit-after-ends")o.exitAfterEnds=std::stoi(v);
        else if(k=="--display")o.display=v=="1";else if(k=="--ffmpeg")o.ffmpeg=v;
        else if(k=="--queue-bytes")o.queueBytes=std::stoull(v);
        else throw std::runtime_error("unknown option "+k);
    }
    bool compressed=o.codec=="h264";
    if(o.codec!="raw" && !compressed)throw std::runtime_error("explicit codec raw/h264 required");
    if(o.topic!=CustomerTX::topic(true,compressed) && o.topic!=CustomerTX::topic(false,compressed))throw std::runtime_error("topic/codec mismatch");
    if(o.pixels!="gray8" && o.pixels!="rgb24")throw std::runtime_error("explicit gray8/rgb24 required; RGB order, top-down");
    std::size_t bytes=0;
    if(!CustomerTX::imageBytes(o.width,o.height,o.pixels=="gray8"?CustomerTX::Pixels::Gray8:CustomerTX::Pixels::RGB24,bytes) ||
       (!compressed && bytes>CustomerTX::RawCapacity) || o.width>4096 || o.height>4096 || o.fps<=0 || o.idleMs<0 || o.timeoutMs<0 || o.exitAfterEnds<0 ||
       o.slowMs<0 || o.slowMs>5000 || o.callbackBlockMs<0 || o.callbackBlockMs>5000 ||
       o.queueBytes<640000 || o.queueBytes>256*1024*1024 || o.out.empty() || o.qos.empty())throw std::runtime_error("invalid explicit geometry/capacity/configuration");
    return o;
}
