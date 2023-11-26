#pragma once

#include <symmetri/parsers.h>
#include <symmetri/symmetri.h>
#include <symmetri/types.h>

#include <filesystem>
#include <functional>
#include <optional>

#include "blockingconcurrentqueue.h"
#include "drawable.h"
#include "view.hpp"

namespace fs = std::filesystem;

using namespace symmetri;

struct Model {
  std::filesystem::path working_dir;
  std::optional<std::filesystem::path> active_file;
  // Net net;
  Marking marking;
  std::vector<Drawable> drawables;
};

void draw(const Model &m) {
  for (const auto &drawable : m.drawables) {
    draw(drawable);
  }
}

inline Model initializeModel(Model &&m) {
  m.working_dir = "/users/thomashorstink/Projects/symmetri/nets";
  m.active_file = "/users/thomashorstink/Projects/symmetri/nets/n1.pnml";
  Net net;
  std::tie(net, m.marking) = symmetri::readPnml({m.active_file.value()});
  m.drawables.push_back(net);

  // create a file browser instance
  ImGui::FileBrowser fileDialog;

  // (optional) set browser properties
  fileDialog.SetTitle("title");
  fileDialog.SetTypeFilters({".pnml", ".grml"});
  fileDialog.SetPwd(m.working_dir);
  m.drawables.push_back(fileDialog);
  return m;
}

using Reducer = std::function<Model(Model &&)>;
class MVC {
 private:
  inline static moodycamel::BlockingConcurrentQueue<Reducer> reducers{10};

 public:
  inline static Model model = initializeModel(Model());
  inline static Model getView() { return model; };
  inline static void update(Reducer f) { model = f(std::move(model)); }
  inline static void push(Reducer &&f) {
    reducers.enqueue(std::forward<Reducer>(f));
  }
  inline static std::optional<Reducer> dequeue() {
    static Reducer r;
    return reducers.try_dequeue(r) ? std::optional(r) : std::nullopt;
  }
};
