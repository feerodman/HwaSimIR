#include "IRConfig.h"

#include <fstream>
#include <sstream>

namespace
{
const IRBand kOrderedBands[] = {
	IRBand::Visible,
	IRBand::NearInfrared,
	IRBand::ShortWaveInfrared,
	IRBand::MidWaveInfrared,
	IRBand::LongWaveInfrared
};

std::string joinPath(const std::string& directory, const std::string& fileName)
{
	if (directory.empty())
	{
		return fileName;
	}
	const char last = directory[directory.size() - 1];
	if (last == '/' || last == '\\')
	{
		return directory + fileName;
	}
	return directory + "/" + fileName;
}

bool readWholeFile(const std::string& filePath, std::string& text)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
	{
		return false;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	text = buffer.str();
	return true;
}

bool extractJsonNumber(const std::string& text, const std::string& key, double& value)
{
	const std::string token = "\"" + key + "\"";
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find(':', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	size_t begin = text.find_first_of("-+0123456789.", pos + 1);
	if (begin == std::string::npos)
	{
		return false;
	}
	size_t end = begin;
	while (end < text.size())
	{
		char c = text[end];
		if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
		{
			break;
		}
		++end;
	}
	try
	{
		value = std::stod(text.substr(begin, end - begin));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void assignNumber(const std::string& text, const std::string& key, double& target)
{
	double value = target;
	if (extractJsonNumber(text, key, value))
	{
		target = value;
	}
}

void assignNumber(const std::string& text, const std::string& key, int& target)
{
	double value = static_cast<double>(target);
	if (extractJsonNumber(text, key, value))
	{
		target = static_cast<int>(value + 0.5);
	}
}
}

IRSensorProfileDatabase::IRSensorProfileDatabase()
	: m_loaded(false)
{
	resetToFallbacks();
}

bool IRSensorProfileDatabase::loadFromDirectoryCandidates(const std::vector<std::string>& directories)
{
	resetToFallbacks();

	for (size_t i = 0; i < directories.size(); ++i)
	{
		bool loadedAny = false;
		for (size_t bandIndex = 0; bandIndex < sizeof(kOrderedBands) / sizeof(kOrderedBands[0]); ++bandIndex)
		{
			IRBand band = kOrderedBands[bandIndex];
			const std::string path = joinPath(directories[i], IRSensorProfileFileName(band));
			loadedAny = loadProfileFromFile(band, path) || loadedAny;
		}
		if (loadedAny)
		{
			m_loadedDirectory = directories[i];
			m_loaded = true;
			return true;
		}
	}
	return false;
}

const IRSensorProfile& IRSensorProfileDatabase::profileForBand(IRBand band) const
{
	std::map<IRBand, IRSensorProfile>::const_iterator it = m_profiles.find(band);
	if (it != m_profiles.end())
	{
		return it->second;
	}
	return m_profiles.find(IRBand::MidWaveInfrared)->second;
}

const IRSensorProfile& IRSensorProfileDatabase::profileForProtocolBand(int protocolBand) const
{
	return profileForBand(IRBandFromProtocol(protocolBand));
}

std::vector<IRSensorProfile> IRSensorProfileDatabase::allProfiles() const
{
	std::vector<IRSensorProfile> profiles;
	for (size_t i = 0; i < sizeof(kOrderedBands) / sizeof(kOrderedBands[0]); ++i)
	{
		profiles.push_back(profileForBand(kOrderedBands[i]));
	}
	return profiles;
}

bool IRSensorProfileDatabase::loaded() const
{
	return m_loaded;
}

const std::string& IRSensorProfileDatabase::loadedDirectory() const
{
	return m_loadedDirectory;
}

void IRSensorProfileDatabase::resetToFallbacks()
{
	m_profiles.clear();
	for (size_t i = 0; i < sizeof(kOrderedBands) / sizeof(kOrderedBands[0]); ++i)
	{
		IRBand band = kOrderedBands[i];
		m_profiles[band] = IRDefaultSensorProfile(band);
	}
	m_loadedDirectory.clear();
	m_loaded = false;
}

bool IRSensorProfileDatabase::loadProfileFromFile(IRBand band, const std::string& filePath)
{
	std::string text;
	if (!readWholeFile(filePath, text))
	{
		return false;
	}

	IRSensorProfile profile = IRDefaultSensorProfile(band);
	profile.loadedFromFile = true;
	profile.sourcePath = filePath;
	assignNumber(text, "SpectralResponseRangeLow", profile.spectralLowUm);
	assignNumber(text, "SpectralResponseRangeHigh", profile.spectralHighUm);
	assignNumber(text, "Width", profile.width);
	assignNumber(text, "Height", profile.height);
	assignNumber(text, "FOVH", profile.fovHDeg);
	assignNumber(text, "FOVV", profile.fovVDeg);
	assignNumber(text, "FocalLength", profile.focalLengthMm);
	assignNumber(text, "DetectorPitch", profile.detectorPitchMm);
	assignNumber(text, "ADCBitNumber", profile.adcBits);
	assignNumber(text, "DisplayBits", profile.displayBits);
	assignNumber(text, "NoiseEquivalentTemperatureDifference", profile.netdK);
	m_profiles[band] = profile;
	return true;
}
