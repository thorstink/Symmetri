#include "pnml_parser.h"

#include <iostream>

#include "json.hpp"
#include "tinyxml2/tinyxml2.h"

using namespace tinyxml2;
using namespace symmetri;

// Function to print the
// index of an element
int getIndex(std::vector<std::string> v, const std::string &K) {
  auto it = find(v.begin(), v.end(), K);
  // If element was found
  if (it != v.end()) {
    // calculating the index
    // of K
    int index = it - v.begin();
    return index;
  } else {
    // If the element is not
    // present in the vector
    return -1;
  }
}

std::string toLower(std::string str) {
  transform(str.begin(), str.end(), str.begin(), ::tolower);
  return str;
}

bool contains(std::vector<std::string> v, const std::string &K) {
  auto it = find(v.begin(), v.end(), K);
  // If element was found
  return it != v.end();
}

std::tuple<TransitionMutation, TransitionMutation, Marking, nlohmann::json,
           Conversions, Conversions>
constructTransitionMutationMatrices(std::string file) {
  XMLDocument net;
  net.LoadFile(file.c_str());
  nlohmann::json j;

  std::vector<std::string> places, transitions;
  std::unordered_map<std::string, int> place_initialMarking;

  tinyxml2::XMLElement *levelElement = net.FirstChildElement("pnml")
                                           ->FirstChildElement("net")
                                           ->FirstChildElement("page");

  for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("place");
       child != NULL; child = child->NextSiblingElement("place")) {
    // do something with each child element
    auto place_name = toLower(child->Attribute("id"));
    // auto place_name =
    //     child->FirstChildElement("name")->FirstChildElement("value")->GetText();

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
    int x = std::stoi(child->FirstChildElement("graphics")
                          ->FirstChildElement("position")
                          ->Attribute("x"));
    int y = std::stoi(child->FirstChildElement("graphics")
                          ->FirstChildElement("position")
                          ->Attribute("y"));

    std::cout << "place: " << place_name << ", " << place_id << ", "
              << initial_marking << ", x:" << x << ", y: " << y << std::endl;
    j["states"].push_back(
        nlohmann::json({{"label", std::string(place_id)}, {"y", y}, {"x", x}}));

    places.push_back(std::string(place_id));
  }

  for (tinyxml2::XMLElement *child =
           levelElement->FirstChildElement("transition");
       child != NULL; child = child->NextSiblingElement("transition")) {
    // do something with each child element
    // auto transition_name =
    //     child->FirstChildElement("name")->FirstChildElement("value")->GetText();
    auto transition_name = toLower(child->Attribute("id"));
    auto transition_id = toLower(child->Attribute("id"));

    int x = std::stoi(child->FirstChildElement("graphics")
                          ->FirstChildElement("position")
                          ->Attribute("x"));
    int y = std::stoi(child->FirstChildElement("graphics")
                          ->FirstChildElement("position")
                          ->Attribute("y"));

    std::cout << "transition: " << transition_name << ", " << transition_id
              << ", x:" << x << ", y: " << y << std::endl;
    j["transitions"].push_back(nlohmann::json(
        {{"label", std::string(transition_id)}, {"y", y}, {"x", x}}));

    transitions.push_back(transition_id);
  }

  Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> Dp(places.size(),
                                                        transitions.size()),
      Dm(places.size(), transitions.size());

  Dp.setZero();
  Dm.setZero();

  for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("arc");
       child != NULL; child = child->NextSiblingElement("arc")) {
    // do something with each child element

    auto arc_id = toLower(child->Attribute("id"));
    auto source_id = toLower(child->Attribute("source"));
    auto target_id = toLower(child->Attribute("target"));

    // if (contains(places, source_id) && contains(transitions, target_id)) {
    if (contains(places, source_id) && contains(transitions, target_id)) {
      auto s_idx = getIndex(places, source_id);
      auto t_idx = getIndex(transitions, target_id);
      // if the source is a place, tokens are consumed.
      std::cout << "place-source: " << arc_id << ", " << source_id << ", "
                << target_id << std::endl;
      Dm(s_idx, t_idx) += 1;
      // pre
      for (auto &[key, val] : j["transitions"].items()) {
        if (val["label"] == std::string(target_id)) {
          val["pre"].emplace(std::string(source_id), 1);
        }
      }
    } else if (contains(places, target_id) &&
               contains(transitions, source_id)) {
      auto s_idx = getIndex(transitions, source_id);
      auto t_idx = getIndex(places, target_id);
      std::cout << "trans-source: " << arc_id << ", " << source_id << ", "
                << target_id << std::endl;
      // if the destination is a place, tokens are produced.
      Dp(t_idx, s_idx) += 1;
      for (auto &[key, val] : j["transitions"].items()) {
        if (val["label"] == source_id) {
          nlohmann::json object = val;
          val["post"].emplace(std::string(target_id), 1);
        }
      }
      // post
    } else {
      std::cout << "error: arc " << arc_id
                << " is not connecting a place to a transition." << std::endl;
    }
  }

  std::cout << Dp - Dm << std::endl;

  int transition_count = transitions.size();
  std::unordered_map<std::string, symmetri::Marking> pre_map, post_map;
  Eigen::VectorXi T(transition_count);
  for (const auto &transition_id : transitions) {
    T.setZero();
    T(getIndex(transitions, transition_id)) = 1;
    pre_map.insert({transition_id, (Dm * T).eval().sparseView()});
    post_map.insert({transition_id, (Dp * T).eval().sparseView()});
  }

  std::vector<int> initial_marking;
  std::transform(places.begin(), places.end(),
                 std::back_inserter(initial_marking),
                 [&place_initialMarking](const std::string &s) {
                   return place_initialMarking.at(s);
                 });

  symmetri::Marking M0 = Eigen::Map<const Eigen::VectorXi>(
                             initial_marking.data(), initial_marking.size())
                             .sparseView();

  for (auto [i, dM] : pre_map) {
    std::cout << "deduct " << i << ": " << dM.transpose() << std::endl;
  }

  for (auto [i, dM] : post_map) {
    std::cout << "add " << i << ": " << dM.transpose() << std::endl;
  }

  std::cout << "initial marking: " << M0.transpose() << std::endl;

  std::map<Eigen::Index, std::string> index_place_id_map;
  for (size_t i = 0; i < places.size(); i++) {
    index_place_id_map.insert({i, places.at(i)});
  }

  std::map<Eigen::Index, std::string> index_transition_id_map;
  for (size_t i = 0; i < transitions.size(); i++) {
    index_transition_id_map.insert({i, transitions.at(i)});
  }

  Conversions transition_mapper(index_transition_id_map);
  Conversions marking_mapper(index_place_id_map);

  for (auto [id, name] : index_transition_id_map)
    std::cout << "id: " << id << ", name: " << name << std::endl;

  for (auto [id, name] : index_place_id_map)
    std::cout << "id: " << id << ", name: " << name << std::endl;

  return {pre_map, post_map, M0, j, transition_mapper, marking_mapper};
}

