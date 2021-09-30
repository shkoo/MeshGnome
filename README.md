# MeshGnome

## Introduction

MeshGnome is an Arduino library targeting the ESP8266 chip with
integrated WiFi using the ESP-Now communication API.

Its primary feature is to allow synchronization of data among nodes in
the mesh.  It can synchronize both the firmware/sketch, and arbitrary
in-memory data.

It implements these synchronization primitives on top of
"ProtoDispatch" which adds a protocol number to the ESP-Now packet to
determine which application subsystem is used.

Each protocol number can be handled independently and used for either
a MeshGnome synchronization protocol, or a custom protocol external to
this library.

## BlinkCount example

The "BlinkCount" example shows how to use EspMeshSyncSketch for
synchronizing firmware, and MeshSyncMem for synchronizing arbitrary
data.

## EspMeshSyncSketch

"EspMeshSyncSketch" synchronizes the firmware among nodes and will
upgrade any nodes running a lower version number to the sketch on the
nodes running a higher version number.

## MeshSyncMem

"MeshSyncMem" synchronizes arbitrary data among nodes.  Similarly to
"EspMeshSyncSketch", it synchronizes data from nodes with a higher
version of the data to nodes with a lower version of the data.

MeshSyncMem synchronizes both "data" and "metadata".  "Metadata" must
be small enough to fit in the rest of the packet not used by the
MeshGnome headers.  The regular data must fit in RAM but will use
multiple packets to synchronize when it's out of date.

## Disclaimer

Disclaimer: There is no actual gnome in this mesh; MeshGnome may be a
misnomer (MeshGnomer?).

## Ideas for future development

* Add security features, like encryption and support for sketch
  signing.

* Add more robustness in the case of something going wrong, instead of
  just crashing the sketch.

* Add support for synchronizing LittleFS files to provide an
  alternative to MeshSyncMem if the synchronized item can't entirely
  fit in RAM.

* Add support for more traditional mesh networking.  For instance,
  have the ability to communicate with specific nodes as opposed to
  only broadcast synchronization.

* Clean up log messages to Serial, let users configure where they want them instead.

* Add hook callbacks when synchronizations complete.
