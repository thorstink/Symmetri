#include "load_file.h"

#include <cstdio>
#include <filesystem>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gui/model.h"
#include "petri.h"
#include "position_parsers.h"
#include "reducers.h"
#include "rxdispatch.h"
#include "symmetri/callback.h"
#include "symmetri/colors.hpp"
#include "symmetri/parsers.h"
#include "symmetri/types.h"

namespace {

// A fully-parsed, copyable net (indices local to this net). It is captured by
// value into the install reducer, which makes that reducer a pure, replayable
// EditState -> EditState transition: undo/redo never touch the filesystem.
struct ParsedNet {
  model::Net net;
  std::vector<model::Coordinate> t_positions, p_positions;
  std::vector<symmetri::AugmentedToken> tokens;  // marking, local place ids
};

// Reads and parses the file. Runs on whatever thread calls it (a background
// Computer), so it must not touch UI/model state — it only returns owned data.
ParsedNet parseNet(const std::filesystem::path& file) {
  if (not std::filesystem::exists(file)) {
    throw std::runtime_error(
        std::format("File {} does not exist\n", file.c_str()));
  }
  symmetri::Net net;
  symmetri::Marking marking;
  symmetri::PriorityTable pt;
  std::map<std::string, model::Coordinate> positions;
  if (file.extension() == std::string(".pnml")) {
    std::tie(net, marking) = symmetri::readPnml({file});
    positions = farbart::readPnmlPositions({file});
  } else {
    std::tie(net, marking, pt) = symmetri::readGrml({file});
    positions = farbart::readGrmlPositions({file});
  }

  ParsedNet p;
  std::vector<symmetri::Callback> store;
  std::tie(p.net.transition, p.net.place, store) = symmetri::convert(net);
  std::tie(p.net.input_n, p.net.output_n) =
      symmetri::populateIoLookups(net, p.net.place);
  p.net.priority = symmetri::createPriorityLookup(p.net.transition, pt);
  // Reduce the move-only callbacks to the copyable thing the editor needs: the
  // token each transition fires to.
  p.net.output.reserve(store.size());
  for (const auto& cb : store) p.net.output.push_back(fire(cb));

  p.t_positions.reserve(p.net.transition.size());
  for (const auto& t : p.net.transition) {
    p.t_positions.push_back(positions.at(t));
  }
  p.p_positions.reserve(p.net.place.size());
  for (const auto& pl : p.net.place) {
    p.p_positions.push_back(positions.at(pl));
  }
  for (const auto& [pl, c] : marking) {
    p.tokens.push_back({symmetri::toIndex(p.net.place, pl), c});
  }
  return p;
}

// Replace the current net with the parsed one.
model::EditReducer clearInstaller(ParsedNet p, std::filesystem::path file) {
  return [p = std::move(p), file](model::EditState&& e) {
    e.net.place = p.net.place;
    e.net.transition = p.net.transition;
    e.net.input_n = p.net.input_n;
    e.net.output_n = p.net.output_n;
    e.net.priority = p.net.priority;
    e.net.output = p.net.output;
    e.net.p_to_ts_n = createReversePlaceToTransitionLookup(
        e.net.place.size(), e.net.transition.size(), e.net.input_n);
    e.t_positions = p.t_positions;
    e.p_positions = p.p_positions;
    e.tokens = p.tokens;
    e.active_file = file;
    return e;
  };
}

// Append the parsed net to the current one, offsetting its place ids.
model::EditReducer appendInstaller(ParsedNet p, std::filesystem::path file) {
  return [p = std::move(p), file](model::EditState&& e) {
    const size_t base = e.net.place.size();
    e.net.transition.insert(e.net.transition.end(), p.net.transition.begin(),
                            p.net.transition.end());
    e.net.place.insert(e.net.place.end(), p.net.place.begin(),
                       p.net.place.end());
    e.net.priority.insert(e.net.priority.end(), p.net.priority.begin(),
                          p.net.priority.end());
    e.net.output.insert(e.net.output.end(), p.net.output.begin(),
                        p.net.output.end());
    const auto offset = [base](auto arcs) {
      for (auto& [idx, c] : arcs) idx += base;
      return arcs;
    };
    for (const auto& arcs : p.net.input_n) {
      e.net.input_n.push_back(offset(arcs));
    }
    for (const auto& arcs : p.net.output_n) {
      e.net.output_n.push_back(offset(arcs));
    }
    e.net.p_to_ts_n = createReversePlaceToTransitionLookup(
        e.net.place.size(), e.net.transition.size(), e.net.input_n);
    e.t_positions.insert(e.t_positions.end(), p.t_positions.begin(),
                         p.t_positions.end());
    e.p_positions.insert(e.p_positions.end(), p.p_positions.begin(),
                         p.p_positions.end());
    for (auto tok : p.tokens) {
      std::get<size_t>(tok) += base;
      e.tokens.push_back(tok);
    }
    e.active_file = file;
    return e;
  };
}

// Common flow: refresh transient view state immediately, then parse the file on
// a background thread and dispatch the (undoable) install reducer + a view fit.
void load(const std::filesystem::path& file, bool clear) {
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    v.selected_arc_idxs.reset();
    v.selected_node_idx.reset();
    v.p_highlight.clear();
    v.t_highlight.clear();
    v.colors = symmetri::Token::getColors();
  });
  rxdispatch::push(model::Computer{[file, clear]() -> model::Action {
    try {
      ParsedNet p = parseNet(file);  // file I/O, off the UI thread
      // The parsed data is captured by value in the install reducer, so it is a
      // pure edit -> EditState transition (replayable without re-reading disk).
      rxdispatch::pushEdit(clear ? clearInstaller(std::move(p), file)
                                 : appendInstaller(std::move(p), file));
      // Dispatched after the install (same producer thread -> FIFO), so it sees
      // the loaded net.
      resetNetView();
    } catch (const std::exception& ex) {
      printf("%s", ex.what());
    }
    // Work was dispatched above; nothing more to apply.
    return model::ViewReducer{
        [](model::ViewState&, const model::EditState&) {}};
  }});
}

}  // namespace

void loadPetriNet(const std::filesystem::path& file) { load(file, false); }

void clearAndloadPetriNet(const std::filesystem::path& file) {
  load(file, true);
}
