// This example shows tokens that carry data. The rule is: the type defines
// the token. A color created with CREATE_TYPED_TOKEN is bound 1:1 to a
// payload type; every token of that color carries exactly that type. Colors
// like Success stay dataless. Arcs match on color, so a typed arc is a typed
// contract between two transitions: whatever flows over it carries the bound
// payload.
//
// The net models a tiny inspection pipeline:
//
//   p_trigger --(Success)--> [t_capture] --(ImageToken)--> p_image
//   p_image --(ImageToken)--> [t_inspect] --(DefectsToken)--> p_defects
//                                         \--(Success)------> p_ok
//   p_defects --(DefectsToken)--> [t_report] --(Success)--> p_done
//
// A callback that returns a Marking must shape it in one of two legal ways:
//
//  1. Cover ALL output places (one token per output arc, placed as given).
//     Branching is done by COLOR, not by omission: deposit a color that no
//     downstream arc matches and that branch is dead.
//  2. Use a single color: every output place gets that token — exactly like
//     returning a bare Token (legacy). Place names are not even looked at.
//
// A returned marking that matches neither shape is ignored entirely — the
// transition then produces nothing (t_report below demonstrates this).

#include <iostream>
#include <vector>

#include "symmetri/symmetri.h"

// The payload types. Plain structs; nothing to inherit or register besides
// the CREATE_TYPED_TOKEN binding below.
struct Image {
  int id;
};
struct Defects {
  std::vector<int> box_ids;
};

// The bindings. The bijection is enforced both ways: re-binding ImageToken to
// another type throws at startup, and binding Image to a second color would
// not compile (color_of<Image> would be specialized twice).
CREATE_TYPED_TOKEN(ImageToken, Image)
CREATE_TYPED_TOKEN(DefectsToken, Defects)

using namespace symmetri;

// Deposits one typed token — its single output place is covered, so shape 1
// applies. The place is named, the color is INFERRED from the payload type
// (that is what the bijection buys us): it is impossible to accidentally put
// an Image on a DefectsToken-colored arc.
Marking capture() {
  std::cout << "[t_capture] capturing image 42" << std::endl;
  return {{"p_image", Image{42}}};
}

// The parameter is matched by color, not by position: the executor looks at
// the consumed tokens and hands the Image payload to the `const Image&`
// parameter. The returned marking covers BOTH output places (shape 1); the
// branch is in the colors. On the defect branch, p_ok receives a Failed
// token — no Success-colored arc downstream will ever consume it, so that
// path is dead. On the clean branch it is p_defects that gets the dead
// (dataless Failed) token, and t_report never fires.
Marking inspect(const Image& img) {
  const Defects defects{{7, 13}};  // pretend we found two defects in the image
  if (defects.box_ids.empty()) {
    std::cout << "[t_inspect] image " << img.id << " is clean" << std::endl;
    return {{"p_defects", Failed}, {"p_ok", Success}};
  } else {
    std::cout << "[t_inspect] image " << img.id << " has "
              << defects.box_ids.size() << " defects" << std::endl;
    return {{"p_defects", defects}, {"p_ok", Failed}};
  }
}

// A misbehaving callback: besides its legitimate deposit in p_done, it tries
// to deposit in p_trigger (a place that EXISTS but that t_report has no
// output arc to) and in p_bogus (a place that does not exist at all). The
// marking neither covers t_report's output places ({p_done} != {p_done,
// p_trigger, p_bogus}) nor carries a single color — so it is ignored
// ENTIRELY: not even the legitimate p_done token lands, and the net
// deadlocks with p_done empty.
Marking report(const Defects& defects) {
  std::cout << "[t_report] reporting " << defects.box_ids.size()
            << " defects — and trying to sneak tokens into p_trigger and "
               "p_bogus"
            << std::endl;
  return {
      {"p_done", Success},
      {"p_trigger", Success},
      {"p_bogus", DefectsToken, Defects{}},  // second color: shape 2 ruled out
  };
}

int main(int, char**) {
  const Net net = {
      {"t_capture", {{{"p_trigger", Success}}, {{"p_image", ImageToken}}}},
      {"t_inspect",
       {{{"p_image", ImageToken}},
        {{"p_defects", DefectsToken}, {"p_ok", Success}}}},
      {"t_report", {{{"p_defects", DefectsToken}}, {{"p_done", Success}}}}};

  auto pool = std::make_shared<TaskSystem>(1);
  // No goal marking: the net runs until no transition can fire.
  PetriNet app(net, "typed_tokens", pool, {{"p_trigger", Success}}, {});

  app.registerCallback("t_capture", &capture);
  app.registerCallback("t_inspect", &inspect);
  app.registerCallback("t_report", &report);

  fire(app);

  // Expected final marking:
  //   p_ok, Failed     <- the dead branch token from t_inspect
  // and NOTHING in p_done: t_report fired, but its malformed marking was
  // ignored, so its output place stayed empty. (Had the p_trigger sneak
  // landed, the net would have looped forever!)
  std::cout << "final marking:" << std::endl;
  for (const auto& token : app.getMarking()) {
    std::cout << "  " << token.place << ", " << token.color.toString()
              << std::endl;
  }

  return 0;
}
