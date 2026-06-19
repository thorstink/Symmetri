#pragma once

#include "model.h"

namespace farbart {
// Ask to save the current net: serializes it now (on the caller's thread) and
// opens a confirm popup whose Save button writes the file off-thread.
void requestSaveGraph(const model::ViewModel& vm);

}  // namespace farbart
