#include "DdsStimClient.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace {

const int kPlatID = 1001;
const int kSensorID = 2;
const int kWidth = 800;
const int kHeight = 800;
const int kTargetType = 0x55;

struct Options
{
    int domain = 150;
    int band = 2;
    int ackTimeoutMs = 30000;
    int discoveryWaitMs = 2000;
    int eventWaitMs = 700;
    int roundWaitMs = 2500;
    int tailFrames = 0;
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string scenario = "all";
};

bool ReadValue(int& index, int argc, char** argv, const char* name, std::string& value)
{
    if (std::string(argv[index]) != name || index + 1 >= argc) return false;
    value = argv[++index];
    return true;
}

bool Parse(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index)
    {
        std::string value;
        if (ReadValue(index, argc, argv, "--qos", value)) options.qos = value;
        else if (ReadValue(index, argc, argv, "--domain", value)) options.domain = std::atoi(value.c_str());
        else if (ReadValue(index, argc, argv, "--band", value)) options.band = std::atoi(value.c_str());
        else if (ReadValue(index, argc, argv, "--scenario", value)) options.scenario = value;
        else if (ReadValue(index, argc, argv, "--ack-timeout-ms", value)) options.ackTimeoutMs = std::atoi(value.c_str());
        else if (ReadValue(index, argc, argv, "--discovery-wait-ms", value)) options.discoveryWaitMs = std::atoi(value.c_str());
        else if (ReadValue(index, argc, argv, "--event-wait-ms", value)) options.eventWaitMs = std::atoi(value.c_str());
        else if (ReadValue(index, argc, argv, "--round-wait-ms", value)) options.roundWaitMs = std::atoi(value.c_str());
        else if (ReadValue(index, argc, argv, "--tail-frames", value)) options.tailFrames = std::atoi(value.c_str());
        else
        {
            std::cerr << "unknown or incomplete option: " << argv[index] << std::endl;
            return false;
        }
    }
    return (options.band == 0 || options.band == 2) && options.ackTimeoutMs > 0 &&
        options.discoveryWaitMs >= 0 && options.eventWaitMs >= 0 && options.roundWaitMs >= 0 &&
        options.tailFrames >= 0;
}

