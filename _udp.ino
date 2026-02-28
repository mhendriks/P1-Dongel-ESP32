#ifdef UDP_BCAST
#include <AsyncUDP.h>
#include "uECC.h"
#include "mbedtls/sha256.h"

/*
--- TODO
- checken op verschillende slimme meters
- testen met verschillende meters.

*/

/*
  EB External Energy Meter Messages v1.3
  - Binary struct for UDP payload and Modbus register table
  - Little-endian encoding
  - Strings are fixed length, NOT null-terminated
*/

// debuggen op linux/macos ->> socat -u UDP-RECV:11111,reuseaddr - | hexdump -C
// or sudo tcpdump -i en0 -n -s 0 -X udp port 11111

/* ================= ECC ================= */

static uint8_t g_ecPrivKey[32];
static uint8_t g_ecPubKey[64];
static bool    g_ecKeyValid = false;

static constexpr const char* EB_NVS_PUB  = "ecpubkey";   // 64 bytes
static constexpr const char* EB_NVS_PRIV = "ecprivkey";  // 32 bytes

static int eb_ecc_rng(uint8_t* dest, unsigned size) {
  // ESP32 hardware RNG
  for (unsigned i = 0; i < size; i++) { 
    dest[i] = (uint8_t)esp_random();
  }
  return 1; // success
}

static void ebEnsureEccRng() {
  static bool inited = false;
  if (!inited) {
    uECC_set_rng(&eb_ecc_rng);
    inited = true;
  }
}

static void ebSha256(const void* data, size_t len, uint8_t out32[32]) {
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);

  // 0 = SHA-256 (not 224)
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const uint8_t*)data, len);
  mbedtls_sha256_finish(&ctx, out32);

  mbedtls_sha256_free(&ctx);
}

static const uint8_t* ebGetPrivKey32()
{
  return g_ecKeyValid ? g_ecPrivKey : nullptr;
}

static bool ebSign32(uint8_t sig64[64], const uint8_t hash32[32]) {
  const uint8_t* priv = ebGetPrivKey32();   // jouw bestaande getter uit NVS load
  if (!priv) return false;
  ebEnsureEccRng();
  return uECC_sign(priv, hash32, 32, sig64, uECC_secp256r1()) == 1;
}

static bool ebSignRange(uint8_t sig64[64], const void* start, const void* endExclusive)
{
  const uint8_t* s = (const uint8_t*)start;
  const uint8_t* e = (const uint8_t*)endExclusive;
  if (!s || !e || e <= s) return false;

  uint8_t h[32];
  ebSha256(s, (size_t)(e - s), h);
  return ebSign32(sig64, h);
}

static bool ebKeysSanity(const uint8_t* priv32, const uint8_t* pub64)
{
  // simpele sanity: niet all-00/all-FF
  auto sane = [](const uint8_t* b, size_t n){
    bool all0=true, allFF=true;
    for (size_t i=0;i<n;i++){ all0 &= (b[i]==0); allFF &= (b[i]==0xFF); }
    return !(all0 || allFF);
  };
  return sane(priv32, 32) && sane(pub64, 64);
}

