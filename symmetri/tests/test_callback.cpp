#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "symmetri/callback.h"

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

bool custom_used = false;
symmetri::Token fire(const Foo&) {
  custom_used = true;
  return symmetri::Color::Success;
}

void resume(const Foo& f) {
  CHECK(f.constructor == 1);
  CHECK(f.copy_constructor == 0);
}

using namespace symmetri;

TEST_CASE("Constructing is just as you expect") {
  Foo o("one_constructor");
  CHECK(o.constructor == 1);
  CHECK(o.copy_constructor == 0);
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

TEST_CASE("Custom fire function is used") {
  Callback f(Foo("hi"));
  fire(f);
  CHECK(custom_used);
}
