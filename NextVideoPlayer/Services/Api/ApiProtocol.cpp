#include "Services/Api/ApiProtocol.h"
#include "Services/Api/TextEncoding.h"
#include "Version.h"
#include <cJSON.h>
#include <cmath>
#include <memory>
#include <unordered_set>

namespace Api
{
namespace
{
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
bool Keep(const cJSON *value)
{
    return cJSON_IsNull(value) || (cJSON_IsString(value) && !value->valuestring[0]);
}
bool Number(const cJSON *value, double low, double high, bool integer = false)
{
    return cJSON_IsNumber(value) && std::isfinite(value->valuedouble) && value->valuedouble >= low &&
           value->valuedouble <= high && (!integer || std::floor(value->valuedouble) == value->valuedouble);
}
cJSON *MakeState(const State &state, bool includeFiles)
{
    auto *object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "instanceId", state.instanceId.c_str());
    cJSON_AddStringToObject(object, "version", NVP_VERSION_STRING);
    cJSON_AddNumberToObject(object, "protocolVersion", 1);
    cJSON_AddStringToObject(object, "playback", state.playback.c_str());
    cJSON_AddStringToObject(object, "file", state.file.c_str());
    cJSON_AddStringToObject(object, "mediaError", state.mediaError.c_str());
    cJSON_AddBoolToObject(object, "ready", state.ready);
    cJSON_AddBoolToObject(object, "loading", state.loading);
    cJSON_AddNumberToObject(object, "positionMs", state.positionMs);
    cJSON_AddNumberToObject(object, "durationMs", state.durationMs);
    cJSON_AddNumberToObject(object, "speed", state.speed);
    cJSON_AddNumberToObject(object, "volume", state.volume);
    cJSON_AddBoolToObject(object, "muted", state.muted);
    cJSON_AddBoolToObject(object, "repeat", state.repeat);
    cJSON_AddBoolToObject(object, "fullscreen", state.fullscreen);
    cJSON_AddBoolToObject(object, "topMost", state.topMost);
    cJSON_AddNumberToObject(object, "playlistIndex", state.playlistIndex);
    cJSON_AddNumberToObject(object, "playlistCount", state.playlistCount);
    cJSON_AddNumberToObject(object, "playlistRevision", static_cast<double>(state.playlistRevision));
    cJSON_AddNumberToObject(object, "timestampMs", static_cast<double>(state.timestampMs));
    if (includeFiles)
    {
        auto *files = cJSON_AddArrayToObject(object, "files");
        for (const auto &file : state.files)
            cJSON_AddItemToArray(files, cJSON_CreateString(file.c_str()));
    }
    return object;
}
std::string Print(cJSON *value)
{
    Json owner(value, &cJSON_Delete);
    std::unique_ptr<char, decltype(&cJSON_free)> text(cJSON_PrintUnformatted(value), &cJSON_free);
    if (!text)
        throw std::bad_alloc();
    return text.get();
}
} // namespace

