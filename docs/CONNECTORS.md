# Modbus TCP connectors (5.11)

Open **Connectors** in the web interface. This release has two tabs: **Battery**
and **PV**, each with a universal Modbus TCP connection and editable mapper. Both connectors
only read holding registers (FC03); they never write to the connected device.
The dongle's existing Modbus server and its meter mapping remain independent.

Each connector has an enable switch, IPv4 address, TCP port, unit ID and poll
interval. Each mapped value has a zero-based wire address, datatype (`uint16`,
`int16`, `uint32`, `int32`, `float32`), multiplier and optional word swap.
Address zero is a valid register. Word swap exchanges the two 16-bit words of a
32-bit value; bytes within a register keep Modbus network order.

## Presets

Choose **Custom**, **Victron GX** (Battery), or **SolarEdge SunSpec** (PV).
Selecting an option does not change the connection or mapper. Click **Apply
preset** to fill the TCP port, unit ID, registers, types, factors, word swap,
and battery state codes or PV exponent registers. The IP address, enabled
state, poll interval and installed PV power are preserved.

All mapper fields remain editable. Editing a preset value changes the selection
to **Custom**; this keeps the edited values. **Save** persists and activates
changes, while **Undo** restores the saved fields. Preset names are inferred
from the current form values rather than saved as a separate device setting.
Existing configurations are never overwritten when the page opens.

## Battery

The defaults retain the existing Victron GX system mapping:

| Measurement | Address | Type | Unit |
| --- | --- | --- | --- |
| Active power | 842 | int16 | W |
| State of charge | 843 | uint16 | % |
| Operating state | 844 | uint16 | code |

The default port is 502, unit ID 100 and interval 5 seconds. State codes are
configurable, initially idle=0, charging=1, discharging=2. Positive power means
charging; negative power means discharging. A negative multiplier can reverse
the sign for a device with the opposite convention.

Existing `victron_accu_enabled`, `victron_accu_ip` and `victron_accu_id` settings
continue to work without migration. Missing mapping settings use the defaults.
The existing `/api/v2/accu`, dashboard and ESP-NOW battery consumers continue to
use the battery data. A sample is published only after all three values have
been read successfully. Invalid state-of-charge readings are rejected.

## PV

Enable Modbus TCP on the inverter and use its configured port; the initial
connector port is 1502 and unit ID is 1. The defaults use **base-0** addresses
from the [SolarEdge SunSpec technical note](https://www.solaredge.com/sites/default/files/sunspec-implementation-technical-note.pdf).
Do not add or subtract 40001 from these addresses.

| Measurement | Address | Type | Exponent register |
| --- | --- | --- | --- |
| AC power | 40083 | int16 | 40084 |
| Lifetime production | 40093–40094 | uint32 | 40095 |

The decoded value is multiplied by the configured factor and then by `10^SF`.
Set the SF register to zero to use only the manual multiplier. SunSpec
unavailable readings and invalid exponents are rejected. All values in a poll
cycle must succeed before the dashboard is updated.

Daily production is the difference from the first lifetime reading on each
local calendar day. This baseline is stored as one NVS record and restored
after reboot. Changing the energy source/mapping or resetting the inverter
counter creates a new baseline. On installation day, and if the dongle was off
at the start of a day, production before the first reading is not recoverable.
The clock must be synchronized before daily yield can be calculated.

While enabled, the TCP connector replaces the SolarEdge HTTP contribution to
the PV API, dashboard, daily history total and ESP-NOW output. Other existing
PV sources continue to contribute. Disable the TCP connector to resume the
existing SolarEdge HTTP integration configured through Solar Setup.

Both connectors expire live readings after twice the configured interval, with
a minimum grace period of 20 seconds. Connection and mapping settings are saved
in `DSMRsettings.json`. They are preserved by a network reset.

## Validation

```sh
c++ -std=c++17 -Wall -Wextra -pedantic tests/modbus_connector_test.cpp -o /tmp/modbus_connector_test
/tmp/modbus_connector_test
node tests/test_connectors.cjs
./build.sh ULTRA P1P
```

The host test covers signed and 32-bit decoding, fractional factors, word order,
SunSpec exponents/unavailable values, full-cycle polling, failures, configuration
changes and daily baseline restoration/rollover. Hardware communication still
needs validation against the actual Victron GX and SolarEdge inverter.
