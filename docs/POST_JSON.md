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

`provider` is `generic`, `kemp`, or `meent`. `payload` is `0` for the base
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
