#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ===================== Konstante Einstellungen =====================
#define BROADCAST_CHANNEL 1
#define TX_POWER 60
#define REGISTRATION_CODE 0xABCDEF12

#define MSG_TYPE_REG_REQ   1
#define MSG_TYPE_REG_ACK   2
#define MSG_TYPE_COMMAND   60
#define MSG_TYPE_STATUS    50

#define STATUS_INTERVAL_MS 10000
#define MASTER_TIMEOUT_MS  20000  // Wenn wir so lange keine CMD/REG_REQ bekommen

// ===================== Display-Einstellungen =====================
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

MatrixPanel_I2S_DMA *dma_display = nullptr;
uint16_t myBLACK, myWHITE, myRED;

// Verbindungsstatus
bool isRegistered = false;   // Slave weiß, ob er gerade “verbunden” ist
unsigned long lastMasterContact = 0;
uint8_t masterMac[6] = {0};

// Aktuelle Daten (vom Master)
uint8_t currentSpielerpaar=0, currentAmpelfarbe=0;
int currentCountdown=-1;
unsigned long lastStatusSent=0;

// ===================== Pakete =====================
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
  uint8_t  spielerpaar;
  uint8_t  ampelfarbe;
  uint16_t countdown;
} CommandMsg;

typedef struct __attribute__((packed)) {
  uint8_t  msgType;
  uint32_t regCode;
  float    battery1;
  float    battery2;
  bool     isOnBattery;
} SlaveStatus;

// ===================== Callback-Funktionen =====================
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.print("Sendestatus: ");
  Serial.println(status==ESP_NOW_SEND_SUCCESS ? "OK" : "FEHLER");
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if(len < 1) return;
  uint8_t msgType = data[0];

  // Master sucht Slave -> Slave antwortet
  if(msgType == MSG_TYPE_REG_REQ && len == sizeof(RegRequest)) {
    RegRequest req;
    memcpy(&req, data, sizeof(req));
    if(req.regCode == REGISTRATION_CODE) {
      // Master MAC merken + RegAck senden
      memcpy(masterMac, mac, 6);

      RegAck ack = {MSG_TYPE_REG_ACK, REGISTRATION_CODE};
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, mac, 6);
      peerInfo.channel = BROADCAST_CHANNEL;
      peerInfo.ifidx   = WIFI_IF_STA;
      peerInfo.encrypt = false;
      esp_now_del_peer(mac);
      esp_now_add_peer(&peerInfo);

      for(int i=0; i<3; i++){
        esp_now_send(mac, (uint8_t*)&ack, sizeof(ack));
        delay(5);
      }
      isRegistered = true;
      lastMasterContact = millis();
      Serial.println("Master erkannt & REG_ACK gesendet!");
    }
  }
  // Master schickt Kommando
  else if(msgType == MSG_TYPE_COMMAND && len == sizeof(CommandMsg)) {
    CommandMsg c;
    memcpy(&c, data, sizeof(c));
    if(c.regCode == REGISTRATION_CODE) {
      currentSpielerpaar = c.spielerpaar;
      currentAmpelfarbe  = c.ampelfarbe;
      currentCountdown   = c.countdown;

      // Anzeige aktualisieren
      // (vorhandene Funktion, s.u.)
      // --------------------------------
      // Hier kannst du auch eine andere
      // Animation/Anzeige einbauen, je
      // nachdem was du willst.
      // --------------------------------
      extern void showPanelContent();
      showPanelContent();

      // Wir haben Kontakt zum Master
      memcpy(masterMac, mac, 6);
      isRegistered = true;
      lastMasterContact = millis();
    }
  }
}

// ===================== Display: Inhaltsanzeige =====================
void showPanelContent() {
  dma_display->fillScreen(myBLACK);
  dma_display->setTextSize(1);
  dma_display->setTextColor(myWHITE);

  // Zeigt "A/B" oder "C/D" oben
  dma_display->setCursor(5, 5);
  dma_display->print((currentSpielerpaar == 0) ? "A/B" : "C/D");

  // Countdown-Zahl in Rot
  dma_display->setCursor(5, 20);
  dma_display->setTextColor(myRED);
  dma_display->print(currentCountdown);

  // “Ampelfarbe” als farbiges Rechteck oben rechts
  uint16_t color = myWHITE;
  switch(currentAmpelfarbe) {
    case 0: color = dma_display->color444(0, 15, 0);   break; // grün
    case 1: color = dma_display->color444(15, 15, 0);  break; // gelb
    case 2: color = dma_display->color444(15, 0, 0);   break; // rot
  }
  dma_display->fillRect(54, 0, 10, 10, color);
}

