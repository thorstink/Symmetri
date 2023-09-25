#pragma once
#include <cstdint>
#include <unordered_map>

namespace symmetri {
using Token = uint32_t;

Token registerResult(Token r, std::string s);
std::string printState(symmetri::Token r);
class ResultLookup {
 public:
  ResultLookup() = delete;

 private:
  inline static std::unordered_map<Token, std::string> map = {};
  friend Token registerResult(Token r, std::string s);
  friend std::string printState(symmetri::Token r);
};

namespace state {
unsigned constexpr ConstStringHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) +
                      33 * ConstStringHash(input + 1)
                : 5381;
}

template <unsigned int i>
auto create(const std::string& name) {
  return registerResult(i, name);
};
}  // namespace state
}  // namespace symmetri
