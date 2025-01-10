#include "load_file.h"

#include <map>
#include <numeric>
#include <vector>

#include "petri.h"
#include "position_parsers.h"
#include "rxdispatch.h"
#include "symmetri/parsers.h"
#include "symmetri/symmetri.h"

template <typename T>
inline void append(std::vector<T> source, std::vector<T>& destination) {
  if (destination.empty())
    destination = std::move(source);
  else
    destination.insert(std::end(destination),
                       std::make_move_iterator(std::begin(source)),
                       std::make_move_iterator(std::end(source)));
}

void loadPetriNet(const std::filesystem::path& file) {
  rxdispatch::push([=](model::Model&& model) {
    auto& m = *model.data;
    m.selected_arc_idxs.reset();
    m.selected_node_idx.reset();
    m.active_file = file;
    if (not m.active_file.has_value()) {
      return model;
    }
    symmetri::Net net;
    symmetri::Marking marking;
    symmetri::PriorityTable pt;
    std::map<std::string, model::Coordinate> positions;
    const std::filesystem::path pn_file = m.active_file.value();
    if (pn_file.extension() == std::string(".pnml")) {
      std::tie(net, marking) = symmetri::readPnml({pn_file});
      positions = farbart::readPnmlPositions({pn_file});
    } else {
      std::tie(net, marking, pt) = symmetri::readGrml({pn_file});
    }

    symmetri::Petri::PTNet new_net;
    std::tie(new_net.transition, new_net.place, new_net.store) =
        symmetri::convert(net);

    std::tie(new_net.input_n, new_net.output_n) =
        symmetri::populateIoLookups(net, new_net.place);

    new_net.priority = symmetri::createPriorityLookup(new_net.transition, pt);

    for (const auto& transition : new_net.transition) {
      m.t_positions.push_back(positions.at(transition));
    }
    for (const auto& place : new_net.place) {
      m.p_positions.push_back(positions.at(place));
    }

    const auto old_place_count = m.net.place.size();
    const auto old_transition_count = m.net.transition.size();
    const auto new_place_count = new_net.place.size();
    const auto new_transition_count = new_net.transition.size();

    // update lookups:
    for (auto& inputs : new_net.input_n) {
      for (auto& [idx, color] : inputs) {
        idx += old_place_count;
      }
    }
    for (auto& outputs : new_net.output_n) {
      for (auto& [idx, color] : outputs) {
        idx += old_place_count;
      }
    }

    m.colors = symmetri::Token::getColors();

    append(std::move(new_net.priority), m.net.priority);
    append(std::move(new_net.transition), m.net.transition);
    append(std::move(new_net.place), m.net.place);
    append(std::move(new_net.output_n), m.net.output_n);
    append(std::move(new_net.input_n), m.net.input_n);
    append(std::move(new_net.store), m.net.store);

    m.net.p_to_ts_n = createReversePlaceToTransitionLookup(
        m.net.place.size(), m.net.transition.size(), m.net.input_n);

    m.tokens.clear();
    for (const auto& [p, c] : marking) {
      m.tokens.push_back(
          {symmetri::toIndex(new_net.place, p) + old_place_count, c});
    }

    m.t_view.clear();
    m.p_view.clear();
    m.t_view.resize(new_transition_count);
    m.p_view.resize(new_place_count);

    std::iota(m.t_view.begin(), m.t_view.end(), old_transition_count);
    std::iota(m.p_view.begin(), m.p_view.end(), old_place_count);

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();

    for (auto&& idx : m.t_view) {
      if (min_x > m.t_positions[idx].x) {
        min_x = m.t_positions[idx].x;
      }
      if (min_y > m.t_positions[idx].y) {
        min_y = m.t_positions[idx].y;
      }
    }

    for (auto&& idx : m.p_view) {
      if (min_x > m.p_positions[idx].x) {
        min_x = m.p_positions[idx].x;
      }
      if (min_y > m.p_positions[idx].y) {
        min_y = m.p_positions[idx].y;
      }
    }

    // maybe this can be improved to have a better initialization :-)
    m.scrolling = {min_x, min_y + 50.f};

    m.active_file = file;

    return model;
  });
}
