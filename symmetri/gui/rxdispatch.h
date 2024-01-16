#pragma once

#include "model.h"
#include "rpp/rpp.hpp"
namespace rxdispatch {

void push(model::Reducer&& r);
void dequeue(const rpp::dynamic_subscriber<model::Reducer>& sub);
}  // namespace rxdispatch
