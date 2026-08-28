#include "HwaSimIR.h"

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <map>
#include <string>

namespace
{
bool ReadOptionValue(int argc, char** argv, int& index, const char* name, std::string& value)
{
	const std::string argument = argv[index] ? argv[index] : "";
	const std::string prefix = std::string(name) + "=";
	if (argument.compare(0, prefix.size(), prefix) == 0)
	{
		value = argument.substr(prefix.size());
		return true;
	}
	if (argument == name)
	{
		if (index + 1 >= argc || argv[index + 1] == nullptr)
		{
			std::cerr << "[RuntimeInstance][ERROR] missing value for " << name << std::endl;
			return false;
		}
		value = argv[++index];
		return true;
	}
	return false;
}

std::string ReadEnvironment(const char* name)
{
	#if defined(_MSC_VER)
	char* value = nullptr;
	size_t length = 0;
	if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
	{
		return std::string();
	}
	const std::string result(value);
	std::free(value);
	return result;
	#else
	const char* value = std::getenv(name);
	return value ? std::string(value) : std::string();
	#endif
}

bool IsValidChannel(const std::string& channel)
{
	return channel == "precise" || channel == "coarse";
}

bool SameCloudDescriptor(const IRWorldCloudDescriptor& a, const IRWorldCloudDescriptor& b)
{
	return a.cloudId == b.cloudId && a.seed == b.seed &&
		a.cellX == b.cellX && a.cellY == b.cellY &&
		std::abs(a.worldX - b.worldX) < 1.0e-9 &&
		std::abs(a.worldY - b.worldY) < 1.0e-9 &&
		std::abs(a.worldZ - b.worldZ) < 1.0e-9 &&
		std::abs(a.radiusX - b.radiusX) < 1.0e-9 &&
		std::abs(a.radiusY - b.radiusY) < 1.0e-9 &&
		std::abs(a.radiusZ - b.radiusZ) < 1.0e-9 &&
		std::abs(a.density - b.density) < 1.0e-9 &&
		a.densityTemplate == b.densityTemplate;
}

int RunW15CloudModelCheck()
{
	IRWorldCloudStreamingConfig config;
	config.weatherSeed = 12345;
	IRWorldCloudStreaming streaming;
	streaming.setConfig(config);

	const IRWorldCloudDescriptor first = streaming.descriptorForCell(
		7, -3, 100.0, "Cloudy", 1.0, 0.88, 4);
	const IRWorldCloudDescriptor second = streaming.descriptorForCell(
		7, -3, 100.0, "Cloudy", 1.0, 0.88, 4);
	if (!SameCloudDescriptor(first, second))
	{
		std::cerr << "[World3DCloudModelCheck] result=FAIL reason=deterministic_descriptor" << std::endl;
		return 1;
	}

	const std::vector<IRWorldCloudDescriptor> initial = streaming.queryCandidates(
		0.0, 0.0, 0.0, "Cloudy", 1.0, 0.88, 4);
	(void)streaming.queryCandidates(50000.0, 50000.0, 0.0, "Cloudy", 1.0, 0.88, 4);
	const std::vector<IRWorldCloudDescriptor> returned = streaming.queryCandidates(
		0.0, 0.0, 0.0, "Cloudy", 1.0, 0.88, 4);
	std::map<std::uint64_t, IRWorldCloudDescriptor> returnedById;
	for (size_t i = 0; i < returned.size(); ++i)
	{
		returnedById[returned[i].cloudId] = returned[i];
	}
	for (size_t i = 0; i < initial.size(); ++i)
	{
		const std::map<std::uint64_t, IRWorldCloudDescriptor>::const_iterator found =
			returnedById.find(initial[i].cloudId);
		if (found == returnedById.end() || !SameCloudDescriptor(initial[i], found->second))
		{
			std::cerr << "[World3DCloudModelCheck] result=FAIL reason=unload_reload_identity" << std::endl;
			return 2;
		}
	}
	if (streaming.config().deactivationRadiusM <= streaming.config().streamingRadiusM)
	{
		std::cerr << "[World3DCloudModelCheck] result=FAIL reason=hysteresis_config" << std::endl;
		return 3;
	}

	std::cout << "[World3DCloudModelCheck] result=PASS"
		<< " deterministicDescriptor=1"
		<< " unloadReloadIdentity=1"
		<< " candidateCount=" << initial.size()
		<< " hysteresisM="
		<< (streaming.config().deactivationRadiusM - streaming.config().streamingRadiusM)
		<< " sampleCloudId=" << IRWorldCloudStreaming::cloudIdText(first.cloudId)
		<< std::endl;
	return 0;
}
}

