#pragma once

#include "IRTypes.h"

#include <map>
#include <string>
#include <vector>

class IRSensorProfileDatabase
{
public:
	IRSensorProfileDatabase();

	bool loadFromDirectoryCandidates(const std::vector<std::string>& directories);
	const IRSensorProfile& profileForBand(IRBand band) const;
	const IRSensorProfile& profileForProtocolBand(int protocolBand) const;
	std::vector<IRSensorProfile> allProfiles() const;
	bool loaded() const;
	const std::string& loadedDirectory() const;

private:
	void resetToFallbacks();
	bool loadProfileFromFile(IRBand band, const std::string& filePath);

	std::map<IRBand, IRSensorProfile> m_profiles;
	std::string m_loadedDirectory;
	bool m_loaded;
};
