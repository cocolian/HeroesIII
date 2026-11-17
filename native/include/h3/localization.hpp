#pragma once
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace h3 {
std::string chineseMapText(std::string_view scenario, std::string_view source);
// Translate display fields only. Spell identifiers, coordinates and rule keys stay intact.
void localizeMapData(nlohmann::json&, std::string_view scenario);
}
