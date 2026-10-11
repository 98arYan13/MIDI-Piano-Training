# Protection tests

`protection.cpp` drives the actual sfizz allocator and renderer using a real, unmodified Salamander C4 sample. Red baseline must be run against the pinned stock source before applying `native/patches/sfizz-protected.patch`.

`queue.cpp` drives the production event queue: 24 attacks fill a 32-event queue's attack allowance, the 25th attack is refused, and all 8 reserved note-off slots remain usable. Its red baseline returned false for the first admitted attack; the implemented queue passes 124 assertions.

Native render tests prove source/bus ownership and sample audio. They cannot prove physical MIDI driver timing or wired WASAPI round-trip latency; those require hardware measurements.