static bool ebLoadOrCreateKeypair()
{
  g_ecKeyValid = false;

  // 1) LOAD
  // if (!g_ebPrefs.begin(EB_NVS_NS, /*readOnly=*/true)) {
  //   Debugln("EB/NVS: begin(ro) FAILED");
  //   return false;
  // }

  size_t lpriv = preferences.getBytesLength(EB_NVS_PRIV);
  size_t lpub  = preferences.getBytesLength(EB_NVS_PUB);
  Debugf("EB/NVS: len priv=%u pub=%u\n", (unsigned)lpriv, (unsigned)lpub);

  bool havePriv = (lpriv == 32) && (preferences.getBytes(EB_NVS_PRIV, g_ecPrivKey, 32) == 32);
  bool havePub  = (lpub  == 64) && (preferences.getBytes(EB_NVS_PUB,  g_ecPubKey,  64) == 64);
  // preferences.end();

  Debugf("EB/NVS: read priv=%u pub=%u\n", havePriv, havePub);

  if (havePriv && havePub && ebKeysSanity(g_ecPrivKey, g_ecPubKey)) {
    Debugln("EB: keypair loaded OK");
    g_ecKeyValid = true;
    return true;
  }

  // 2) GENERATE
  Debugln("EB: keypair missing/invalid -> generating");
  const struct uECC_Curve_t* curve = uECC_secp256r1();

  ebEnsureEccRng();
  #ifdef UDP_TEST
    Debugf("esp_random test: %08lX\n", (unsigned long)esp_random());
  #endif
  int mk = uECC_make_key(g_ecPubKey, g_ecPrivKey, curve);
  Debugf("EB: uECC_make_key=%d\n", mk);

  if (!mk) {
    Debugln("EB: make_key FAILED (likely RNG)");
    return false;
  }

  // 3) STORE
  // if (!g_ebPrefs.begin(EB_NVS_NS, /*readOnly=*/false)) {
  //   Debugln("EB/NVS: begin(rw) FAILED");
  //   return false;
  // }

  size_t wpriv = preferences.putBytes(EB_NVS_PRIV, g_ecPrivKey, 32);
  size_t wpub  = preferences.putBytes(EB_NVS_PUB,  g_ecPubKey,  64);
  // preferences.end();

  Debugf("EB/NVS: wrote priv=%u pub=%u\n", (unsigned)wpriv, (unsigned)wpub);

  if (wpriv != 32 || wpub != 64) {
    Debugln("EB: store FAILED");
    return false;
  }

  g_ecKeyValid = true;
  Debugln("EB: keypair generated+stored OK");
  return true;
} // END LOAD or create


static const uint8_t* ebGetPubKey64()
{
  return g_ecKeyValid ? g_ecPubKey : nullptr;
}

// ----


static void writeFixedString(char* dst, size_t dstLen, const String& src) {
  // Strings are not null-terminated (spec 4.3.2) :contentReference[oaicite:38]{index=38}
  memset(dst, 0, dstLen);
  size_t n = min(dstLen, (size_t)src.length());
  memcpy(dst, src.c_str(), n);
}

static void ebInitStatic(EB_Payload& p,
                         const String& devname,
                         const String& manufacturer,
                         const String& model,
                         const String& sn,
                         uint32_t hwrev,
                         const String& fwrev,
                         const uint8_t pubkey64[64]) {
  memset(&p, 0, sizeof(p));

  p.gh.magic   = MAGIC_EBEM;
  p.gh.version = EB_VERSION;

  p.st.di.magic   = MAGIC_EBDI;
  p.st.di.version = EB_SUBVER;

  if (pubkey64) memcpy(p.st.ecpubkey, pubkey64, 64);

  writeFixedString(p.st.devname,       sizeof(p.st.devname),       devname);
  writeFixedString(p.st.manufacturer,  sizeof(p.st.manufacturer),  manufacturer);
  writeFixedString(p.st.model,         sizeof(p.st.model),         model);
  writeFixedString(p.st.sn,            sizeof(p.st.sn),            sn);
  p.st.hwrev = hwrev;
  writeFixedString(p.st.fwrev,         sizeof(p.st.fwrev),         fwrev);

  if (!ebSignRange(p.st.ecdsa_sec,
                  &p.st.ecpubkey[0],
                  (const uint8_t*)p.st.ecpubkey + sizeof(p.st.ecpubkey))) {
    Debugln("EB: ecdsa_sec sign FAILED");
  }

  if (!ebSignRange(p.st.ecdsa_dev,
                  &p.st.devname[0],
                  (const uint8_t*)p.st.fwrev + sizeof(p.st.fwrev))) {
    Debugln("EB: ecdsa_dev sign FAILED");
  }
}

static void ebInitDynamicHeader(EB_Payload& p) {
  p.dy.md.magic   = MAGIC_EBMD;
  p.dy.md.version = EB_SUBVER;
}

