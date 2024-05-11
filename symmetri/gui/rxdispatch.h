#pragma once

#include "model.h"
#include "rpp/rpp.hpp"
namespace rxdispatch {
void unsubscribe();
void push(model::Reducer&& r);
void dequeue(rpp::dynamic_observer<model::Reducer>&& observer);
}  // namespace rxdispatch
