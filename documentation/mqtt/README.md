# MQTT
Hieronder de verschillende mqtt berichten.
Elk bericht is opgebouwd uit top-topic\topic en payload. Bv <br>

`P1-Dongle\p1_version met payload 50.` <-- slimme meter versie 5.0

Static waardes zijn waardes die niet snel of nooit zullen wijzigen. Daarom zullen deze waardes eens per 10 minuten retained worden verzonden.

LWT wordt tijdens start / reboot aangepast naar On of Off. Ook de LWT is retained.

Afhankelijk van  slimme meter versie en configuratie, bv: wel/geen energie opwekking, 1 of 3 fases etc, worden topics wel of niet verzonden. 

>TIP<br>
>Via MQTTExplorer kan je zien welke topics door de dongle worden verzonden.

## Topic overzicht

| topic                      | type                            | static |
| -------------------------- | ------------------------------- | ------ |
| identification             |   /\* String \*/                | y      |
| p1\_version                |   /\* String \*/                | y      |
| p1\_version\_be            |   /\* String \*/                | y      |
| timestamp                  |   /\* String \*/                |        |
| equipment\_id              |   /\* String \*/                | y      |
| energy\_delivered\_tariff1 |   /\* FixedValue \*/            |        |
| energy\_delivered\_tariff2 |   /\* FixedValue \*/            |        |
| energy\_returned\_tariff1  |   /\* FixedValue \*/            |        |
| energy\_returned\_tariff2  |   /\* FixedValue \*/            |        |
| electricity\_tariff        |   /\* String \*/                |        |
| power\_delivered           |   /\* FixedValue \*/            |        |
| power\_returned            |   /\* FixedValue \*/            |        |
| message\_short             |   /\* String \*/                |        |
| message\_long              |   /\* String \*/                |        |
| voltage\_l1                |   /\* FixedValue \*/            |        |
| voltage\_l2                |   /\* FixedValue \*/            |        |
| voltage\_l3                |   /\* FixedValue \*/            |        |
| current\_l1                |   /\* FixedValue \*/            |        |
| current\_l2                |   /\* FixedValue \*/            |        |
| current\_l3                |   /\* FixedValue \*/            |        |
| power\_delivered\_l1       |   /\* FixedValue \*/            |        |
| power\_delivered\_l2       |   /\* FixedValue \*/            |        |
| power\_delivered\_l3       |   /\* FixedValue \*/            |        |
| power\_returned\_l1        |   /\* FixedValue \*/            |        |
| power\_returned\_l2        |   /\* FixedValue \*/            |        |
| power\_returned\_l3        |   /\* FixedValue \*/            |        |
| mbus1\_device\_type        |   /\* uint16\_t \*/             | y      |
| mbus1\_equipment\_id\_tc   |   /\* String \*/                | y      |
| mbus1\_equipment\_id\_ntc  |   /\* String \*/                | y      |
| mbus1\_valve\_position     |   /\* uint8\_t \*/              |        |
| mbus1\_delivered           |   /\* TimestampedFixedValue \*/ |        |
| mbus1\_delivered\_ntc      |   /\* TimestampedFixedValue \*/ |        |
| mbus1\_delivered\_dbl      |   /\* TimestampedFixedValue \*/ |        |
| mbus2\_device\_type        |   /\* uint16\_t \*/             | y      |
| mbus2\_equipment\_id\_tc   |   /\* String \*/                | y      |
| mbus2\_equipment\_id\_ntc  |   /\* String \*/                | y      |
| mbus2\_valve\_position     |   /\* uint8\_t \*/              |        |
| mbus2\_delivered           |   /\* TimestampedFixedValue \*/ |        |
| mbus2\_delivered\_ntc      |   /\* TimestampedFixedValue \*/ |        |
| mbus2\_delivered\_dbl      |   /\* TimestampedFixedValue \*/ |        |
| mbus3\_device\_type        |   /\* uint16\_t \*/             | y      |
| mbus3\_equipment\_id\_tc   |   /\* String \*/                | y      |
| mbus3\_equipment\_id\_ntc  |   /\* String \*/                | y      |
| mbus3\_valve\_position     |   /\* uint8\_t \*/              |        |
| mbus3\_delivered           |   /\* TimestampedFixedValue \*/ |        |
| mbus3\_delivered\_ntc      |   /\* TimestampedFixedValue \*/ |        |
| mbus3\_delivered\_dbl      |   /\* TimestampedFixedValue \*/ |        |
| mbus4\_device\_type        |   /\* uint16\_t \*/             | y      |
| mbus4\_equipment\_id\_tc   |   /\* String \*/                | y      |
| mbus4\_equipment\_id\_ntc  |   /\* String \*/                | y      |
| mbus4\_valve\_position     |   /\* uint8\_t \*/              |        |
| mbus4\_delivered           |   /\* TimestampedFixedValue \*/ |        |
| mbus4\_delivered\_ntc      |   /\* TimestampedFixedValue \*/ |        |
| mbus4\_delivered\_dbl      |   /\* TimestampedFixedValue \*/ |        |
| gas\_delivered             |   /\* FixedValue \*/            |        |
| gas\_delivered\_timestamp  |   /\* String \*/                |        |
| water\_delivered           |   /\* FixedValue \*/            |        |
| LWT		          		 |   On or Off			           | y      |