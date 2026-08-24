#include "ZRDDSCppSimpleInterface.h"
#include "DataReaderListener.h"
#include "ZRDDSSemaphore.h"
#include "ZRDDSTimeUtility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

using namespace DDS;

class CCReplayListener : public DataReaderListener
{
public:
    CCReplayListener(ZRDDSSemaphore& sema)
        :m_sema(sema)
    {

    }
    virtual void on_data_available(DataReader* reader)
    {
        m_reader = (BytesDataReader*)reader;
        // 收到反馈数据，通知writer继续发送下一包
        m_sema.post();
    }
    InstanceHandle_t get_free_reader_handle() 
    { 
        Bytes sample;
        SampleInfo sampleInfo;
        ReturnCode_t ret = m_reader->take_next_sample(sample, sampleInfo);
        if (ret != RETCODE_OK)
        {
            printf("take_next_sample failed(%d).\n", ret);
            return HANDLE_NIL_NATIVE;
        }
        if (!sampleInfo.valid_data)
        {
            return HANDLE_NIL_NATIVE;
        }
        InstanceHandle_t readerHandle;
        readerHandle.valid = true;
        memcpy(readerHandle.value, sample.value._contiguousBuffer, 16);
        return readerHandle; 
    }
private:
    ZRDDSSemaphore& m_sema;
    BytesDataReader* m_reader;
};

int publication(int argc, char* argv[])
{
    unsigned int sampleSize = 10484760;
    unsigned int printGap = 10;
    if (argc > 2)
    {
        sscanf(argv[2], "%u", &sampleSize);
    }
    if (argc > 3)
    {
        sscanf(argv[3], "%u", &printGap);
    }

    printf("set sample size to %u print gap to %u\n", sampleSize, printGap);
    // 初始化
    DomainParticipantFactory* factory = DDSIF::Init("concurrent_example_qos.xml", "default");
    if (factory == NULL)
    {
        printf("DDSIF::Init failed.\n");
        return -1;
    }
    // 创建使用TCP的域参与者
    DomainParticipant* tcpDp = DDSIF::CreateDP(150, "tcp_dp");
    if (tcpDp == NULL)
    {
        printf("DDSIF::CreateDP failed.\n");
        return -2;
    }
    // 创建配置并发传输的数据写者
    DataWriter* writer = DDSIF::PubTopic(tcpDp, "cc_example",
        ConcurrentZeroCopyBytesTypeSupport::get_instance(),
        "cc_datawrtier", NULL);
    if (writer == NULL)
    {
        printf("DDSIF::PubTopic failed.\n");
        return -3;
    }
    ConcurrentZeroCopyBytesDataWriter* ccWriter = (ConcurrentZeroCopyBytesDataWriter*)writer;
    DomainParticipant* udpDp = DDSIF::CreateDP(151, "udp_dp");
    if (udpDp == NULL)
    {
        printf("DDSIF::CreateDP failed.\n");
        return -2;
    }
    // 创建信号量以便反馈后发送
    ZRDDSSemaphore sema;
    int ret = sema.init(0, 0x7fffffff);
    if (ret < 0)
    {
        printf("init sema failed(%d).\n", ret);
        return -4;
    }
    // 创建接收收端反馈的数据读者
    CCReplayListener listener(sema);
    DataReader* reader = DDSIF::SubTopic(udpDp, "cc_example_free_handle",
        BytesTypeSupport::get_instance(), "cc_free_datareader", &listener);
    if (reader == 0)
    {
        printf("DDSIF::SubTopic failed.\n");
        return -4;
    }
    // 循环发送数据
    ConcurrentZeroCopyBytes sample;
    memset(&sample, 0, sizeof(sample));
    sample.contiguous.length = sampleSize;
    sample.contiguous.buffer = (char*)malloc(sample.contiguous.length);
    if (sample.contiguous.buffer == NULL)
    {
        printf("malloc for sample failed.\n");
        return -5;
    }
    unsigned int count = 0;
    const unsigned int totalCount = 100000;
    uint64_t beginTime = 0;
    ZRSleep(1000);
    while (count++ < totalCount)
    {
        // 等待接收端空闲信息
        //printf("wait free handle...\n");
        sema.take();
        if (count == 1)
        {
            ZRDDSTimeUtility::start();
            beginTime = ZRDDSTimeUtility::gettimestamp();
        }
        // 发送数据
        sample.totalLength = sample.contiguous.length;
        sample.curLength = sample.totalLength;
        memcpy(sample.contiguous.buffer, &count, sizeof(count));
        memcpy(sample.contiguous.buffer + sizeof(count), &totalCount, sizeof(totalCount));
        // 其他数据
        //InstanceHandleSeq readers;
        //ccWriter->get_matched_subscriptions(readers);
        //if (readers.length() == 0)
        //{
        //    continue;
        //}
        //InstanceHandle_t dstHandle = readers[count % readers.length()];
        InstanceHandle_t dstHandle = listener.get_free_reader_handle();
        if (!dstHandle.valid)
        {
            continue;
        }
        printf("get free handle(%08x)...\n", *((int*)(dstHandle.value + 4)));
        ReturnCode_t ret = ccWriter->write_w_dst(sample, HANDLE_NIL_NATIVE, dstHandle);
        if (ret != RETCODE_OK)
        {
            printf("write %d\n", ret);
        }
        if (count % printGap == 0)
        {
            uint64_t nowTime = ZRDDSTimeUtility::gettimestamp();
            printf("send(%d) sample length(%d), timeused(%lld) us\n", printGap, sampleSize, nowTime - beginTime);
            printf("throughput(%.3f) MB/s\n", (printGap * (sampleSize / 1048576.0)) / (1.0 * (nowTime - beginTime) / 1000 / 1000));
            beginTime = nowTime;
        }
    }
    // 清理空间
    return 0;
}

extern int publication(int argc, char* argv[]);

extern int subscription(int argc, char* argv[]);

void printHelp()
{
    printf("./concurrent_example s [sample_size(B, default 1048576)] [print_gap(default 1)]\n");
    printf("./concurrent_example r [use_contiguous(1 for use, 0 for not use, default 0)] [print_gap(default 1)]\n");
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printHelp();
        return 0;
    }
    if (argv[1][0] == 's')
    {
        return publication(argc, argv);
    }
    if (argv[1][0] == 'r')
    {
        return subscription(argc, argv);
    }
    else
    {
        printHelp();
    }
    return 0;
}