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
// t_inspect BRANCHES: it has two output places but deposits a token in
// exactly one of them, depending on what it finds in the image.
//
// t_report additionally misbehaves on purpose: it tries to deposit tokens in
// places it has no output arc to. Those deposits are DROPPED by the executor
// — a transition can only put tokens where it is connected, exactly like it
// can only consume tokens where it is connected.

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

// Deposits one typed token. The place is named, the color is INFERRED from
// the payload type (that is what the bijection buys us): it is impossible to
// accidentally put an Image on a DefectsToken-colored arc.
Marking capture() {
  std::cout << "[t_capture] capturing image 42" << std::endl;
  return {{"p_image", Image{42}}};
}

// The parameter is matched by color, not by position: the executor looks at
// the consumed tokens and hands the Image payload to the `const Image&`
// parameter. Returning a Marking with ONE entry — while the transition has
// TWO output places — is the branch: the other place simply stays empty.
Marking inspect(const Image& img) {
  const Defects defects{{7, 13}};  // pretend we found two defects in the image
  if (defects.box_ids.empty()) {
    std::cout << "[t_inspect] image " << img.id << " is clean" << std::endl;
    return {{"p_ok", Success}};  // dataless deposit on the Success-colored arc
  } else {
    std::cout << "[t_inspect] image " << img.id << " has "
              << defects.box_ids.size() << " defects" << std::endl;
    return {{"p_defects", defects}};  // typed deposit; p_ok stays empty
  }
}

// A misbehaving callback: besides its legitimate deposit in p_done, it tries
// to deposit in p_trigger (a place that EXISTS but that t_report has no
// output arc to) and in p_bogus (a place that does not exist at all). Both
// stray deposits are dropped; only the p_done token lands.
Marking report(const Defects& defects) {
  std::cout << "[t_report] reporting " << defects.box_ids.size()
            << " defects — and trying to sneak tokens into p_trigger and "
               "p_bogus"
            << std::endl;
  return {
      {"p_done", Success},     // connected: this token is placed
      {"p_trigger", Success},  // NOT an output of t_report: dropped
      {"p_bogus", Success},    // no such place: dropped
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

  // The final marking is a single Success token in p_done. In particular:
  //  - p_ok is empty (t_inspect branched to p_defects),
  //  - p_trigger is empty (the stray deposit was dropped; had it landed, the
  //    net would have looped forever!),
  //  - p_bogus does not appear anywhere.
  std::cout << "final marking:" << std::endl;
  for (const auto& token : app.getMarking()) {
    std::cout << "  " << token.place << ", " << token.color.toString()
              << std::endl;
  }

  return 0;
}
