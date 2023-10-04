#pragma once
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
namespace symmetri {
using Token = unsigned int;
Token registerToken(std::string s);
std::string printState(symmetri::Token r);

namespace state {
unsigned constexpr ConstStringHash(char const* input) {
  return *input ? static_cast<unsigned int>(*input) +
                      33 * ConstStringHash(input + 1)
                : 5381;
}

constexpr Token Scheduled(ConstStringHash("Scheduled"));
constexpr Token Started(ConstStringHash("Started"));
constexpr Token Completed(ConstStringHash("Completed"));
constexpr Token Deadlock(ConstStringHash("Deadlock"));
constexpr Token UserExit(ConstStringHash("UserExit"));
constexpr Token Paused(ConstStringHash("Paused"));
constexpr Token Error(ConstStringHash("Error"));

}  // namespace state

class TokenLookup {
 public:
  TokenLookup() = delete;

  inline static std::unordered_map<Token, std::string> map = {
      {state::Scheduled, "Scheduled"}, {state::Started, "Started"},
      {state::Completed, "Completed"}, {state::Deadlock, "Deadlock"},
      {state::UserExit, "UserExit"},   {state::Paused, "Paused"},
      {state::Error, "Error"}};

  inline static const std::string& Completed =
      TokenLookup::map[state::Completed];

 private:
  friend Token registerToken(Token r, std::string s);
  friend std::string printState(symmetri::Token r);
};
}  // namespace symmetri
