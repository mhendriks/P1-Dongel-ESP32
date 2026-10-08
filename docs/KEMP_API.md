# KEMP HTTP API contract

This contract applies when `/post.json` selects `"provider": "kemp"`. The
dongle initiates every HTTP request and sends JSON to the configured endpoint.

## Sample message

Samples are sent at the active `interval_s`. The server replies to every
sample. A normal reply is HTTP 200 with the desired `interval_s`; the dongle
uses that value for its next scheduling decision. HTTP 401 or any other
non-200 response does not execute a device command.

```json
{
  "message_type": "sample",
  "id": "123456789",
  "api_key": "...",
  "p_from_grid": 420,
  "p_to_grid": 0,
  "t1": 123456,
  "t2": 234567,
  "t1r": 3456,
  "t2r": 4567,
  "v_l1": 2314,
  "v_l2": 2311,
  "v_l3": 2308,
  "voltage_sag_l1_count": 5,
  "voltage_sag_l2_count": 6,
  "voltage_sag_l3_count": 7,
  "voltage_swell_l1_count": 8,
  "voltage_swell_l2_count": 9,
  "voltage_swell_l3_count": 10,
  "timestamp": 1790000000
}
```

Meter fields unavailable in the incoming P1 telegram are omitted. The server
may include one command in the HTTP 200 body:

```json
{
  "interval_s": 60,
  "command": "REQconfig",
  "command_id": "c-1001"
}
```

`interval` is accepted as a temporary backwards-compatible alias for
`interval_s`.

## Config message

Config messages are independent POSTs to the same endpoint. They are sent on
startup and as the acknowledgement of a command. A config response only needs
to be HTTP 200 and must not contain a new command.

```json
{
  "message_type": "config",
  "id": "123456789",
  "api_key": "...",
  "firmware_version": "5.10.1",
  "hardware": "NRGD",
  "smart_meter": "...",
  "uptime_s": 12,
  "reboot_count": 8,
  "last_reset_reason": "ESP_RST_SW",
  "p1_count": 111,
  "p1_error": 1,
  "interval_s": 60
}
```

An acknowledgement adds `ack`, `command_id`, `success`, and `msg` to this
message.

## Commands

| Command | Server fields | Config acknowledgement | Device action after HTTP 200 |
| --- | --- | --- | --- |
| `REQconfig` | `command_id` | `configuration sent` | None. |
| `APIupdate` | `command_id`, `api_key_new` | `api-key verified`, authenticated with the candidate key | Persist the candidate key. |
| `OTAupdate` | `command_id`, `firmware_version` | `ota update started`; or `success: false` when unavailable | Download and install the requested firmware. |
| `URLupdate` | `command_id`, `url_new` | `upload-url verified`, posted to the candidate URL | Persist the candidate HTTPS URL. |
| `REBOOT` | `command_id` | `rebooting` | Restart the device. |

The first startup config after an OTA or reboot is the final confirmation: it
reports the running firmware, boot counter, uptime, and reset reason.
