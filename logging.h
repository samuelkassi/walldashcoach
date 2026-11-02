#pragma once

#include <format>
#include <memory>
#include <string>
#include <utility>

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"

extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

// Prints a formatted line to the BakkesMod console.
template <typename... Args>
void LOG(std::format_string<Args...> format, Args&&... args)
{
	_globalCvarManager->log(std::format(format, std::forward<Args>(args)...));
}
