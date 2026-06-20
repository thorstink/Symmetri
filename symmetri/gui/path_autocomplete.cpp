#define IMGUI_DEFINE_MATH_OPERATORS

#include "path_autocomplete.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "gui/model.h"
#include "imgui.h"
#include "load_file.h"  // loadPetriNet / clearAndloadPetriNet
#include "reducers.h"   // updateActiveFile / addPopup / removePopup

namespace farbart {
namespace {

namespace fs = std::filesystem;

bool isLoadableExt(const std::string& ext) {
  return ext == ".pnml" || ext == ".grml";
}

// Only directories and loadable nets are offered as completions.
bool isLoadable(const fs::directory_entry& entry, std::error_code& ec) {
  return entry.is_directory(ec) ||
         isLoadableExt(entry.path().extension().string());
}

struct Candidate {
  std::string name;
  bool is_dir;
};

// The set of filesystem entries that complete the typed path.
struct Completions {
  std::string prefix;            // typed text up to and including the last '/'
  std::string leaf;              // the partial name after that
  std::vector<Candidate> items;  // sorted, filtered to dirs/.pnml/.grml
};

Completions computeCompletions(const std::string& input) {
  Completions c;
  if (input.empty()) return c;

  // Split on the last '/', preserving the user's exact prefix (no path
  // normalisation, so relative/absolute typing is kept verbatim).
  const auto slash = input.find_last_of('/');
  c.prefix = slash == std::string::npos ? "" : input.substr(0, slash + 1);
  c.leaf = slash == std::string::npos ? input : input.substr(slash + 1);

  const fs::path dir = c.prefix.empty() ? fs::path(".") : fs::path(c.prefix);
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return c;

  for (const auto& entry : fs::directory_iterator(
           dir, fs::directory_options::skip_permission_denied, ec)) {
    const std::string name = entry.path().filename().string();
    if (name.size() <= c.leaf.size()) continue;  // nothing to add
    if (name.compare(0, c.leaf.size(), c.leaf) != 0) continue;  // startswith
    if (!c.leaf.starts_with('.') && name.starts_with('.'))
      continue;  // dotfiles
    std::error_code kind_ec;
    if (!isLoadable(entry, kind_ec)) continue;
    c.items.push_back({name, entry.is_directory(kind_ec)});
  }
  std::sort(
      c.items.begin(), c.items.end(),
      [](const Candidate& a, const Candidate& b) { return a.name < b.name; });
  return c;
}

// The text to append to complete the typed path to `item`.
std::string remainderOf(const Completions& c, const Candidate& item) {
  std::string r = item.name.substr(c.leaf.size());
  if (item.is_dir) r += "/";
  return r;
}

// Shared between drawPathInput and its InputText callback for one frame.
struct CbContext {
  int* selected = nullptr;         // highlighted candidate, moved by Up/Down
  int count = 0;                   // number of candidates
  std::string tab_remainder;       // remainder of the selected candidate (Tab)
  std::string* pending = nullptr;  // remainder to insert (set by Tab/click)
  std::string current;             // the model's path, to detect real edits
};

// One callback drives everything while the field stays focused:
//  - History (Up/Down): move the selection (and stop nav from changing widget).
//  - Completion (Tab): queue the selected candidate's remainder.
//  - Always: apply any queued completion into the live buffer, and dispatch to
//    the model only when the text actually changed (completions modify the
//    *active* buffer, which an external model write would not).
int inputCallback(ImGuiInputTextCallbackData* data) {
  auto* ctx = static_cast<CbContext*>(data->UserData);
  if (!ctx) return 0;
  switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackHistory:
      if (ctx->count > 0) {
        if (data->EventKey == ImGuiKey_UpArrow && *ctx->selected > 0) {
          --*ctx->selected;
        } else if (data->EventKey == ImGuiKey_DownArrow &&
                   *ctx->selected < ctx->count - 1) {
          ++*ctx->selected;
        }
      }
      break;
    case ImGuiInputTextFlags_CallbackCompletion:
      if (!ctx->tab_remainder.empty()) *ctx->pending = ctx->tab_remainder;
      break;
    case ImGuiInputTextFlags_CallbackAlways:
      if (ctx->pending && !ctx->pending->empty()) {
        data->InsertChars(data->BufTextLen, ctx->pending->c_str());
        ctx->pending->clear();
      }
      if (data->Buf && ctx->current != data->Buf) {
        updateActiveFile(data);  // clears selection + dispatches the new path
      }
      break;
  }
  return 0;
}

constexpr size_t kMaxShown = 12;

}  // namespace

