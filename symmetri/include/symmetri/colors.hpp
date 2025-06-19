#pragma once

/** @file colors.hpp */

#include <string.h>

#include <algorithm>
#include <array> // std::array
#include <cassert>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility> // std::index_sequence
#include <vector>  // hash
#include <cstdio>

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace sym_impl
{
    // https://rodusek.com/posts/2021/03/09/getting-an-unmangled-type-name-at-compile-time/

    template <std::size_t... Idxs>
    constexpr auto substring_as_array(std::string_view str,
                                      std::index_sequence<Idxs...>)
    {
        return std::array{str[Idxs]..., '\0'};
    }

    template <typename T>
    constexpr auto type_name_array()
    {
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
    struct type_name_holder
    {
        static inline constexpr auto value = type_name_array<T>();
    };

    template <typename T>
    constexpr auto type_name() -> std::string_view
    {
        constexpr auto &value = type_name_holder<T>::value;
        return std::string_view{value.data(), value.size()};
    }

#endif /* DOXYGEN_SHOULD_SKIP_THIS */
}
namespace symmetri
{

    /**
     * @brief Tokens are elements that can reside in places. Tokens can have a color
     * which makes them distinguishable from other tokens. Tokens that have the same
     * color are not distinguishable. Users can create their own token-colors by
     * either using the CREATE_CUSTOM_TOKEN-macro (compile-time) or by calling
     * Token's public constructor which takes a token-name.
     *
     */
    class Token
    {
    public:
        /**
         * @brief Get a list of all the colors
         *
         * @return std::vector<const char *>
         */
        static std::vector<const char *> getColors()
        {
            std::vector<const char *> _colors;
            _colors.reserve(colors.size());
            std::copy_if(colors.begin(), colors.end(), std::back_inserter(_colors),
                         [](const auto &color)
                         { return color != nullptr; });
            return _colors;
        }

        constexpr const auto &toString() const { return colors[id]; }

        template <class T>
        constexpr bool operator==(const T &t) const
        {
            return id == t.id;
        }

        constexpr bool operator<(const Token &rhs) const
        {
            return id < rhs.toIndex();
        }

        constexpr bool operator>(const Token &rhs) const
        {
            return id > rhs.toIndex();
        }

        constexpr size_t toIndex() const { return id; }

        static constexpr bool equals(const char *a, const char *b)
        {
            return *a == *b && (*a == '\0' || equals(a + 1, b + 1));
        }

        template <class X, class V>
        static constexpr auto find(X &x, V key)
        {
            std::size_t i = 0;
            while (i < x.size())
            {
                if (x[i] != nullptr && equals(x[i], key))
                    return i;
                ++i;
            }

            return i;
        }

        template <class X>
        static constexpr auto findSlot(X &x)
        {
            std::size_t i = 0;
            while (i < x.size())
            {
                if (x[i] == nullptr)
                    return i;
                ++i;
            }

            return i;
        }

        constexpr Token(const char *_id)
            : id(find(colors, _id) < kMaxTokenColors ? find(colors, _id) : findSlot(colors))
        {
            assert(id < colors.size() &&
                   "There can only be kMaxTokenColors different token-colors.");
            colors[id] = _id;
        }

    protected:
        const static size_t kMaxTokenColors =
            100; ///< Maximum amount of different colors
        inline static std::array<const char *, kMaxTokenColors> colors = {nullptr};
        size_t id;
    };

} // namespace symmetri

// Custom specialization of std::hash can be injected in namespace std.
template <>
struct std::hash<symmetri::Token>
{
    constexpr std::size_t operator()(const symmetri::Token &s) const noexcept
    {
        return s.toIndex();
    }
};

/**
 * @brief A macro from which we can create token-colors. Colors ceated this way
 * end up in the symmetri namespace.
 *
 */
#define CREATE_CUSTOM_TOKEN(name)                              \
    namespace symmetri                                         \
    {                                                          \
        struct name : public Token                             \
        {                                                      \
            constexpr name()                                   \
                : Token(sym_impl::type_name<name>().data()) {} \
        };                                                     \
        static inline name name;                               \
    }

CREATE_CUSTOM_TOKEN(Scheduled)
CREATE_CUSTOM_TOKEN(Started)
CREATE_CUSTOM_TOKEN(Success)
CREATE_CUSTOM_TOKEN(Deadlocked)
CREATE_CUSTOM_TOKEN(Canceled)
CREATE_CUSTOM_TOKEN(Paused)
CREATE_CUSTOM_TOKEN(Failed)