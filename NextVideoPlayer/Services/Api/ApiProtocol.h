#pragma once
#include "Services/Api/ApiTypes.h"

namespace Api
{
bool Parse(const std::string &text, Command &command, std::string &error);
std::string Response(const std::string &requestId, const Result &result, const State *state = nullptr);
std::string StateEvent(const State &state);
} // namespace Api
