#pragma once
#include <filesystem>

// Append the net in `file` to the current net.
void loadPetriNet(const std::filesystem::path& file);

// Replace the current net with the net in `file`.
void clearAndloadPetriNet(const std::filesystem::path& file);
