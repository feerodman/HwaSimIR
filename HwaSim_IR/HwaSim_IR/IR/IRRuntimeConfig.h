#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

// HwaSimIR runtime switch/config reader.
// Priority for runtime switches is: environment variable > HwaSimIRRuntime.ini > code default.
class IRRuntimeConfig
{
public:
    bool loadFromCandidates(const std::vector<std::string>& candidates);

    bool loaded() const { return m_loaded; }
    const std::string& loadedPath() const { return m_loadedPath; }
    size_t iniValueCount() const { return m_values.size(); }
    size_t envOverrideCount() const { return m_seenEnvOverrides.size(); }
    const char* sourcePriority() const { return "env>ini>default"; }

    bool hasIniValue(const std::string& section, const std::string& key) const;
    bool hasEnvValue(const std::string& envName) const;

    bool getBool(const std::string& section,
                 const std::string& key,
                 const std::string& envName,
                 bool defaultValue,
                 std::string* source = 0);

    int getInt(const std::string& section,
               const std::string& key,
               const std::string& envName,
               int defaultValue,
               std::string* source = 0);

    double getDouble(const std::string& section,
                     const std::string& key,
                     const std::string& envName,
                     double defaultValue,
                     std::string* source = 0);

    std::string getString(const std::string& section,
                          const std::string& key,
                          const std::string& envName,
                          const std::string& defaultValue,
                          std::string* source = 0);

private:
    static std::string trim(const std::string& text);
    static std::string normalizeKey(const std::string& section, const std::string& key);
    static bool parseBool(const std::string& text, bool fallback);
    static std::string readEnv(const std::string& envName, bool* present);

    std::string getIniString(const std::string& section,
                             const std::string& key,
                             const std::string& defaultValue,
                             bool* present) const;

    bool m_loaded = false;
    std::string m_loadedPath;
    std::map<std::string, std::string> m_values;
    std::set<std::string> m_seenEnvOverrides;
};
