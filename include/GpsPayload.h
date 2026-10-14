#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>

namespace linkProtocol {
// Version 1: PONG:<sequence>|GPS:1,<status>,<satellites>,<lat>,<lon>
// Status: 0 = no fix, 1 = fresh fix, 2 = unavailable/stale.
struct GpsPayload {
  unsigned int status = 2;
  unsigned int satellites = 0;
  double latitude = 0;
  double longitude = 0;
};

inline bool parseGpsPong(const char *message, const char *expected, GpsPayload &out) {
  const size_t prefixLength = std::strlen(expected);
  if (std::strncmp(message, expected, prefixLength) != 0 ||
      std::strncmp(message + prefixLength, "|GPS:1,", 7) != 0) return false;
  GpsPayload parsed;
  int consumed = 0;
  const char *fields = message + prefixLength + 7;
  if (std::sscanf(fields, "%u,%u,%lf,%lf%n", &parsed.status, &parsed.satellites,
                  &parsed.latitude, &parsed.longitude, &consumed) != 4 ||
      fields[consumed] != '\0') return false;
  if (parsed.status > 2 || parsed.satellites > 99 ||
      !std::isfinite(parsed.latitude) || !std::isfinite(parsed.longitude) ||
      parsed.latitude < -90 || parsed.latitude > 90 ||
      parsed.longitude < -180 || parsed.longitude > 180) return false;
  if (parsed.status != 1 && (parsed.latitude != 0 || parsed.longitude != 0)) return false;
  out = parsed;
  return true;
}
}  // namespace linkProtocol