void requestLoad(const model::ViewModel& vm,
                 const std::filesystem::path& file) {
  if (vm.net.place.empty() && vm.net.transition.empty()) {
    clearAndloadPetriNet(file);  // nothing to merge with
    return;
  }
  const std::string id = "Load net";
  addPopup(id, [file, id](const model::ViewModel&) {
    ImGui::TextUnformatted(
        "The current net is not empty.\n"
        "Append the loaded net, or clear the current net and load?");
    ImGui::Separator();
    if (ImGui::Button("Append")) {
      loadPetriNet(file);
      removePopup(id);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear and load")) {
      clearAndloadPetriNet(file);
      removePopup(id);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      removePopup(id);
    }
  });
}

void drawPathInput(const model::ViewModel& vm) {
  const std::string& active_file = vm.active_file;

  static char buffer[512] = "";
  std::snprintf(buffer, sizeof(buffer), "%s", active_file.c_str());

  // Hit the filesystem only when the path changed, not every frame.
  static std::string last_input;
  static Completions completions;
  static int selected = 0;      // highlighted candidate
  static std::string pending;   // remainder queued for insertion
  static bool refocus = false;  // re-focus the input after a click
  if (active_file != last_input) {
    last_input = active_file;
    completions = computeCompletions(active_file);
    selected = 0;
  }
  if (selected < 0 || selected >= static_cast<int>(completions.items.size())) {
    selected = 0;
  }

  CbContext ctx;
  ctx.selected = &selected;
  ctx.count = static_cast<int>(completions.items.size());
  ctx.pending = &pending;
  ctx.current = active_file;
  ctx.tab_remainder =
      completions.items.empty()
          ? std::string{}
          : remainderOf(completions, completions.items[selected]);

  // Validity colouring: green for an existing loadable file, red for a wrong
  // extension, default for a valid-but-not-yet-existing name.
  const fs::path fs_path(active_file);
  const bool loadable = isLoadableExt(fs_path.extension().string());
  const bool exists = fs::is_regular_file(fs_path);
  bool recoloured = true;
  if (!loadable) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
  } else if (exists) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
  } else {
    recoloured = false;
  }

  // After a click, steal focus back to the input so its callback applies the
  // queued completion (clicking the dropdown otherwise deactivates the field).
  if (refocus) {
    ImGui::SetKeyboardFocusHere();
    refocus = false;
  }
  ImGui::PushItemWidth(-1);
  const bool entered =
      ImGui::InputText("##filepath", buffer, sizeof(buffer),
                       ImGuiInputTextFlags_CallbackCompletion |
                           ImGuiInputTextFlags_CallbackHistory |
                           ImGuiInputTextFlags_CallbackAlways |
                           ImGuiInputTextFlags_EnterReturnsTrue,
                       &inputCallback, &ctx);
  ImGui::PopItemWidth();

  const bool editing = ImGui::IsItemActive();
  const ImVec2 input_min = ImGui::GetItemRectMin();
  const ImVec2 input_max = ImGui::GetItemRectMax();

  // Ghost the selected candidate's remainder right after the typed text.
  if (!ctx.tab_remainder.empty() && editing) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float x =
        input_min.x + style.FramePadding.x + ImGui::CalcTextSize(buffer).x;
    const float y = input_min.y + style.FramePadding.y;
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(x, y), ImGui::GetColorU32(ImGuiCol_TextDisabled),
        ctx.tab_remainder.c_str());
  }

  if (recoloured) {
    ImGui::PopStyleColor();
  }

  // Enter loads the typed path, same flow as opening from the File menu — but
  // only when it's "green" (an existing, loadable file). Otherwise ignore it so
  // we never try to load a non-existent / wrong-type path.
  if (entered) {
    const fs::path typed(buffer);
    if (isLoadableExt(typed.extension().string()) &&
        fs::is_regular_file(typed)) {
      requestLoad(vm, typed);
    }
  }

  // Dropdown of all matches (Up/Down to move, Tab/click to accept).
  if (editing && completions.items.size() > 1) {
    ImGui::SetNextWindowPos(ImVec2(input_min.x, input_max.y));
    ImGui::SetNextWindowSize(
        ImVec2(input_max.x - input_min.x, 0.0f));  // auto h
    ImGui::Begin(
        "##path_completions", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavInputs);
    const size_t shown = std::min(completions.items.size(), kMaxShown);
    for (size_t i = 0; i < shown; ++i) {
      const Candidate& item = completions.items[i];
      const std::string label = item.is_dir ? item.name + "/" : item.name;
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(label.c_str(),
                            i == static_cast<size_t>(selected))) {
        selected = static_cast<int>(i);
        pending = remainderOf(completions, item);
        refocus = true;
      }
      ImGui::PopID();
    }
    if (completions.items.size() > shown) {
      ImGui::TextDisabled("… %zu more", completions.items.size() - shown);
    }
    ImGui::End();
  }
}

}  // namespace farbart