// fill with dynamic data
static void ebUpdateDynamic(EB_Payload& p, uint64_t epochSeconds) {
  p.dy.seqno++;
  p.dy.timestamp  = epochSeconds;
  
  // ---- power ----
  if (DSMRdata.power_delivered_l1_present || DSMRdata.power_returned_l1_present) {
    int32_t del = DSMRdata.power_delivered_l1_present ? DSMRdata.power_delivered_l1.int_val() * 1000: 0;
    int32_t ret = DSMRdata.power_returned_l1_present  ? DSMRdata.power_returned_l1.int_val() * 1000: 0;
    p.dy.pactive1 = del - ret;
  }

  if (DSMRdata.power_delivered_l2_present || DSMRdata.power_returned_l2_present) {
    int32_t del = DSMRdata.power_delivered_l2_present ? DSMRdata.power_delivered_l2.int_val() * 1000: 0;
    int32_t ret = DSMRdata.power_returned_l2_present  ? DSMRdata.power_returned_l2.int_val() * 1000: 0;
    p.dy.pactive2 = del - ret;
  }

  if (DSMRdata.power_delivered_l3_present || DSMRdata.power_returned_l3_present) {
    int32_t del = DSMRdata.power_delivered_l3_present ? DSMRdata.power_delivered_l3.int_val() * 1000: 0;
    int32_t ret = DSMRdata.power_returned_l3_present  ? DSMRdata.power_returned_l3.int_val() * 1000: 0;
    p.dy.pactive3 = del - ret;
  }

  if (DSMRdata.power_delivered_present || DSMRdata.power_returned_present) {
    int32_t del = DSMRdata.power_delivered_present ? DSMRdata.power_delivered.int_val() * 1000 : 0;
    int32_t ret = DSMRdata.power_returned_present  ? DSMRdata.power_returned.int_val() * 1000 : 0;
    p.dy.pactiveTot = del - ret;
  }

  // ---- Energy counters ----
  if (DSMRdata.energy_delivered_tariff1_present) p.dy.energyPos  =  DSMRdata.energy_delivered_tariff1.int_val();
  if (DSMRdata.energy_delivered_tariff2_present) p.dy.energyPos  += DSMRdata.energy_delivered_tariff2.int_val();
  if (DSMRdata.energy_returned_tariff1_present)  p.dy.energyNeg  =  DSMRdata.energy_returned_tariff1.int_val();
  if (DSMRdata.energy_returned_tariff2_present)  p.dy.energyNeg  += DSMRdata.energy_returned_tariff2.int_val();

  // ---- voltage ----
  if (DSMRdata.voltage_l1_present) p.dy.vrms1 = DSMRdata.voltage_l1.int_val();
  if (DSMRdata.voltage_l2_present) p.dy.vrms2 = DSMRdata.voltage_l2.int_val();
  if (DSMRdata.voltage_l3_present) p.dy.vrms3 = DSMRdata.voltage_l3.int_val();
  
  // ---- current ----
  if (DSMRdata.current_l1_present) p.dy.i1 = (p.dy.pactive1<0? -1 : 1) * (int32_t)DSMRdata.current_l1.int_val();
  if (DSMRdata.current_l2_present) p.dy.i2 = (p.dy.pactive2<0? -1 : 1) * (int32_t)DSMRdata.current_l2.int_val();
  if (DSMRdata.current_l3_present) p.dy.i3 = (p.dy.pactive3<0? -1 : 1) * (int32_t)DSMRdata.current_l3.int_val();
  p.dy.in = p.dy.i1 + p.dy.i2 + p.dy.i3;

  if (!ebSignRange(p.dy.ecdsa_md, &p.dy.seqno, (const uint8_t*)&p.dy.energyNeg + sizeof(p.dy.energyNeg))) {
    Debugln("EB: ecdsa_md sign FAILED");
  }
}

/* ================= UDP broadcast sender ================= */

static AsyncUDP g_udp;
static IPAddress g_bcast(255, 255, 255, 255);
static bool g_udpCryptoReady = false;

static bool ebUdpStart() {
  // AsyncUDP: connect to broadcast endpoint
  if (!g_udp.connect(g_bcast, EB_UDP_PORT)) return false;
  return true;
}

static bool ebUdpSend(const EB_Payload& p) {
  if ( netw_state == NW_NONE ) return false;
  if (!g_udp.connected()) {
    if (!ebUdpStart()) return false;
  }
  if ( (telegramCount - telegramErrors) <= 3 || DUE(FullBroadCast) ) return g_udp.write((const uint8_t*)&p, sizeof(p)) == sizeof(p);
  else return g_udp.write((const uint8_t*)&p.dy, sizeof(p.dy)) == sizeof(p.dy);
}

EB_Payload g_payload;

