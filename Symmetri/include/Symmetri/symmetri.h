#pragma once
#include "Symmetri/types.h"
#include <string>

namespace symmetri {

std::function<void()> start(const std::string &path_to_petri,
                            const TransitionActionMap &);
} // namespace symmetri
