#include "ZRDDSCppSimpleInterface.h"

#include <cstdio>

using namespace DDS;

class D1BytesListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader> {
public:
    virtual void on_process_sample(DataReader*, const Bytes&, const SampleInfo&) {}
};

int main(int argc, char** argv) {
    const char* qos = argc > 1 ? argv[1] : "ZRDDS_QOS_PROFILES.xml";
    DomainParticipantFactory* factory = DDSIF::Init(qos, "hwasimir_factory");
    if (factory == NULL) { std::fprintf(stderr, "DDSIF::Init failed\n"); return 2; }
    DomainParticipant* participant = DDSIF::CreateDP(150, "hwasimir_tcp");
    if (participant == NULL) { std::fprintf(stderr, "DDSIF::CreateDP failed\n"); DDSIF::Finalize(); return 3; }
    D1BytesListener listener;
    DataWriter* writer = DDSIF::PubTopic(participant, "HwaSimIR.D1.CrossLinkCheck",
        BytesTypeSupport::get_instance(), "hwasimir_reliable_writer", NULL);
    DataReader* reader = DDSIF::SubTopic(participant, "HwaSimIR.D1.CrossLinkCheck",
        BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &listener);
    if (writer == NULL || reader == NULL) {
        std::fprintf(stderr, "DDSIF::PubTopic/SubTopic failed\n");
        DDSIF::Finalize();
        return 4;
    }
    const ReturnCode_t result = DDSIF::Finalize();
    if (result != RETCODE_OK) { std::fprintf(stderr, "DDSIF::Finalize failed code=%d\n", result); return 5; }
    std::printf("ZRDDS_CPP_INIT_RUNTIME_PASS\n");
    return 0;
}
