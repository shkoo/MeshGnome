#include <AUnitVerbose.h>
#include <Arduino.h>
#include <FakeProtoDispatch.h>
#include <MeshSyncMem.h>
#include <MeshSyncStruct.h>

using namespace aunit;

void runSome(size_t numReps, std::initializer_list<FakeProtoDispatch*> ds) {
  for (size_t i = 0; i != numReps; ++i) {
    for (FakeProtoDispatch* d : ds) {
      d->transmitAndReceive();
    }
    delay(100);
  }
}

using eth_addr = FakeProtoDispatch::eth_addr;

test(simpleTest) {
  FakeProtoDispatch a(eth_addr(123));
  MeshSyncMem memsync;
  DispatchProto protos[] = {{1, &memsync}};
  a.begin(protos);
  memsync.update(10, "Version 10 metadata...", "Version 10 data!");
  runSome(20, {&a});

  assertEqual(memsync.localMetadata(), "Version 10 metadata...");
  assertEqual(memsync.localData(), "Version 10 data!");
}

test(simpleTransferTest) {
  FakeProtoDispatch d1(eth_addr(123));
  MeshSyncMem memsync1;
  DispatchProto protos1[] = {{1, &memsync1}};

  FakeProtoDispatch d2(eth_addr(456));
  MeshSyncMem memsync2;
  DispatchProto protos2[] = {{1, &memsync2}};

  d1.begin(protos1);
  d2.begin(protos2);
  memsync1.update(10, "Version 10 metadata...A", "Version 10 data!A");
  memsync2.update(10, "Version 10 metadata...B", "Version 10 data!B");
  runSome(20, {&d1, &d2});
  assertEqual(memsync1.localMetadata(), "Version 10 metadata...A");
  assertEqual(memsync1.localData(), "Version 10 data!A");
  assertEqual(memsync2.localMetadata(), "Version 10 metadata...B");
  assertEqual(memsync2.localData(), "Version 10 data!B");

  memsync1.update(11, "Version 11 metadata...", "Version 11 data!");
  runSome(20, {&d1, &d2});
  assertEqual(memsync1.localMetadata(), "Version 11 metadata...");
  assertEqual(memsync1.localData(), "Version 11 data!");
  assertEqual(memsync2.localMetadata(), "Version 11 metadata...");
  assertEqual(memsync2.localData(), "Version 11 data!");
}

test(bigTransfer) {
  String bigData;
  while (bigData.length() < 300) {
    bigData += " BIG";
  }

  FakeProtoDispatch d1(eth_addr(123));
  MeshSyncMem memsync1;
  DispatchProto protos1[] = {{1, &memsync1}};

  FakeProtoDispatch d2(eth_addr(456));
  MeshSyncMem memsync2;
  DispatchProto protos2[] = {{1, &memsync2}};

  d1.begin(protos1);
  d2.begin(protos2);
  memsync1.update(10, "Version 10 metadata...", bigData);

  runSome(20, {&d1, &d2});
  assertEqual(memsync1.localMetadata(), "Version 10 metadata...");
  assertEqual(memsync1.localData(), bigData);
  assertEqual(memsync2.localMetadata(), "Version 10 metadata...");
  assertEqual(memsync2.localData(), bigData);
}

test(lossyBigTransfer) {
  String bigData;
  while (bigData.length() < 1234) {
    bigData += " BIG";
  }

  FakeProtoDispatch d1(eth_addr(123));
  MeshSyncMem memsync1;
  DispatchProto protos1[] = {{1, &memsync1}};

  FakeProtoDispatch d2(eth_addr(456));
  MeshSyncMem memsync2;
  DispatchProto protos2[] = {{1, &memsync2}};

  d1.setSendLossy(0.3);

  d1.begin(protos1);
  d2.begin(protos2);
  memsync1.update(10, "Version 10 metadata...", bigData);

  runSome(50, {&d1, &d2});
  assertEqual(memsync1.localMetadata(), "Version 10 metadata...");
  assertEqual(memsync1.localData(), bigData);
  assertEqual(memsync2.localMetadata(), "Version 10 metadata...");
  assertEqual(memsync2.localData(), bigData);
}

test(memConversionTest) {
  struct A {
    size_t a1;
  };

  FakeProtoDispatch d(eth_addr(123));
  MeshSyncMem memsync;
  DispatchProto protos[] = {{1, &memsync}};
  d.begin(protos);

  memsync.update(10, "Md", "D");

  assertEqual(memsync.localMetadata(), "Md");
  assertEqual(memsync.localData(), "D");

  assertEqual(memsync.localMetadataBufferLen(), 2UL);
  assertEqual(0, memcmp(memsync.localMetadataBuffer(), "Md", 2));

  assertEqual(memsync.localDataBufferLen(), 1UL);
  assertEqual(0, memcmp(memsync.localDataBuffer(), "D", 1));
}

test(structTest) {
  struct T {
    int v1 = 10;
    size_t v2 = 20;

    T() = default;
    T(int initialV1) : v1(initialV1) {}
  };

  FakeProtoDispatch d1(eth_addr(123));
  MeshSyncStruct<T> sync1;
  DispatchProto protos1[] = {{1, &sync1}};

  FakeProtoDispatch d2(eth_addr(456));
  MeshSyncStruct<T> sync2(5);
  DispatchProto protos2[] = {{1, &sync2}};

  d1.begin(protos1);
  d2.begin(protos2);

  assertEqual(sync1->v1, 10);
  assertEqual(sync2->v1, 5);

  sync2->v2 = 23;
  sync2.push();

  assertEqual(sync1.localVersion() + 1, sync2.localVersion());

  runSome(20, {&d1, &d2});

  assertEqual(sync1->v1, 5);
  assertEqual(sync1->v2, 23UL);
  assertEqual(sync2->v1, 5);
  assertEqual(sync2->v2, 23UL);

  assertEqual(sync1.localVersion(), sync2.localVersion());
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