void Wait(int milliseconds)
{
    if (milliseconds > 0) std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

BYHWICD::ControlP2cX1ObjTrackingCmd Control(int command, int round, int roundCount)
{
    BYHWICD::ControlP2cX1ObjTrackingCmd value = {};
    value.flag = 0x41;
    value.JB = 1;
    value.platID = kPlatID;
    value.simCommand = command;
    value.currentRound = round;
    value.roundCut = roundCount;
    return value;
}

BYHWICD::InitP2cObjectTrackingCmd Init(const Options& options, int round,
    const std::string& scenario)
{
    BYHWICD::InitP2cObjectTrackingCmd value = {};
    value.flag = 0x36;
    value.JB = 1;
    value.platID = kPlatID;
    value.sensorID = kSensorID;
    value.platParamInit.id = kPlatID;
    value.platParamInit.type = 1;
    if (scenario != "zero_init_then_valid")
    {
        value.platParamInit.spatial.lat = 29.0 + round * 0.01;
        value.platParamInit.spatial.lon = 109.0 + round * 0.01;
        value.platParamInit.spatial.alt = 1.0;
    }
    value.trackingInit.enable = true;
    value.trackingInit.envTerrain = 0;
    value.trackingInit.envSky = 0;
    value.trackingInit.envTemp = 25.0;
    value.trackingInit.envHumidity = 60.0;
    value.trackingInit.envVisibility = 6000.0;
    value.trackingInit.envWindV = 2.0;
    value.trackingInit.envWindDir = 30.0;
    value.trackingInit.envRadScaleTerrain = 1.0;
    value.trackingInit.envRadScaleSky = 1.0;
    value.trackingInit.simMode = 2;
    value.trackingInit.videoFps = 60;
    BYHWICD::trackerSensorParam& sensor = value.trackingInit.trackerSensor[0];
    sensor.h264En = true;
    sensor.realtimeAnnotation = true;
    // The independent fixture drives the production DDS video path.  Enabling
    // recording lets an unmodified VideoDisplay receiver retain short evidence
    // clips; it does not create a local renderer-side recording.
    sensor.saveMP4En = true;
    sensor.trackerSensorBand = options.band;
    sensor.trackerSensorWidth = kWidth;
    sensor.trackerSensorHeight = kHeight;
    sensor.trackerSensorViewMin = 1;
    sensor.trackerSensorViewMax = 100000;
    sensor.trackerSensorPixelAngle = 25.0;
    value.MissileMaxCountResv1 = 1;
    return value;
}

BYHWICD::DisplayC2cObjTrackingData Sample(double timeMs, double lat, double lon,
    double altM, bool viewValid, bool includeTarget)
{
    BYHWICD::DisplayC2cObjTrackingData value = {};
    value.flag = 0x38;
    value.platID = kPlatID;
    value.sensorID = kSensorID;
    value.time = timeMs;
    value.platLoc.lat = lat;
    value.platLoc.lon = lon;
    value.platLoc.alt = altM;
    value.platLoc.yaw = 15.0;
    value.platLoc.speed = 36.0;
    value.weaponState.targetType = kTargetType;
    value.weaponState.targetPlatID = 3;
    value.weaponState.targetID = 3;
    value.weaponState.lookatEn = true;
    value.weaponState.viewValid = viewValid;
    value.targetNumValid = includeTarget ? 1 : 0;
    if (includeTarget)
    {
        BYHWICD::TargetState& target = value.targetState[0];
        target.targetType = kTargetType;
        target.targetPlatID = 3;
        target.targetID = 3;
        target.engineState = true;
        target.viewValid = viewValid;
        target.targetLoc.lat = lat;
        target.targetLoc.lon = lon + 0.0045;
        target.targetLoc.alt = altM;
        target.targetLoc.yaw = 180.0;
        target.targetLoc.speed = 18.0;
        target.targetState = 0x01;
    }
    return value;
}

BYHWICD::DisplayC2cObjTrackingData Placeholder()
{
    BYHWICD::DisplayC2cObjTrackingData value = {};
    value.flag = 0x38;
    value.platID = kPlatID;
    value.sensorID = kSensorID;
    return value;
}

bool Send(DdsStimClient& client, const std::string& scenario, const std::string& event,
    const BYHWICD::DisplayC2cObjTrackingData& sample, const char* expected, std::string& error)
{
    std::cout << "[P14FixtureEvent] scenario=" << scenario << " event=" << event
              << " timeMs=" << sample.time << " lat=" << sample.platLoc.lat
              << " lon=" << sample.platLoc.lon << " altM=" << sample.platLoc.alt
              << " targetNumValid=" << sample.targetNumValid
              << " inputViewValid=" << (sample.weaponState.viewValid ? 1 : 0)
              << " expectedReceiver=" << expected << std::endl;
    return client.sendRealtime(sample, error);
}

std::vector<std::string> Scenarios(const std::string& requested)
{
    if (requested != "all") return std::vector<std::string>(1, requested);
    return {"start_no_packet", "zero_init_then_valid", "placeholder_then_valid",
        "target_late", "viewvalid_independent", "valid_then_invalid",
        "stop_start_same_generation", "legal_zero_geography"};
}

bool IsKnownScenario(const std::string& value)
{
    std::vector<std::string> known = Scenarios("all");
    known.push_back("boundary_demo");
    for (std::size_t index = 0; index < known.size(); ++index)
        if (known[index] == value) return true;
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!Parse(argc, argv, options)) return 2;
    const std::vector<std::string> scenarios = Scenarios(options.scenario);
    for (std::size_t index = 0; index < scenarios.size(); ++index)
    {
        if (!IsKnownScenario(scenarios[index]))
        {
            std::cerr << "unknown scenario: " << scenarios[index] << std::endl;
            return 2;
        }
    }

    DdsStimConfig config;
    config.domainId = options.domain;
    config.qosFile = options.qos;
    config.observeStopStatus = true;
    config.channel = "precise";
    DdsStimClient client;
    std::string error;
    if (!client.start(config, error))
    {
        std::cerr << "[P14Fixture][FATAL] " << error << std::endl;
        return 3;
    }
    Wait(options.discoveryWaitMs);

    for (std::size_t index = 0; index < scenarios.size(); ++index)
    {
        const std::string& scenario = scenarios[index];
        const int round = static_cast<int>(index) + 1;
        const int roundCount = static_cast<int>(scenarios.size());
        std::cout << "[P14FixtureScenario] name=" << scenario << " phase=begin round="
                  << round << " source=generated_no_file strictIdentity=1001/2 resolution=800x800" << std::endl;
        if (!client.sendControl(Control(1, round, roundCount), error)) return 4;
        Wait(200);
        const BYHWICD::InitP2cObjectTrackingCmd init = Init(options, round, scenario);
        std::cout << "[P14FixtureEvent] scenario=" << scenario
                  << " event=init_prewarm lat=" << init.platParamInit.spatial.lat
                  << " lon=" << init.platParamInit.spatial.lon
                  << " altM=" << init.platParamInit.spatial.alt
                  << " expectedReceiver=temporary_only_not_formal" << std::endl;
        if (!client.sendInit(init, error)) return 5;
        BYHWICD::InitAckC2pObjectTrackingCmd ack = {};
        if (!client.waitForInitAck(options.ackTimeoutMs, ack) || !ack.trackingReady ||
            ack.platID != kPlatID || ack.sensorID != kSensorID)
        {
            std::cerr << "[P14Fixture][FATAL] init ack missing or invalid scenario=" << scenario << std::endl;
            return 6;
        }
        if (!client.sendControl(Control(2, round, roundCount), error)) return 7;
        if (!client.waitForRunningStatus(options.ackTimeoutMs, error))
        {
            std::cerr << "[P14Fixture][FATAL] START readiness missing scenario="
                      << scenario << " error=" << error << std::endl;
            return 7;
        }
        std::cout << "[P14FixtureEvent] scenario=" << scenario
                  << " event=start expectedReceiver=wait_first_valid_realtime" << std::endl;

        const double baseLat = 30.0 + round * 0.01;
        const double baseLon = 110.0 + round * 0.01;
        if (scenario == "start_no_packet")
        {
            Wait(options.eventWaitMs * 2);
            std::cout << "[P14FixtureEvent] scenario=" << scenario
                      << " event=no_packet_window elapsedMs=" << options.eventWaitMs * 2
                      << " expectedReceiver=no_formal_reference_no_frame" << std::endl;
            if (!Send(client, scenario, "first_valid", Sample(1000.0, baseLat, baseLon, 1.0, true, true),
                "commit_once_from_realtime", error)) return 8;
        }
        else if (scenario == "zero_init_then_valid")
        {
            if (!Send(client, scenario, "first_valid", Sample(1500.0, baseLat, baseLon, 1.0, true, true),
                "commit_once_from_realtime_after_zero_init", error)) return 8;
        }
        else if (scenario == "placeholder_then_valid")
        {
            if (!Send(client, scenario, "all_zero_placeholder", Placeholder(),
                "reject_placeholder_no_reference_no_frame", error)) return 8;
            Wait(options.eventWaitMs);
            if (!Send(client, scenario, "first_valid", Sample(2000.0, baseLat, baseLon, 1.0, true, true),
                "commit_once_from_realtime", error)) return 8;
        }
        else if (scenario == "target_late")
        {
            if (!Send(client, scenario, "platform_valid_target_absent", Sample(3000.0, baseLat, baseLon, 1.0, false, false),
                "commit_platform_reference_target_count_zero", error)) return 8;
            Wait(options.eventWaitMs);
            if (!Send(client, scenario, "target_arrives_late", Sample(3016.0, baseLat, baseLon, 1.0, true, true),
                "retain_reference_map_target_full_key", error)) return 8;
        }
        else if (scenario == "viewvalid_independent")
        {
            if (!Send(client, scenario, "view_flag_zero", Sample(4000.0, baseLat, baseLon, 1.0, false, true),
                "commit_reference_accept_business_row_hide_target", error)) return 8;
            Wait(options.eventWaitMs);
            if (!Send(client, scenario, "view_flag_one", Sample(4016.0, baseLat, baseLon, 1.0, true, true),
                "retain_reference_show_target", error)) return 8;
        }
        else if (scenario == "valid_then_invalid")
        {
            if (!Send(client, scenario, "first_valid", Sample(5000.0, baseLat, baseLon, 1.0, true, true),
                "commit_once_from_realtime", error)) return 8;
            Wait(options.eventWaitMs);
            if (!Send(client, scenario, "invalid_after_valid_placeholder", Placeholder(),
                "reject_no_rebase_no_frame", error)) return 8;
            BYHWICD::DisplayC2cObjTrackingData outOfRange = Sample(5016.0, 95.0, baseLon, 1.0, true, true);
            if (!Send(client, scenario, "invalid_after_valid_range", outOfRange,
                "reject_no_rebase_no_frame", error)) return 8;
            BYHWICD::DisplayC2cObjTrackingData notFinite = Sample(5020.0, baseLat, baseLon, 1.0, true, true);
            notFinite.platLoc.lat = std::numeric_limits<double>::quiet_NaN();
            if (!Send(client, scenario, "invalid_after_valid_nan", notFinite,
                "reject_no_rebase_no_frame", error)) return 8;
            BYHWICD::DisplayC2cObjTrackingData wrongIdentity =
                Sample(5024.0, baseLat, baseLon, 1.0, true, true);
            wrongIdentity.platID = 999;
            if (!Send(client, scenario, "invalid_after_valid_wrong_identity", wrongIdentity,
                "reject_protocol_route_no_frame", error)) return 8;
            Wait(options.eventWaitMs);
            if (!Send(client, scenario, "valid_after_invalid_different_position",
                Sample(5032.0, baseLat + 0.02, baseLon + 0.02, 1.0, true, true),
                "accept_frame_retain_original_reference", error)) return 8;
        }
        else if (scenario == "stop_start_same_generation")
        {
            if (!Send(client, scenario, "first_valid", Sample(7000.0, baseLat, baseLon, 1.0, true, true),
                "commit_once_from_realtime", error)) return 8;
            Wait(options.eventWaitMs);
            if (!client.sendControl(Control(3, round, roundCount), error)) return 9;
            if (!client.waitForAcknowledgments(options.ackTimeoutMs, error)) return 10;
            std::cout << "[P14FixtureEvent] scenario=" << scenario
                      << " event=mid_generation_stop_complete expectedReceiver=retain_reference" << std::endl;
            if (!client.sendControl(Control(2, round, roundCount), error)) return 7;
            if (!client.waitForRunningStatus(options.ackTimeoutMs, error)) return 7;
            std::cout << "[P14FixtureEvent] scenario=" << scenario
                      << " event=same_generation_restart expectedReceiver=reference_already_committed" << std::endl;
            if (!Send(client, scenario, "valid_after_restart_different_position",
                Sample(7016.0, baseLat + 0.02, baseLon + 0.02, 1.0, true, true),
                "accept_frame_retain_original_reference", error)) return 8;
        }
        else if (scenario == "legal_zero_geography")
        {
            BYHWICD::DisplayC2cObjTrackingData zeroGeo = {};
            zeroGeo.flag = 0x38;
            zeroGeo.platID = kPlatID;
            zeroGeo.sensorID = kSensorID;
            zeroGeo.time = 6000.0;
            if (!Send(client, scenario, "zero_coordinates_nonzero_time", zeroGeo,
                "valid_commit_reference_view_independent_atmosphere_may_fail_closed", error)) return 8;
        }
        else if (scenario == "boundary_demo")
        {
            Wait(options.eventWaitMs * 2);
            std::cout << "[P14FixtureEvent] scenario=" << scenario
                      << " event=no_packet_window elapsedMs=" << options.eventWaitMs * 2
                      << " expectedReceiver=no_formal_reference_no_frame" << std::endl;
            if (!Send(client, scenario, "all_zero_placeholder", Placeholder(),
                "reject_placeholder_no_reference_no_frame", error)) return 8;
            Wait(options.eventWaitMs);
            std::cout << "[P14FixtureVisibility] policy=ForceVisibleForDemo"
                      << " sourceViewValid=0 effectiveViewValid=1 productionFilterChanged=0" << std::endl;
            BYHWICD::DisplayC2cObjTrackingData demo =
                Sample(8000.0, baseLat, baseLon, 1.0, true, true);
            if (!Send(client, scenario, "first_valid", demo,
                "commit_once_from_realtime", error)) return 8;
            if (options.tailFrames > 0)
            {
                std::cout << "[P14FixtureTail] phase=begin frames=" << options.tailFrames
                          << " cadenceHz=60" << std::endl;
                for (int frame = 0; frame < options.tailFrames; ++frame)
                {
                    demo.time = 8016.0 + static_cast<double>(frame) * (1000.0 / 60.0);
                    if (!client.sendRealtime(demo, error)) return 8;
                    Wait(16);
                }
                std::cout << "[P14FixtureTail] phase=end frames=" << options.tailFrames << std::endl;
            }
        }

        Wait(options.eventWaitMs);
        if (!client.sendControl(Control(3, round, roundCount), error)) return 9;
        std::cout << "[P14FixtureScenario] name=" << scenario
                  << " phase=stop_sent result=INPUT_COMPLETE" << std::endl;
        if (!client.waitForAcknowledgments(options.ackTimeoutMs, error)) return 10;
        Wait(options.roundWaitMs);
    }

    std::cout << "[P14FixtureSummary] scenarios=" << scenarios.size()
              << " fileReads=0 inputGenerated=1 strictIdentity=1001/2 resolution=800x800"
              << " wireLayoutChanged=0 result=INPUT_COMPLETE" << std::endl;
    client.shutdown();
    return 0;
}
