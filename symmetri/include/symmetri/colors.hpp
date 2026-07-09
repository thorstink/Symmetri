#pragma once

/** @file colors.hpp */

#include <string.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <typeinfo>
#include <vector>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace sym_impl {
// https://rodusek.com/posts/2021/03/09/getting-an-unmangled-type-name-at-compile-time/

template <std::size_t... Idxs>
constexpr auto substring_as_array(std::string_view str,
                                  std::index_sequence<Idxs...>) {
  return std::array{str[Idxs]..., '\0'};
}

template <typename T>
constexpr auto type_name_array() {
#if defined(__clang__)
  constexpr auto prefix = std::string_view{"[T = symmetri::"};
  constexpr auto suffix = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
  constexpr auto prefix = std::string_view{"with T = symmetri::"};
  constexpr auto suffix = std::string_view{"]"};
  constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
  constexpr auto prefix = std::string_view{"type_name_array<symmetri::"};
  constexpr auto suffix = std::string_view{">(void)"};
  constexpr auto function = std::string_view{__FUNCSIG__};
#else
#error Unsupported compiler
#endif

  constexpr auto start = function.find(prefix) + prefix.size();
  constexpr auto end = function.rfind(suffix);

  static_assert(start < end);

  constexpr auto name = function.substr(start, (end - start));
  return substring_as_array(name, std::make_index_sequence<name.size()>{});
}

template <typename T>
struct type_name_holder {
  static inline constexpr auto value = type_name_array<T>();
};

template <typename T>
constexpr auto type_name() -> std::string_view {
  constexpr auto& value = type_name_holder<T>::value;
  return std::string_view{value.data(), value.size()};
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */
}
namespace symmetri {

/**
 * @brief Thrown when a payload is attached to (or read from) a token in a way
 * that contradicts the color's type binding — e.g. putting data on a dataless
 * color like Success, or putting a std::string on a color bound to a Path.
 */
struct TokenTypeError : public std::logic_error {
  using std::logic_error::logic_error;
};

/**
 * @brief Maps a payload type to its (unique) token-color. Specialized by
 * CREATE_TYPED_TOKEN; the bijection is enforced at compile time — binding the
 * same payload type to a second color is a redefinition error.
 */
template <typename T>
struct color_of;  // undefined primary: type not bound to any color

/**
 * @brief Tokens are elements that can reside in places. Tokens can have a color
 * which makes them distinguishable from other tokens. Tokens that have the same
 * color are not distinguishable. Users can create their own token-colors by
 * either using the CREATE_CUSTOM_TOKEN-macro (compile-time) or by calling
 * Token's public constructor which takes a token-name. A color created with
 * CREATE_TYPED_TOKEN is additionally bound 1:1 to a payload type: tokens of
 * that color carry that type, and only that type.
 *
 */
class Token {
 public:
  /**
   * @brief Get a list of all the colors
   *
   * @return std::vector<std::string_view>
   */
  static std::vector<std::string_view> getColors();

  std::string_view toString() const;

  template <class T>
  constexpr bool operator==(const T& t) const {
    return id == t.id;
  }

  constexpr bool operator<(const Token& rhs) const {
    return id < rhs.toIndex();
  }

  constexpr bool operator>(const Token& rhs) const {
    return id > rhs.toIndex();
  }

  constexpr uint8_t toIndex() const { return id; }

  Token(const char* _id);

  /**
   * @brief The payload type this color is bound to, or nullptr for a dataless
   * color. The binding is established by CREATE_TYPED_TOKEN at static
   * initialization; colors created any other way (including colors parsed from
   * net files that were never defined in C++) are dataless.
   */
  const std::type_info* boundType() const;

 protected:
  /**
   * @brief Bind this color to payload type T. Re-binding to a different type
   * is a programming error (the type defines the token).
   */
  template <typename T>
  void bind() const {
    bindTo(typeid(T));
  }
  void bindTo(const std::type_info& t) const;

  // The color registries (name and payload-type, indexed by color id) live as
  // function-local statics defined in colors.cpp — deliberately NOT inline
  // statics in this header. Inline statics are weak symbols, and when a static
  // library and the linking application both instantiate them, the linker (in
  // particular macOS ld) is not guaranteed to coalesce the copies: colors
  // registered by the application would be invisible to library code. One
  // definition in one translation unit makes the registry unambiguous.
  static std::array<std::array<char, 64>, 128>& nameRegistry();
  static std::array<const std::type_info*, 128>& typeRegistry();

  uint8_t id;
};

}  // namespace symmetri

// Custom specialization of std::hash can be injected in namespace std.
template <>
struct std::hash<symmetri::Token> {
  constexpr std::size_t operator()(const symmetri::Token& s) const noexcept {
    return s.toIndex();
  }
};

/**
 * @brief A macro from which we can create token-colors. Colors ceated this way
 * end up in the symmetri namespace.
 *
 */
#define CREATE_CUSTOM_TOKEN(name)                         \
  namespace symmetri {                                    \
  struct name : public Token {                            \
    name() : Token(sym_impl::type_name<name>().data()) {} \
  };                                                      \
  static inline name name;                                \
  }

/**
 * @brief Creates a token-color bound 1:1 to a payload type: every token of
 * this color carries exactly that type (the type defines the token). The
 * variadic parameter is the payload type (variadic so template types
 * containing commas work). The bijection is enforced both ways: re-binding
 * the color throws, and binding the same payload type to a second color is a
 * compile-time redefinition error (color_of<T> is specialized twice). Because
 * of the bijection, the color can be inferred from a payload value — see
 * BasicToken's inference constructor.
 *
 */
#define CREATE_TYPED_TOKEN(name, ...)                                         \
  namespace symmetri {                                                        \
  struct name : public Token {                                                \
    using payload_t = __VA_ARGS__;                                            \
    name() : Token(sym_impl::type_name<name>().data()) { bind<payload_t>(); } \
  };                                                                          \
  static inline name name;                                                    \
  template <>                                                                 \
  struct color_of<__VA_ARGS__> {                                              \
    static Token value() { return name; }                                     \
  };                                                                          \
  }

CREATE_CUSTOM_TOKEN(Scheduled)
CREATE_CUSTOM_TOKEN(Started)
CREATE_CUSTOM_TOKEN(Success)
CREATE_CUSTOM_TOKEN(Deadlocked)
CREATE_CUSTOM_TOKEN(Canceled)
CREATE_CUSTOM_TOKEN(Paused)
CREATE_CUSTOM_TOKEN(Failed)
CREATE_CUSTOM_TOKEN(Cancel)
