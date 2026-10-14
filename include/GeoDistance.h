#pragma once
#include <cmath>

namespace linkProtocol {
constexpr double kCloseMeters = 20.0;
inline double bearingDegrees(double lat1, double lon1, double lat2, double lon2) {
  constexpr double radians = 0.017453292519943295;
  const double delta = (lon2 - lon1) * radians;
  const double y = std::sin(delta) * std::cos(lat2 * radians);
  const double x = std::cos(lat1 * radians) * std::sin(lat2 * radians) -
      std::sin(lat1 * radians) * std::cos(lat2 * radians) * std::cos(delta);
  return std::fmod(std::atan2(y, x) / radians + 360.0, 360.0);
}
inline const char *compassDirection(double bearing) {
  static const char *directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return directions[static_cast<unsigned int>((bearing + 22.5) / 45.0) % 8];
}
inline double distanceMeters(double lat1, double lon1, double lat2, double lon2) {
  constexpr double radians = 0.017453292519943295;
  const double dLat = (lat2 - lat1) * radians;
  const double dLon = (lon2 - lon1) * radians;
  const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
      std::cos(lat1 * radians) * std::cos(lat2 * radians) *
      std::sin(dLon / 2) * std::sin(dLon / 2);
  const double bounded = a < 0 ? 0 : (a > 1 ? 1 : a);
  return 6371000.0 * 2 * std::atan2(std::sqrt(bounded), std::sqrt(1 - bounded));
}
}  // namespace linkProtocol
