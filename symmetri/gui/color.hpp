#pragma once
#include <array>
#include <cmath>

// https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
inline double ratio() {
  const static double phi = (std::sqrt(5) + 1.0) / 2.0;
  static double n = 0;
  return std::fmod(++n * phi, 1.0);
}

// https://martin.ankerl.com/2009/12/09/how-to-create-random-colors-programmatically/
inline std::array<int, 3> hsv_to_rgb(float h, float s, float v) {
  auto h_i = (h * 6);
  auto f = h * 6 - h_i;
  auto p = v * (1 - s);
  auto q = v * (1 - f * s);
  auto t = v * (1 - (1 - f) * s);
  constexpr auto tf = [](float a, float b, float c) -> std::array<int, 3> {
    return {static_cast<int>(255. * a), static_cast<int>(255. * b),
            static_cast<int>(255. * c)};
  };
  switch (static_cast<int>(h_i)) {
    case 0:
      return tf(v, t, p);
    case 1:
      return tf(q, v, p);
    case 2:
      return tf(p, v, t);
    case 3:
      return tf(p, q, v);
    case 4:
      return tf(t, p, v);
    case 5:
      return tf(v, p, q);
    default:
      return tf(1, 1, 1);
  }
}
