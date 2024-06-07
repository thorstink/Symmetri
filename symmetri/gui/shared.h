#pragma once
#include <functional>
#include <string>

void drawColorDropdownMenu(const std::string& menu_name,
                           const std::vector<std::string>& colors,
                           const std::function<void(const std::string&)>& func);
