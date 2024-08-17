#pragma once

/** @file colors.hpp */

#include <algorithm>
#include <array>  // std::array
#include <cassert>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>  // std::index_sequence
#include <vector>
namespace symmetri {

// https://rodusek.com/posts/2021/03/09/getting-an-unmangled-type-name-at-compile-time/

template <std::size_t... Idxs>
constexpr auto substring_as_array(std::string_view str,
                                  std::index_sequence<Idxs...>) {
  return std::array{str[Idxs]...};
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

class Token {
 protected:
  inline static std::array<std::string_view, 100> v{};
  size_t idx;
  template <class T>
  constexpr Token(T* const) : idx(unique_id<T>()) {
    static_assert(unique_id<T>() < v.size(), "Token is not big enough");
    v[idx] = type_name<T>();
  }

 public:
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
      assert(v[idx].empty() && "Token is not big enough");
      v[idx] = s;
    }
  }

  Token() = delete;

  static std::vector<std::string_view> getColors() {
    std::vector<std::string_view> colors;
    colors.reserve(v.size());
    std::copy_if(v.begin(), v.end(), std::back_inserter(colors),
                 [](const auto& color) { return not color.empty(); });
    return colors;
  }

  constexpr operator size_t() const { return idx; }
  constexpr auto getIdx() const { return idx; }
  constexpr const auto& getName() const { return v[idx]; }
  constexpr bool operator==(const Token& c) const { return idx == c.idx; }
  template <class T>
  constexpr bool operator==(const T&) const {
    return idx == unique_id<T>();
  }
};

}  // namespace symmetri

#define CREATE_CUSTOM_TOKEN(name)                     \
  namespace symmetri {                                \
  struct name : public Token {                        \
    constexpr name() : Token(this) {}                 \
    constexpr bool operator==(const Token& c) const { \
      return idx == c.getIdx();                       \
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