// Application entry point.
int main(int argc, char *argv[]) {
	std::string cliChannel;
	std::string cliNetworkConfig;
	bool w15CloudModelCheck = false;
	int frameworkArgc = 1;
	for (int i = 1; i < argc; ++i)
	{
		const std::string argument = argv[i] ? argv[i] : "";
		if (argument == "--w15-cloud-model-check")
		{
			w15CloudModelCheck = true;
			continue;
		}
		if (argument == "--channel" || argument.compare(0, 10, "--channel=") == 0)
		{
			if (!ReadOptionValue(argc, argv, i, "--channel", cliChannel))
			{
				return 2;
			}
			continue;
		}
		if (argument == "--network-config" || argument.compare(0, 17, "--network-config=") == 0)
		{
			if (!ReadOptionValue(argc, argv, i, "--network-config", cliNetworkConfig))
			{
				return 2;
			}
			continue;
		}
		argv[frameworkArgc++] = argv[i];
	}
	argv[frameworkArgc] = nullptr;
	if (w15CloudModelCheck)
	{
		return RunW15CloudModelCheck();
	}

	if (!cliChannel.empty() && !IsValidChannel(cliChannel))
	{
		std::cerr << "[RuntimeInstance][ERROR] invalid --channel=" << cliChannel
			<< " expected=precise|coarse" << std::endl;
		return 2;
	}

	HwaSimIRLaunchOptions launchOptions;
	const std::string envNetworkConfig = ReadEnvironment("HWASIMIR_NETWORK_CONFIG");
	const std::string envChannel = ReadEnvironment("HWASIMIR_CHANNEL");
	if (!cliNetworkConfig.empty())
	{
		launchOptions.networkConfigPath = cliNetworkConfig;
		launchOptions.channel = cliChannel.empty() ? "precise" : cliChannel;
		launchOptions.configSource = "cli-network-config";
	}
	else if (!cliChannel.empty())
	{
		launchOptions.channel = cliChannel;
		launchOptions.configSource = "cli-channel";
	}
	else if (!envNetworkConfig.empty())
	{
		launchOptions.networkConfigPath = envNetworkConfig;
		launchOptions.channel = IsValidChannel(envChannel) ? envChannel : "precise";
		launchOptions.configSource = "env-network-config";
	}
	else if (!envChannel.empty())
	{
		if (!IsValidChannel(envChannel))
		{
			std::cerr << "[RuntimeInstance][ERROR] invalid HWASIMIR_CHANNEL=" << envChannel
				<< " expected=precise|coarse" << std::endl;
			return 2;
		}
		launchOptions.channel = envChannel;
		launchOptions.configSource = "env-channel";
	}

	std::cout << "[ProtocolLayout] component=HwaSim_IR"
		<< " ControlP2cX1ObjTrackingCmd=" << sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)
		<< " InitP2cObjectTrackingCmd=" << sizeof(BYHWICD::InitP2cObjectTrackingCmd)
		<< " DisplayC2cObjTrackingData=" << sizeof(BYHWICD::DisplayC2cObjTrackingData)
		<< " InitAckC2pObjectTrackingCmd=" << sizeof(BYHWICD::InitAckC2pObjectTrackingCmd)
		<< std::endl;

	// Create the runtime instance after removing application-specific CLI options.
	HwaSimIR app(frameworkArgc, argv, launchOptions);
	if (!app.startupSucceeded())
	{
		return app.startupExitCode() != 0 ? app.startupExitCode() : 1;
	}

	// Run the render loop.
	app.run();

	// Destructors release runtime resources.
	return 0;
}
