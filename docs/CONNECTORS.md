# Modbus TCP connectors (5.11)

Open **Connectors** in the web interface. **Battery** and **PV · Modbus TCP**
provide a universal Modbus TCP connection and editable mapper. These two connectors
only read holding registers (FC03); they never write to the connected device.
Separate **Enphase**, **SMA** and **Omniksol** tabs configure the HTTP PV sources.
The dongle's existing Modbus server and its meter mapping remain independent.

Each connector has an enable switch, IPv4 address, TCP port, unit ID and poll
interval. Each mapped value has a zero-based wire address, datatype (`uint16`,
`int16`, `uint32`, `int32`, `float32`), multiplier and optional word swap.
Address zero is a valid register. Word swap exchanges the two 16-bit words of a
32-bit value; bytes within a register keep Modbus network order.

## Presets

Choose **Custom**, **Victron GX** or **SolarEdge** (Battery), or
**SolarEdge SunSpec** (PV).
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

### SolarEdge battery preset

The **SolarEdge** battery preset reads battery 1 through the inverter's Modbus
TCP connection, initially port 1502 and unit ID 1. Enable Modbus TCP on the
inverter and adjust these connection values to match its configuration.

| Measurement | Base-0 address | Type | Multiplier | Word swap |
| --- | --- | --- | --- | --- |
| Active power | 57716 (0xE174) | float32 | 1 | Yes |
| State of charge (SOE) | 57732 (0xE184) | float32 | 1 | Yes |
| Operating state | 57734 (0xE186) | uint32 | 1 | Yes |

The addresses and state codes (idle=7, charging=3, discharging=4) follow the
[SolarEdge Power Control Protocol, pages 19–20](https://www.haroldhalewijn.nl/wp-content/uploads/2023/08/Power-Control-Open-Protocol-for-SolarEdge-Inverters.pdf).
The little-word-first decoding follows the
[SolarEdge Modbus battery implementation](https://github.com/nmakel/solaredge_modbus/blob/master/src/solaredge_modbus/__init__.py).
Other state codes retain the existing unknown-state behavior. This preset
requires hardware validation, including the power sign while charging and
discharging; reverse the power multiplier if necessary. The inverter must
expose these battery registers. This reads one battery, not an aggregate of
multiple batteries.

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

The Modbus TCP connector contributes alongside enabled Enphase, SMA and Omniksol
sources to the PV API, dashboard, daily history total and ESP-NOW output.
SolarEdge HTTP support (including the experimental V2 route) and Solar Setup have
been removed. SolarEdge uses the PV Modbus TCP connector; disabling it stops its
contribution without starting any HTTP requests. Old `solaredge.json` files are
ignored and are not deleted.

Both connectors expire live readings after twice the configured interval, with
a minimum grace period of 20 seconds. Connection and mapping settings are saved
in `DSMRsettings.json`. They are preserved by a network reset.

## HTTP PV sources

Enphase, SMA and Omniksol each have an independent enable switch, gateway URL,
credential, installed power (Wp) and polling interval. Supply a full `http://`
or `https://` URL. SMA uses the inverter base URL and its user password;
Enphase uses the Envoy production endpoint and bearer token. Omniksol retains
its existing URL, X-API-Key and response handling. Multiple sources can remain
active together. Configure each physical source once to avoid counting it twice.

**Save** activates changes; **Undo** restores the last loaded/saved values.
Existing `enphase.json`, `sma.json` and `omniksol.json` configurations remain in
use. A missing `enabled` property means enabled for compatibility. Saving adds
an explicit boolean. Disabled sources retain their configuration and do not
contribute power, daily production or installed Wp. Minimum intervals are 60
seconds for Enphase and 15 seconds for SMA/Omniksol.

The authenticated `/api/v2/connectors/solar/{source}` endpoint supports `GET`
and `POST`, where source is `enphase`, `sma` or `omniksol`. POST accepts the
complete configuration: `enabled`, `gateway-url`, `token`, `wp` and
`refresh-interval`. Invalid configurations are rejected before writing. Files
are replaced via a temporary file so a failed write preserves the saved config.
GET returns credentials for editing; responses use `Cache-Control: no-store`.
The Modbus diagnostics below apply to the two Modbus connectors.

## Diagnostics

Diagnostics are **off by default**. Enable **Diagnostics** on the Connectors
page before reproducing a problem; no Save is needed. The switch applies to
both connectors on this dongle, including when another browser opens the page.
It does not change connection settings or enable either connector.

While enabled, the page refreshes every two seconds and shows status, time since
the last successful complete reading, the last error code/message and register,
request/success/error counts, and up to 12 recent results per connector (newest
first). Each result identifies its measurement and register, raw register words,
any SunSpec exponent, and the calculated value or error. The value on a normal
register row includes the manual multiplier; the following SF-register result
includes the SunSpec exponent as well. Out-of-range measurements are rejected
and reported rather than published as valid readings. Connection errors and
Modbus exception codes remain available after communication recovers.

**Copy diagnostics** copies a JSON report with the firmware version, capture
timestamp, saved IP/port/unit ID, polling interval, mapper and recent results.
It uses the saved configuration, not unsaved edits in the form. It does not
include passwords or settings for unrelated integrations. Local HTTP browsers
use a copy fallback; if copying is blocked, the report is selected for manual
copying. Include the inverter model and the test time when sharing the report.

Capture remains active when leaving the page until switched off or the dongle
reboots. No diagnostic records or enable flag are written to flash. Switching
off clears the records; changing a connector configuration starts a fresh capture
for that connector. Closing the page stops browser polling but not Modbus polling.
This is separate from the existing Modbus monitor for requests to the dongle's
own server.

The authenticated `/api/v2/connectors/diagnostics` endpoint supports `GET` for
the current snapshot and `POST` with `{"enabled":true}` or `{"enabled":false}`
to start or clear capture. Error messages preserve the underlying eModbus code;
codes `0x100`–`0x103` describe invalid responses, invalid/unavailable values,
invalid SunSpec exponents and rejected measurements respectively. Ages are in
milliseconds relative to the snapshot, not inverter timestamps.

## Validation

```sh
c++ -std=c++17 -Wall -Wextra -pedantic tests/modbus_connector_test.cpp -o /tmp/modbus_connector_test
/tmp/modbus_connector_test
node tests/test_connectors.cjs
node tests/test_solar_connectors.cjs
python3 tests/test_solar_connector_api.py
node tests/test_connector_diagnostics.cjs
./build.sh ULTRA P1P
```

The host test covers signed and 32-bit decoding, fractional factors, word order,
SunSpec exponents/unavailable values, full-cycle polling, failures, configuration
changes and daily baseline restoration/rollover. Hardware communication still
needs validation against the actual Victron GX and SolarEdge inverter.
