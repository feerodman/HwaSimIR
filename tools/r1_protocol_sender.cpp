#include "HwaSim_IR/HwaSim_IR/Common/CommonData.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

namespace
{
std::map<std::string, std::string> ParseArgs(int argc, char** argv)
{
    std::map<std::string, std::string> values;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        const std::size_t eq = arg.find('=');
        if (eq != std::string::npos)
        {
            values[arg.substr(0, eq)] = arg.substr(eq + 1);
        }
    }
    return values;
}

std::string GetString(
    const std::map<std::string, std::string>& values,
    const std::string& name,
    const std::string& fallback)
{
    const std::map<std::string, std::string>::const_iterator it = values.find(name);
    return it == values.end() ? fallback : it->second;
}

int GetInt(
    const std::map<std::string, std::string>& values,
    const std::string& name,
    int fallback)
{
    const std::string text = GetString(values, name, "");
    if (text.empty())
    {
        return fallback;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    return end != text.c_str() ? static_cast<int>(parsed) : fallback;
}

bool ConfigureAddress(sockaddr_in& address, const std::string& host, int port)
{
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    return inet_pton(AF_INET, host.c_str(), &address.sin_addr) == 1;
}

bool SendPacket(
    SOCKET socketHandle,
    const sockaddr_in& target,
    const void* packet,
    int packetSize)
{
    return sendto(
        socketHandle,
        reinterpret_cast<const char*>(packet),
        packetSize,
        0,
        reinterpret_cast<const sockaddr*>(&target),
        sizeof(target)) == packetSize;
}
}

