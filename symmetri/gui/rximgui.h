#pragma once

namespace rximgui {
rpp::subjects::publish_subject<int> framebus;
auto frameout = framebus.get_subscriber();
auto sendframe = []() { frameout.on_next(1); };
auto frames = framebus.get_observable();
inline rpp::schedulers::run_loop rl{};

}  // namespace rximgui
