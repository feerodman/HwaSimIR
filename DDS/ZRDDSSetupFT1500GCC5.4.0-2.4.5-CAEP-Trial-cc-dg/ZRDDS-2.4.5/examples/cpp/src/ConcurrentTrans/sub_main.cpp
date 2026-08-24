#include "ZRDDSCppSimpleInterface.h"
#include "DataReaderListener.h"
#include "ZRDDSSemaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

class CCListener : public DataReaderListener
{
public:
    CCListener(ZRDDSSemaphore& sema, bool useContiguous, unsigned int printGap)
        :m_sema(sema), m_useContiguous(useContiguous), m_printGap(printGap)
    {
        m_recvCount = 0;
        memset(&m_ccZeroCopyBytesSample, 0, sizeof(m_ccZeroCopyBytesSample));
        if (useContiguous)
        {
            m_ccZeroCopyBytesSample.contiguous.length = 100 * 1048576;
            m_ccZeroCopyBytesSample.contiguous.buffer = (char*)malloc(m_ccZeroCopyBytesSample.contiguous.length);
        }
    }
    virtual void on_data_available(DataReader* reader)
    {
        SampleInfo curInfo;
        ConcurrentZeroCopyBytesDataReader* typedReader = (ConcurrentZeroCopyBytesDataReader*)reader;
        ReturnCode_t retCode = typedReader->take_next_sample(m_ccZeroCopyBytesSample, curInfo);
        if (retCode != RETCODE_OK)
        {
            printf("ConcurrentZeroCopyBytesDataReader::take_next_sample failed(%d).", retCode);
            return;
        }
        if (!curInfo.valid_data)
        {
            return;
        }
        // 只有样本齐全进行业务
        if (m_ccZeroCopyBytesSample.curLength < m_ccZeroCopyBytesSample.totalLength)
        {
            // 分片尚未收全，应继续等待
            return;
        }
        // 统计
        unsigned int counter = 0;
        unsigned int totalCount = 0;
        memcpy(&counter, m_ccZeroCopyBytesSample.fragments[0].buffer, sizeof(counter));
        memcpy(&totalCount, m_ccZeroCopyBytesSample.fragments[0].buffer + sizeof(counter), sizeof(totalCount));
        if (m_recvCount++ % m_printGap == 0)
        {
            printf("received[%d/%d] length(%d)\n", counter, totalCount, m_ccZeroCopyBytesSample.totalLength);
        }
#if 0
        for (int fragIndex = 0; fragIndex < m_ccZeroCopyBytesSample.fragmentNum; ++fragIndex)
        {
            printf("fragment[%d]: %u ", fragIndex, m_ccZeroCopyBytesSample.fragments[fragIndex].length);
            for (int index = fragIndex == 0 ? 4 : 0; index < m_ccZeroCopyBytesSample.fragments[fragIndex].length; ++index)
            {
                printf("%c", m_ccZeroCopyBytesSample.fragments[fragIndex].buffer[index]);
            }
            printf("\n");
        }
        printf("\n");
#endif
        // 将当前样本清零，以重新接收新的样本
        m_ccZeroCopyBytesSample.curLength = 0;
        m_ccZeroCopyBytesSample.fragmentNum = 0;
        // 收到反馈数据，通知writer继续发送下一包
        m_sema.post();
    }
private:
    ZRDDSSemaphore& m_sema;
    ConcurrentZeroCopyBytes m_ccZeroCopyBytesSample;
    bool m_useContiguous;
    unsigned int m_printGap;
    unsigned int m_recvCount;
};

int subscription(int argc, char* argv[])
{
    unsigned int useContiguous = 0;
    unsigned int printGap = 1;
    if (argc > 2)
    {
        sscanf(argv[2], "%u", &useContiguous);
    }
    if (argc > 3)
    {
        sscanf(argv[3], "%u", &printGap);
    }

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
    // 创建信号量以便反馈后发送
    ZRDDSSemaphore sema;
    int ret = sema.init(1, 0x7fffffff);
    if (ret < 0)
    {
        printf("init sema failed(%d).\n", ret);
        return -4;
    }
    CCListener listener(sema, useContiguous != 0, printGap);
    // 创建配置并发传输的数据读者
    DataReader* reader = DDSIF::SubTopic(tcpDp, "cc_example",
        ConcurrentZeroCopyBytesTypeSupport::get_instance(),
        "cc_datareader", &listener);
    if (reader == NULL)
    {
        printf("DDSIF::SubTopic failed.\n");
        return -3;
    }
    // 创建使用UDP的域参与者
    DomainParticipant* udpDp = DDSIF::CreateDP(151, "udp_dp");
    if (udpDp == NULL)
    {
        printf("DDSIF::CreateDP failed.\n");
        return -2;
    }
    // 创建空闲标志数据写者
    DataWriter* writer = DDSIF::PubTopic(udpDp, "cc_example_free_handle",
        BytesTypeSupport::get_instance(), "cc_free_datawriter", NULL);
    if (writer == NULL)
    {
        printf("DDSIF::PubTopic failed.\n");
        return -4;
    }
    BytesDataWriter* bytesWriter = (BytesDataWriter*)writer;
    // 发送空闲标志，将对应的reader的标识发送到对端，对端根据该标识来发送数据到该reader
    Bytes sample;
    sample.value.ensure_length(16, 16);
    InstanceHandle_t readerHandle = reader->get_instance_handle();
    memcpy(sample.value._contiguousBuffer, readerHandle.value, sizeof(readerHandle.value));
    while (true)
    {
        // 等待处理完成
        sema.take();
        // 发送数据
        ReturnCode_t ret = bytesWriter->write(sample, HANDLE_NIL_NATIVE);
        if (ret != RETCODE_OK)
        {
            printf("write failed.%d\n", ret);
        }
    }
    // 清理空间
    return 0;
}
