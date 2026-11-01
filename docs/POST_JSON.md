# Generic HTTP POST provisioning

HTTP POST is part of every 5.10.0 build and is inactive by default. Put a
`post.json` file on the dongle, then restart it. A valid file is copied to NVS
and removed from the filesystem, so API keys are not left in LittleFS.

```json
{
  "enabled": true,
  "provider": "generic",
  "url": "https://example.invalid/measurements",
  "interval": 300,
  "payload": 3,
  "auth": 1,
  "auth_key": "replace-me",
  "auth_name": "Authorization",
  "extra_header_name": "X-MAC-Address",
  "extra_header_value": "{mac}",
  "accept_interval": false
}
```

`provider` is `generic`, `kemp`, `meent`, or `powerc`. `payload` is `0` for the base
message, `1` for totals, `2` for voltage and sag/swell counters, and `3` for
all fields. `auth` is `0` (none), `1` (Bearer), `2` (named header), or `3`
(JSON field). `{mac}` in the additional-header value is replaced by the
dongle MAC ID.

Production firmware accepts HTTPS endpoints only. A `DEBUG` build additionally
accepts `http://` so a local mock server can be used during development.

For KEMP select `"provider": "kemp"`; its endpoint must use HTTPS and its
server response contract is documented in `KEMP_API.md`. Selecting MEENT in
the settings page retains its provisioning screens and writes MEENT's data
endpoint and credentials to the same NVS connector.

## PowerC (5.10.5)

Provision the standard firmware with this file at `/post.json` in LittleFS:

```json
{
  "enabled": true,
  "provider": "powerc",
  "url": "https://example.invalid/measurements",
  "interval": 300,
  "payload": 0,
  "auth": 0,
  "accept_interval": false
}
```

The interval, payload and authentication above are examples; use the agreed
server settings. PowerC uses the generic POST transport and payload,
including the Chip-ID in `id`; it does not use KEMP's command protocol.

Startup imports the file before starting networking, stores it in NVS and
removes the file. Subsequent boots restore the same provider from NVS.
Existing KEMP/MEENT/generic NVS records retain their provider values.

With PowerC enabled:

- Both the WiFi hotspot SSID and password are `p1-hotspot`.
- The normal HTTP/REST and browser WebSocket servers and their mDNS
  advertisements are disabled, except in `DEBUG` firmware.
- The captive portal stays available for WiFi setup at `4.3.2.1`.
- A five-to-fifteen-second button press performs the same network reset as every other profile:
  clear WiFi/cached AP credentials, disable `skip-network`, remove fixed-IP
  settings and reboot. The settings document and POST provisioning survive.
- Holding the button for more than fifteen seconds gives red blinking feedback.
  Releasing then erases settings and POST provisioning and reboots to defaults.
  Upload the customer settings and `post.json` again to re-enable the profile.
- LED feedback follows the active network and latest POST result: no usable
  WiFi/Ethernet connection or a failed request gives steady red; HTTP 404
  gives red blinking at 500 ms on / 500 ms off. A successful 2xx response
  restores normal blue/green activity. Before the first response, normal
  network/activity feedback applies. Button feedback has priority over errors,
  and `LED: false` still turns the LED off. These indications also apply in
  `DEBUG` firmware; other providers keep their existing LED behavior.

Provisioning `"enabled": false` or selecting a different provider restores
normal hotspot and local-server behavior at the next boot. The physical
button reset thresholds are the same for every provider.
No PowerC compiler option is required. Flashing a firmware sketch alone
does not provision files; include the LittleFS filesystem image as well.
