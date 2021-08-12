#pragma once
#include "application.hpp"
#include <tuple>

std::tuple<application::TransitionMutation, application::TransitionMutation,
           application::Marking>
constructTransitionMutationMatrices(std::string file);