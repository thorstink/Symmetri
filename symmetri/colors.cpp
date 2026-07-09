#include <symmetri/colors.hpp>

namespace symmetri {

// The single program-wide color registries. Function-local statics in this
// (one) translation unit: every user of the symmetri library — including code
// in the library itself — sees the same arrays, which an inline static member
// does not guarantee across a static-library boundary (see colors.hpp).
std::array<std::array<char, 64>, 128>& Token::nameRegistry() {
  static std::array<std::array<char, 64>, 128> colors = {};
  return colors;
}

std::array<const std::type_info*, 128>& Token::typeRegistry() {
  static std::array<const std::type_info*, 128> payload_types = {};
  return payload_types;
}

Token::Token(const char* _id) {
  auto& colors = nameRegistry();
  uint8_t i = 0;
  uint8_t slot = colors.size();
  for (; i < colors.size(); ++i) {
    if (std::string_view(colors[i].data()) == std::string_view(_id)) {
      id = i;
      return;
    }
    if (slot == colors.size() && std::string_view(colors[i].data()).empty()) {
      slot = i;  // first free slot, used when the name is not found
    }
  }
  id = slot;
  assert(id < colors.size() &&
         "There can only be kMaxTokenColors different token-colors.");
  strcpy(colors[id].data(), _id);
}

std::string_view Token::toString() const {
  return std::string_view(nameRegistry()[id].data());
}

const std::type_info* Token::boundType() const { return typeRegistry()[id]; }

void Token::bindTo(const std::type_info& t) const {
  auto& payload_types = typeRegistry();
  if (payload_types[id] == nullptr) {
    payload_types[id] = &t;
  } else if (*payload_types[id] != t) {
    throw TokenTypeError(std::string(toString()) +
                         " is already bound to a different payload type");
  }
}

std::vector<std::string_view> Token::getColors() {
  const auto& colors = nameRegistry();
  std::vector<std::string_view> _colors;
  _colors.reserve(colors.size());
  for (const auto& s : colors) {
    if (strlen(s.data()) > 0) {
      _colors.emplace_back(s.data());
    }
  }
  return _colors;
}

}  // namespace symmetri
