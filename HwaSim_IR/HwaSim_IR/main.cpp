#include "HwaSimIR.h"

#include <cstdlib>
#include <iostream>
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
}

// Application entry point.
int main(int argc, char *argv[]) {
	std::string cliChannel;
	std::string cliNetworkConfig;
	int frameworkArgc = 1;
	for (int i = 1; i < argc; ++i)
	{
		const std::string argument = argv[i] ? argv[i] : "";
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

	// Run the render loop.
	app.run();

	// Destructors release runtime resources.
	return 0;
}
