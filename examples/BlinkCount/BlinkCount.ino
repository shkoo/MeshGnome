#include <MeshGnome.h>

// This simple example just blinks the builtin LED.  Upload this code
// to multiple nodes, and they will synchronize via WiFi.
//
// As a demonstration of MeshSyncMem, you can enter a new number in
// the Arduino Monitor.  This will set the number of blinks between
// pauses to the value you specify, and propagate it to all other
// nodes.
//
// As a demonstration of MeshSyncSketch, you can increase the version
// number passed to sketchUpdate.  If you upload this to one node, the
// new sketch will be propagated to all other reachable nodes.  To
// observe this via the builtin LED, you could also toggle the value
// of ON_IS_LOW so that blinks will be on instead of off or vice
// versa.

struct BlinkCountData {
  int numBlinks = 1;
};
MeshSyncStruct<BlinkCountData> blinkcount;
MeshSyncSketch sketchUpdate(1 /* sketch version */);
DispatchProto protos[] = {  //
    {1 /* protocol id */, &sketchUpdate},
    {2 /* protocol id */, &blinkcount}};

// If true, set LED_BUILTIN to LOW during blinks; otherwise set it to HIGH.
static constexpr bool ON_IS_LOW = false;

void setup() {
  delay(300);
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  EspProtoDispatch.begin(protos);
  Serial.printf("BlinkCount startup complete, running sketch version %d\n",
                sketchUpdate.localVersion());
}

// Number of blinks remaining before pausing and resetting.
int blinkCountLeft = 0;

void blinkLED() {
  static bool on = false;
  static uint32_t lastChange = 0;
  if ((uint32_t(millis()) - lastChange) < 300) {
    return;
  }
  Serial.print(".");
  lastChange = millis();

  if (on) {
    digitalWrite(LED_BUILTIN, ON_IS_LOW ? LOW : HIGH);
    on = false;
    return;
  }

  --blinkCountLeft;
  if (blinkCountLeft >= 0) {
    digitalWrite(LED_BUILTIN, ON_IS_LOW ? HIGH : LOW);
    on = true;
    return;
  }

  if (blinkCountLeft < -4) {
    blinkCountLeft = blinkcount->numBlinks;
    printf("Blinking %d times\n", blinkCountLeft);
  }
}

void loop() {
  blinkLED();
  EspProtoDispatch.espTransmitIfNeeded();

  if (Serial.available()) {
    int newBlinks = Serial.parseInt();
    if (!newBlinks) {
      return;
    }
    int oldBlinks = blinkcount->numBlinks;
    int oldVersion = blinkcount.localVersion();
    blinkcount->numBlinks = newBlinks;
    blinkcount.push();
    Serial.printf("Ok, blinks %d -> %d, configuration version=%d -> %d\n", oldBlinks, newBlinks,
                  oldVersion, blinkcount.localVersion());
  }
}
