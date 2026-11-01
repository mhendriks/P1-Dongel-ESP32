# Stroomwaarden en `try_calc_i`

`DSMRdata.current_l1/l2/l3` blijven de oorspronkelijke telegramwaarden.
`GetTelegramCurrent(phase)` geeft die bron terug, in milliampères en met een
presence-vlag. Deze functie negeert `try_calc_i`.

`GetMeterCurrent(phase)` is de centrale afgeleide/gebruikswaarde:

- `try_calc_i = false`: oorspronkelijke meterstroom.
- `try_calc_i = true`: berekende stroom per fase uit fasevermogen en spanning.
- Kan een fase niet worden berekend (ontbrekende/ongeldige spanning of geen
  niet-nul fasevermogen), dan blijft de meterstroom de terugvalwaarde.
- Ontbreekt ook die meterstroom, dan is de stroom afwezig, niet nul.

De berekening gebruikt `(afname_W + teruglevering_W) / spanning_V` als positieve
stroomgrootte. Dit is stroom op basis van actief vermogen; de berekening gebruikt
geen arbeidsfactor. Richting wordt waar nodig apart bepaald. Beide bronnen
blijven beschikbaar; het vinkje kiest bij het uitlezen. CT-schaling wordt pas
bij de uitvoer toegepast.

| Uitgang / gebruik | Stroombron |
| --- | --- |
| Dashboard, actuals, REST fields en WebSocket actuals | `GetMeterCurrent` |
| MQTT, Home Assistant en Victron-MQTT | `GetMeterCurrent` |
| Modbus TCP/RTU: `current_l1_a/l2_a/l3_a`, totaal en signed stroom | `GetMeterCurrent` |
| Shelly-emulatie, inclusief berekeningen die deze stroom gebruiken | `GetMeterCurrent` |
| UDP | `GetMeterCurrent`, verzonden als gehele milliampères |
| Stroompieken | `GetMeterCurrent` op het moment van registreren |
| Ruw telegram / diagnostische brondata | Oorspronkelijk telegram |

Alle Modbus-mappings volgen het vinkje voor fasestromen, totale stroom en
signed stroom. Er is geen aparte Modbus-bron die deze instelling omzeilt.

Een wijziging van het vinkje herberekent eerder geregistreerde stroompieken
niet. Die blijven de piek van de destijds geselecteerde waarde.
