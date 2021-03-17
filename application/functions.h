#pragma once
#include "model.hpp"
#include "transitions.hpp"
#include <string>

using OptionalReducer = std::optional<model::Reducer>;
OptionalReducer execute(const transitions::Transition &transition);
