#pragma once
#include <iostream>

#include "model.h"

void drawUi(const model::ViewModel& vm) {
  std::cout << "Draw" << std::endl;
  for (auto drawable : vm.drawables) {
    drawable(vm);
  }
}