bool Parse(const std::string &text, Command &command, std::string &error)
{
    command = {};
    error.clear();
    if (text.empty() || text.size() > MaxMessageBytes)
    {
        error = "Message must be between 1 byte and 1 MiB";
        return false;
    }
    // Bound parser recursion and reject embedded NULs before entering cJSON.
    int depth = 0;
    bool quoted = false, escaped = false;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const char value = text[index];
        if (!value)
        {
            error = "NUL bytes are not supported";
            return false;
        }
        if (quoted)
        {
            if (escaped)
            {
                if (value == 'u' && text.compare(index, 5, "u0000") == 0)
                {
                    error = "NUL strings are not supported";
                    return false;
                }
                escaped = false;
            }
            else if (value == '\\')
                escaped = true;
            else if (value == '"')
                quoted = false;
        }
        else if (value == '"')
            quoted = true;
        else if (value == '{' || value == '[')
        {
            if (++depth > 32)
            {
                error = "JSON nesting limit exceeded";
                return false;
            }
        }
        else if (value == '}' || value == ']')
            --depth;
    }
    try
    {
        ToWide(text);
    }
    catch (...)
    {
        error = "Message must be UTF-8";
        return false;
    }
    const char *end = nullptr;
    Json root(cJSON_ParseWithLengthOpts(text.c_str(), text.size() + 1, &end, 0), &cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get()))
    {
        error = "Expected a JSON object";
        return false;
    }
    while (end && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        ++end;
    if (!end || *end)
    {
        error = "Unexpected text after JSON";
        return false;
    }
    const auto *identifier = cJSON_GetObjectItemCaseSensitive(root.get(), "requestId");
    if (cJSON_IsString(identifier) && std::string(identifier->valuestring).size() <= 128)
        command.requestId = identifier->valuestring;
    auto *type = cJSON_GetObjectItemCaseSensitive(root.get(), "type");
    if (!cJSON_IsString(type) || std::string(type->valuestring) != "all")
    {
        error = "type must be all";
        return false;
    }
    std::unordered_set<std::string> keys;
    for (auto *value = root->child; value; value = value->next)
    {
        const std::string key = value->string;
        if (!keys.insert(key).second)
        {
            error = "Duplicate key: " + key;
            return false;
        }
        if (key == "type")
            continue;
        if (key == "requestId")
        {
            if (cJSON_IsNull(value))
                continue;
            if (!cJSON_IsString(value) || std::string(value->valuestring).size() > 128)
            {
                error = "requestId must be a string of at most 128 bytes";
                return false;
            }
            command.requestId = value->valuestring;
            continue;
        }
        const bool known = key == "files" || key == "playlistIndex" || key == "playback" || key == "positionMs" ||
                           key == "speed" || key == "volume" || key == "muted" || key == "repeat" ||
                           key == "fullscreen" || key == "topMost" || key == "subscribe" || key == "executeAtMs";
        if (!known)
        {
            error = "Unknown key: " + key;
            return false;
        }
        if (Keep(value))
            continue;
        bool valid = true;
        if (key == "files")
        {
            valid = cJSON_IsArray(value) && cJSON_GetArraySize(value) <= 512;
            std::vector<std::string> files;
            if (valid)
                for (auto *file = value->child; file; file = file->next)
                {
                    if (!cJSON_IsString(file) || !file->valuestring[0] || std::string(file->valuestring).size() > 32767)
                    {
                        valid = false;
                        break;
                    }
                    files.emplace_back(file->valuestring);
                }
            if (valid && !files.empty())
                command.files = std::move(files);
        }
        else if (key == "playback")
        {
            valid = cJSON_IsString(value) &&
                    (std::string(value->valuestring) == "playing" || std::string(value->valuestring) == "paused" ||
                     std::string(value->valuestring) == "stopped");
            if (valid)
                command.playback = value->valuestring;
        }
        else if (key == "playlistIndex")
        {
            valid = Number(value, 0, 2147483647, true);
            if (valid)
                command.playlistIndex = static_cast<int>(value->valuedouble);
        }
        else if (key == "positionMs")
        {
            valid = Number(value, 0, 9007199254740991.0);
            if (valid)
                command.positionMs = value->valuedouble;
        }
        else if (key == "speed")
        {
            valid = Number(value, 0.20, 16.0);
            if (valid)
                command.speed = value->valuedouble;
        }
        else if (key == "volume")
        {
            valid = Number(value, 0, 100, true);
            if (valid)
                command.volume = static_cast<int>(value->valuedouble);
        }
        else if (key == "executeAtMs")
        {
            valid = Number(value, 0, 9007199254740991.0, true);
            if (valid)
                command.executeAtMs = static_cast<std::int64_t>(value->valuedouble);
        }
        else
        {
            valid = cJSON_IsBool(value);
            if (valid)
            {
                const bool flag = cJSON_IsTrue(value) != 0;
                if (key == "muted")
                    command.muted = flag;
                else if (key == "repeat")
                    command.repeat = flag;
                else if (key == "fullscreen")
                    command.fullscreen = flag;
                else if (key == "topMost")
                    command.topMost = flag;
                else
                    command.subscribe = flag;
            }
        }
        if (!valid)
        {
            error = "Invalid value for " + key;
            return false;
        }
    }
    return true;
}

std::string Response(const std::string &requestId, const Result &result, const State *state)
{
    auto *object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "type", "all");
    if (!requestId.empty())
        cJSON_AddStringToObject(object, "requestId", requestId.c_str());
    cJSON_AddBoolToObject(object, "ok", result.ok);
    cJSON_AddStringToObject(object, "status", result.ok ? (result.accepted ? "accepted" : "applied") : "error");
    if (!result.ok)
    {
        auto *error = cJSON_AddObjectToObject(object, "error");
        cJSON_AddStringToObject(error, "code", result.code.c_str());
        cJSON_AddStringToObject(error, "message", result.message.c_str());
    }
    if (state)
        cJSON_AddItemToObject(object, "state", MakeState(*state, true));
    return Print(object);
}
std::string StateEvent(const State &state)
{
    auto *object = cJSON_CreateObject();
    cJSON_AddStringToObject(object, "type", "all");
    cJSON_AddStringToObject(object, "event", "state");
    cJSON_AddItemToObject(object, "state", MakeState(state, false));
    return Print(object);
}
} // namespace Api
