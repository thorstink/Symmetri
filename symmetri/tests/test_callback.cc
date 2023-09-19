#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "symmetri/callback.h"

using namespace symmetri;

class Foo {
 public:
  Foo(std::string name) : name(name), constructor(1), copy_constructor(0) {
    std::cout << "ctor " << name << std::endl;
  }
  Foo(const Foo& o)
      : name(o.name),
        constructor(o.constructor),
        copy_constructor(o.copy_constructor + 1) {
    std::cout << "copy " << name << std::endl;
  }
  Foo(Foo&& o) noexcept = default;

  const std::string name;
  const int constructor;
  const int copy_constructor;
};

Result fire(const Foo&) { return state::Completed; }

void resume(const Foo& f) {
  REQUIRE(f.constructor == 1);
  REQUIRE(f.copy_constructor == 0);
}

TEST_CASE("Constructing is just as you expect") {
  Foo o("one_constructor");
  REQUIRE(o.constructor == 1);
  REQUIRE(o.copy_constructor == 0);
}

TEST_CASE("Creating and and inserting is ok") {
  Callback f(Foo("hi"));
  std::vector<Callback> p;
  p.push_back(f);
  resume(p.back());
}

TEST_CASE("Constructing in place is the same") {
  std::vector<Callback> p;
  p.push_back(Foo("lol"));
  resume(p.back());
}
