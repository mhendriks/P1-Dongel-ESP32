# OpenHAB integratie
Hieronder twee voorbeelden voor de integratie met Tasmota en DSMR Api oplossing.
Files tref je in deze folder aan.

## Onderstaande voorbeelden zijn voor beide integraties
In file mqtt.things voeg je onderstaande toe aan je mqtt bridge.

```
	// P1 - slimmemeter P1 API implementatie
     Thing topic slimmemeter "Slimmemeter" @ "Hal" {
    Channels:
		Type string : time	"update tijd"	[ stateTopic="DSMR-API/timestamp", transformationPattern="JSONPATH:$.timestamp.[0].value" ]	
        Type number : T1     "kWh Teller 1" [ stateTopic="DSMR-API/energy_delivered_tariff1", transformationPattern="JSONPATH:$.energy_delivered_tariff1.[0].value" ]
        Type number : T2     "kWh Teller 2" [ stateTopic="DSMR-API/energy_delivered_tariff2", transformationPattern="JSONPATH:$.energy_delivered_tariff2.[0].value" ]
        Type number : P     "W"             [ stateTopic="DSMR-API/power_delivered", transformationPattern="JSONPATH:$.power_delivered.[0].value" ]   
        Type number : U     "V"        		[ stateTopic="DSMR-API/voltage_l1", transformationPattern="JSONPATH:$.voltage_l1.[0].value" ]
        Type number : I     "A"        		[ stateTopic="DSMR-API/current_l1", transformationPattern="JSONPATH:$.current_l1.[0].value" ]
        Type number : G     "m3"            [ stateTopic="DSMR-API/gas_delivered", transformationPattern="JSONPATH:$.gas_delivered.[0].value" ]     
    }
    
// P1 - slimmemeter Tasmota implementatie
     Thing topic slimmemeter_tas "Slimmemeter" @ "Hal" {
    Channels:
		Type string : p1	"receive data"	[ stateTopic="tele/tasmota_C3C439/RESULT", transformationPattern="JSONPATH:SSerialReceived" ]	
    }
  
``` 
    
in de rules folder maak je een nieuw bestand aan p1-dsmr.rules met onderstaande code. 
```
// Alleen nodig voor de Tasmota
rule "P2 P"
when
    Item P2 received update
then
	if (P2.state == NULL || P2 == "") return;
    var rawString = P2.state.toString
	//P1_I 		1-0:31.7.0(002*A)
	val rawString3 = transform("REGEX", ".*([0-9]{3}).A.*", rawString)
	if ( rawString3 != NULL && rawString3 !== null && rawString3 != "") { 
		P1_I.postUpdate(rawString3) 
		} 
	//P1_T1		1-0:1.8.1(000013.000*kWh) (Totaal geleverd tarief 1 (nacht))
	 rawString3 = transform("REGEX", ".*1.8.1.([0-9]{6}.[0-9]{3}).kWh.*", rawString)
	if ( rawString3 != NULL && rawString3 !== null && rawString3 != "") { 
		P1_T1.postUpdate(rawString3) 
		} 
	//P1_T2 	1-0:1.8.2(000084.000*kWh) (Totaal verbruik tarief 2 (dag))
	 rawString3 = transform("REGEX", ".*1.8.2.([0-9]{6}.[0-9]{3}).kWh.*", rawString)
	if ( rawString3 != NULL && rawString3 !== null && rawString3 != "") { 
		P1_T2.postUpdate(rawString3) 
		} 
	//P2_GAS 	0-1:24.2.1(191222151005W)(02728.449*m3)
	rawString3 = transform("REGEX", ".*[0-9]{12}.*([0-9]{5}.[0-9]{3}).m3.*", rawString)
	if ( rawString3 != NULL && rawString3 !== null && rawString3 != "") { 
		P1_GAS.postUpdate(rawString3) 
		} 
	// P2_P 		1-0:21.7.0(00.523*kW)
	rawString3 = transform("REGEX", ".*21.7.0.([0-9]{2}.[0-9]{3}).kW.*", rawString)
	if ( rawString3 != NULL && rawString3 !== null && rawString3 != "") { 
		P1_P.postUpdate(rawString3) 
		} 
		//time: 0-0:1.0.0(200716222227S)
	rawString3 = transform("REGEX", ".*1.0.0.([0-9]{12})S.*", rawString)
	if ( rawString3 != NULL && rawString3 !== null && rawString3 != "") { 
		P1_time.postUpdate(rawString3) 
		} 
end

//Voor Tasmota en API 
rule "P1 last update" //convert time to normal readable time
when
    Item P1_time received update
then
	if (P1_time.state == NULL || P1_time == "") return;
    var rawString = P1_time.state.toString
	val res = String::format("%s-%s-%s %s:%s:%s", rawString.substring(4, 6), rawString.substring(2, 4), rawString.substring(0, 2),rawString.substring(6, 8), rawString.substring(8, 10),rawString.substring(10, 12))
	if ( res != NULL && res !== null && res != "") { 
		P1_tijd.postUpdate(res) 
		} 
end   
``` 

In het .items bestand voeg je de waardes toe die je wilt zien. Hieronder een voorbeeld (dit voorbeeld is voor tasmota + standalone implementatie geschikt).
```
Number P1_P "Actueel vermogen [%s]"	 {channel="mqtt:topic:mosquitto:slimmemeter:P" } 
Number P1_GAS "Gasmeter: [%.1f]"	 {channel="mqtt:topic:mosquitto:slimmemeter:G" }
Number P1_T1 "kWh Teller 1: [%s]"	 {channel="mqtt:topic:mosquitto:slimmemeter:T1" } 
Number P1_T2 "kWh Teller 1: [%s]"	 {channel="mqtt:topic:mosquitto:slimmemeter:T2" } 
Number P1_I "Stroom: [%s]"			 {channel="mqtt:topic:mosquitto:slimmemeter:I" }
Number P1_U "Spanning: [%s]"		 {channel="mqtt:topic:mosquitto:slimmemeter:U" } 
String P1_time "tijd: [%s]" 		 {channel="mqtt:topic:mosquitto:slimmemeter:time" } //dit is de systeemtijd uit de slimmemeter
String P1_tijd "tijd: [%s]"

String P2 "tasmota only [%s]"				 {channel="mqtt:topic:mosquitto:slimmemeter_tas:p1" } 
```

In het .sitemap bestand laat je de waardes uit .items terugkomen
```
Frame label="Slimmemeter" {
    Text item=P1_T1 label="kWh Teller Dal: [%.1f kWh]"
    Text item=P1_T2 label="kWh Teller Normaal: [%.1f kWh]"
    Text item=P1_P label="Actuele vermogen: [%.3f kW]"
    Text item=P1_I label="Actuele stroom: [%.0f A]"
    Text item=P1_U label="Actuele Spanning: [%.1f V]"
    Text item=P1_GAS label="Gasmeter: [%.1f m3]"
   	Text item=P1_tijd label="Laatste P1 update: [%s]"
   	Text item=P1_time label="Laatste P1 update: [%s]" 
   	}
```


