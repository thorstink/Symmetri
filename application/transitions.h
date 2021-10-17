#pragma once
#include "Symmetri/symmetri.h"

symmetri::OptionalMarkingMutation action0();
symmetri::OptionalMarkingMutation action1();
symmetri::OptionalMarkingMutation action2();
symmetri::OptionalMarkingMutation action3();
symmetri::OptionalMarkingMutation action4();
symmetri::OptionalMarkingMutation action5();
symmetri::OptionalMarkingMutation action6();

inline symmetri::TransitionActionMap getStore() {
  return {{"t0", &action0},  {"t1", &action1},  {"t2", &action2},
          {"t3", &action3},  {"t4", &action4},  {"t5", &action5},
          {"t6", &action6},  {"t18", &action0}, {"t19", &action1},
          {"t20", &action2}, {"t21", &action3}, {"t22", &action4},
          {"t23", &action5}, {"t24", &action6}, {"t25", &action3},
          {"t26", &action4}, {"t27", &action5}, {"t28", &action6},
          {"t29", &action6}};
};

// inline TransitionActionMap getStore() {
//   return {{"t18", &action0}, {"t19", &action1}, {"t20", &action2},
//           {"t21", &action3}, {"t22", &action4}, {"t23", &action5},
//           {"t24", &action6}, {"t25", &action3}, {"t26", &action4},
//           {"t27", &action5}, {"t28", &action6}, {"t29", &action6}};
// };
