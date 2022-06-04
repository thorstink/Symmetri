#pragma once
#include "Symmetri/polyaction.h"
#include "Symmetri/types.h"

namespace symmetri {

symmetri::PolyAction retryFunc(const symmetri::PolyAction &f,
                               const symmetri::Transition &t,
                               const std::string &case_id,
                               unsigned int retry_count = 3);

}
