#pragma once
#include "Symmetri/symmetri.h"

namespace T1 {
symmetri::OptionalError action0();
symmetri::OptionalError action1();
} // namespace T1

symmetri::OptionalError action2();
symmetri::OptionalError action3();
symmetri::OptionalError action4();
symmetri::OptionalError action5();
symmetri::OptionalError action6();

// inline TransitionActionMap getStore() {
//   return {{"t18", &action0}, {"t19", &action1}, {"t20", &action2},
//           {"t21", &action3}, {"t22", &action4}, {"t23", &action5},
//           {"t24", &action6}, {"t25", &action3}, {"t26", &action4},
//           {"t27", &action5}, {"t28", &action6}, {"t29", &action6}};
// };