nlohmann::json webMarking(
    const Marking &M,
    const std::map<Eigen::Index, std::string> &index_marking_map)

{
  nlohmann::json j;

  for (uint8_t i = 0; i < M.size(); i++) {
    j.emplace(index_marking_map.at(i), M.coeff(i));
  }
  return j;
}

std::tuple<TransitionMutation, TransitionMutation, Marking, nlohmann::json,
           Conversions, Conversions>
constructTransitionMutationMatrices(const std::set<std::string> &files) {
  std::vector<std::string> places, transitions;
  std::unordered_map<std::string, int> place_initialMarking;
  int offset_x = 0;
  int offset_y = 0;
  nlohmann::json j;
  for (auto file : files) {
    XMLDocument net;
    net.LoadFile(file.c_str());
    std::cout << "file: " << file << std::endl;

    tinyxml2::XMLElement *levelElement = net.FirstChildElement("pnml")
                                             ->FirstChildElement("net")
                                             ->FirstChildElement("page");

    for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("place");
         child != NULL; child = child->NextSiblingElement("place")) {
      auto place_id = toLower(child->Attribute("id"));

      if (std::find(places.begin(), places.end(), place_id) != places.end()) {
        offset_x += std::stoi(child->FirstChildElement("graphics")
                                  ->FirstChildElement("position")
                                  ->Attribute("x"));
        offset_y += std::stoi(child->FirstChildElement("graphics")
                                  ->FirstChildElement("position")
                                  ->Attribute("y"));
        break;
      }
    }

    for (tinyxml2::XMLElement *child = levelElement->FirstChildElement("place");
         child != NULL; child = child->NextSiblingElement("place")) {
      // do something with each child element
      auto place_name = toLower(child->Attribute("id"));
      // auto place_name =
      //     child->FirstChildElement("name")->FirstChildElement("value")->GetText();

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
      int x = std::stoi(child->FirstChildElement("graphics")
                            ->FirstChildElement("position")
                            ->Attribute("x")) +
              offset_x;
      int y = std::stoi(child->FirstChildElement("graphics")
                            ->FirstChildElement("position")
                            ->Attribute("y")) +
              offset_y;

      if (std::find(places.begin(), places.end(), place_id) != places.end()) {
        /* v contains x */
      } else {
        std::cout << "place: " << place_name << ", " << place_id << ", "
                  << initial_marking << ", x:" << x << ", y: " << y
                  << std::endl;
        j["states"].push_back(nlohmann::json(
            {{"label", std::string(place_id)}, {"y", y}, {"x", x}}));

        places.push_back(std::string(place_id));
      }
    }



    for (tinyxml2::XMLElement *child =
             levelElement->FirstChildElement("transition");
         child != NULL; child = child->NextSiblingElement("transition")) {
      // do something with each child element
      // auto transition_name =
      //     child->FirstChildElement("name")->FirstChildElement("value")->GetText();
      auto transition_name = toLower(child->Attribute("id"));
      auto transition_id = toLower(child->Attribute("id"));

      int x = std::stoi(child->FirstChildElement("graphics")
                            ->FirstChildElement("position")
                            ->Attribute("x"))+offset_x;
      int y = std::stoi(child->FirstChildElement("graphics")
                            ->FirstChildElement("position")
                            ->Attribute("y"))+offset_y;

      if (std::find(transitions.begin(), transitions.end(), transition_id) !=
          transitions.end()) {
        /* v contains x */
      } else {
        std::cout << "transition: " << transition_name << ", " << transition_id
                  << ", x:" << x << ", y: " << y << std::endl;
        j["transitions"].push_back(nlohmann::json(
            {{"label", std::string(transition_id)}, {"y", y}, {"x", x}}));

        transitions.push_back(transition_id);
      }
    }
  }
  Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> Dp(places.size(),
                                                        transitions.size()),
      Dm(places.size(), transitions.size());

  Dp.setZero();
  Dm.setZero();

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

      // if (contains(places, source_id) && contains(transitions, target_id)) {
      if (contains(places, source_id) && contains(transitions, target_id)) {
        auto s_idx = getIndex(places, source_id);
        auto t_idx = getIndex(transitions, target_id);
        // if the source is a place, tokens are consumed.
        std::cout << "place-source: " << arc_id << ", " << source_id << ", "
                  << target_id << std::endl;
        Dm(s_idx, t_idx) += 1;
        // pre
        for (auto &[key, val] : j["transitions"].items()) {
          if (val["label"] == std::string(target_id)) {
            val["pre"].emplace(std::string(source_id), 1);
          }
        }
      } else if (contains(places, target_id) &&
                 contains(transitions, source_id)) {
        auto s_idx = getIndex(transitions, source_id);
        auto t_idx = getIndex(places, target_id);
        std::cout << "trans-source: " << arc_id << ", " << source_id << ", "
                  << target_id << std::endl;
        // if the destination is a place, tokens are produced.
        Dp(t_idx, s_idx) += 1;
        for (auto &[key, val] : j["transitions"].items()) {
          if (val["label"] == source_id) {
            nlohmann::json object = val;
            val["post"].emplace(std::string(target_id), 1);
          }
        }
        // post
      } else {
        std::cout << "error: arc " << arc_id
                  << " is not connecting a place to a transition." << std::endl;
      }
    }
  }
  std::cout << Dp - Dm << std::endl;

  int transition_count = transitions.size();
  std::unordered_map<std::string, symmetri::Marking> pre_map, post_map;
  Eigen::VectorXi T(transition_count);
  for (const auto &transition_id : transitions) {
    T.setZero();
    T(getIndex(transitions, transition_id)) = 1;
    pre_map.insert({transition_id, (Dm * T).eval().sparseView()});
    post_map.insert({transition_id, (Dp * T).eval().sparseView()});
  }

  std::vector<int> initial_marking;
  std::transform(places.begin(), places.end(),
                 std::back_inserter(initial_marking),
                 [&place_initialMarking](const std::string &s) {
                   return place_initialMarking.at(s);
                 });

  symmetri::Marking M0 = Eigen::Map<const Eigen::VectorXi>(
                             initial_marking.data(), initial_marking.size())
                             .sparseView();

  for (auto [i, dM] : pre_map) {
    std::cout << "deduct " << i << ": " << dM.transpose() << std::endl;
  }

  for (auto [i, dM] : post_map) {
    std::cout << "add " << i << ": " << dM.transpose() << std::endl;
  }

  std::cout << "initial marking: " << M0.transpose() << std::endl;

  std::map<Eigen::Index, std::string> index_place_id_map;
  for (size_t i = 0; i < places.size(); i++) {
    index_place_id_map.insert({i, places.at(i)});
  }

  std::map<Eigen::Index, std::string> index_transition_id_map;
  for (size_t i = 0; i < transitions.size(); i++) {
    index_transition_id_map.insert({i, transitions.at(i)});
  }

  Conversions transition_mapper(index_transition_id_map);
  Conversions marking_mapper(index_place_id_map);

  for (auto [id, name] : index_transition_id_map)
    std::cout << "id: " << id << ", name: " << name << std::endl;

  for (auto [id, name] : index_place_id_map)
    std::cout << "id: " << id << ", name: " << name << std::endl;

  std::cout << j << std::endl;

  return {pre_map, post_map, M0, j, transition_mapper, marking_mapper};
}
