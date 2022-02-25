#include "pnml_parser.h"

#include <spdlog/fmt/ostr.h>  // must be included
#include <spdlog/spdlog.h>

#include <iostream>

#include "tinyxml2/tinyxml2.h"

using namespace tinyxml2;
using namespace symmetri;

std::string toLower(std::string str) {
  transform(str.begin(), str.end(), str.begin(), ::tolower);
  return str;
}

bool contains(std::vector<std::string> v, const std::string &K) {
  auto it = find(v.begin(), v.end(), K);
  // If element was found
  return it != v.end();
}

std::tuple<ArcList, StateNet, NetMarking> constructTransitionMutationMatrices(
    const std::set<std::string> &files) {
  std::vector<std::string> places, transitions;
  NetMarking place_initialMarking;
  ArcList arcs;
  StateNet state_net;

  for (auto file : files) {
    XMLDocument net;
    net.LoadFile(file.c_str());
    spdlog::info("PNML file-path: {0}", file);

    tinyxml2::XMLElement *levelElement = net.FirstChildElement("pnml")
                                             ->FirstChildElement("net")
                                             ->FirstChildElement("page");

    for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("place");
         child != NULL; child = child->NextSiblingElement("place")) {
      // do something with each child element
      auto place_name = toLower(child->Attribute("id"));

      auto place_id = toLower(child->Attribute("id"));

      auto only_digits = [](std::string source) {
        std::string target = "";
        for (char c : source) {
          if (std::isdigit(c)) target += c;
        }
        return target;
      };

      auto initial_marking =
          (child->FirstChildElement("initialMarking") == nullptr)
              ? 0
              : std::stoi(only_digits(child->FirstChildElement("initialMarking")
                                          ->FirstChildElement("text")
                                          ->GetText()));

      place_initialMarking.insert({place_id, initial_marking});

      if (std::find(places.begin(), places.end(), place_id) != places.end()) {
        /* v contains x */
      } else {
        places.push_back(std::string(place_id));
      }
    }

    for (tinyxml2::XMLElement *child =
             levelElement->FirstChildElement("transition");
         child != NULL; child = child->NextSiblingElement("transition")) {
      auto transition_name = toLower(child->Attribute("id"));
      auto transition_id = toLower(child->Attribute("id"));

      if (std::find(transitions.begin(), transitions.end(), transition_id) !=
          transitions.end()) {
        /* v contains x */
      } else {
        transitions.push_back(transition_id);
      }
    }
  }

  for (auto file : files) {
    XMLDocument net;
    net.LoadFile(file.c_str());

    tinyxml2::XMLElement *levelElement = net.FirstChildElement("pnml")
                                             ->FirstChildElement("net")
                                             ->FirstChildElement("page");

    for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("arc");
         child != NULL; child = child->NextSiblingElement("arc")) {
      // do something with each child element

      auto arc_id = toLower(child->Attribute("id"));
      auto source_id = toLower(child->Attribute("source"));
      auto target_id = toLower(child->Attribute("target"));

      if (contains(places, source_id) && contains(transitions, target_id)) {
        // if the source is a place, tokens are consumed.

        if (state_net.contains(target_id)) {
          state_net.find(target_id)->second.first.insert(source_id);
        } else {
          state_net.insert({target_id, {{source_id}, {}}});
        }
        arcs.push_back({true, std::string(target_id), std::string(source_id)});

      } else if (contains(places, target_id) &&
                 contains(transitions, source_id)) {
        // if the destination is a place, tokens are produced.

        if (state_net.contains(source_id)) {
          state_net.find(source_id)->second.second.insert(target_id);
        } else {
          state_net.insert({source_id, {{}, {target_id}}});
        }
        arcs.push_back({false, std::string(source_id), std::string(target_id)});

      } else {
        spdlog::error(
            "error: arc {0} is not connecting a place to a transition.",
            arc_id);
      }
    }
  }

  // sorting and reversing makes nicer mermaids diagrams.
  std::sort(std::begin(arcs), std::end(arcs));
  std::reverse(std::begin(arcs), std::end(arcs));

  for (auto [transition, mut] : state_net) {
    auto [pre, post] = mut;
    std::cout << "transition: " << transition;
    std::cout << ", pre: ";
    for (auto p : pre) {
      std::cout << p << ",";
    }
    std::cout << "post: ";
    for (auto p : post) {
      std::cout << p << ",";
    }
    std::cout << std::endl;
  }

  for (auto [p, tokens] : place_initialMarking) {
    std::cout << p << " : " << tokens << "\n";
  }

  return {arcs, state_net, place_initialMarking};
}
