#include "../VideoTopicResolver.h"

#include <iostream>

int main()
{
    int failures = 0;
    std::string value, error;
    if (!ResolveVideoTopic("HwaSimIR.Video.{platID}.{sensorID}.{codec}", 1001, 2,
            "h264", value, error) || value != "HwaSimIR.Video.1001.2.H264") ++failures;
    if (!ResolveVideoTopic("HwaSimIR.Video.{platID}.{sensorID}.{codec}", 1001, 3,
            "raw_gray8", value, error) || value != "HwaSimIR.Video.1001.3.RawGray8") ++failures;
    if (ResolveVideoTopic("HwaSimIR.Video.{platID}.{unknown}.{codec}", 1001, 2,
            "h264", value, error)) ++failures;
    if (ResolveVideoTopic("", 1001, 2, "h264", value, error)) ++failures;
    if (ResolveVideoTopic("HwaSimIR.Video.{platID}.{sensorID}.{codec}", 1001, 2,
            "jpeg", value, error)) ++failures;
    std::cout << "[VideoTopicResolverTest] cases=5 failures=" << failures
              << " status=" << (failures == 0 ? "PASS" : "FAIL") << std::endl;
    return failures == 0 ? 0 : 1;
}
