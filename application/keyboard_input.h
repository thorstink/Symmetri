#pragma once
#include "model.hpp"
#include <fstream>
#include <iostream>
#include <rxcpp/rx.hpp>
using namespace rxcpp::rxo;

rxcpp::observable<model::Reducer> itemSource() {

  auto keyboard =
      rxcpp::observable<>::create<char>([](rxcpp::subscriber<char> dest) {
        for (;;)
          dest.on_next(std::cin.get());
      }) |
      subscribe_on(rxcpp::observe_on_new_thread()) | publish();

  auto add_tokens = keyboard | filter([](char i) {
                      // check if digit, check if not negative.
                      return isdigit(i) ? (i >= 0 ? true : false) : false;
                    }) |
                    map([](char i) { return int(i - '0'); }) | map([](int i) {
                      // update state so there are i new tokens in the source.
                      return model::Reducer([i](model::Model model) {
                        // tricky. Why do we know this is the source? :-)
                        model.data->M(0) += i;
                        return model;
                      });
                    });

  auto add_coin =
      keyboard | filter([](char i) { return i == 'c'; }) | map([](char i) {
        // update state so there are i new tokens in the source.
        return model::Reducer([](model::Model model) {
          // tricky. Why do we know this is the source? :-)
          model.data->M(2) += 1;
          return model;
        });
      });

  auto log = keyboard | filter([](char i) { return i == 'l'; }) | map([](char) {
               return model::Reducer([](model::Model model) {
                 std::ofstream log;
                 log.open("db.csv", std::ios::app);
                 for (const auto &[a, b, c, d] : model.data->log) {
                   log << a << ',' << b << ',' << c << ',' << d << '\n';
                 }
                 log.close();
                 return model;
               });
             });

  return log | merge(add_tokens, add_coin) | ref_count(keyboard);
}
