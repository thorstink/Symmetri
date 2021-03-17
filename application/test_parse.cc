#include "parser.h"
#include <fstream>
#include <iostream>
#include <mpark/patterns.hpp>

using namespace mpark::patterns;

int main(int argc, const char *argv[]) {
  nlohmann::json net;
  if (argc == 2 && std::ifstream(argv[1]).good()) {
    std::ifstream i(argv[1]);
    if (i.good()) {
      i >> net;
    }
  }

  const auto [Dm, Dp, initial_marking] = constructTransitionMutationMatrices(net);
  // std::cout << std::endl;
  // std::cout << Dp << std::endl;
  // std::cout << "---------" << std::endl;
  // std::cout << Dm << std::endl;

  // auto transitions = Dm.cols();
  // auto places = Dm.rows();
  // application::TransitionMutation Dp_verify(transitions, places),
  //     Dm_verify(transitions, places);

  // Dm_verify = (Eigen::MatrixXi(transitions, places) << 1, 0, 0, 1, 0, 0, 0, 0,
  //              0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
  //              0, 0, 0, 0, 0, 0, 0, 1, 0, 0)
  //                 .finished();
  // Dp_verify = (Eigen::MatrixXi(transitions, places) << 0, 1, 0, 0, 0, 0, 0, 0,
  //              0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1,
  //              0, 0, 0, 0, 0, 0, 0, 0, 1, 1)
  //                 .finished();

  // std::string res = ((Dm_verify.transpose()).isApprox(Dm) &&
  //                    (Dp_verify.transpose()).isApprox(Dp))
  //                       ? "true"
  //                       : "false";
  // std::cout << res << std::endl;



  for (auto [i, dM] : Dm) {
    std::cout << "deduct " << i << ": " << dM.transpose() << std::endl;
  }

  for (auto [i, dM] : Dp) {
    std::cout << "add " << i << ": " << dM.transpose() << std::endl;
  }



  for (auto s : initial_marking)
    std::cout << s << ", ";
  std::cout << '\n';


  Eigen::Map<const Eigen::VectorXi> M0(initial_marking.data(), initial_marking.size()); 




  std::cout << M0.transpose() << std::endl;
  std::cout << Dm.at(0).transpose() << std::endl; 
  Eigen::VectorXi dM = Dm.at(0).transpose();
  std::cout << (M0-dM).transpose() << std::endl; 

  for_let (pattern(ds(arg, arg)) = Dm) = [M0](auto T, auto dM) {
    WHEN(((M0-dM).array() < 0).any()) { std::cout << T << " HAS ZERO\n";return Break; };
  };

  return 0;
}
