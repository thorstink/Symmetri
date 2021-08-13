#include "Systemetri/systemetri.h"
#include "transitions.h"
int main(int argc, const char *argv[]) {

  auto pnml_path = std::string(argv[1]);

  auto go = systemetri::start(pnml_path, getStore());

  go();
}