int main(int argc, char** argv)
{
    const std::map<std::string, std::string> args = ParseArgs(argc, argv);
    const std::string type = GetString(args, "--type", "display");
    const std::string host = GetString(args, "--host", "127.0.0.1");
    const int port = GetInt(args, "--port", 18888);
    const int sourcePort = GetInt(args, "--source-port", 0);
    const int platID = GetInt(args, "--plat-id", 1001);
    const int sensorID = GetInt(args, "--sensor-id", 2);
    const int simMode = GetInt(args, "--sim-mode", 2);
    const int videoFps = GetInt(args, "--video-fps", 60);
    const int h264En = GetInt(args, "--h264", 0);
    const int saveMP4En = GetInt(args, "--save-mp4", 0);
    const int simCommand = GetInt(args, "--command", 1);
    const int ackPort = GetInt(args, "--ack-port", 0);
    const int expectAck = GetInt(args, "--expect-ack", -1);
    const int expectedAckPlatID = GetInt(args, "--ack-plat-id", platID);
    const int expectedAckSensorID = GetInt(args, "--ack-sensor-id", sensorID);
    const int timeoutMs = GetInt(args, "--timeout-ms", 15000);

    std::cout << "[ProtocolLayout] component=r1_protocol_sender"
        << " ControlP2cX1ObjTrackingCmd=" << sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)
        << " InitP2cObjectTrackingCmd=" << sizeof(BYHWICD::InitP2cObjectTrackingCmd)
        << " DisplayC2cObjTrackingData=" << sizeof(BYHWICD::DisplayC2cObjTrackingData)
        << " InitAckC2pObjectTrackingCmd=" << sizeof(BYHWICD::InitAckC2pObjectTrackingCmd)
        << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed" << std::endl;
        return 2;
    }

    SOCKET sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET ackSocket = INVALID_SOCKET;
    if (sendSocket == INVALID_SOCKET)
    {
        WSACleanup();
        return 3;
    }

    if (sourcePort > 0)
    {
        sockaddr_in sourceAddress;
        if (!ConfigureAddress(sourceAddress, "127.0.0.1", sourcePort) ||
            bind(sendSocket, reinterpret_cast<const sockaddr*>(&sourceAddress), sizeof(sourceAddress)) == SOCKET_ERROR)
        {
            std::cerr << "source bind failed port=" << sourcePort << std::endl;
            closesocket(sendSocket);
            WSACleanup();
            return 4;
        }
    }

    if (ackPort > 0)
    {
        ackSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in ackAddress;
        if (ackSocket == INVALID_SOCKET ||
            !ConfigureAddress(ackAddress, "127.0.0.1", ackPort) ||
            bind(ackSocket, reinterpret_cast<const sockaddr*>(&ackAddress), sizeof(ackAddress)) == SOCKET_ERROR)
        {
            std::cerr << "ack bind failed port=" << ackPort << std::endl;
            if (ackSocket != INVALID_SOCKET) closesocket(ackSocket);
            closesocket(sendSocket);
            WSACleanup();
            return 5;
        }
        const DWORD timeout = static_cast<DWORD>(timeoutMs);
        setsockopt(ackSocket, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    }

    sockaddr_in targetAddress;
    if (!ConfigureAddress(targetAddress, host, port))
    {
        std::cerr << "invalid target address" << std::endl;
        return 6;
    }

    bool sent = false;
    int flag = 0;
    int packetSize = 0;
    if (type == "control")
    {
        BYHWICD::ControlP2cX1ObjTrackingCmd packet{};
        packet.flag = 0x41;
        packet.JB = 1;
        packet.platID = platID;
        packet.simCommand = simCommand;
        packet.roundCut = 1;
        packet.currentRound = 1;
        flag = packet.flag;
        packetSize = static_cast<int>(sizeof(packet));
        sent = SendPacket(sendSocket, targetAddress, &packet, packetSize);
    }
    else if (type == "init")
    {
        BYHWICD::InitP2cObjectTrackingCmd packet{};
        packet.flag = 0x36;
        packet.JB = 1;
        packet.platID = platID;
        packet.sensorID = sensorID;
        packet.platParamInit.id = 1;
        packet.platParamInit.type = 1;
        packet.trackingInit.enable = true;
        packet.trackingInit.simMode = simMode;
        packet.trackingInit.videoFps = videoFps;
        packet.trackingInit.trackerSensor[0].trackerSensorBand = 2;
        packet.trackingInit.trackerSensor[0].trackerSensorWidth = 800;
        packet.trackingInit.trackerSensor[0].trackerSensorHeight = 800;
        packet.trackingInit.trackerSensor[0].trackerSensorViewMin = 1;
        packet.trackingInit.trackerSensor[0].trackerSensorViewMax = 50000;
        packet.trackingInit.trackerSensor[0].trackerSensorPixelAngle = 2.5;
        packet.trackingInit.trackerSensor[0].h264En = h264En != 0;
        packet.trackingInit.trackerSensor[0].saveMP4En = saveMP4En != 0;
        flag = packet.flag;
        packetSize = static_cast<int>(sizeof(packet));
        sent = SendPacket(sendSocket, targetAddress, &packet, packetSize);
    }
    else if (type == "display")
    {
        BYHWICD::DisplayC2cObjTrackingData packet{};
        packet.flag = 0x38;
        packet.platID = platID;
        packet.sensorID = sensorID;
        packet.targetNumValid = 0;
        flag = packet.flag;
        packetSize = static_cast<int>(sizeof(packet));
        sent = SendPacket(sendSocket, targetAddress, &packet, packetSize);
    }
    else
    {
        std::cerr << "unsupported type=" << type << std::endl;
        return 7;
    }

    if (!sent)
    {
        std::cerr << "send failed error=" << WSAGetLastError() << std::endl;
        return 8;
    }
    std::cout << "[ProtocolSend] type=" << type
        << " flag=0x" << std::hex << flag << std::dec
        << " bytes=" << packetSize
        << " target=" << host << ":" << port
        << " sourcePort=" << sourcePort
        << " platID=" << platID
        << " sensorID=" << (type == "control" ? -1 : sensorID)
        << " simMode=" << simMode
        << " videoFps=" << videoFps
        << " h264En=" << h264En
        << " saveMP4En=" << saveMP4En
        << std::endl;

    int result = 0;
    if (ackSocket != INVALID_SOCKET && expectAck >= 0)
    {
        BYHWICD::InitAckC2pObjectTrackingCmd ack{};
        sockaddr_in senderAddress;
        int senderLength = sizeof(senderAddress);
        const int received = recvfrom(
            ackSocket,
            reinterpret_cast<char*>(&ack),
            sizeof(ack),
            0,
            reinterpret_cast<sockaddr*>(&senderAddress),
            &senderLength);
        const bool gotAck = received == static_cast<int>(sizeof(ack)) && ack.flag == 0x37;
        std::cout << "[ProtocolAck] expected=" << expectAck
            << " received=" << (gotAck ? 1 : 0);
        if (gotAck)
        {
            std::cout << " platID=" << ack.platID
                << " sensorID=" << ack.sensorID
                << " trackingReady=" << (ack.trackingReady ? 1 : 0);
        }
        std::cout << std::endl;

        if ((expectAck == 1) != gotAck)
        {
            result = 9;
        }
        else if (gotAck &&
            (ack.platID != expectedAckPlatID || ack.sensorID != expectedAckSensorID))
        {
            std::cerr << "ACK identity mismatch expectedPlatID=" << expectedAckPlatID
                << " expectedSensorID=" << expectedAckSensorID << std::endl;
            result = 10;
        }
    }

    if (ackSocket != INVALID_SOCKET) closesocket(ackSocket);
    closesocket(sendSocket);
    WSACleanup();
    return result;
}
