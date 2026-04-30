    /* 
    todo
      - apart proces van maken
      - UI: check expire claimcode + url (frontend)
    */

    #if __has_include("./../../_secrets/energyid.h")
      #include "./../../_secrets/energyid.h"
    #else
      // Optional EnergyID provisioning secrets. Empty defaults keep compilation working.
      #ifndef EID_PROV_URL
        #define EID_PROV_URL ""
      #endif
      #ifndef EID_PROF_KEY
        #define EID_PROF_KEY ""
      #endif
      #ifndef EID_PROF_SECR
        #define EID_PROF_SECR ""
      #endif
    #endif

    String  eid_webhook;
    String  eid_header_auth;
    String  eid_header_twinid;
    String  recordNumber;
    String  apiAccessToken;
    String  EIDDirectiveID;

    bool bGotDirective = false;
    bool bGetPlannerDetails = false;

    uint32_t eid_interval_sec = 300;

#ifdef DEBUG
  #define EID_IDLE_TIME 15 //sec
  #define EID_CLAIM_RETRY 20*1 //sec
  #define EID_REFRESH_TIME 1*60
#else
  #define EID_IDLE_TIME 3600
  #define EID_CLAIM_RETRY 60*1
  #define EID_REFRESH_TIME 24*60
#endif

    DECLARE_TIMER_SEC(T_EID_IDLE, EID_IDLE_TIME); //idle timeout
    DECLARE_TIMER_SEC(T_EID, 10); // eid throttling
    DECLARE_TIMER_SEC(T_EID_CLAIM, EID_CLAIM_RETRY); 
    DECLARE_TIMER_MIN(T_EID_REFRESH, EID_REFRESH_TIME);
    DECLARE_TIMER_MIN(T_EID_PLAN, 60); //refresh planner evry hour
    DECLARE_TIMER_SEC(T_EID_HELLO_FAIL, 1);

    static inline bool isEIDNetworkAvailable() {
      return ( !skipNetwork
            && ( (netw_state == NW_ETH)
              || (netw_state == NW_WIFI && WiFi.status() == WL_CONNECTED && !bNoNetworkConn) ) );
    }

  void EID_RESTART_IDLE_TIMER(){
    RESTART_TIMER(T_EID_IDLE);
  }

    void EIDStart(){
      if ( bEID_enabled ) EIDPostHello();
    }

    void clearEIDPlannerData(bool forceupdate){
      EIDDirectiveID = "";
      bGetPlannerDetails = false;
      bGotDirective = false;
      if ( StroomPlanData.size() > 0 || forceupdate ) { StroomPlanData.clear();PSPUpdatePlanner();}
    }

    void handleEnergyID(){  

       if ( !bEID_enabled || !isEIDNetworkAvailable() ) {
        clearEIDPlannerData(false);
        return;
      }

      //detect state change
      static uint8_t last_state = 255;
      if (last_state != P1Status.eid_state) {
        last_state = P1Status.eid_state;
        if (last_state == EID_IDLE) RESTART_TIMER(T_EID_IDLE);
        if (last_state == EID_CLAIMING) RESTART_TIMER(T_EID_CLAIM);
        if (last_state == EID_ENROLLED) RESTART_TIMER(T_EID);
        String log = "EID: State change -> " + String(P1Status.eid_state);
        LogFile( log.c_str(),true );
        P1StatusWrite(); //write state change
      }

      switch ( P1Status.eid_state ) {
        case EID_ENROLLED:
            if ( !eid_webhook.length() ) { if ( DUE(T_EID_HELLO_FAIL) ) EIDPostHello(); } //get the webhook url
            else if ( DUE(T_EID) )  { //is enrolled and got webhook url
              PostEnergyID();
            }
            if ( DUE(T_EID_REFRESH) ) EIDPostHello(); //refresh every 24h
            if ( recordNumber != "" && apiAccessToken != "" && !bGotDirective ) EIDGetDirectives(); //4.16 feature
            if ( bGetPlannerDetails ) EIDGetDirectDetails();
            if ( DUE(T_EID_PLAN) ) bGetPlannerDetails = true;
          break;
        case EID_CLAIMING:
          if ( DUE(T_EID_CLAIM) ) {
            DebugTln("EID_CLAIMING");
            EIDPostHello(); //refresh every 1m
          }
          break;
        case EID_IDLE:
          if ( DUE(T_EID_IDLE) ) { 
            //inden na 3600 sec nog geen status verandering dan disable
            bEID_enabled = false;
            writeSettings(); //write state change
            LogFile( "EID - auto turn off", true );
            return;
          }
          break;
      } //switch
    } //handleEnergyID()

    /*
    curl --location 'https://hooks.energyid.eu/hello' \
    --header 'X-Provisioning-Key: *********** \
    --header 'X-Provisioning-Secret: ************************' \
    --header 'Content-Type: application/json' \

    --data '{
        "claimCode": "",
        "deviceId": "p1-dongle-pro-12345678",
        "deviceName": "P1 Dongle Pro",
        "firmwareVersion": "4.8.6",
        "ipAddress": "127.0.0.1",
        "macAddress": "34.AC.57.12",
        "localDeviceUrl": "http://p1-dongle-pro.local"
    }'

    response 

    {
        "claimCode": "5363a3e6-a6b2-4f6b-97d9-xxxxxxxxxx",
        "claimUrl": "https://app.energyid.eu/integrations/8b20ade6-679c-4df1-ba6a-f36d2f4b0702/new?code=5363a3e6-a6b2-4f6b-97d9-xxxxxxxxxx",
        "exp": 1695822974

    }
    */

    String buildProvisioningPayload(){
      String _hostname = String(settingHostname);
      _hostname.toLowerCase();

      String json_data = "{";
      json_data += "\"deviceId\": \"p1-dongle-pro-" + String(_getChipId()) + "\",";
      json_data += "\"deviceName\": \"" + String(settingHostname) + "\",";
      json_data += "\"firmwareVersion\": \"" _VERSION_ONLY "\",";
      json_data += "\"ipAddress\": \"" + String(IP_Address()) + "\",";
      json_data += "\"macAddress\": \"" + String(macStr) + "\",";
      json_data += "\"localDeviceUrl\": \"http://" + _hostname + ".local\"";
      json_data += "}";

      DebugT(F("Json data: ")); Debugln(json_data);
      return json_data;
    }

    // {"webhookUrl":"https://sbns-energyid-prod.servicebus.windows.net/sbq-smartstuff-p1/messages","headers":{"authorization":"<gone>","x-twin-id":"<>"},"recordNumber":"EA-14195189","recordName":"Mijn woning","webhookPolicy":{"allowedInterval":"PT5M","uploadInterval":300}}

    void EIDPostHello(ApiResponse* response) {
      if (!isEIDNetworkAvailable()) {
        if (response) *response = {503, "application/json", ""};
        return;
      }

      /*
        States:
        0 - IDLE: Enabled maar nog niet in claiming fase
        1 - CLAIMING: /hello opgevraagd maar nog niet geclaimed
        2 - ENROLLED: gekoppeld
      */

      HTTPClient http;
      JsonDocument doc;
      String payload;

      DebugT(F("Post URL: ")); Debugln(EID_PROV_URL);
      
      // HTTP setup
      http.begin(EID_PROV_URL);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-Provisioning-Key", EID_PROF_KEY);
      http.addHeader("X-Provisioning-Secret", EID_PROF_SECR);

      int httpResponseCode = http.POST( buildProvisioningPayload() );
      Debug(F("httpResponseCode: ")); Debugln(httpResponseCode);

      if (httpResponseCode == 200) {
        payload = http.getString();
        Debug(F("response body: ")); Debugln(payload);

        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          DebugTln(F("JSON parsing failed in claim response"));
          if (response) *response = {400, "application/json", ""};
        } else {
          const char* claimCode = doc["claimCode"];

          if (claimCode) {
            Debugln(F("ClaimCode obtained"));
            P1Status.eid_state = EID_CLAIMING;
            // Toekomstig: eid_claim_code = claimCode;
            // eventueel claimUrl, exp ook loggen
          } else {
            Debugln(F("Webhook obtained"));

            eid_webhook       = doc["webhookUrl"] | "";
            eid_header_auth   = doc["headers"]["authorization"] | "";
            eid_header_twinid = doc["headers"]["x-twin-id"] | "";
            eid_interval_sec  = doc["webhookPolicy"]["uploadInterval"] | 0;

            if (doc["recordNumber"].is<const char*>())
              recordNumber = doc["recordNumber"].as<const char*>();
            if (doc["apiAccessToken"].is<const char*>()) {
              apiAccessToken = doc["apiAccessToken"].as<const char*>();
              bGotDirective = false;
            } else {
              apiAccessToken = "";
              clearEIDPlannerData(true);
            }

            Debug(F("webhookUrl      : ")); Debugln(eid_webhook);
            Debug(F("authorization   : ")); Debugln(eid_header_auth);
            Debug(F("x-twin-id       : ")); Debugln(eid_header_twinid);
            Debug(F("eid_interval_sec: ")); Debugln(eid_interval_sec);
            Debug(F("recordNumber    : ")); Debugln(recordNumber);
            Debug(F("apiAccessToken  : ")); Debugln(apiAccessToken);

            P1Status.eid_state = EID_ENROLLED;
          }

          if (response) *response = {200, "application/json", payload};
        }
      } else {
        Debugln(F("HTTP request failed or invalid response"));
        eid_webhook = "";
        RESTART_TIMER(T_EID_HELLO_FAIL);
        if (response) *response = {400, "application/json", ""};
      }
      
      static bool firstHelloDone = false;
      if (!firstHelloDone) {
        CHANGE_INTERVAL_SEC(T_EID_HELLO_FAIL, 60);  // na eerste hello
        firstHelloDone = true;
      }

      http.end();
    }

    void EIDGetDirectives(){
      if (!isEIDNetworkAvailable()) return;

      String ApiURL = "https://api.energyid.eu/api/v1/records/"+recordNumber+"/directives";
      DebugT(F("ApiURL:"));Debugln(ApiURL);
      
      HTTPClient http;
      http.begin( ApiURL );
      http.addHeader("Authorization", "device " + apiAccessToken);

      int httpResponseCode = http.GET();
      Debug(F("httpResponseCode: "));Debugln(httpResponseCode);
      String payload = http.getString();
      #ifdef DEBUG
        Debug(F("response body: "));Debugln(payload); 
      #endif
      if ( httpResponseCode == 200 ) { 
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) DebugTln(F("JSON parsing failed in EIDGetDirectives"));
        else {
          const char* directiveId = doc[0]["id"].is<const char*>() ? doc[0]["id"].as<const char*>() : nullptr;
          if (directiveId && directiveId[0] != '\0') {
            EIDDirectiveID = directiveId;
            DebugTf("Directive ID: %s", EIDDirectiveID.c_str());
            bGetPlannerDetails = true;
          }
          else DebugTln(F("No directive id found in response"));
        }
      }  
      http.end();
      bGotDirective = true;

    }

    void EIDGetDirectDetails(){
      if (!isEIDNetworkAvailable()) return;
      
      if ( EIDDirectiveID.length() == 0 )  { DebugTln("No ID present"); bGetPlannerDetails = false; return; }

      String ApiURL = "https://api.energyid.eu/api/v1/records/"+recordNumber+"/directives/"+EIDDirectiveID+"?limit=14&offset=0";
      DebugT(F("ApiURL:"));Debugln(ApiURL);
      
      HTTPClient http;
      http.begin( ApiURL );
      http.addHeader("Authorization", "device " + apiAccessToken);

      int httpResponseCode = http.GET();
      Debug(F("httpResponseCode: "));Debugln(httpResponseCode);
      String payload = http.getString();
      #ifdef DEBUG
        Debug(F("response body: "));Debugln(payload); 
      #endif
      if ( httpResponseCode == 200 ) {
        // EIDDirectiveID = "";
        bGetPlannerDetails = false;
        StroomPlanData.clear();
        DeserializationError error = deserializeJson(StroomPlanData, payload);
        if (error) DebugTln(F("JSON parsing failed in EIDGetDirectDetails"));
        else {
          JsonArray dataArray = StroomPlanData["data"].as<JsonArray>();
          size_t plannerCount = dataArray.size();
          Debugf("EID planner records parsed: %u\n", (unsigned)plannerCount);
          int maxRecords = plannerCount < 14 ? plannerCount : 14;
          for (int i = 0; i < maxRecords; i++) {
            const char* signal = dataArray[i]["signal"] | "<missing>";
            Debugf("signal [%i] : ",i);Debugln(signal);
          }
          #ifdef DEBUG
            ApiResponse planner = JsonEIDplanner();
            Debugln(planner.body);
          #endif
          PSPUpdatePlanner(); //push update to display
        }
      }
      else DebugTln(F("Failed to refresh EID planner data"));
      http.end();
    }

    ApiResponse EIDGetClaimApiResponse(const String& action){

      if ( !bEID_enabled ) return {503, "application/json", ""};
      //get server parameters
      if ( action == "reset" ) P1Status.eid_state = EID_IDLE;
      ApiResponse response = {500, "application/json", ""};
      EIDPostHello(&response);
      return response;
    }

    String IsoTS () {
      // convert to this format -> 2023-06-16T08:01+0200
      
      String tmpTS = String(actTimestamp);
      String DateTime = "";
      DateTime   = "20" + tmpTS.substring(0, 2);    // YY
      DateTime  += "-"  + tmpTS.substring(2, 4);    // MM
      DateTime  += "-"  + tmpTS.substring(4, 6);    // DD
      DateTime  += "T";
      DateTime  += tmpTS.substring(6, 8);    // HH
      DateTime  += ":"  + tmpTS.substring(8, 10);   // MM
      DateTime  += ":"  + tmpTS.substring(10, 12);  // SS
      DateTime  += tmpTS[12]=='S' ? "+0200" : "+0100";
      return DateTime;
    }

    String JsonBuildMBus(String key, time_t epoch, long value ){
    /*  
        {
            "ts": 1695887917,
            "g": 123456.001
        },
    */
      String Json;
      Json  = ",{\"ts\":" + String(epoch);
      Json += ",\"" + key + "\":" + String(value);
      Json += "}";
      
      Debug("Mbus Json : "); Debugln(Json);
      
      return Json;
    }

    void PostEnergyID(){
    /*
    [
        {
            "ts": 1695887917,
            "t1": 3206.454,
            "t2": 3856.175,
            "t1-i": 0,
            "t2-i": 0,
            "pwr": 0.464,
          "pwr-i": 0,
        },
        {
            "ts": 1695887917,
            "g": 123456.001
        },
        {
            "ts": 1695887917,
            "w": 123456.001
        },
        {
            "ts":1695887917,
            "high-pp": 123456.001
        }
    ]

    */
      if ( !isEIDNetworkAvailable() || eid_webhook.length() == 0 || !telegramCount ) return;
      String payload;
      int httpResponseCode;
      String Json = "[{";
      
      HTTPClient http;
    //  snprintf( cMsg, sizeof( cMsg ), "%s%s",EID_BASE_URL, eid_token.c_str() );  
      http.begin( eid_webhook );
      http.addHeader("Content-Type", "application/json");
      http.addHeader("authorization", eid_header_auth);
      http.addHeader("x-twin-id", eid_header_twinid);

      DebugT(F("Post URL:"));Debugln(eid_webhook);
      uint16_t utc_comp = 3600;
      if ( actTimestamp[12] == 'S') utc_comp = 7200;
      
      Json += "\"ts\":" + String( actT - utc_comp );
      
      if ( DSMRdata.energy_delivered_tariff1_present ) {
        Json += ",\"t1\":" + String((int)(DSMRdata.energy_delivered_tariff1*1000.0));
      }
      
      if ( DSMRdata.energy_delivered_tariff2_present ) {
        Json += ",\"t2\":" + String((int)(DSMRdata.energy_delivered_tariff2*1000.0));
      }

      if ( DSMRdata.energy_returned_tariff1_present ) {
        Json += ",\"t1-i\":" + String((int)(DSMRdata.energy_returned_tariff1*1000.0));
      }

      if ( DSMRdata.energy_returned_tariff2_present ) {
        Json += ",\"t2-i\":" + String((int)(DSMRdata.energy_returned_tariff2*1000.0));
      }
      if ( DSMRdata.power_delivered_present ) {
        Json += ",\"pwr\":" + String((int)(DSMRdata.power_delivered*1000.0));
      }
      if ( DSMRdata.power_returned_present ) {
        Json += ",\"pwr-i\":" + String((int)(DSMRdata.power_returned*1000.0));
      }
      
      Json += "}";
      
      if ( mbusGas ) {
        Json += JsonBuildMBus("g", epoch(gasDeliveredTimestamp.c_str(), 10, false) - utc_comp, (int)(gasDelivered*1000.0));
      }

      if ( WtrMtr ) {
        Json += JsonBuildMBus("w", epoch(waterDeliveredTimestamp.c_str(), 10, false) - utc_comp, (int)(waterDelivered*1000.0));
      }

      if ( DSMRdata.highest_peak_pwr_present ) {
        Json += JsonBuildMBus("high-pp", epoch(DSMRdata.highest_peak_pwr.timestamp.c_str(), 10, false), 0);   
      } 
      
      Json += "]";
      
      Debug("Json payload: "); Debugln(Json);

      httpResponseCode = http.POST(Json);
      Debug(F("httpResponseCode: "));Debugln(httpResponseCode);
      payload = http.getString();
      Debug(F("response body: "));Debugln(payload); 
        if ( httpResponseCode != 201 ) { 
          DebugTln(F("Response Error"));
          eid_webhook = "";
          RESTART_TIMER(T_EID_HELLO_FAIL);
        }
      CHANGE_INTERVAL_SEC(T_EID, eid_interval_sec);
      http.end();
    }