// ===================== Status an Master schicken =====================
void sendStatus() {
  if(!isRegistered) return;
  SlaveStatus st;
  st.msgType     = MSG_TYPE_STATUS;
  st.regCode     = REGISTRATION_CODE;
  st.battery1    = 3.7f;
  st.battery2    = 3.8f;
  st.isOnBattery = false;
  esp_now_send(masterMac, (uint8_t*)&st, sizeof(st));
}

// ===================== Start-Animation (flüssiger Pfeil) =====================
void playArrowAnimation() {
  dma_display->clearScreen();

  // Zielscheibe (Center x=50, y=16), 
  // Außen: blau, Mitte: rot, Innen: gelb
  uint16_t blau = dma_display->color565(0, 0, 255);
  uint16_t rot  = dma_display->color565(255, 0, 0);
  uint16_t gelb = dma_display->color565(255, 255, 0);

  // Anzahl Frames und Delay
  // Pfeil wandert von x=0 bis x=48 in 16 Schritten => 3 px pro Schritt
  // => ~16 Frames. Verkürze / erhöhe frameDelay, um das Timing zu variieren.
  const int totalFrames = 16;
  const int frameDelay  = 45; // ms pro Frame

  for (int frame = 0; frame <= totalFrames; frame++) {
    dma_display->clearScreen();

    // Zielscheibe erneut zeichnen
    dma_display->fillCircle(50, 16, 10, blau);
    dma_display->fillCircle(50, 16, 6,  rot);
    dma_display->fillCircle(50, 16, 3,  gelb);

    // Position Pfeilspitze
    int arrowX = (48 * frame) / totalFrames; 
    int arrowY = 14;

    // Farben
    uint16_t arrowColor      = dma_display->color565(255, 255, 255); // Weiß
    uint16_t arrowFeatherCol = dma_display->color565(0,   255, 0);   // Grün

    // Schaft (28 Pixel lang)
    dma_display->fillRect(arrowX - 28, arrowY + 1, 28, 2, arrowColor);

    // Pfeilspitze
    dma_display->drawPixel(arrowX, arrowY + 1, arrowColor);

    // Federn am hinteren Ende (z.B. 3×4 Pixel)
    int featherX = arrowX - 28;
    int featherY = arrowY;
    dma_display->fillRect(featherX - 3, featherY, 3, 4, arrowFeatherCol);

    // Zeige aktuellen Frame
    dma_display->flipDMABuffer();
    delay(frameDelay);
  }

  // Explosion/Einschlag: 3 kurze weiße Blitz-Frames
  for(int i = 0; i < 3; i++) {
    dma_display->fillCircle(50, 16, 12 + i*2, dma_display->color565(255, 255, 255));
    dma_display->flipDMABuffer();
    delay(55);
  }

  // Ausblenden in ~5 Steps
  for(int fade = 0; fade < 5; fade++) {
    int intensity = 255 - fade * 50;
    if(intensity < 0) intensity = 0;

    uint16_t fadeColor = dma_display->color565(intensity, intensity, intensity);
    dma_display->clearScreen();
    dma_display->fillCircle(50, 16, 18, fadeColor);
    dma_display->flipDMABuffer();
    delay(70);
  }

  // Schwarz am Schluss
  dma_display->clearScreen();
  dma_display->flipDMABuffer();
}

// ===================== Setup und Loop =====================
void setupDisplay() {
  // GPIO-Pins an dein Layout anpassen:
  HUB75_I2S_CFG::i2s_pins _pins = {
    42, 41, 40, 38, 39, 37,
    45, 36, 48, 35, -1, 47,
    1, 2
  };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, _pins);
  mxconfig.driver = HUB75_I2S_CFG::FM6124;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(255); // 0..255
  dma_display->clearScreen();
  dma_display->setRotation(90);

  myBLACK = dma_display->color565(0,0,0);
  myWHITE = dma_display->color565(255,255,255);
  myRED   = dma_display->color565(255,0,0);
}

void setup() {
  Serial.begin(115200);

  setupDisplay();

  // --- Starte die Pfeil-Animation einmalig beim Einschalten ---
  playArrowAnimation();

  WiFi.mode(WIFI_STA);
  esp_wifi_start();
  esp_wifi_set_channel(BROADCAST_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_max_tx_power(TX_POWER);

  esp_now_init();
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Slave bereit.");
}

void loop() {
  // Regelmäßig Status schicken
  if(millis() - lastStatusSent >= STATUS_INTERVAL_MS) {
    lastStatusSent = millis();
    sendStatus();
  }

  // Timeout prüfen
  if(isRegistered && (millis() - lastMasterContact > MASTER_TIMEOUT_MS)) {
    Serial.println("Master verloren -> isRegistered = false.");
    isRegistered = false;
    dma_display->clearScreen();
  }

  delay(20);
}
