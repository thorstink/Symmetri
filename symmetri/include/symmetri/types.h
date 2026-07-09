#pragma once

/** @file types.h */

#include <stddef.h>
#include <stdint.h>

#include <any>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "symmetri/colors.hpp"

namespace symmetri {
using Place = std::string;       ///< The string representation of the place the
                                 ///< Petri net definition
using Transition = std::string;  ///< The string representation of the
                                 ///< transition the Petri net definition
using Clock =
    std::chrono::steady_clock;  ///< The clock definition in Symmetri is the
                                ///< steady clock for monotonic reasons

/**
 * @brief A token in a place: a color plus the payload the color is bound to.
 * The type defines the token: colors created with CREATE_TYPED_TOKEN carry
 * exactly their bound payload type; all other colors (Success, Failed, colors
 * parsed from net files, ...) are dataless. Attaching a payload that
 * contradicts the binding throws TokenTypeError at construction. Equality is
 * (place, color) only: enabling and consuming keep plain multiset semantics
 * and never inspect the data. The payload is shared and const, so a token can
 * be copied (e.g. fan out to multiple places) and read concurrently without
 * synchronization.
 *
 * @tparam P how the place is referenced: by index (size_t, executor-side) or
 * by name (Place, user-side).
 */
template <typename P>
struct BasicToken {
  P place;      ///< The place holding (or receiving) this token
  Token color;  ///< The color; arcs match on this
  std::shared_ptr<const std::any> data;  ///< The color's bound payload

  BasicToken(P p, Token c) : place(std::move(p)), color(c), data(nullptr) {}
  BasicToken(P p, Token c, std::shared_ptr<const std::any> d)
      : place(std::move(p)), color(c), data(std::move(d)) {
    if (data != nullptr) {
      requireBinding(color, data->type());
    }
  }
  /**
   * @brief Construct a token carrying `v` as payload; the value is moved or
   * copied into a shared immutable slot. The color must be bound to
   * exactly decltype(v) — dataless colors cannot carry payloads.
   */
  template <typename T, typename = std::enable_if_t<!std::is_convertible_v<
                            T, std::shared_ptr<const std::any>>>>
  BasicToken(P p, Token c, T&& v)
      : place(std::move(p)),
        color((requireBinding(c, typeid(std::decay_t<T>)), c)),
        data(std::make_shared<const std::any>(std::forward<T>(v))) {}

  /**
   * @brief Construct a token from a payload value alone: the color is
   * inferred through the type↔color bijection. Only compiles for types bound
   * with CREATE_TYPED_TOKEN.
   */
  template <typename T>
    requires requires { color_of<std::decay_t<T>>::value(); }
  BasicToken(P p, T&& v)
      : place(std::move(p)),
        color(color_of<std::decay_t<T>>::value()),
        data(std::make_shared<const std::any>(std::forward<T>(v))) {}

  /**
   * @brief Checked access to the payload. Throws std::bad_any_cast if the
   * token carries no payload or a payload of a different type.
   */
  template <typename T>
  const T& get() const {
    if (data == nullptr) {
      throw std::bad_any_cast();
    }
    return std::any_cast<const T&>(*data);
  }

  /**
   * @brief Throws TokenTypeError unless color `c` is bound to payload type
   * `t`.
   */
  static void requireBinding(Token c, const std::type_info& t) {
    const std::type_info* bound = c.boundType();
    if (bound == nullptr) {
      throw TokenTypeError(std::string(c.toString()) +
                           " is a dataless color; it cannot carry a payload");
    }
    if (*bound != t) {
      throw TokenTypeError(std::string(c.toString()) +
                           " is bound to a different payload type");
    }
  }

  /// Equality deliberately ignores the payload: tokens of the same color in
  /// the same place are indistinguishable to the net.
  friend bool operator==(const BasicToken& a, const BasicToken& b) {
    return a.place == b.place && a.color == b.color;
  }

  /// Ordering (used to sort markings for multiset comparison) likewise
  /// ignores the payload.
  friend bool operator<(const BasicToken& a, const BasicToken& b) {
    return a.place < b.place ||
           (a.place == b.place && a.color.toIndex() < b.color.toIndex());
  }
};

/**
 * @brief AugmentedToken describes a token with a color in a particular place,
 * the place referenced by index. This is the executor-side representation.
 */
using AugmentedToken = BasicToken<size_t>;

/**
 * @brief PlaceToken references its place by name. It is the user-side
 * representation: initial/goal markings, getMarking() and the rich
 * callback-return (a set of deposits) are all made of these.
 */
using PlaceToken = BasicToken<Place>;

/**
 * @brief This struct defines a subset of data that we associate with the
 * result of firing a transition.
 *
 */
struct Event {
  std::string case_id;      ///< The case_id of this event
  std::string transition;   ///< The transition that generated the event
  Token state;              ///< The result of the event
  Clock::time_point stamp;  ///< The timestamp of the event
};

using Eventlog = std::vector<Event>;  ///< The eventlog is simply a log of
                                      ///< events, sorted by their stamp

using Net = std::unordered_map<
    Transition,
    std::pair<std::vector<std::pair<Place, Token>>,
              std::vector<std::pair<Place, Token>>>>;  ///< This is the multiset
                                                       ///< definition of a
                                                       ///< Petri net. For each
                                                       ///< transition there is
                                                       ///< a pair of lists for
                                                       ///< colored input and
                                                       ///< output places

using Marking =
    std::vector<PlaceToken>;  ///< The Marking is a vector of tokens, each
                              ///< naming the place it resides in. Because
                              ///< tokens can carry payloads, a Marking is also
                              ///< what a callback returns to deposit tokens
                              ///< into (a subset of) its output places.

using PriorityTable =
    std::vector<std::pair<Transition, int8_t>>;  ///< Priority is limited from
                                                 ///< -128 to 127

/**
 * @brief A DirectMutation is a synchronous no-operation function. It simply
 * mutates the mutation on the petri net executor loop. This way the deferring
 * to the threadpool and back to the petri net loop is avoided.
 *
 */
struct DirectMutation {};
class PetriNet;

/**
 * @brief A DirectMutation is a synchronous Callback that always
 * completes.
 *
 */
bool isSynchronous(const DirectMutation&);

/**
 * @brief fire for a direct mutation does no work at all.
 *
 * @return Token
 */
Token fire(const DirectMutation&);

/**
 * @brief The fire specialization for a PetriNet runs the Petri net until it
 * completes, deadlocks or is preempted. It returns an event log along with the
 * result state.
 *
 * @return Token
 */
Token fire(const PetriNet&);

/**
 * @brief The cancel specialization for a PetriNet breaks the PetriNets'
 * internal loop. It will not queue any new Callbacks and it will cancel all
 * child Callbacks that are running. The cancel function will return before the
 * PetriNet is preempted completely.
 *
 * @return void
 */
void cancel(const PetriNet&);

/**
 * @brief The pause specialization for a PetriNet prevents new fire-able
 * Callbacks from being scheduled. It will also pause all child Callbacks that
 * are running. The pause function will return before the PetriNet will pause.
 *
 */
void pause(const PetriNet&);

/**
 * @brief The resume specialization for a PetriNet undoes pause and puts the
 * PetriNet back in a normal state where all fireable Callback are scheduled for
 * execution. The resume function will return before the PetriNet will resume.
 *
 */
void resume(const PetriNet&);

/**
 * @brief Get the Log object
 *
 * @return Eventlog
 */
Eventlog getLog(const PetriNet&);

}  // namespace symmetri
