# Slimmemeter MQTT interface - DSMR-API - Hardware versie 2
Gebaseerd op versie 1 met een aantal verbeteringen en aanvullingen
- ESP-M2 in plaats van M3
- gebruikt secundaire seriale interface (sneller en geen extra library meer nodig)
- 2 SMD optocouplers; 1 voor de P1 en een extra voor externe input (bijvoorbeeld deurbel)
- mogelijkheid om ook iets te kunnen doen met I2C ... meer experimenteel
- iets compacter

## SCHEMA
Gekozen voor iets compactere ESP-M2 (ESP8285) vooral omdat de secundaire seriele interface van de ESP bereikbaar is op de M2 en niet op de M3. 
Op het printje zitten de volgende modules:
- signaal inverter om het P1 signaal te inverteren (level shifter)
- optocoupler voor het aansturen van de P1 (DTR : data request)
- optocoupler voor extra input; bv. een deurbel die vaak in de buurt van de slimme meter zit
- spanningsregulator voor het naar 3.3Volt brengen van de P1 spanning.
- de ESP-M2 natuurlijk
- I2C aansluiting mogelijk (experimenteel)

Alle modules samen zie je in het onderstaande schema.
![Kicad schema](hardware/v2-kicad-schema.png) 

In het schema is rekening gehouden met het aansluiten van een deurbel van 12 of 8 Volt AC. De weerstanden zorgen dan voor een juiste werking van de optocoupler. Er kan ook een andere digitale of analoge bron worden aangesloten. Zorg dan even voor de juiste weerstanden. De diode in de optocoupler heeft 1.2V en 20mA typical nodig.

Omgezet naar een board ziet dit er zo uit:
Bovenkant             |  Onderkant |  Eindresultaat
:-------------------------:|:-------------------------:|:-------------------------:
![hardware bovenkant](hardware/v2-print-boven.png)  |  ![hardware onderkant](hardware/v2-print-onder.png) | ![hardware onderkant](hardware/v2-eindresultaat.png)

Afmeting van de print is 19 x 35mm (iets compacter dan de v1)

## SOFTWARE
Er is veel software online te vinden. Keuze voor de gebruiker vind ik belangrijk daarom gekozen voor twee mogelijkheden, namelijk:
- Tasmota : Out of the box (MQTT) [setup/tasmota](setup/tastmota/README.md)
- DSMR API firmware : Json API, MQTT + web user interface (op basis van de Willem AandeWiel oplossing) [setup/dsmr-api](setup/dsmr-api/README.md)

### Tasmota
Zelf ben ik een groot fan van de Tasmota software voor de ESP8266 familie. Deze is dan ook als eerste gebruikt om de oplossing werkend te krijgen. Tasmota kan out of the box op de module geflasht worden (vanaf versie 8.5.0). Instructie is te vinden in de [setup/tasmota](setup/tastmota/README.md) folder.

### REST-API DSMR Logger van Willem AandeWiel
Veel dank aan Willem AandeWiel voor zijn oplossing. Deze oplossing is als basis genomen en diverse veranderingen aan doorgevoerd. Oplossing van Willem is gemaakt voor een 4MB esp module en de M2 heeft er maar 1. Meer dan genoeg als alle extra functionaliteit verwijderd wordt en de overige compacter wordt gemaakt.
Aanpassingen zijn:
- alle statische pagina's komen uit een CDN (esp modules hebben maar een beperkte capaciteit en zijn geen hele goede webservers;)
- alle plaatjes zijn nu iconen geworden, ook van cdn
- files zijn omgezet naar Json zodat dit makkelijk te onderhouden is en compacter wordt
- json API communicatie is ook gewijzigd (compacter en als een burst ipv gesegmenteerd)
- opmaak is zo veel als mogelijk uit de html / js files gehaald en in de css gestopt
- voor de extra input (deurbel) is functionele uitbreiding nodig (work in progress)
Instructie is te vinden in de [setup/dsmr-api](setup/dsmr-api/README.md) folder.

# Hardware maken of aanschaffen
Je kan je eigenhardware maken of deze los / compleet aanschaffen. Wil je deze aanschaffen dan kan je mij altijd een DM sturen. De oplossing is ook Plug-and-play te koop met Tasmota of DSMR-API firmware.