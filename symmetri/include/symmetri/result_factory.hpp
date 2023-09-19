#pragma once
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
namespace symmetri {
using Result = std::type_index;
Result registerResult(Result r, std::string s);
std::string printState(symmetri::Result r);
class ResultLookup {
 public:
  ResultLookup() = delete;

 private:
  inline static std::unordered_map<Result, std::string> map = {};
  friend Result registerResult(Result r, std::string s);
  friend std::string printState(symmetri::Result r);
};

namespace state {
unsigned constexpr ConstStringHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) +
                      33 * ConstStringHash(input + 1)
                : 5381;
}

template <unsigned int>
struct unique {};

template <unsigned int i>
auto create(const std::string& name) {
  return registerResult(std::type_index(typeid(unique<i>)), name);
};
}  // namespace state
}  // namespace symmetri
