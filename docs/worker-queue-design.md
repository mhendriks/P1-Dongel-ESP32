# Worker Queue Ontwerp (voor later)

Doel: niet-kritische taken uit de hoofdloop halen zodat P1/Modbus/bridging voorspelbaar blijven.

Status: ontwerpnotitie, nog niet toepassen in de huidige release-tak.

## Scope (huidige inschatting)

Naar worker(s):
- UDP verzenden (niet bridge-kritisch pad)
- Opslaan gegevens (FS/NVS/history/logs)
- Ophalen solar data
- Ophalen accu data
- NetSwitch aansturing (met name HTTP calls)
- MQTT publish/subscribe verwerking (callbacks kort houden)
- EnergieID afhandeling
- NRG Monitor / posts

In proces houden (direct/realtime pad):
- P1 inlezen
- P1 bridging
- Modbus clients
- API ingress (later AsyncWebServer)

## Ontwerpprincipes

- Realtime pad mag niet wachten op netwerk/FS.
- Background taken krijgen timeouts/retries/backoff.
- Queue payloads zijn klein en vast van vorm (geen grote `String`/JSON blobs).
- Voor write/send taken een snapshot meesturen als correcte data belangrijk is.

Belangrijk: `enum + payload id` is niet altijd genoeg.
Voorbeeld dagovergang naar storage: 6 x `uint32_t` moet als snapshot mee in de job zodat later schrijven exact dezelfde waarden gebruikt.

## Queue payload model (aanbevolen)

Gebruik een vaste `struct Job` met type + metadata + union payload.

Voorbeeld (schematisch):

```cpp
enum class JobType : uint8_t {
  StorageDayClose,
  SolarFetch,
  AccuFetch,
  UdpSend,
  MqttPublishSnapshot,
  EnergyIdPush,
  NrgMonitorPush,
  NetSwitchToggle
};

struct DayClosePayload {
  uint32_t v[6];
};

struct Job {
  JobType type;
  uint8_t priority;      // zie prioriteitssectie
  uint32_t createdAt;    // uptime/timestamp
  union {
    DayClosePayload dayClose;
    // andere vaste payloads
  } data;
};
```

Richtlijn:
- Wel in queue: kleine snapshots (`uint32_t`, `float`, flags, korte vaste arrays).
- Niet in queue: grote JSON strings, dynamische `String`, grote buffers.
- Grote payload nodig: pointer/handle naar pre-allocated buffer/ringbuffer + ownershipregels.

## Worker-indeling (praktisch)

Niet alles in 1 worker stoppen als jobs kunnen blokkeren.

Aanbevolen fasering:
- `ioWorker`: HTTP/MQTT/posts/UDP sends
- `storageWorker`: LittleFS/NVS/history/logs (serialiseert writes)
- optioneel later `pollWorker`: periodieke fetches (solar/accu)

Voordeel:
- Een trage HTTP call blokkeert geen file writes.
- Minder lock/race-complexiteit.

## Prioriteit: kan de workerqueue prioriteit krijgen?

Ja, op 2 niveaus:

1. Task-prioriteit (FreeRTOS task priority)
- Je kunt de worker-task(s) zelf een prioriteit geven via `xTaskCreate(..., priority, ...)`.
- Dit bepaalt wanneer de worker CPU krijgt t.o.v. andere tasks.
- Richtlijn: lager dan P1 reader, meestal lager of gelijk aan MQTT, afhankelijk van latency-eisen.

2. Job-prioriteit (welke queued job eerst)
- FreeRTOS `xQueue` is FIFO en heeft geen ingebouwde job-prioriteit.
- Prioriteit voor jobs moet je zelf modelleren.

Praktische opties voor job-prioriteit:

### Optie A (aanbevolen): meerdere queues per prioriteit

Bijv.:
- `qHigh` (kritische background jobs)
- `qNormal`
- `qLow`

Worker leest altijd in volgorde `High -> Normal -> Low`.

Voordelen:
- Simpel, voorspelbaar, weinig CPU-overhead
- Goed te debuggen

Nadelen:
- Kans op starvation van low-prio als high-prio continu volloopt

Oplossing:
- budget/fairness (bijv. max 5 high jobs achter elkaar, dan 1 normal/low proberen)

### Optie B: 1 queue + sorted insert (custom)

Zelf prioriteit sorteren in een buffer/heap/list.

Voordelen:
- Flexibeler

Nadelen:
- Meer code, meer foutkans, extra allocatie/locking-risico op ESP32

Conclusie:
- Start met meerdere FIFO queues per prioriteit.

## Prioriteitstoewijzing (voorstel)

High:
- NetSwitch toggle (reactiegedrag)
- MQTT publish van actuele status (indien nodig voor HA/automatisering)
- UDP send (als near-realtime gewenst)

Normal:
- EnergyID push
- NRG Monitor push
- Solar/accu fetch result processing
- reguliere storage writes

Low:
- bulk/history writes
- retries van externe endpoints
- housekeeping/cleanup

## Backpressure / foutafhandeling

- Queue vol:
  - drop of dedupe telemetriejobs (latest-wins)
  - nooit realtime pad blokkeren
- Retries:
  - exponential backoff voor HTTP/cloud
- Timeouts:
  - altijd op netwerkcalls
- Dedupe:
  - voorkom meerdere gelijke `SolarFetch` jobs tegelijk

## Migratieplan (na release/push)

1. Basis toevoegen: `Worker` + `Job` + `ioWorker` + queue(s)
2. Eerst verplaatsen:
   - EnergyID
   - NRG Monitor / posts
   - solar/accu fetch triggers
3. Daarna MQTT publish pad
4. Daarna storage writes
5. P1/Modbus pas aanpassen als nodig (waarschijnlijk niet)

## Acceptatiecriteria (eerste implementatie)

- Geen functieverlies in bestaande features
- P1 verwerking blijft stabiel onder netwerkvertraging
- Geen corrupte/onjuiste dagovergang-data (snapshot payload gebruikt)
- Queue overflow wordt gelogd en faalt gecontroleerd
