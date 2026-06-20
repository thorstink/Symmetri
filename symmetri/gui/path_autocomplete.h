#pragma once

#include <filesystem>

namespace model {
struct ViewModel;
}

namespace farbart {

// Draws the active-file path input (the textbox above the graph) with inline
// filesystem autocomplete: the first/selected match shows as ghost text,
// Up/Down move the selection, Tab or a click accepts it, and Enter loads the
// file. Owns its item width and validity colouring so the call site stays a
// one-liner.
void drawPathInput(const model::ViewModel& vm);

// Loads `file` exactly like the File-menu browser does: into an empty net it
// loads directly, otherwise it asks whether to append or clear-and-load.
void requestLoad(const model::ViewModel& vm, const std::filesystem::path& file);

}  // namespace farbart
