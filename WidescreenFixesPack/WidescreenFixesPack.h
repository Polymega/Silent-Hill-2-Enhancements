#pragma once

#include <Windows.h>
#include <string>
#include <string_view>
#include "Common\Settings.h"

void WSFInit();
int GetValue(std::string_view, std::string_view szKey, int);
