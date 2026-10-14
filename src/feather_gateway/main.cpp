#include "app.h"

void setup() {
  featherGateway::begin();
}

void loop() {
  featherGateway::tick();
}
