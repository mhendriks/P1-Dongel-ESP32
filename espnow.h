#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR2 "%02x%02x%02x%02x%02x%02x"
#define PEER_NAME_CONTAINS "NRGM35"
#define ESP_NOW_CHUNK_SIZE 232
#define MAX_CHANNEL 13  // 13 in Europe
#ifdef DEBUG
  #define PAIR_TIMEOUT 15000 //10sec timeout
#else
  #define PAIR_TIMEOUT 120000 //120sec timeout
#endif

// const int ESP_NOW_CHUNK_SIZE = 1450; //from SDK 3.3.0

enum MessageType  { COMMAND, CONFIRMED, NRGACTUALS, NRGTARIFS, NRGSTATIC, UPD_DATA, UPD_VER_RSP, UPD_VER_REQ, UPD_ACK, UPD_GO_UPDATE, STROOMPLANNER,} messageType;
enum sAction      { CONN_REQUEST, CONN_RESPONSE, CONN_CLEAR, PAIRING, ASK_TARIF, ASK_STATIC, ASK_PLANNER }; 

typedef struct {
    uint8_t msgType = COMMAND;
    sAction action = CONN_RESPONSE;       // command
    uint8_t channel;
    char host[20];
} __attribute__((packed)) command_t;

typedef struct {
  uint8_t   type = NRGACTUALS;       // PEER_ACTUALS
  time_t    epoch;      // sec from 1/1/1970
  uint32_t  P;          // W
  uint32_t  Pr;         // W
  uint32_t  e_t1;       // Wh
  uint32_t  e_t2;       // Wh
  uint32_t  e_t1r;      // Wh
  uint32_t  e_t2r;      // Wh
  uint32_t  Gas;        // dm3/Liter
  uint32_t  Water;      // dm3/Liter
  uint32_t  Psolar;     // W  
  uint32_t  Esolar;     // Wh  
} ActualData_t;

//all values x 100.000
typedef struct { 
    uint8_t   msgType = NRGTARIFS;
    uint32_t  Efixed;   //per maand in centen
    uint32_t  Et1;      //per kwh in centen
    uint32_t  Et2;      //per kwh in centen
    uint32_t  Et1r;     //terugleververgoeding per kwh in centen
    uint32_t  Et2r;     //terugleververgoeding per kwh in centen
    uint32_t  Gfixed;   //per maand in centen
    uint32_t  G;        //per m3 in centen
    uint32_t  Wfixed;   //per maand in centen
    uint32_t  W;        //per m3 in centen
    uint32_t  Efine;    //terugleverboete per Kwh in centen
} tariff_t;

typedef struct { 
    uint8_t   hour;
    uint8_t   type;
} planner_rec_t;

typedef struct { 
    uint8_t   msgType = STROOMPLANNER;
    planner_rec_t rec[10];   
} planner_t;

struct OTA_VER {
  uint8_t type;           // == MSG_TYPE_VER_REQ
  char version[8];        // bijv. "1.2.3"
  char manifest[10];      // bijv. "main" of "prod"
  char update_url[35];      // bijv. "main" of "prod"
  char file[35];
} client_ota_data;

struct { 
    uint8_t   msgType = NRGSTATIC;
    uint16_t  WpSolar;    //watt peak solar system
    char      ssid[32];   //wifi ssid
    char      pw[63];     //wifi password
} Static;

// Structuur voor ESP-NOW data (moet overeenkomen met de Slave)
typedef struct {
  uint8_t type = UPD_DATA;
  uint32_t totalSize;
  uint32_t currentOffset;
  uint16_t chunkSize;
  uint16_t packetId; // Uniek ID voor elk verzonden pakket
  uint8_t data[ESP_NOW_CHUNK_SIZE];
  bool isLastChunk;
  uint32_t crc32;    // CRC/checksum voor data integriteit
} __attribute__((packed)) esp_now_update_data_t;

// Structuur voor ESP-NOW ACK (van Slave naar Master, moet overeenkomen met de Slave)
typedef struct {
  uint8_t type = UPD_ACK;
  uint16_t packetId;      // Het ID van het ontvangen pakket
  bool success;           // True als het pakket succesvol is ontvangen
  uint32_t currentOffset; // De laatst succesvol ontvangen offset
} __attribute__((packed)) esp_now_ack_data_t;