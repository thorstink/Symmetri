#pragma once

#include <filesystem>

#include "model.h"

namespace farbart {
model::Reducer writeToDisk(const std::filesystem::path& path);
}  // namespace farbart
