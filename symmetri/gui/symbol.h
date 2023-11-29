/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

#pragma once

#include <cstdint>
#include <functional>
#include <string>

using Key = std::uint64_t;
class Symbol {
 protected:
  unsigned char c_;
  std::uint64_t j_;

 public:
  Symbol() : c_(0), j_(0) {}

  Symbol(const Symbol& key) : c_(key.c_), j_(key.j_) {}

  Symbol(unsigned char c, std::uint64_t j) : c_(c), j_(j) {}

  Symbol(Key key);

  Key key() const;

  operator Key() const { return key(); }

  void print(const std::string& s = "") const;

  bool equals(const Symbol& expected, double tol = 0.0) const;

  unsigned char chr() const { return c_; }

  std::uint64_t index() const { return j_; }

  operator std::string() const;

  std::string string() const { return std::string(*this); }

  bool operator<(const Symbol& comp) const {
    return c_ < comp.c_ || (comp.c_ == c_ && j_ < comp.j_);
  }

  bool operator==(const Symbol& comp) const {
    return comp.c_ == c_ && comp.j_ == j_;
  }

  bool operator==(Key comp) const { return comp == (Key)(*this); }

  bool operator!=(const Symbol& comp) const {
    return comp.c_ != c_ || comp.j_ != j_;
  }

  bool operator!=(Key comp) const { return comp != (Key)(*this); }

  static std::function<bool(Key)> ChrTest(unsigned char c);

  friend std::ostream& operator<<(std::ostream&, const Symbol&);
};

inline Key symbol(unsigned char c, std::uint64_t j) {
  return (Key)Symbol(c, j);
}

inline unsigned char symbolChr(Key key) { return Symbol(key).chr(); }

inline std::uint64_t symbolIndex(Key key) { return Symbol(key).index(); }

namespace symbol_shorthand {
inline Key A(std::uint64_t j) { return Symbol('a', j); }
inline Key B(std::uint64_t j) { return Symbol('b', j); }
inline Key C(std::uint64_t j) { return Symbol('c', j); }
inline Key D(std::uint64_t j) { return Symbol('d', j); }
inline Key E(std::uint64_t j) { return Symbol('e', j); }
inline Key F(std::uint64_t j) { return Symbol('f', j); }
inline Key G(std::uint64_t j) { return Symbol('g', j); }
inline Key H(std::uint64_t j) { return Symbol('h', j); }
inline Key I(std::uint64_t j) { return Symbol('i', j); }
inline Key J(std::uint64_t j) { return Symbol('j', j); }
inline Key K(std::uint64_t j) { return Symbol('k', j); }
inline Key L(std::uint64_t j) { return Symbol('l', j); }
inline Key M(std::uint64_t j) { return Symbol('m', j); }
inline Key N(std::uint64_t j) { return Symbol('n', j); }
inline Key O(std::uint64_t j) { return Symbol('o', j); }
inline Key P(std::uint64_t j) { return Symbol('p', j); }
inline Key Q(std::uint64_t j) { return Symbol('q', j); }
inline Key R(std::uint64_t j) { return Symbol('r', j); }
inline Key S(std::uint64_t j) { return Symbol('s', j); }
inline Key T(std::uint64_t j) { return Symbol('t', j); }
inline Key U(std::uint64_t j) { return Symbol('u', j); }
inline Key V(std::uint64_t j) { return Symbol('v', j); }
inline Key W(std::uint64_t j) { return Symbol('w', j); }
inline Key X(std::uint64_t j) { return Symbol('x', j); }
inline Key Y(std::uint64_t j) { return Symbol('y', j); }
inline Key Z(std::uint64_t j) { return Symbol('z', j); }
}  // namespace symbol_shorthand

class SymbolGenerator {
  const unsigned char c_;

 public:
  constexpr SymbolGenerator(const unsigned char c) : c_(c) {}
  Symbol operator()(const std::uint64_t j) const { return Symbol(c_, j); }
  constexpr unsigned char chr() const { return c_; }
};
