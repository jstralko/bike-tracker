#include "GeoDistance.h"
#include <cassert>
#include <cmath>
#include <cstring>

int main() {
  using namespace linkProtocol;
  assert(distanceMeters(37, -122, 37, -122) == 0);
  assert(std::abs(bearingDegrees(0, 0, 1, 0)) < 0.001);
  assert(std::abs(bearingDegrees(0, 0, 0, 1) - 90) < 0.001);
  assert(std::abs(bearingDegrees(0, 0, -1, 0) - 180) < 0.001);
  assert(std::abs(bearingDegrees(0, 0, 0, -1) - 270) < 0.001);
  assert(std::strcmp(compassDirection(45), "NE") == 0);
  assert(std::strcmp(compassDirection(359), "N") == 0);
  // One degree along the equator is about 111.195 km.
  assert(std::abs(distanceMeters(0, 0, 0, 1) - 111194.9266) < 0.1);
  assert(distanceMeters(0, 0, 0, 0.0001) < kCloseMeters);
  assert(distanceMeters(0, 0, 0, 0.0003) > kCloseMeters);
  assert(std::abs(distanceMeters(0, 179.999, 0, -179.999) - 222.38985) < 0.1);
  assert(std::isfinite(distanceMeters(0, 0, 0, 180)));
  assert(std::abs(distanceMeters(40, -120, 41, -121) -
                  distanceMeters(41, -121, 40, -120)) < 0.001);
}
