#include "load_file.h"

#include <map>
#include <numeric>
#include <vector>

#include "petri.h"
#include "position_parsers.h"
#include "rxdispatch.h"
#include "symmetri/parsers.h"
#include "symmetri/symmetri.h"

void addNet(symmetri::Petri::PTNet& old_net,
            const symmetri::Petri::PTNet& new_net) {}

void loadPetriNet(const std::filesystem::path& file) {
  rxdispatch::push([=](model::Model&& model) {
    auto& m = *model.data;
    m.selected_arc = nullptr;
    m.selected_node = nullptr;
    m.active_file = file;
    if (not m.active_file.has_value()) {
      return model;
    }
    symmetri::Net net;
    symmetri::Marking marking;
    symmetri::PriorityTable pt;
    std::map<std::string, ImVec2> positions;
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

    m.colors.clear();
    for (const auto& [t, c] : symmetri::Color::getColors()) {
      m.colors.push_back(c);
    }

    // Move elements from src to dest.
    m.net.transition.insert(m.net.transition.end(),
                            std::make_move_iterator(new_net.transition.begin()),
                            std::make_move_iterator(new_net.transition.end()));
    m.net.place.insert(m.net.place.end(),
                       std::make_move_iterator(new_net.place.begin()),
                       std::make_move_iterator(new_net.place.end()));
    m.net.input_n.insert(m.net.input_n.end(),
                         std::make_move_iterator(new_net.input_n.begin()),
                         std::make_move_iterator(new_net.input_n.end()));
    m.net.output_n.insert(m.net.output_n.end(),
                          std::make_move_iterator(new_net.output_n.begin()),
                          std::make_move_iterator(new_net.output_n.end()));
    m.net.priority.insert(m.net.priority.end(),
                          std::make_move_iterator(new_net.priority.begin()),
                          std::make_move_iterator(new_net.priority.end()));

    m.t_view.clear();
    m.p_view.clear();
    m.t_view.resize(new_transition_count);
    m.p_view.resize(new_place_count);

    std::iota(m.t_view.begin(), m.t_view.end(), old_transition_count);
    std::iota(m.p_view.begin(), m.p_view.end(), old_place_count);

    return model;
  });
}