void UdpBegin() {

  g_udpCryptoReady = false;
  bool keyOk = ebLoadOrCreateKeypair();
  if (!keyOk) {
    LogFile("UDP disabled: ECC keypair init failed", true);
    return;
  }

  const uint8_t* pk = ebGetPubKey64();
  Debugf("ECPUBKEY loaded: %s\n", pk ? "YES" : "NO");
  if (!pk) {
    LogFile("UDP disabled: ECC public key missing", true);
    return;
  }

  if (pk) {
    Debug("PUBKEY first 8 bytes: ");
    for (int i = 0; i < 8; i++) {
      Debugf("%02X", g_ecPubKey[i]);
    }
    Debugln();
  }
  
  ebInitStatic(g_payload,
               settingHostname,
               "Smartstuff",
               hwTypeNameSafe(HardwareType),
               macID,
               HardwareVersion,
               _VERSION_ONLY,
               pk);

  ebInitDynamicHeader(g_payload);
  g_udpCryptoReady = true;
}

void ebUdpSetStatc(){
  // --- static meter data 
/* 
  unavailable capabilities in smr 2/4/5
  CAP_PAPPAR
  CAP_PREACT  
  CAP_ANGLE 
  CAP_FREQ 
  CAP_PAPPARTOT
  CAP_PREACTTOT
  CAP_POWFACT
*/
  g_payload.dy.capability = (DSMRdata.voltage_l1_present?CAP_VRMS:0) | (DSMRdata.current_l1_present?CAP_I:0) | CAP_PACTIVE | CAP_PACTIVETOT | CAP_ENERGYPOS | CAP_ENERGYNEG; // cf. spec 3.3 voorbeeld :contentReference[oaicite:44]{index=44}
  g_payload.dy.phasemask = (DSMRdata.voltage_l2_present && !DSMRdata.voltage_l2?PH_N:0) | (DSMRdata.current_l1_present?PH_L1:0 ) | (DSMRdata.current_l2_present?PH_L2:0) | (DSMRdata.current_l3_present?PH_L3:0);
  uint8_t phases = (DSMRdata.voltage_l1_present?1:0) + (DSMRdata.voltage_l2_present?1:0) + (DSMRdata.voltage_l3_present?1:0);
  EBTopology topo = (EBTopology) phases;
  if ( phases == 3 && DSMRdata.voltage_l2.int_val() < 1000 ) topo = TOPO_DELTA;
  g_payload.dy.topology = topo;
}

void handleUDP() {
  static bool udpCheckOnce = false;

  if (!New_P1_UDP || !bUDPenabled || !g_udpCryptoReady ) return;

  DebugTln("UDP -- PUSH");
  New_P1_UDP = false;
  
  if ( !udpCheckOnce ) { 
    udpCheckOnce = true;
    ebUdpSetStatc();
  }
  
  ebUpdateDynamic(g_payload, actT);
  ebUdpSend(g_payload);

  #ifdef UDP_TEST
  dumpJson(g_payload);
  dumpHex(&g_payload, sizeof(g_payload)); // exact bytes
  ebVerifyAllOncePerMinute(g_payload);
  #endif

}

#ifdef UDP_TEST

#define EB_JSON_FULL_HEX 1   // 0 = short, 1 = full 64 bytes

static void dumpHex(const void* data, size_t len) {
  const uint8_t* b = (const uint8_t*)data;
  for (size_t i = 0; i < len; i++) {
    if ((i % 16) == 0) {
      Debugf("\n%04u: ", (unsigned)i);
    }
    Debugf("%02X ", b[i]);
  }
  Debugln();
}

static void printHexBytes(const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; i++) Debugf("%02X", b[i]);
}

static void printHexShort64(const uint8_t b[64]) {
  #if EB_JSON_FULL_HEX
    printHexBytes(b, 64);
  #else
    printHexBytes(b, 8); Debug("...");
  #endif
}

static void jsonEscapedFixedString(const char* s, size_t n) {
  // fixed-len (not null-terminated): trim trailing 0/space, then JSON-escape minimal set
  size_t end = n;
  while (end > 0 && (s[end-1] == '\0' || s[end-1] == ' ')) end--;

  Debug("\"");
  for (size_t i = 0; i < end; i++) {
    char c = s[i];
    if (c == '\\' || c == '"') { Debug("\\"); Debugf("%c", c); }
    else if (c == '\n') Debug("\\n");
    else if (c == '\r') Debug("\\r");
    else if (c == '\t') Debug("\\t");
    else Debugf("%c", c);
  }
  Debug("\"");
}

