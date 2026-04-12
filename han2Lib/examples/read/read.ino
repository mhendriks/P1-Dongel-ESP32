#include <han2.h>

HanReader reader(&Serial1);
HanData data;
String err;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
}

void loop() {
  reader.loop();
  if (!reader.available()) return;

  if (reader.parse(&data, &err)) {
    Serial.print("List type: ");
    Serial.println((int)data.list_type);
    if (data.timestamp.present()) {
      Serial.print("Timestamp: ");
      Serial.println(data.timestamp.val());
    }
    if (data.power_delivered_kw.present()) {
      Serial.print("Power delivered kW: ");
      Serial.println(data.power_delivered_kw.val(), 3);
    }
    if (data.energy_delivered_total_kwh.present()) {
      Serial.print("Energy delivered total kWh: ");
      Serial.println(data.energy_delivered_total_kwh.val(), 3);
    }
  } else {
    Serial.print("Parse error: ");
    Serial.println(err);
  }
}
