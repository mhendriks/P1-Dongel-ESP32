# Setup Tasmota voor de P1 interface
Er zijn een aantal stappen nodig om Tasmota op de p1 adapter te laten draaien.
- Stap 1: Tasmota flashen (of zo geleverd krijgen)
- Stap 2: Tasmota configuratie instellen
- Stap 3: Domotica oplossing instellen (bv. OpenHAB, HomeAssistant, ...)

## 1 Tasmota flashen
Vanaf de Tasmota 8.5.0 versie is het mogelijk om out of the box deze firmware te gebruiken. Koop de P1 adapter met Tasmota geinstalleerd of flash deze zelf op de hardware. Voor het flashen is een USB naar serial converter nodig.

## 2 Tasmota configuratie instellen
Prik de P1 adapter hierna in de slimme meter en wacht totdat je de tasmota-... adapter in je wifi netwerk ziet.
Klik op dit netwerk en wacht totdat je het instellingenscherm van tasmota ziet om de adapter aan het netwerk te koppelen.
Zoek de P1 module in het netwerk op.
In de configuratie van Tasmota geef geef je aan dat je de configuratie wilt terugzetten (Restore).
Gebruik het Config_tasmota_C3C439_1081_8.4.0.3.dmp bestand in deze folder.

## 3 Domotica oplossing instellen 
Stel de mqtt host in op de adapter.
Daarna dien je in je domotica oplossing het ontvangen van de berichten in te richten. Hieronder de voorbeelden voor OpenHAB en HomeAssistant.

## Klaar
Nu is de adapter voorzien van Tasmota firmware en is mqtt geconfigureerd. Volgende stap is de domotica integratie inrichten. In de folder [integratie](../../integratie) tref je een voorbeeld aan van een [OpenHab](../../integratie/openhab/README.md) en [Home Assistant](../../integratie/home_assistant/README.md) integratie.