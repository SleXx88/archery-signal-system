#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#define BROADCAST_CHANNEL 1
#define TX_POWER 30
#define REGISTRATION_CODE 0xABCDEF12
#define MSG_TYPE_REG_REQ   1
#define MSG_TYPE_REG_ACK   2
#define MSG_TYPE_STATUS    50
#define MSG_TYPE_COMMAND   60

#define MAX_SLAVES 4
#define SLAVE_TIMEOUT_MS 10000
#define REG_BROADCAST_INTERVAL_MS 5000
#define STATUS_CHECK_INTERVAL_MS 3000
#define SEND_RETRIES 3
#define SEND_DELAY_MS 10

// --- Slaves ---
uint8_t knownSlaves[MAX_SLAVES][6];
unsigned long lastSeen[MAX_SLAVES];
int slaveCount = 0;

// --- Nextion ---
#define NEXTION_SERIAL Serial2
#define START_BYTE 0xEF
#define STOP_BYTE  0xFF
#define PACKET_SIZE 10
#define TIMEOUT_MS 50

uint8_t spielerpaar = 0, ampelfarbe = 0;
uint16_t countdown = 0;
bool newCommandAvailable = false;

// --- Pakete ---
typedef struct __attribute__((packed)) {
  uint8_t  msgType;
  uint32_t regCode;
} RegRequest;

typedef struct __attribute__((packed)) {
  uint8_t  msgType;
  uint32_t regCode;
} RegAck;

typedef struct __attribute__((packed)) {
  uint8_t  msgType;
  uint32_t regCode;
  float    battery1;
  float    battery2;
  bool     isOnBattery;
} SlaveStatus;

typedef struct __attribute__((packed)) {
  uint8_t  msgType;
  uint32_t regCode;
  uint8_t  spielerpaar;
  uint8_t  ampelfarbe;
  uint16_t countdown;
} CommandMsg;

static const uint8_t BROADCAST_ADDR[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

int getSlaveIndex(const uint8_t *mac) {
  for(int i=0; i<slaveCount; i++) {
    if(memcmp(knownSlaves[i], mac, 6) == 0) return i;
  }
  return -1;
}

bool parseNextionPacket() {
  static uint8_t buffer[PACKET_SIZE];
  static uint8_t index = 0;
  static unsigned long lastByteTime = 0;

  while(NEXTION_SERIAL.available()) {
    uint8_t b = NEXTION_SERIAL.read();
    unsigned long now = millis();
    if(now - lastByteTime > TIMEOUT_MS) index = 0;
    lastByteTime = now;

    if(index == 0 && b != START_BYTE) continue;
    if(index < 3) {
      if(b == START_BYTE) buffer[index++] = b; else index = 0;
      continue;
    }
    buffer[index++] = b;
    if(index == PACKET_SIZE) {
      index = 0;
      if(buffer[7] == STOP_BYTE && buffer[8] == STOP_BYTE && buffer[9] == STOP_BYTE) {
        spielerpaar = buffer[3];
        ampelfarbe  = buffer[4];
        countdown   = (buffer[6] << 8) | buffer[5];
        newCommandAvailable = true;
        return true;
      }
    }
  }
  return false;
}

void sendCommandBroadcast() {
  CommandMsg cmd;
  cmd.msgType = MSG_TYPE_COMMAND;
  cmd.regCode = REGISTRATION_CODE;
  cmd.spielerpaar = spielerpaar;
  cmd.ampelfarbe  = ampelfarbe;
  cmd.countdown   = countdown;
  for(int i=0; i<SEND_RETRIES; i++) {
    esp_now_send(BROADCAST_ADDR, (uint8_t*)&cmd, sizeof(cmd));
    delay(SEND_DELAY_MS);
  }
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if(len < 1) return;
  uint8_t msgType = data[0];

  if(msgType == MSG_TYPE_REG_ACK && len == sizeof(RegAck)) {
    RegAck ack;
    memcpy(&ack, data, sizeof(ack));
    if(ack.regCode != REGISTRATION_CODE) return;
    if(getSlaveIndex(mac) != -1) return; 
    if(slaveCount < MAX_SLAVES) {
      memcpy(knownSlaves[slaveCount], mac, 6);
      lastSeen[slaveCount] = millis();
      slaveCount++;
      // Master fügt Peer hinzu
      esp_now_peer_info_t p = {};
      memcpy(p.peer_addr, mac, 6);
      p.channel = BROADCAST_CHANNEL;
      p.ifidx   = WIFI_IF_STA;
      p.encrypt = false;
      esp_now_add_peer(&p);
      Serial.print("Neuer Slave: ");
      for(int i=0; i<6; i++){Serial.printf("%02X%s", mac[i], i<5?":":"\n");}
    }
  }
  else if(msgType == MSG_TYPE_STATUS && len == sizeof(SlaveStatus)) {
    SlaveStatus s; 
    memcpy(&s, data, sizeof(s));
    if(s.regCode != REGISTRATION_CODE) return;
    int idx = getSlaveIndex(mac);
    if(idx >= 0) {
      lastSeen[idx] = millis();
      Serial.printf("Slave %d Status: %.2fV %.2fV (%s)\n", idx, s.battery1, s.battery2, s.isOnBattery?"Akku":"Netz");
    }
  }
}

void setup() {
  Serial.begin(115200);
  NEXTION_SERIAL.begin(115200, SERIAL_8N1, 16, 17);

  WiFi.mode(WIFI_STA);
  esp_wifi_start();
  esp_wifi_set_channel(BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_max_tx_power(TX_POWER);

  esp_now_init();
  esp_now_register_recv_cb(onDataRecv);

  // Broadcast-Peer
  esp_now_peer_info_t p = {};
  memcpy(p.peer_addr, BROADCAST_ADDR, 6);
  p.channel = BROADCAST_CHANNEL;
  p.ifidx   = WIFI_IF_STA;
  p.encrypt = false;
  esp_now_add_peer(&p);

  Serial.println("Master bereit.");
}

void loop() {
  static unsigned long lastRegBroadcast = 0;
  static unsigned long lastStatusCheck  = 0;
  unsigned long now = millis();

  // 1) Regelmäßig Registrierungs-Broadcast (auch nach initialer Phase)
  if(now - lastRegBroadcast >= REG_BROADCAST_INTERVAL_MS) {
    lastRegBroadcast = now;
    RegRequest req = {MSG_TYPE_REG_REQ, REGISTRATION_CODE};
    esp_now_send(BROADCAST_ADDR, (uint8_t*)&req, sizeof(req));
    Serial.println("REG_REQ gesendet (Broadcast)...");
  }

  // 2) Nextion abfragen
  parseNextionPacket();
  if(newCommandAvailable) {
    sendCommandBroadcast(); 
    newCommandAvailable = false;
  }

  // 3) Slave-Timeout prüfen
  if(now - lastStatusCheck >= STATUS_CHECK_INTERVAL_MS) {
    lastStatusCheck = now;
    for(int i=0; i<slaveCount; i++) {
      if(millis() - lastSeen[i] > SLAVE_TIMEOUT_MS) {
        Serial.printf("WARNUNG: Slave %d reagiert nicht mehr!\n", i);
        // Optional: Man könnte ihn hier aus der Liste entfernen
        // und neu erkennen lassen:
        // memmove(...) usw. – je nach Bedarf.
      }
    }
  }
}
