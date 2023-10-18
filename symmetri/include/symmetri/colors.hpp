#pragma once

#include <unordered_map>
namespace symmetri {

using Token = unsigned int;

namespace impl {
Token constexpr HashColor(char const* input) {
  return *input ? static_cast<unsigned int>(*input) + 33 * HashColor(input + 1)
                : 5381;
}

}  // namespace impl

class Color {
 public:
  Color() = delete;
  static constexpr Token Scheduled = impl::HashColor("Scheduled");
  static constexpr Token Started = impl::HashColor("Started");
  static constexpr Token Success = impl::HashColor("Success");
  static constexpr Token Deadlock = impl::HashColor("Deadlock");
  static constexpr Token UserExit = impl::HashColor("UserExit");
  static constexpr Token Paused = impl::HashColor("Paused");
  static constexpr Token Error = impl::HashColor("Error");

  static const std::unordered_map<Token, std::string>& getColors();
  static const std::string& toString(symmetri::Token r);
  static const Token& registerToken(const std::string& s);

 private:
  inline static std::unordered_map<Token, std::string> map = {
      {Scheduled, "Scheduled"}, {Started, "Started"},   {Success, "Success"},
      {Deadlock, "Deadlock"},   {UserExit, "UserExit"}, {Paused, "Paused"},
      {Error, "Error"}};
};

}  // namespace symmetri
