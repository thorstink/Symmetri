#pragma once

/** @file colors.hpp */

#include <algorithm>
#include <array>  // std::array
#include <cassert>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>  // std::index_sequence
#include <vector>   // hash
namespace symmetri {

#ifndef DOXYGEN_SHOULD_SKIP_THIS

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

template <auto Id>
struct counter {
  using tag = counter;

  struct generator {
    template <typename...>
    friend constexpr auto is_defined(tag) {
      return true;
    }
  };

  template <typename...>
  friend constexpr auto is_defined(tag);

  template <typename Tag = tag, auto I = (int)is_defined(Tag{})>
  static constexpr auto exists(decltype(I)) {
    return true;
  }

  static constexpr auto exists(...) { return generator(), false; }
};

template <typename T, auto Id = int{}>
constexpr auto unique_id() {
  if constexpr (counter<Id>::exists(Id)) {
    return unique_id<T, Id + 1>();
  } else {
    return Id;
  }
}

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief Tokens are elements that can reside in places. Tokens can have a color
 * which makes them distinguishable from other tokens. Tokens that have the same
 * color are not distinguishable. Users can create their own token-colors by
 * either using the CREATE_CUSTOM_TOKEN-macro (compile-time) or by calling
 * Token's public constructor which takes a token-name.
 *
 */
class Token {
  const static size_t kMaxTokenColors =
      100;     ///< Maximum amount of different colors
  size_t idx;  ///< A numerical id ("color") for this particular token
  inline static std::array<const char*, kMaxTokenColors> v{
      NULL};  ///< The human read-able string representation of the "color" is
              ///< stored in this buffer using the numerical id as index.

 protected:
  /**
   * @brief Creates a Token with a unique numerical id and a string
   * representation based on the name of the argument-type at compile-time.
   *
   * @tparam T the type representing the token-color
   */
  template <class T>
  constexpr Token(T* const) : idx(unique_id<T>()) {
    static_assert(unique_id<T>() < v.size(),
                  "There can only be 100 different token-colors.");
    v[idx] = type_name<T>().data();
  }

 public:
  /**
   * @brief Construct a new Token object from a string at run-time. A unique id
   * is generated and if it fails it will exit the application through a failing
   * assert.
   *
   * @param s string-representation of the color
   */
  Token(const char* s)
      : idx([&]() -> size_t {
          static size_t i = 0;
          auto it = std::find(v.cbegin(), v.cend(), s);
          if (it == std::cend(v)) {
            i++;
            return v.size() - i;
          } else {
            return std::distance(v.cbegin(), it);
          }
        }()) {
    if (std::find(v.cbegin(), v.cend(), s) == std::cend(v)) {
      assert(v[idx].empty() && "There can only be 100 different token-colors.");
      v[idx] = strdup(s);
    }
  }

  Token() = delete;

  /**
   * @brief Get a list of all the colors
   *
   * @return std::vector<const char *>
   */
  static std::vector<const char*> getColors() {
    std::vector<const char*> colors;
    colors.reserve(v.size());
    std::copy_if(v.begin(), v.end(), std::back_inserter(colors),
                 [](const auto& color) { return color != NULL; });
    return colors;
  }
  constexpr bool operator<(const Token& rhs) const {
    return idx < rhs.toIndex();
  }
  constexpr bool operator>(const Token& rhs) const {
    return idx > rhs.toIndex();
  }
  /**
   * @brief returns the unique index for this color.
   *
   * @return constexpr size_t
   */
  constexpr size_t toIndex() const { return idx; }

  /**
   * @brief returns the string-representation for this color.
   *
   * @return constexpr const auto&
   */
  constexpr auto toString() const { return v[idx]; }
  constexpr bool operator==(const Token& c) const { return idx == c.idx; }
  template <class T>
  constexpr bool operator==(const T&) const {
    return idx == unique_id<T>();
  }
};

}  // namespace symmetri

// Custom specialization of std::hash can be injected in namespace std.
template <>
struct std::hash<symmetri::Token> {
  std::size_t operator()(const symmetri::Token& s) const noexcept {
    return s.toIndex();
  }
};

/**
 * @brief A macro from which we can create token-colors. Colors ceated this way
 * end up in the symmetri namespace.
 *
 */
#define CREATE_CUSTOM_TOKEN(name)                     \
  namespace symmetri {                                \
  struct name : public Token {                        \
    constexpr name() : Token(this) {}                 \
    constexpr bool operator==(const Token& c) const { \
      return toIndex() == c.toIndex();                \
    }                                                 \
  };                                                  \
  static inline name name;                            \
  }

CREATE_CUSTOM_TOKEN(Scheduled)
CREATE_CUSTOM_TOKEN(Started)
CREATE_CUSTOM_TOKEN(Success)
CREATE_CUSTOM_TOKEN(Deadlocked)
CREATE_CUSTOM_TOKEN(Canceled)
CREATE_CUSTOM_TOKEN(Paused)
CREATE_CUSTOM_TOKEN(Failed)
