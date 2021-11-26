#include <AUnitVerbose.h>
#include <Arduino.h>
#include <FakeProtoDispatch.h>
#include <MeshSyncTime.h>

using namespace aunit;

test(fastLocal) {
  MeshSyncTime t;

  // Jump set
  uint32_t now = 1000;
  uint32_t synced = 1000 * 1000;

  t.applySync(synced, now);

  assertEqual(t.localToSynced(now), synced);
  assertEqual(t.syncedToLocal(synced), now);

  // Jump set should induce no adjustment
  assertEqual(t.localToSynced(now + 10), synced + 10);
  assertEqual(t.syncedToLocal(synced + 10), now + 10);

  // Adjustment - local is running fast by 40 so we should be running
  // the synced clock at half speed for 80.
  uint32_t now2 = now + 100;
  uint32_t synced2 = synced + 100;
  t.applySync(synced2 - 40, now2);

  // When adjustment starts, we should not have adjusted at all.
  assertEqual(t.localToSynced(now2), synced2);
  assertEqual(t.syncedToLocal(synced2), now2);

  // When adjustment is half way through, we should half adjusted half way.
  assertEqual(t.localToSynced(now2 + 40), synced2 + 20);
  assertEqual(t.syncedToLocal(synced2 + 20), now2 + 40);

  // When adjustment is done, the post-sync time should be in place.
  assertEqual(t.localToSynced(now2 + 80), synced2 + 40);
  assertEqual(t.syncedToLocal(synced2 + 40), now2 + 80);

  // And then should continue running at normal speed.
  assertEqual(t.localToSynced(now2 + 80 + 10), synced2 + 40 + 10);
  assertEqual(t.syncedToLocal(synced2 + 40 + 10), now2 + 80 + 10);
}

test(slowLocal) {
  MeshSyncTime t;

  // Jump set
  uint32_t now = 1000;
  uint32_t synced = 1000 * 1000;

  t.applySync(synced, now);

  assertEqual(t.localToSynced(now), synced);
  assertEqual(t.syncedToLocal(synced), now);

  // Jump set should induce no adjustment
  assertEqual(t.localToSynced(now + 10), synced + 10);
  assertEqual(t.syncedToLocal(synced + 10), now + 10);

  // Adjustment - local is running slow by 40, so we should be running
  // the synced clock at double speed for 40.
  uint32_t now2 = now + 100;
  uint32_t synced2 = synced + 100;
  t.applySync(synced2 + 40, now2);

  // When adjustment starts, we should not have adjusted at all.
  assertEqual(t.localToSynced(now2), synced2);
  assertEqual(t.syncedToLocal(synced2), now2);

  // When adjustment is half way through, we should half adjusted half way.
  assertEqual(t.localToSynced(now2 + 20), synced2 + 40);
  assertEqual(t.syncedToLocal(synced2 + 40), now2 + 20);

  // When adjustment is done, the post-sync time should be in place.
  assertEqual(t.localToSynced(now2 + 40), synced2 + 80);
  assertEqual(t.syncedToLocal(synced2 + 80), now2 + 40);

  // And then should continue running at normal speed.
  assertEqual(t.localToSynced(now2 + 40 + 10), synced2 + 80 + 10);
  assertEqual(t.syncedToLocal(synced2 + 80 + 10), now2 + 40 + 10);
}

void setup() {
  TestRunner::setTimeout(30);
#if !defined(EPOXY_DUINO)
  delay(1000);  // wait to prevent garbage on SERIAL_PORT_MONITOR
#endif
  SERIAL_PORT_MONITOR.begin(115200);
  while (!SERIAL_PORT_MONITOR)
    ;  // needed for Leonardo/Micro
}

void loop() { TestRunner::run(); }
