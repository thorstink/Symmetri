#pragma once
#include <cstdint>
#include <unordered_map>

namespace symmetri {
using Token = uint32_t;

Token registerResult(std::string s);
std::string printState(Token r);
class TokenLookup {
 public:
  TokenLookup() = delete;
  static const std::vector<std::string>& getColors() { return colors; }

 private:
  inline static std::vector<std::string> colors = {};
  friend Token registerResult(std::string s);
  friend std::string printState(Token r);
};

namespace state {
unsigned constexpr ConstStringHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) +
                      33 * ConstStringHash(input + 1)
                : 5381;
}

template <unsigned int i>
auto create(const std::string& name) {
  return registerResult(name);
};
}  // namespace state
}  // namespace symmetri
