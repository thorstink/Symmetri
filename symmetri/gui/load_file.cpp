#include "load_file.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "gui/model.h"
#include "petri.h"
#include "position_parsers.h"
#include "reducers.h"
#include "rxdispatch.h"
#include "small_vector.hpp"
#include "symmetri/colors.hpp"
#include "symmetri/parsers.h"
#include "symmetri/types.h"

void loadPetriNet(const std::filesystem::path& file) {
  // Loading clears the transient selection/highlight and refreshes the colour
  // table (view), then installs the parsed net (edit).
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    v.selected_arc_idxs.reset();
    v.selected_node_idx.reset();
    v.p_highlight.clear();
    v.t_highlight.clear();
    v.colors = symmetri::Token::getColors();
  });
  // NOTE: this edit reducer reads the filesystem, so it is not strictly
  // replay-pure (a redo re-reads the file). Acceptable for now; revisit when
  // wiring undo/redo by parsing at push-time and capturing the result.
  rxdispatch::pushEdit([=](model::EditState&& e) {
    e.active_file = file;
    if (not e.active_file.has_value()) {
      return e;
    }
    symmetri::Net net;
    symmetri::Marking marking;
    symmetri::PriorityTable pt;
    std::map<std::string, model::Coordinate> positions;
    const std::filesystem::path pn_file = e.active_file.value();
    if (not std::filesystem::exists(pn_file)) {
      throw std::runtime_error(
          std::format("File {} does not exist\n", pn_file.c_str()));
    }
    if (pn_file.extension() == std::string(".pnml")) {
      std::tie(net, marking) = symmetri::readPnml({pn_file});
      positions = farbart::readPnmlPositions({pn_file});
    } else {
      std::tie(net, marking, pt) = symmetri::readGrml({pn_file});
      positions = farbart::readGrmlPositions({pn_file});
    }

    symmetri::Petri::PTNet new_net;
    std::tie(new_net.transition, new_net.place, new_net.store) =
        symmetri::convert(net);

    std::tie(new_net.input_n, new_net.output_n) =
        symmetri::populateIoLookups(net, new_net.place);

    new_net.priority = symmetri::createPriorityLookup(new_net.transition, pt);

    e.t_positions.reserve(e.t_positions.size() + new_net.transition.size());
    for (const auto& transition : new_net.transition) {
      e.t_positions.push_back(positions.at(transition));
    }

    e.p_positions.reserve(e.p_positions.size() + new_net.place.size());
    for (const auto& place : new_net.place) {
      e.p_positions.push_back(positions.at(place));
    }

    const auto old_place_count = e.net.place.size();

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

    e.tokens.clear();
    for (const auto& [p, c] : marking) {
      e.tokens.push_back(
          {symmetri::toIndex(new_net.place, p) + old_place_count, c});
    }

    std::ranges::move(new_net.priority, std::back_inserter(e.net.priority));
    std::ranges::move(new_net.transition, std::back_inserter(e.net.transition));
    std::ranges::move(new_net.place, std::back_inserter(e.net.place));
    std::ranges::move(new_net.output_n, std::back_inserter(e.net.output_n));
    std::ranges::move(new_net.input_n, std::back_inserter(e.net.input_n));
    std::ranges::move(new_net.store, std::back_inserter(e.net.store));

    e.net.p_to_ts_n = createReversePlaceToTransitionLookup(
        e.net.place.size(), e.net.transition.size(), e.net.input_n);

    e.active_file = file;

    return e;
  });

  resetNetView();  // convenient to view new graph
}
