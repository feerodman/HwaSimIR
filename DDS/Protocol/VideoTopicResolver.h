#pragma once

#include <algorithm>
#include <cctype>
#include <string>

inline std::string CanonicalVideoCodecToken(const std::string& codec)
{
    std::string value(codec);
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "h264") return "H264";
    if (value == "raw_gray8" || value == "gray8") return "RawGray8";
    if (value == "raw_bgr24" || value == "bgr24") return "RawBGR24";
    return std::string();
}

inline void ReplaceTopicToken(std::string& value, const std::string& token,
    const std::string& replacement)
{
    std::string::size_type offset = 0;
    while ((offset = value.find(token, offset)) != std::string::npos)
    {
        value.replace(offset, token.size(), replacement);
        offset += replacement.size();
    }
}

inline bool ResolveIdentityTopicPattern(const std::string& pattern, int platID,
    int sensorID, const std::string& codecToken, std::string& resolved,
    std::string& error)
{
    resolved.clear();
    error.clear();
    if (pattern.empty()) { error = "topic pattern is empty"; return false; }
    resolved = pattern;
    ReplaceTopicToken(resolved, "{platID}", std::to_string(platID));
    ReplaceTopicToken(resolved, "{sensorID}", std::to_string(sensorID));
    if (resolved.find("{codec}") != std::string::npos)
    {
        if (codecToken.empty()) { error = "codec token is required by pattern"; return false; }
        ReplaceTopicToken(resolved, "{codec}", codecToken);
    }
    if (resolved.empty()) { error = "resolved topic is empty"; return false; }
    if (resolved.find('{') != std::string::npos || resolved.find('}') != std::string::npos)
    {
        error = "unknown topic token in pattern: " + pattern;
        return false;
    }
    return true;
}

inline bool ResolveVideoTopic(const std::string& pattern, int platID, int sensorID,
    const std::string& codec, std::string& resolved, std::string& error)
{
    const std::string token = CanonicalVideoCodecToken(codec);
    if (token.empty())
    {
        error = "unsupported video codec: " + codec;
        resolved.clear();
        return false;
    }
    return ResolveIdentityTopicPattern(pattern, platID, sensorID, token, resolved, error);
}