static void jsonMagic4(uint32_t magic) {
  Debug("\"");
  Debugf("%c%c%c%c",
    (char)(magic & 0xFF),
    (char)((magic >> 8) & 0xFF),
    (char)((magic >> 16) & 0xFF),
    (char)((magic >> 24) & 0xFF));
  Debug("\"");
}

static void dumpJson(const EB_Payload& p) {
  Debugln("{");

  // ---- gh ----
  Debugln("  \"gh\":{");
  Debug("    \"magic\":"); jsonMagic4(p.gh.magic); Debugln(",");
  Debugf("    \"version\":%u\n", (unsigned)p.gh.version);
  Debugln("  },");

  // ---- st ----
  Debugln("  \"st\":{");

  // subheader di
  Debugln("    \"di\":{");
  Debug("      \"magic\":"); jsonMagic4(p.st.di.magic); Debugln(",");
  Debugf("      \"version\":%u\n", (unsigned)p.st.di.version);
  Debugln("    },");

  // pubkey + signatures (short)
  Debug("    \"ecpubkey64\":\"");
  #if EB_JSON_FULL_HEX
    printHexBytes(p.st.ecpubkey, 64);
  #else
    printHexBytes(p.st.ecpubkey, 8); Debug("...");
  #endif
  Debugln("\",");

  Debug("    \"ecdsa_sec64\":\""); printHexShort64(p.st.ecdsa_sec); Debugln("\",");
  Debug("    \"devname\":");        jsonEscapedFixedString(p.st.devname, sizeof(p.st.devname)); Debugln(",");
  Debug("    \"manufacturer\":");   jsonEscapedFixedString(p.st.manufacturer, sizeof(p.st.manufacturer)); Debugln(",");
  Debug("    \"model\":");          jsonEscapedFixedString(p.st.model, sizeof(p.st.model)); Debugln(",");
  Debug("    \"sn\":");             jsonEscapedFixedString(p.st.sn, sizeof(p.st.sn)); Debugln(",");
  Debugf("    \"hwrev\":%u,\n", (unsigned)p.st.hwrev);
  Debug("    \"fwrev\":");          jsonEscapedFixedString(p.st.fwrev, sizeof(p.st.fwrev)); Debugln(",");
  Debug("    \"ecdsa_dev64\":\"");  printHexShort64(p.st.ecdsa_dev); Debugln("\"");

  Debugln("  },");

  // ---- dy ----
  Debugln("  \"dy\":{");

  // subheader md
  Debugln("    \"md\":{");
  Debug("      \"magic\":"); jsonMagic4(p.dy.md.magic); Debugln(",");
  Debugf("      \"version\":%u\n", (unsigned)p.dy.md.version);
  Debugln("    },");

  Debugf("    \"seqno\":%lu,\n", (unsigned long)p.dy.seqno);
  Debugf("    \"capability\":%lu,\n", (unsigned long)p.dy.capability);
  Debugf("    \"timestamp\":%llu,\n", (unsigned long long)p.dy.timestamp);

  Debugf("    \"topology\":%u,\n", (unsigned)p.dy.topology);
  Debugf("    \"phasemask\":%u,\n", (unsigned)p.dy.phasemask);

  Debugf("    \"vrms_mV\":[%lu,%lu,%lu],\n",
    (unsigned long)p.dy.vrms1, (unsigned long)p.dy.vrms2, (unsigned long)p.dy.vrms3);

  Debugf("    \"i_mA\":[%ld,%ld,%ld,%ld],\n",
    (long)p.dy.i1, (long)p.dy.i2, (long)p.dy.i3, (long)p.dy.in);

  // alle overige velden uit EB_DynamicBlock (ook als 0)
  Debugf("    \"pappar_mW\":[%ld,%ld,%ld],\n",
    (long)p.dy.pappar1, (long)p.dy.pappar2, (long)p.dy.pappar3);

  Debugf("    \"pactive_mW\":[%ld,%ld,%ld],\n",
    (long)p.dy.pactive1, (long)p.dy.pactive2, (long)p.dy.pactive3);

  Debugf("    \"preact_mW\":[%ld,%ld,%ld],\n",
    (long)p.dy.preact1, (long)p.dy.preact2, (long)p.dy.preact3);

  Debugf("    \"angle_mdeg\":[%ld,%ld,%ld],\n",
    (long)p.dy.angle1, (long)p.dy.angle2, (long)p.dy.angle3);

  Debugf("    \"freq_mHz\":%u,\n", (unsigned)p.dy.freq);
  Debugf("    \"powfact_1e-3\":%u,\n", (unsigned)p.dy.powfact);

  Debugf("    \"papparTot_mW\":%ld,\n", (long)p.dy.papparTot);
  Debugf("    \"pactiveTot_mW\":%ld,\n", (long)p.dy.pactiveTot);
  Debugf("    \"preactTot_mW\":%ld,\n", (long)p.dy.preactTot);

  Debugf("    \"energyPos_Wh\":%llu,\n", (unsigned long long)p.dy.energyPos);
  Debugf("    \"energyNeg_Wh\":%llu,\n", (unsigned long long)p.dy.energyNeg);

  Debug("    \"ecdsa_md64\":\""); printHexShort64(p.dy.ecdsa_md); Debugln("\"");

  Debugln("  }");

  Debugln("}");
}

