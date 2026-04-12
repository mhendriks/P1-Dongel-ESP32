# Han2Lib

Experimental Arduino reader/parser library for Scandinavian HAN/AMS push messages.

## Goals

- Keep the public API close to `dsmr2Lib`
- Allow `HanReader` and `P1Reader` to coexist in the same sketch
- Parse byte-oriented HAN frames such as list 1 and list 2
- Retain hourly totals from list 2 while list 1 keeps updating live values

## Current public API

```cpp
#include <han2.h>

HanReader reader(&Serial1);
HanData data;
String err;

void loop() {
  reader.loop();
  if (reader.available()) {
    if (reader.parse(&data, &err)) {
      // use data
    }
  }
}
```

## Current data model

`HanData` currently exposes:

- timestamp
- identification
- equipment_id
- list_type
- active import/export power
- reactive import/export power
- voltage L1/L2/L3
- current L1/L2/L3
- total import/export energy
- total reactive import/export energy

`HanReader::state()` exposes a retained `HanState` with:

- `last_list1`
- `last_list2`
- `effective`

## Notes

- This is a scaffold intended to migrate the HAN support out of the firmware.
- CRC/HDLC validation is not yet implemented in this first cut.
- OBIS coverage is intentionally minimal and based on the currently used Kamstrup list 1 / list 2 fields.

