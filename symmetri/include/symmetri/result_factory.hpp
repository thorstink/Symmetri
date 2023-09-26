#pragma once
#include <cstdint>
#include <unordered_map>

namespace symmetri {
using Token = uint32_t;

Token registerToken(const std::string& s);
std::string printState(Token r);
class TokenLookup {
 public:
  TokenLookup() = delete;
  static const std::vector<std::string>& getColors() { return colors; }

 private:
  inline static std::vector<std::string> colors = {};
  friend Token registerToken(const std::string& s);
  friend std::string printState(Token r);
};

}  // namespace symmetri
