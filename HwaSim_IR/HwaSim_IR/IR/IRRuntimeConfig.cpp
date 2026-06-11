#include "IRRuntimeConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool FileExists(const std::string& path)
{
    std::ifstream in(path.c_str(), std::ios::in);
    return in.good();
}

}

bool IRRuntimeConfig::loadFromCandidates(const std::vector<std::string>& candidates)
{
    m_loaded = false;
    m_values.clear();
    m_seenEnvOverrides.clear();
    m_loadedPath = candidates.empty() ? "Config/HwaSimIRRuntime.ini" : candidates.front();

    std::string selectedPath;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        if (FileExists(candidates[i]))
        {
            selectedPath = candidates[i];
            break;
        }
    }

    if (selectedPath.empty())
    {
        return false;
    }

    std::ifstream in(selectedPath.c_str(), std::ios::in);
    if (!in.good())
    {
        return false;
    }

    std::string section;
    std::string line;
    while (std::getline(in, line))
    {
        std::string cleaned = trim(line);
        if (cleaned.empty() || cleaned[0] == ';' || cleaned[0] == '#')
        {
            continue;
        }

        if (cleaned.front() == '[' && cleaned.back() == ']')
        {
            section = trim(cleaned.substr(1, cleaned.size() - 2));
            continue;
        }

        const size_t eq = cleaned.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        std::string key = trim(cleaned.substr(0, eq));
        std::string value = trim(cleaned.substr(eq + 1));

        const size_t commentSemi = value.find(';');
        const size_t commentHash = value.find('#');
        size_t comment = std::string::npos;
        if (commentSemi != std::string::npos)
        {
            comment = commentSemi;
        }
        if (commentHash != std::string::npos)
        {
            comment = (comment == std::string::npos) ? commentHash : std::min(comment, commentHash);
        }
        if (comment != std::string::npos)
        {
            value = trim(value.substr(0, comment));
        }

        if (!section.empty() && !key.empty())
        {
            m_values[normalizeKey(section, key)] = value;
        }
    }

    m_loaded = true;
    m_loadedPath = selectedPath;
    return true;
}

bool IRRuntimeConfig::hasIniValue(const std::string& section, const std::string& key) const
{
    return m_values.find(normalizeKey(section, key)) != m_values.end();
}

bool IRRuntimeConfig::hasEnvValue(const std::string& envName) const
{
    bool present = false;
    readEnv(envName, &present);
    return present;
}

bool IRRuntimeConfig::getBool(const std::string& section,
                              const std::string& key,
                              const std::string& envName,
                              bool defaultValue,
                              std::string* source)
{
    bool envPresent = false;
    const std::string envValue = readEnv(envName, &envPresent);
    if (envPresent)
    {
        m_seenEnvOverrides.insert(envName);
        if (source)
        {
            *source = "env";
        }
        return parseBool(envValue, defaultValue);
    }

    bool iniPresent = false;
    const std::string iniValue = getIniString(section, key, std::string(), &iniPresent);
    if (iniPresent)
    {
        if (source)
        {
            *source = "ini";
        }
        return parseBool(iniValue, defaultValue);
    }

    if (source)
    {
        *source = "default";
    }
    return defaultValue;
}

int IRRuntimeConfig::getInt(const std::string& section,
                            const std::string& key,
                            const std::string& envName,
                            int defaultValue,
                            std::string* source)
{
    std::string valueSource;
    const std::string text = getString(section, key, envName, std::string(), &valueSource);
    if (valueSource == "default")
    {
        if (source)
        {
            *source = valueSource;
        }
        return defaultValue;
    }

    char* end = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (source)
    {
        *source = valueSource;
    }
    return (end != text.c_str()) ? static_cast<int>(parsed) : defaultValue;
}

double IRRuntimeConfig::getDouble(const std::string& section,
                                  const std::string& key,
                                  const std::string& envName,
                                  double defaultValue,
                                  std::string* source)
{
    std::string valueSource;
    const std::string text = getString(section, key, envName, std::string(), &valueSource);
    if (valueSource == "default")
    {
        if (source)
        {
            *source = valueSource;
        }
        return defaultValue;
    }

    char* end = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (source)
    {
        *source = valueSource;
    }
    return (end != text.c_str()) ? parsed : defaultValue;
}

std::string IRRuntimeConfig::getString(const std::string& section,
                                       const std::string& key,
                                       const std::string& envName,
                                       const std::string& defaultValue,
                                       std::string* source)
{
    bool envPresent = false;
    const std::string envValue = readEnv(envName, &envPresent);
    if (envPresent)
    {
        m_seenEnvOverrides.insert(envName);
        if (source)
        {
            *source = "env";
        }
        return envValue;
    }

    bool iniPresent = false;
    const std::string iniValue = getIniString(section, key, defaultValue, &iniPresent);
    if (iniPresent)
    {
        if (source)
        {
            *source = "ini";
        }
        return iniValue;
    }

    if (source)
    {
        *source = "default";
    }
    return defaultValue;
}

std::string IRRuntimeConfig::trim(const std::string& text)
{
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])))
    {
        ++first;
    }

    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])))
    {
        --last;
    }

    return text.substr(first, last - first);
}

std::string IRRuntimeConfig::normalizeKey(const std::string& section, const std::string& key)
{
    return ToLower(trim(section)) + "." + ToLower(trim(key));
}

bool IRRuntimeConfig::parseBool(const std::string& text, bool fallback)
{
    const std::string value = ToLower(trim(text));
    if (value == "1" || value == "true" || value == "yes" || value == "on")
    {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off")
    {
        return false;
    }
    return fallback;
}

std::string IRRuntimeConfig::readEnv(const std::string& envName, bool* present)
{
    if (present)
    {
        *present = false;
    }
    if (envName.empty())
    {
        return std::string();
    }

#if defined(_MSC_VER)
    size_t required = 0;
    getenv_s(&required, 0, 0, envName.c_str());
    if (required == 0)
    {
        return std::string();
    }

    std::vector<char> buffer(required);
    getenv_s(&required, &buffer[0], buffer.size(), envName.c_str());
    if (present)
    {
        *present = true;
    }
    return std::string(&buffer[0]);
#else
    const char* value = std::getenv(envName.c_str());
    if (!value)
    {
        return std::string();
    }
    if (present)
    {
        *present = true;
    }
    return std::string(value);
#endif
}

std::string IRRuntimeConfig::getIniString(const std::string& section,
                                          const std::string& key,
                                          const std::string& defaultValue,
                                          bool* present) const
{
    const std::map<std::string, std::string>::const_iterator it = m_values.find(normalizeKey(section, key));
    if (it == m_values.end())
    {
        if (present)
        {
            *present = false;
        }
        return defaultValue;
    }

    if (present)
    {
        *present = true;
    }
    return it->second;
}