static bool ebVerifyRange(const uint8_t pubkey64[64],
                          const uint8_t sig64[64],
                          const void* start,
                          const void* endExclusive)
{
  const uint8_t* s = (const uint8_t*)start;
  const uint8_t* e = (const uint8_t*)endExclusive;
  if (!s || !e || e <= s) return false;

  uint8_t h[32];
  ebSha256(s, (size_t)(e - s), h);

  // uECC_verify verwacht hash32 als "message"
  return uECC_verify(pubkey64, h, 32, sig64, uECC_secp256r1()) == 1;
}

static void ebDumpSecCompatOnce(const EB_Payload& p)
{
  static bool dumped = false;
  if (dumped) return;
  dumped = true;

  uint8_t hashPubKey[32];
  ebSha256(&p.st.ecpubkey[0], sizeof(p.st.ecpubkey), hashPubKey);

  bool okSec = ebVerifyRange(p.st.ecpubkey, p.st.ecdsa_sec,
                            &p.st.ecpubkey[0],
                            (const uint8_t*)p.st.ecpubkey + sizeof(p.st.ecpubkey));

  Debugln("EB_COMPAT_SEC: begin");
  Debug("  pubkey64="); printHexBytes(p.st.ecpubkey, sizeof(p.st.ecpubkey)); Debugln("");
  Debug("  hash32 =");  printHexBytes(hashPubKey, sizeof(hashPubKey));       Debugln("");
  Debug("  sig64  =");  printHexBytes(p.st.ecdsa_sec, sizeof(p.st.ecdsa_sec)); Debugln("");
  Debugf("  verify_local=%s\n", okSec ? "OK" : "FAIL");
  Debugln("EB_COMPAT_SEC: end");
}

static void ebVerifyAllOncePerMinute(const EB_Payload& p)
{
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < 60000UL) return;
  lastMs = now;

  ebDumpSecCompatOnce(p);

  bool okSec = ebVerifyRange(p.st.ecpubkey, p.st.ecdsa_sec,
                            &p.st.ecpubkey[0],
                            (const uint8_t*)p.st.ecpubkey + sizeof(p.st.ecpubkey));

  bool okDev = ebVerifyRange(p.st.ecpubkey, p.st.ecdsa_dev,
                            &p.st.devname[0],
                            (const uint8_t*)p.st.fwrev + sizeof(p.st.fwrev));

  bool okMd  = ebVerifyRange(p.st.ecpubkey, p.dy.ecdsa_md,
                            &p.dy.seqno,
                            (const uint8_t*)&p.dy.energyNeg + sizeof(p.dy.energyNeg));

  Debugf("EB_VERIFY: sec=%s dev=%s md=%s\n",
         okSec ? "OK" : "FAIL",
         okDev ? "OK" : "FAIL",
         okMd  ? "OK" : "FAIL");
}


#endif

#else
  void handleUDP(){}
  void UdpBegin(){}
#endif
