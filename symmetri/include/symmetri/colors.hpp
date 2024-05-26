#pragma once

/** @file colors.hpp */

#include <string>
#include <unordered_map>
#include <cstdint>
namespace symmetri {

using Token = uint32_t;

namespace impl {
/**
 * @brief this creates a hash for a token-color. It is probably not the best way
 * to get a unique integer for a string.
 *
 * @param input
 * @return Token constexpr
 */
Token constexpr HashColor(char const* input) {
  return *input ? static_cast<Token>(*input) + 33 * HashColor(input + 1) : 5381;
}

}  // namespace impl

/**
 * @brief A completely static class that hosts the basic Tokens (Scheduled,
 * Started, Success, Deadlocked, Canceled, Paused and Failed). It also hosts a
 * dictonairy matching the color's string description to the token.
 *
 */
class Color {
 public:
  Color() = delete;
  static constexpr Token Scheduled = impl::HashColor(
      "Scheduled");  ///< Scheduled means the callback has been
                     ///< deferred to the pool but not yet fired.
  static constexpr Token Started = impl::HashColor(
      "Started");  ///< Started means the callback has been fired.
  static constexpr Token Success = impl::HashColor(
      "Success");  ///< Success means the callback returned the Color::Success
                   ///< token, allowing the Petri net to product tokens.
  static constexpr Token Deadlocked =
      impl::HashColor("Deadlocked");  ///< Deadlocked means the callback was a
                                      ///< Petri net and it deadlocked.
  static constexpr Token Canceled =
      impl::HashColor("Canceled");  ///< Canceled means the Petri net was
                                    ///< terminated and gracefully exited early.
  static constexpr Token Paused =
      impl::HashColor("Paused");  ///< The Petri net does not queue any active
                                  ///< transition callbacks.
  static constexpr Token Failed =
      impl::HashColor("Failed");  ///< The Petri net will resume queuing active
                                  ///< transitions callbacks.

  /**
   * @brief Get the Colors object. The map contains all the colors of tokens
   * that are registered.
   *
   * @return const std::unordered_map<Token, std::string>&
   */
  static std::unordered_map<Token, std::string> getColors();

  /**
   * @brief Get the string representation of a Token. This matches the
   * string-description of the Token-color in the XML if applicable.
   *
   * @param r
   * @return const std::string
   */
  static std::string toString(Token r);

  /**
   * @brief By registering a color, a Token is calculated that represents that
   * color. It is stored in a map and it can be assigned to a value which can be
   * used by Reducers to return.
   *
   * @param color string describing a color
   * @return const Token
   */
  static Token registerToken(const std::string& color);

 private:
  inline static std::unordered_map<Token, std::string> map = {
      {Scheduled, "Scheduled"},   {Started, "Started"},   {Success, "Success"},
      {Deadlocked, "Deadlocked"}, {Canceled, "Canceled"}, {Paused, "Paused"},
      {Failed, "Failed"}};
};

}  // namespace symmetri
