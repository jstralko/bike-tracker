#include "GpsPayload.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace linkProtocol;
  GpsPayload position;
  assert(parseGpsPong("PONG:42|GPS:1,1,5,37.123456,-122.123456", "PONG:42", position));
  assert(position.status == 1 && position.satellites == 5 && position.longitude < 0);
  assert(parseGpsPong("PING:42|GPS:1,1,5,37.123456,-122.123456", "PING:42", position));
  assert(!parseGpsPong("PING:420|GPS:1,1,5,1,2", "PING:42", position));
  assert(parseGpsPong("PONG:42|GPS:1,1,5,0.000000,0.000000", "PONG:42", position));
  assert(parseGpsPong("PONG:42|GPS:1,0,0,0,0", "PONG:42", position));
  assert(parseGpsPong("PONG:42|GPS:1,2,0,0,0", "PONG:42", position));
  const char *invalid[] = {
      "", "PONG:42", "PONG:420|GPS:1,1,5,1,2", "PONG:41|GPS:1,1,5,1,2",
      "PONG:42|GPS:2,1,5,1,2", "PONG:42|GPS:1,3,5,1,2",
      "PONG:42|GPS:1,1,5,91,2", "PONG:42|GPS:1,1,5,1,-181",
      "PONG:42|GPS:1,1,5,nan,2", "PONG:42|GPS:1,1,5,1,inf",
      "PONG:42|GPS:1,1,5,1", "PONG:42|GPS:1,1,5,1,2junk",
      "PONG:42|GPS:1,0,5,1,2", "PONG:42|GPS:1,1,100,1,2"};
  for (const char *packet : invalid) assert(!parseGpsPong(packet, "PONG:42", position));
  std::cout << "GPS payload tests passed\n";
}
