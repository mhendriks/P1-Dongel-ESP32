#pragma once

// #define UDP_TEST

static constexpr uint16_t EB_UDP_PORT = 11111;   
static constexpr uint16_t EB_VERSION  = 1;
static constexpr uint16_t EB_SUBVER   = 1;

static constexpr uint32_t MAGIC_EBEM = 0x4D454245u; // 'E''B''E''M' in LE memory
static constexpr uint32_t MAGIC_EBDI = 0x49444245u; // 'E''B''D''I'
static constexpr uint32_t MAGIC_EBMD = 0x444D4245u; // 'E''B''M''D'

/* ---------------- Capability bits (spec 2 + 3.3) :contentReference[oaicite:8]{index=8} ---------------- */
enum EBCapability : uint32_t {
  CAP_VRMS       = (1u << 0),
  CAP_I          = (1u << 1),
  CAP_PAPPAR     = (1u << 2),
  CAP_PACTIVE    = (1u << 3),
  CAP_PREACT     = (1u << 4),
  CAP_ANGLE      = (1u << 5),
  CAP_FREQ       = (1u << 6),
  CAP_PAPPARTOT  = (1u << 7),
  CAP_PACTIVETOT = (1u << 8),
  CAP_PREACTTOT  = (1u << 9),
  CAP_POWFACT    = (1u << 10),
  CAP_ENERGYPOS  = (1u << 11),
  CAP_ENERGYNEG  = (1u << 12),
};

/* ---------------- Topology enum (spec 3.4) :contentReference[oaicite:9]{index=9} ---------------- */
enum EBTopology : uint16_t {
  TOPO_UNDEF = 0,
  TOPO_MONO  = 1,
  TOPO_DELTA = 2,
  TOPO_STAR  = 3,
};

/* ---------------- Phase mask bits (spec 3.5) :contentReference[oaicite:10]{index=10} ---------------- */
enum EBPhaseMask : uint16_t {
  PH_L1 = (1u << 0),
  PH_L2 = (1u << 1),
  PH_L3 = (1u << 2),
  PH_N  = (1u << 3),
};

#if defined(__GNUC__)
  #define PACKED __attribute__((packed))
#else
  #define PACKED
#endif

/* ========== Building blocks ========== */

struct PACKED EB_GlobalHeader {
  uint32_t magic;    // "EBEM" :contentReference[oaicite:11]{index=11}
  uint16_t version;  // 1 :contentReference[oaicite:12]{index=12}
};

struct PACKED EB_SubHeader {
  uint32_t magic;    // "EBDI" or "EBMD" :contentReference[oaicite:13]{index=13}
  uint16_t version;  // 1 :contentReference[oaicite:14]{index=14}
};

struct PACKED EB_StaticBlock {
  EB_SubHeader di;         // Device Info subframe header
  uint8_t  ecpubkey[64];   // P-256 pubkey (uncompressed w/o format byte) :contentReference[oaicite:15]{index=15}
  uint8_t  ecdsa_sec[64];  // signature of previous data (Security info) :contentReference[oaicite:16]{index=16}

  char     devname[32];
  char     manufacturer[32];
  char     model[32];
  char     sn[32];
  uint32_t hwrev;          // uint32 in table :contentReference[oaicite:17]{index=17}
  char     fwrev[16];

  uint8_t  ecdsa_dev[64];  // signature DEVNAME..FWREV :contentReference[oaicite:18]{index=18}
};

struct PACKED EB_DynamicBlock {
  EB_SubHeader md;       // Meter Data subframe header
  uint32_t seqno;        // increments each frame :contentReference[oaicite:19]{index=19}
  uint32_t capability;   // bitfield :contentReference[oaicite:20]{index=20}
  uint64_t timestamp;    // epoch seconds :contentReference[oaicite:21]{index=21}

  uint16_t topology;     // enum (0..3) :contentReference[oaicite:22]{index=22}
  uint16_t phasemask;    // bitfield :contentReference[oaicite:23]{index=23}

  uint32_t vrms1, vrms2, vrms3;  // mV :contentReference[oaicite:24]{index=24}
  int32_t  i1, i2, i3, in;       // mA signed :contentReference[oaicite:25]{index=25}

  int32_t  pappar1, pappar2, pappar3;   // mW :contentReference[oaicite:26]{index=26}
  int32_t  pactive1, pactive2, pactive3; // mW :contentReference[oaicite:27]{index=27}
  int32_t  preact1, preact2, preact3;   // mW :contentReference[oaicite:28]{index=28}

  int32_t  angle1, angle2, angle3;      // mdegree :contentReference[oaicite:29]{index=29}
  uint16_t freq;                         // mHz :contentReference[oaicite:30]{index=30}
  uint16_t powfact;                      // 10e-3 :contentReference[oaicite:31]{index=31}

  int32_t  papparTot;                    // mW :contentReference[oaicite:32]{index=32}
  int32_t  pactiveTot;                   // mW :contentReference[oaicite:33]{index=33}
  int32_t  preactTot;                    // mW :contentReference[oaicite:34]{index=34}

  uint64_t energyPos;                    // Wh :contentReference[oaicite:35]{index=35}
  uint64_t energyNeg;                    // Wh :contentReference[oaicite:36]{index=36}

  uint8_t  ecdsa_md[64];                 // signature of meter data :contentReference[oaicite:37]{index=37}
};

/*
  TOTAL payload = header + static + dynamic
  -> Dit is exact wat je vroeg: totale struct “gemaakt door samenvoegen”.
*/
struct PACKED EB_Payload {
  EB_GlobalHeader gh;
  EB_StaticBlock  st;
  EB_DynamicBlock dy;
};

static_assert(sizeof(EB_Payload) == 550, "EB_Payload size must be 550 bytes (v1.3).");

#ifdef UDP_TEST
  DECLARE_TIMER_SEC(FullBroadCast, 20);
#else
  DECLARE_TIMER_SEC(FullBroadCast, 300);
#endif
