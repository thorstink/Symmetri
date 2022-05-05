#include "pnml_parser.h"

#include <spdlog/fmt/ostr.h>  // must be included
#include <spdlog/spdlog.h>

#include <sstream>

#include "tinyxml2/tinyxml2.h"

using namespace tinyxml2;
using namespace symmetri;

std::tuple<StateNet, NetMarking> readPetriNets(
    const std::set<std::string> &files) {
  std::set<std::string> places, transitions;
  NetMarking place_initialMarking;
  StateNet state_net;

  for (auto file : files) {
    XMLDocument net;
    spdlog::debug("PNML file-path: {0}", file);
    net.LoadFile(file.c_str());

    tinyxml2::XMLElement *levelElement = net.FirstChildElement("pnml")
                                             ->FirstChildElement("net")
                                             ->FirstChildElement("page");

    // loop places.
    for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("place");
         child != NULL; child = child->NextSiblingElement("place")) {
      auto place_id = child->Attribute("id");
      auto initial_marking =
          (child->FirstChildElement("initialMarking") == nullptr)
              ? 0
              : std::stoi(child->FirstChildElement("initialMarking")
                              ->FirstChildElement("text")
                              ->GetText());

      place_initialMarking.insert({place_id, initial_marking});
      places.insert(std::string(place_id));
    }

    // loop transitions
    for (tinyxml2::XMLElement *child =
             levelElement->FirstChildElement("transition");
         child != NULL; child = child->NextSiblingElement("transition")) {
      auto transition_id = child->Attribute("id");
      transitions.insert(transition_id);
    }

    // loop arcs
    for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("arc");
         child != NULL; child = child->NextSiblingElement("arc")) {
      // do something with each child element

      auto source_id = child->Attribute("source");
      auto target_id = child->Attribute("target");
      auto multiplicity =
          (child->FirstChildElement("inscription") == nullptr)
              ? 1
              : std::stoi(child->FirstChildElement("inscription")
                              ->FirstChildElement("text")
                              ->GetText());

      for (int i = 0; i < multiplicity; i++) {
        if (places.contains(source_id)) {
          // if the source is a place, tokens are consumed.
          if (state_net.contains(target_id)) {
            state_net.find(target_id)->second.first.insert(source_id);
          } else {
            state_net.insert({target_id, {{source_id}, {}}});
          }
        } else if (transitions.contains(source_id)) {
          // if the destination is a place, tokens are produced.
          if (state_net.contains(source_id)) {
            state_net.find(source_id)->second.second.insert(target_id);
          } else {
            state_net.insert({source_id, {{}, {target_id}}});
          }
        } else {
          auto arc_id = child->Attribute("id");
          spdlog::error(
              "error: arc {0} is not connecting a place to a transition.",
              arc_id);
        }
      }
    }
  }

  std::stringstream netstring;
  netstring << "\n========\n";
  for (auto [transition, mut] : state_net) {
    auto [pre, post] = mut;
    netstring << "transition: " << transition;
    netstring << ", pre: ";
    for (auto p : pre) {
      netstring << p << ",";
    }
    netstring << " post: ";
    for (auto p : post) {
      netstring << p << ",";
    }
    netstring << std::endl;
  }

  for (auto [p, tokens] : place_initialMarking) {
    netstring << p << " : " << tokens << "\n";
  }
  netstring << "========\n";

  spdlog::debug(netstring.str());

  return {state_net, place_initialMarking};
}
