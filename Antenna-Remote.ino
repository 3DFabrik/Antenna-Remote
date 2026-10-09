#include "FS.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <Preferences.h>
#include <Update.h>
Preferences netPrefs;  // WLAN credentials live here, not in config.txt, so "sendconfig" never reveals them
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
#include <WebServer.h>
#include "index.h"  //Web page header file
WebServer server(80);

#include <TFT_eSPI.h>  // Graphics and font library for ST7789 driver chip, be careful with updating as the fucking update will delete your pin-settings
#include <SPI.h>

#define TFT_GREY 0x5AEB  // New colours....
#define TFT_POPPY 0XD208
#define TFT_MINTGR 0XEFFF
#define TFT_NPBLUE 0X9EDB
#define TFT_UCLABLUE 0X43B3
#define TFT_DELFTBLUE 0X198A

int COLOR_BG = TFT_BLACK;
int COLOR_HIGHLIGHT = TFT_POPPY;

TFT_eSPI tft = TFT_eSPI();  // Invoke library
TFT_eSprite img = TFT_eSprite(&tft);

// Touch business
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false
#define KEY_X 295  // Center of key
#define AUTO_CX 98  // The AUTOMATIC button runs from the left edge to x = 196; the link icons sit between it and MISC
#define AUTO_W 196
#define NUM_KEYS 7
String b0_txt = "UP";
String b1_txt = "ST";
String b2_txt = "DN";
String b3_p1_txt = "MISC";
String b3_p2_txt = "BACK";
String b4_txt = "AUTOMATIC MODE OFF";
String b5_txt = "WLAN setup";
String b7_txt_off = "BT -> WLAN";  // shown while in Bluetooth mode: what the button switches to
String b7_txt_on = "WLAN -> BT";
int n_presscount_b1 = 0;
TFT_eSPI_Button key[NUM_KEYS];
unsigned long previousMillis = 0;  // will store last time touchscreen was checked
const long buttonIntervall = 100;

// Connection: either Bluetooth or WLAN, chosen at boot (config key b_wifi_on). Both radios at once do not fit.
bool b_wifi_on = false;  // true = WLAN mode, false = Bluetooth mode
bool bt_active = false;
String wifi_ssid = "";
String wifi_pass = "";
volatile int wifi_reason = 0;  // last disconnect reason from the WiFi stack, 0 = none
bool wifi_hold = false;        // true while the setup pages scan, so the retry timer stays out of the way
bool wifi_was_conn = false;
unsigned long wifi_last_try = 0;
bool servers_started = false;
unsigned long modeConfirmUntil = 0;
unsigned long hintUntil = 0;
String hintText = "";

// Remote protocol "AntennaRemote 1" for SDROxide: text lines on TCP, see crates/sdroxide-types/src/antremote.rs
const uint16_t RIG_PORT = 4540;
const unsigned long RIG_IDLE_MS = 5000;  // SDROxide polls once a second; silence this long means the client is gone
const unsigned long RIG_REPLACE_MS = 2000;  // a newcomer takes over from a client quiet for this long
WiFiServer rigServer(RIG_PORT);
WiFiClient rigClient;
String rigLine = "";
unsigned long rigLastSeen = 0;

// Firmware update over WLAN (POST /update, from the Update tab of the web page)
// Set by the release build (-DFW_VERSION="v1.2.3"); a local build shows its build time.
#ifndef FW_VERSION
#define FW_VERSION __DATE__ " " __TIME__
#endif
const char fwVersion[] = FW_VERSION;
bool otaBusy = false;  // from the first byte of an upload: the loop does nothing but serve it
bool otaBeginOk = false;
bool otaOk = false;
bool otaUploadStarted = false;
bool otaDenied = false;
unsigned long otaWifiLostAt = 0;
int otaPct = -1;
String otaErr = "";

// Setup pages (3 = network list, 4 = keyboard)
struct UiBtn {
  int16_t x, y, w, h;
  const char* label;
};
struct KbKey {
  int16_t x, y, w, h;
  char label[8];
  int16_t code;  // ASCII, or one of the KC_ values
};
const int KC_SHIFT = -1, KC_DEL = -2, KC_SYM = -3, KC_SHOW = -4, KC_CANCEL = -5, KC_OK = -6;
const int MAX_NETS = 16;
String netSsid[MAX_NETS];
int netRssi[MAX_NETS];
bool netOpen[MAX_NETS];
int netCount = 0;
int netTop = 0;
String pendingSsid = "";
String kbText = "";
int kbTarget = 1;  // 0 = SSID, 1 = password
int kbLayout = 0;  // 0 = letters, 2 = symbols
bool kbShift = false;
bool kbShow = false;
bool setupTouchWas = true;
String p2Last[8];

// Serial receive business
int icom_use = 0;        // sett to 1 in case you want tp use the icom CIV function of the TRX
byte TRX_address(0x98);  // HEX 0x70/112 = Icom IC-7000, 0x94/148 = Icom IC-7300, 0x98/152 = Icom IC-7610, store in SPIFFS as decimal value
const byte numChars = 128;  // a WLAN password can be 63 characters
char receivedChars[numChars];
char tempChars[numChars];         // temporary array for use when parsing
char SRX_TYPE[numChars] = { 0 };  // variables to hold the parsed data
float RX_FREQ = 0.0;
boolean newData = false;

// Misc Variabel
float QRG = 0;
int QRG_len_old = 0;
int BAND = 0;
int BAND_len_old = 0;
String ANT = "";
String ANT_OLD = "";
bool b_automatic = false;
bool b_booting = true;

//Define Bands:
int l160 = 1810;
int h160 = 2000;
int l80 = 3500;
int h80 = 3800;
int l60 = 5250;
int h60 = 5450;
int l40 = 7000;
int h40 = 7200;
int l30 = 10100;
int h30 = 10150;
int l20 = 14000;
int h20 = 14350;
int l17 = 18068;
int h17 = 18168;
int l15 = 21000;
int h15 = 21450;
int l12 = 24890;
int h12 = 24990;
int l10 = 28000;
int h10 = 29700;
int l6 = 50000;
int h6 = 52000;
int l4 = 70150;
int h4 = 70200;

//Define Antennas
String A1NAME = "1";  // Antenna names (Will be overwritten by what is found in SPIFFS config.txt)
String A2NAME = "2";  // ...
String A3NAME = "3";  // ..
String A4NAME = "4";  // .
String A5NAME = "5";
String A6NAME = "6";
String A7NAME = "7";
String A8NAME = "8";

String TUNEROFF = "0";  // Holds a decimal converted 8 bit binary which represents the use of the external tuner for each antenna
bool A1EXTT = 0;        // if bit is set this antenna uses an external tuner
bool A2EXTT = 0;        // ...
bool A3EXTT = 0;        // ..
bool A4EXTT = 0;        // .
bool A5EXTT = 0;
bool A6EXTT = 0;
bool A7EXTT = 0;
bool A8EXTT = 0;

//Define Antenna 2 Band
int a160m = 1;
int a80m = 1;
int a60m = 1;
int a40m = 1;
int a30m = 1;
int a20m = 1;
int a17m = 1;
int a15m = 1;
int a12m = 1;
int a10m = 1;
int a6m = 1;
int a4m = 1;

// The same settings as tables, for the web interface
String* antNames[8] = { &A1NAME, &A2NAME, &A3NAME, &A4NAME, &A5NAME, &A6NAME, &A7NAME, &A8NAME };
int* bandAnt[12] = { &a160m, &a80m, &a60m, &a40m, &a30m, &a20m, &a17m, &a15m, &a12m, &a10m, &a6m, &a4m };
const char* BAND_KEY[12] = { "a160m", "a80m", "a60m", "a40m", "a30m", "a20m", "a17m", "a15m", "a12m", "a10m", "a6m", "a4m" };
const int BAND_M[12] = { 160, 80, 60, 40, 30, 20, 17, 15, 12, 10, 6, 4 };

int antenna_selected = 1;  // the currently selected antenna number (1-n_ant_max)
int n_ant_max = 8;         // amount of antennas the switch can handle
int n_disp_page_old = 0;   // total amount of available display pages
int n_disp_page = 1;       // currently selected display page

// alive message
unsigned long aliveMillis = 0;
const long aliveintervall = 1000;  // intervall time in which a status-String gets send
String alivemsg = "";              // contains the alive message

// Scrolltext
unsigned long anttxtMillis = 0;
const long anttxtintervall = 35;
int textboxdiffy = 45;
int textboxstartfromtop = 10;
int scrolltarget = 0;
int scrollposact = 0;
//bool b_ant_col = true;

// Shift Register Pins
const int CLR_pin = 25;  // CLR-Pin of the 74HC164
const int AB_pin = 26;   // Input pin A or B of the 74HC164, remember to put 2nd open input to VCC
const int CLK_pin = 27;  // CLK-Pin of the 74HC164

// ICOM Tuner relais pins
const int TUNE_EXT_pin = 33;
const int TUNE_REQ_pin = 32;
bool tuning = false;
bool tuner = false;
int antenna_driven = 0;  // antenna the shift register currently holds

// ICOM coms variables
const byte startMarker = 0xFE;  // Indicates where the icom signal string starts
const byte endMarker = 0xFD;    // Indicates where the icom signal string ends
const int maxDataLength = 16;
byte receivedData[maxDataLength];  // Array for the recieved data bytes
int dataIndex = 0;                 // Pointer into the data array
bool newData2 = false;             // Set to true when a new data-array has been received
float SValue_raw = 0;
float SValue = 0;


//  ************************************************** Setup *******************************************************************************
void setup(void) {
  // Shift register pins:
  pinMode(AB_pin, OUTPUT);
  pinMode(CLK_pin, OUTPUT);
  pinMode(CLR_pin, OUTPUT);
  pinMode(TUNE_EXT_pin, OUTPUT);
  pinMode(TUNE_REQ_pin, OUTPUT);

  Serial.begin(115200);  // Serial console
  Serial2.begin(19200);  // ICOM CI-V
  Serial.println("Booting Sketch...");

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(COLOR_BG);
  touch_calibrate();
  tft.setTextColor(TFT_GOLD);
  tft.setCursor(0, 0);
  tft.println("Booting");
  img.createSprite(260, 130);
  // Here we try to get some variables out of the software:
  tft.println("Reading Config values");
  readConfig();

  loadWifiCreds();
  if (b_wifi_on) {
    tft.println("Mode: WLAN");
    startWifi();  // connects in the background; the loop brings the servers up
  } else {
    tft.println("Mode: Bluetooth");
    SerialBT.begin("AntennaRemote");  //Name des ESP32
    bt_active = true;
    Serial.println("Bluetooth ready to connect");
  }

  tft.println("Selecting Antennas");
  sel_antenna();

  tft.println("Starting Display");
  tft_update();
  b_booting = false;
}

// *************************************************** LOOP ********************************************************************************
void loop(void) {

  if (otaBusy) {  // an update is coming in: nothing else may take heap, time or the relays
    if (WiFi.status() != WL_CONNECTED) {
      if (!otaWifiLostAt) otaWifiLostAt = millis();
      else if (millis() - otaWifiLostAt > 8000) ESP.restart();
    } else {
      otaWifiLostAt = 0;
    }
    server.handleClient();
    if (otaUploadStarted && !otaOk && otaErr.length()) {
      otaFailScreen(otaErr);
      delay(2000);
      ESP.restart();
    }
    yield();
    return;
  }


  recvWithStartEndMarkers();
  if (newData == true) {
    strcpy(tempChars, receivedChars);
    parseData();
    //showParsedData();
    newData = false;
  }

  if ((WiFi.status() == WL_CONNECTED) && (b_wifi_on)) {  // Only serve in case we are connected to the network
    server.handleClient();
    rigPoll();
  }
  wifiTick();

  if (TRX_address >= 1) geticomdata();
  if (newData2) {
    // Neue Daten verarbeiten oder anzeigen
    float QRGreaded = processReceivedData();
    if (QRGreaded > 0)  // new QRG
    {
      QRG = QRGreaded;
      tft_update();
    }
    // Zurücksetzen für den nächsten Durchlauf
    newData2 = false;
  }


  if (millis() - anttxtMillis >= anttxtintervall) {
    anttxtMillis = millis();
    if (n_disp_page == 1) { scrollAntennas(); }
  }

  //Touchscreen Business
  if (millis() - previousMillis >= (n_disp_page >= 3 ? 30UL : (unsigned long)buttonIntervall)) {  // the keyboard needs quicker polling than the buttons
    previousMillis = millis();
    uint16_t t_x = 0, t_y = 0;                // To store the touch coordinates
    bool pressed = tft.getTouch(&t_x, &t_y);  // Get current touch state and coordinates
    for (uint8_t b = 0; b < NUM_KEYS; b++) {  // Adjust press state of each key appropriately
      if (pressed && key[b].contains(t_x, t_y))
        key[b].press(true);  // tell the button it is pressed
      else
        key[b].press(false);  // tell the button it is NOT pressed
    }

    if (n_disp_page >= 3) { setupTouch(pressed, t_x, t_y); }

    if (n_disp_page == 1) {  // Check if any key has changed state
      if (b_automatic == false) {
        tft.setTextFont(4);
        if (key[0].justPressed()) {  // UP Button
          key[0].drawButton(true, b0_txt);
          if (antenna_selected <= (n_ant_max - 1)) {
            antenna_selected++;
            sel_antenna();
          }
        }
        if (key[0].justReleased()) { key[0].drawButton(false, b0_txt); }

        if (key[1].isPressed()) {
          n_presscount_b1++;
          if (n_presscount_b1 == 10) {
            writeConfig("a" + String(BAND) + "m=" + String(antenna_selected));                   // Write Band to antenna to SPIFFS
            key[1].initButton(&tft, KEY_X, 75, 50, 50, TFT_WHITE, TFT_GREEN, TFT_WHITE, "", 1);  // ST Button
            key[1].drawButton(false, b1_txt);
          }
        }
        if (key[1].justPressed()) {  // ST Button
          key[1].drawButton(true, b1_txt);
        }
        if (key[1].justReleased()) {
          key[1].initButton(&tft, KEY_X, 75, 50, 50, TFT_WHITE, COLOR_BG, TFT_WHITE, "", 1);  // ST Button
          key[1].drawButton(false, b1_txt);
          n_presscount_b1 = 0;
        }

        if (key[2].justPressed()) {  // DN Button
          key[2].drawButton(true, b2_txt);
          if (antenna_selected >= 2) {
            antenna_selected--;
            sel_antenna();
          }
        }
        if (key[2].justReleased()) { key[2].drawButton(false, b2_txt); }
      }
    }

    if (n_disp_page == 1 && key[4].justPressed()) {  // MODE Button
      tft.setTextFont(2);
      if (b_automatic == true) {
        b_automatic = false;
        writeConfig("b_automatic=false");
        b4_txt = "AUTOMATIC MODE OFF";
        key[4].initButton(&tft, AUTO_CX, 20, AUTO_W, 30, TFT_WHITE, TFT_GREY, TFT_WHITE, "", 1);
        Serial.println("b_automatic=false");
      } else {
        b_automatic = true;
        writeConfig("b_automatic=true");
        b4_txt = "AUTOMATIC MODE ON";
        sel_antenna();
        tft_update();
        key[4].initButton(&tft, AUTO_CX, 20, AUTO_W, 30, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "", 1);
        key[4].drawButton(false, b4_txt);
        Serial.println("b_automatic=true");
      }
    }
    if (n_disp_page == 1 && key[4].justReleased()) {
      tft.setTextFont(2);
      key[4].drawButton(false, b4_txt);
    }

    if (n_disp_page <= 2 && key[3].justPressed()) {  // PAGE Button
      if (n_disp_page == 1) {
        n_disp_page = 2;
      } else {
        n_disp_page = 1;
      }
      if (n_disp_page == 1) { key[3].drawButton(true, b3_p1_txt); }
      if (n_disp_page == 2) { key[3].drawButton(true, b3_p2_txt); }
      tft_update();
    }

    if (n_disp_page == 2) {

      if (key[5].justPressed()) {  // WLAN setup button
        if (b_wifi_on) {
          netListOpen();
        } else {
          hintText = "Switch to WLAN mode first";
          hintUntil = millis() + 3000;
          tft_update();
        }
      }

      if (n_disp_page == 2 && key[6].justPressed()) {  // Bluetooth <-> WLAN, takes effect after a restart
        if (millis() < modeConfirmUntil) {
          tft.fillScreen(COLOR_BG);
          tft.setTextColor(TFT_WHITE, COLOR_BG);
          tft.drawString("Restarting...", 10, 100, 4);
          writeConfig(b_wifi_on ? "b_wifi_on=false" : "b_wifi_on=true");
          delay(800);
          ESP.restart();
        }
        modeConfirmUntil = millis() + 3000;
        tft.setTextFont(1);
        key[6].drawButton(true, "Tap again");
      }
      if (n_disp_page == 2 && modeConfirmUntil != 0 && millis() > modeConfirmUntil) {
        modeConfirmUntil = 0;
        tft.setTextFont(1);
        key[6].drawButton(false, b_wifi_on ? b7_txt_on : b7_txt_off);
      }
    }
    //getsmeter();
  }

  if (millis() - aliveMillis >= aliveintervall) {  // here an alive message will be triggered every 1000ms
    aliveMillis = millis();
    getqrg();  //Request QRG from the TRX
    delay(10);
    sendalivemessage();
    if (n_disp_page == 2) { tft_update(); }  // keeps the connection state current
    if (n_disp_page == 1) { drawLinkStatus(false); }
  }
}

// ************************************************ SUBROUTINES ****************************************************************************
void sendalivemessage() {
  int outputstate = digitalRead(TUNE_EXT_pin);
  int tuningstate = digitalRead(TUNE_REQ_pin);
  alivemsg = String(antenna_selected) + "=" + String(b_automatic) + "=" + String(outputstate) + "=" + String(tuningstate);
  Serial.println(alivemsg);
  if (bt_active) { SerialBT.println(alivemsg); }
}

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0';  // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    } else if (rc == startMarker) {
      recvInProgress = true;
    }
  }

  while (bt_active && SerialBT.available() > 0 && newData == false) {
    rc = SerialBT.read();

    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedChars[ndx] = '\0';  // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    } else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

void parseData() {  // split the data into its parts
  bool B_EXIT = false;
  char* strtokIndx;  // this is used by strtok() as an index

  strtokIndx = strtok(tempChars, ",");  // get the first part - the string
  if (strtokIndx == NULL) return;
  strcpy(SRX_TYPE, strtokIndx);         // check which message we have received
  String CSTRING = String(SRX_TYPE);
  if (CSTRING == "QRG") {
    strtokIndx = strtok(NULL, ",");
    RX_FREQ = atof(strtokIndx);  // convert to float
    sel_band(RX_FREQ);
    QRG = RX_FREQ;
    tft_update();
    B_EXIT = true;
  }
  if (B_EXIT == true) return;

  String address = CSTRING;
  strtokIndx = strtok(NULL, ",");
  String setvalue = strtokIndx;

  // WLAN from the console or the PC tool: <WIFI,ssid,password>, <NETMODE,wifi|bt>, <WIFISTATUS>
  if (address == "WIFI") {
    char* pass = strtok(NULL, "");  // the rest of the line, so a password may contain commas
    wifi_ssid = setvalue;
    wifi_pass = pass ? String(pass) : String("");
    saveWifiCreds();
    if (b_wifi_on) { wifiConnectNow(); }
    Serial.println("WLAN saved: " + wifi_ssid + (b_wifi_on ? ", connecting" : ", takes effect in WLAN mode"));
    return;
  }
  if (address == "NETMODE") {
    writeConfig(setvalue == "wifi" ? "b_wifi_on=true" : "b_wifi_on=false");
    Serial.println("Restarting in " + String(setvalue == "wifi" ? "WLAN" : "Bluetooth") + " mode");
    delay(500);
    ESP.restart();
  }
  if (address == "WIFISTATUS") {
    String s = String(b_wifi_on ? "WLAN" : "BT") + " ssid=" + wifi_ssid + " state=" + wifiStateText() + " ip=" + WiFi.localIP().toString() + " rssi=" + String(WiFi.RSSI());
    Serial.println(s);
    if (bt_active) { SerialBT.println(s); }
    return;
  }

  // Change selected antenna in case requested from application tool
  if (address == "ANTNUMBER") {
    antenna_selected = setvalue.toInt();
    sel_antenna();
  }

  // ******************************************Receive and store antenna names
  if (address == "A1NAME") {
    A1NAME = setvalue;  // convert this part to a String
    writeConfig(address + "=" + setvalue);
  }
  if (address == "A2NAME") {
    A2NAME = setvalue;  // convert this part to a String
    writeConfig(address + "=" + setvalue);
  }
  if (address == "A3NAME") {
    A3NAME = setvalue;  // convert this part to a String
    writeConfig(address + "=" + setvalue);
  }
  if (address == "A4NAME") {
    A4NAME = setvalue;  // convert this part to a String
    writeConfig(address + "=" + setvalue);
  }
  if (address == "A5NAME") {
    A5NAME = setvalue;  // convert this part to a String
    writeConfig(address + "=" + setvalue);
  }
  if (address == "A6NAME") {
    A6NAME = setvalue;  // convert this part to a String
    writeConfig(address + "=" + setvalue);
  }
  if (address == "A7NAME") {
    A7NAME = setvalue;  // convert this part to a String
    writeConfig(address + "=" + setvalue);
  }
  if (address == "A8NAME") {
    A8NAME = setvalue;
    writeConfig(address + "=" + setvalue);
  }

  // ******************************************Receive and store Antenna 2 Band assignment
  if (address == "a160m") {
    writeConfig(address + "=" + setvalue);
    a160m = setvalue.toInt();
  }
  if (address == "a80m") {
    writeConfig(address + "=" + setvalue);
    a80m = setvalue.toInt();
  }
  if (address == "a60m") {
    writeConfig(address + "=" + setvalue);
    a60m = setvalue.toInt();
  }
  if (address == "a40m") {
    writeConfig(address + "=" + setvalue);
    a40m = setvalue.toInt();
  }
  if (address == "a30m") {
    writeConfig(address + "=" + setvalue);
    a30m = setvalue.toInt();
  }
  if (address == "a20m") {
    writeConfig(address + "=" + setvalue);
    a20m = setvalue.toInt();
  }
  if (address == "a17m") {
    writeConfig(address + "=" + setvalue);
    a17m = setvalue.toInt();
  }
  if (address == "a15m") {
    writeConfig(address + "=" + setvalue);
    a15m = setvalue.toInt();
  }
  if (address == "a12m") {
    writeConfig(address + "=" + setvalue);
    a12m = setvalue.toInt();
  }
  if (address == "a10m") {
    writeConfig(address + "=" + setvalue);
    a10m = setvalue.toInt();
  }
  if (address == "a6m") {
    writeConfig(address + "=" + setvalue);
    a6m = setvalue.toInt();
  }
  if (address == "a4m") {
    writeConfig(address + "=" + setvalue);
    a4m = setvalue.toInt();
  }
  if (address == "n_ant_max") {
    writeConfig(address + "=" + setvalue);
    n_ant_max = setvalue.toInt();
  }
  if (address == "TRX_address") {
    writeConfig(address + "=" + setvalue);
    TRX_address = byte(setvalue.toInt());
  }

  if (address == "b_automatic") { setAutomatic(!b_automatic); }

  if (address == "TUNEROFF") {
    TUNEROFF = setvalue;
    writeConfig("TUNEROFF=" + setvalue);
    //Serial.println("TUNEROFF RECEIVED");
    sel_antenna();
  }
  if (address == "TUNE") { setTuning(!tuning); }
  if (address == "TUNR") { setTuner(!tuner); }
  if (address == "sendconfig") { sendConfig(); }
  if (address == "reboot") { ESP.restart(); }
  if (address == "reset") {
    SPIFFS.remove("/config.txt");
    delay(100);
    ESP.restart();
  }

  scrollposact = scrollposact - 1;
  tft_update();
}

void createDefaultConfig() {
  Serial.println("Keine default config in SPIFFS gefunden, wird erstellt...");
  File file = SPIFFS.open("/config.txt", "a");
  if (!file) {
    Serial.println("Fehler beim Erstellen der Konfigurationsdatei");
    return;
  } else {
    Serial.println("Datei erfolgreich erstellt");
  }

  // Schreibe Standard-Konfigurationswerte in die Datei
  file.println("b_wifi_on=false");
  file.println("b_automatic=true");
  file.println("a160m=1");
  file.println("a80m=2");
  file.println("a60m=3");
  file.println("a40m=4");
  file.println("a30m=5");
  file.println("a20m=6");
  file.println("a17m=7");
  file.println("a15m=8");
  file.println("a12m=7");
  file.println("a10m=6");
  file.println("a6m=5");
  file.println("a4m=4");
  file.println("A1NAME=ANTENNA 1");
  file.println("A2NAME=ANTENNA 2");
  file.println("A3NAME=ANTENNA 3");
  file.println("A4NAME=ANTENNA 4");
  file.println("A5NAME=ANTENNA 5");
  file.println("A6NAME=ANTENNA 6");
  file.println("A7NAME=ANTENNA 7");
  file.println("A8NAME=ANTENNA 8");
  file.println("TUNEROFF=00000000");
  file.println("n_ant_max=8");
  file.println("icom_use=0");
  file.println("TRX_address=152");
  file.close();
}

void geticomdata() {
  while (Serial2.available() > 0 && newData2 == false) {
    byte currentByte = Serial2.read();

    if (currentByte == startMarker) {
      dataIndex = 0;
    } else if (currentByte == endMarker) {
      newData2 = true;
    } else {
      if (dataIndex < maxDataLength) {
        receivedData[dataIndex] = currentByte;
        dataIndex++;
      }
    }
  }
}

unsigned long processReceivedData(void) {

  uint32_t t_QRG = 0;
  uint8_t GHZ = 0, MHZ = 0, KHZ = 0, HZ = 0, mHZ = 0;
  static unsigned long lastfreq = 0;

  if (receivedData[0] == 0xE0 && receivedData[1] == TRX_address && receivedData[2] == 0x3) {
    GHZ = receivedData[7];           // 1GHz & 100Mhz
    GHZ = GHZ - (((GHZ / 16) * 6));  // explanation here: https://stackoverflow.com/questions/28133020/how-to-convert-bcd-to-decimal
    if (GHZ >= 100) return (0);

    MHZ = receivedData[6];           // 10Mhz & 1Mhz
    MHZ = MHZ - (((MHZ / 16) * 6));  // Transform bytes ICOM CAT
    if (MHZ >= 100) return (0);

    KHZ = receivedData[5];           // 100Khz & 10KHz
    KHZ = KHZ - (((KHZ / 16) * 6));  // Transform bytes ICOM CAT
    if (KHZ >= 100) return (0);

    HZ = receivedData[4];         // 1Khz & 100Hz
    HZ = HZ - (((HZ / 16) * 6));  // Transform bytes ICOM CAT
    if (HZ >= 100) return (0);

  } else if (receivedData[0] == 0x1 && receivedData[1] == TRX_address && receivedData[2] == 0x1C && receivedData[3] == 0x3) {

    GHZ = receivedData[8];           // 1GHz & 100Mhz
    GHZ = GHZ - (((GHZ / 16) * 6));  // explanation here: https://stackoverflow.com/questions/28133020/how-to-convert-bcd-to-decimal
    if (GHZ >= 100) return (0);

    MHZ = receivedData[7];           // 10Mhz & 1Mhz
    MHZ = MHZ - (((MHZ / 16) * 6));  // Transform bytes ICOM CAT
    if (MHZ >= 100) return (0);

    KHZ = receivedData[6];           // 100Khz & 10KHz
    KHZ = KHZ - (((KHZ / 16) * 6));  // Transform bytes ICOM CAT
    if (KHZ >= 100) return (0);

    HZ = receivedData[5];         // 1Khz & 100Hz
    HZ = HZ - (((HZ / 16) * 6));  // Transform bytes ICOM CAT
    if (HZ >= 100) return (0);

  } else if (receivedData[0] == TRX_address && receivedData[1] == 0xE0 && receivedData[2] == 0x15 && receivedData[3] == 0x2) {
    SValue_raw = (receivedData[4] * 100 + receivedData[5]);  // S-meter level *(0000=S0, 0120=S9, 0241=S9+60dB)
    if (SValue_raw <= 241) {
      if (SValue_raw >= SValue) {
        SValue = SValue_raw;
      } else {
        SValue = 0.97 * SValue + 0.03 * SValue_raw;
      }
      Serial.println("SValue=" + String(SValue, 0));
    }
    return (0);
  }

  t_QRG = ((GHZ * 1000000) + (MHZ * 10000) + (KHZ * 100) + (HZ * 1));  // QRG variable stores frequency in GMMMkkkH format - Frequ divided by 100

  if (t_QRG == lastfreq) return (0);
  lastfreq = t_QRG;
  sel_band(t_QRG);
  return (t_QRG);
}

void sendConfig() {
  File file = SPIFFS.open("/config.txt", "r");
  if (!file) {
    Serial.println("Fehler beim Öffnen der Konfigurationsdatei");
    return;
  } else {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      Serial.println(line);    // Zeile ausgeben
      SerialBT.println(line);  // Zeile ausgeben
      delay(20);
    }
    file.close();
  }
}

void readConfig() {

  if (!SPIFFS.exists("/config.txt")) {
    createDefaultConfig();  // Standard-Konfigurationsdatei erstellen
  }
  File file = SPIFFS.open("/config.txt", "r");
  if (!file) {
    Serial.println("Fehler beim Öffnen der Konfigurationsdatei");
    return;
  } else {
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    int equalIndex = line.indexOf('=');
    if (equalIndex != -1) {
      String key = line.substring(0, equalIndex);
      String value = line.substring(equalIndex + 1);

      if ((key == "b_wifi_on") && (value == "true")) { b_wifi_on = true; }
      if ((key == "b_wifi_on") && (value == "false")) { b_wifi_on = false; }
      if ((key == "b_automatic") && (value == "true")) { b_automatic = true; }
      if ((key == "b_automatic") && (value == "false")) { b_automatic = false; }
      if (key == "a160m") { a160m = value.toInt(); }
      if (key == "a80m") { a80m = value.toInt(); }
      if (key == "a60m") { a60m = value.toInt(); }
      if (key == "a40m") { a40m = value.toInt(); }
      if (key == "a30m") { a30m = value.toInt(); }
      if (key == "a20m") { a20m = value.toInt(); }
      if (key == "a17m") { a17m = value.toInt(); }
      if (key == "a15m") { a15m = value.toInt(); }
      if (key == "a12m") { a12m = value.toInt(); }
      if (key == "a10m") { a10m = value.toInt(); }
      if (key == "a6m") { a6m = value.toInt(); }
      if (key == "a4m") { a4m = value.toInt(); }
      if (key == "A1NAME") { A1NAME = value; }
      if (key == "A2NAME") { A2NAME = value; }
      if (key == "A3NAME") { A3NAME = value; }
      if (key == "A4NAME") { A4NAME = value; }
      if (key == "A5NAME") { A5NAME = value; }
      if (key == "A6NAME") { A6NAME = value; }
      if (key == "A7NAME") { A7NAME = value; }
      if (key == "A8NAME") { A8NAME = value; }
      if (key == "TUNEROFF") { TUNEROFF = value; }
      if (key == "n_ant_max") { n_ant_max = value.toInt(); }
      if (key == "TRX_address") { TRX_address = byte(value.toInt()); }
      if (b_booting) { tft.println("Key " + key + " value =" + value); }
    }
  }
  file.close();
}

void writeConfig(const String& configString) {
  String key = configString.substring(0, configString.indexOf('='));
  String updatedContent = "";

  // Durchlaufe die Konfigurationsdatei und erstelle eine neue Version ohne vorhandene Einträge des Schlüssels
  File file = SPIFFS.open("/config.txt", "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (!line.startsWith(key + "=")) {
        updatedContent += line + "\n";
      }
    }
    file.close();
  }

  // Füge den neuen Eintrag immer hinzu
  updatedContent += configString + "\n";

  // Überschreibe die Konfigurationsdatei mit der aktualisierten Version
  file = SPIFFS.open("/config.txt", "w");
  if (file) {
    file.print(updatedContent);
    file.close();
    readConfig();
  } else {
    Serial.println("Fehler beim Aktualisieren der Konfigurationsdatei");
  }
}

void setAutomatic(bool on) {
  if (b_automatic == on) return;
  b_automatic = on;
  writeConfig(on ? "b_automatic=true" : "b_automatic=false");
  b4_txt = on ? "AUTOMATIC MODE ON" : "AUTOMATIC MODE OFF";
  if (on) {
    sel_antenna();
    tft_update();
  }
  key[4].initButton(&tft, AUTO_CX, 20, AUTO_W, 30, TFT_WHITE, on ? TFT_DARKGREEN : TFT_GREY, TFT_WHITE, "", 1);
  sendalivemessage();
  if (n_disp_page == 1) drawButtons_p1();
}

void setTuning(bool on) {
  tuning = on;
  digitalWrite(TUNE_REQ_pin, on ? HIGH : LOW);
  sendalivemessage();
}

void setTuner(bool on) {
  tuner = on;
  digitalWrite(TUNE_EXT_pin, on ? HIGH : LOW);
  sendalivemessage();
}

// ************************************************ Web interface ***************************************************************************
String jsonStr(const String& s) {
  String o = "\"";
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') o += '\\';
    if (c >= 32) o += c;
  }
  return o + "\"";
}

String cleanName(String s) {  // printable ASCII only: that is all the display can show
  String o = "";
  for (unsigned int i = 0; i < s.length() && o.length() < 20; i++) {
    if (s[i] >= 32 && s[i] <= 126) o += s[i];
  }
  o.trim();
  return o;
}

String namesJson() {
  String o = "[";
  for (int i = 0; i < 8; i++) {
    if (i) o += ",";
    o += jsonStr(*antNames[i]);
  }
  return o + "]";
}

String stateJson() {
  int n = constrain(n_ant_max, 1, 8);
  unsigned long hz = (unsigned long)(QRG * 100.0f + 0.5f);
  String o = "{\"ant\":" + String(antenna_selected) + ",\"auto\":" + String(b_automatic ? 1 : 0) + ",\"n\":" + String(n);
  o += ",\"trx\":" + String(TRX_address) + ",\"qrg\":" + String(hz) + ",\"band\":" + String(BAND) + ",\"ver\":" + jsonStr(fwVersion);
  o += ",\"name\":" + jsonStr(ANT) + ",\"names\":" + namesJson();
  o += ",\"tuneExt\":" + String(digitalRead(TUNE_EXT_pin)) + ",\"tuning\":" + String(digitalRead(TUNE_REQ_pin));
  o += ",\"rssi\":" + String(WiFi.RSSI()) + ",\"bars\":" + String(wifiRssiBars()) + ",\"sdr\":" + String((rigClient && rigClient.connected()) ? 1 : 0) + "}";
  return o;
}

String configJson() {
  String o = "{\"n\":" + String(constrain(n_ant_max, 1, 8)) + ",\"trx\":" + String(TRX_address) + ",\"tuneroff\":" + String(TUNEROFF.toInt());
  o += ",\"names\":" + namesJson() + ",\"bands\":[";
  for (int i = 0; i < 12; i++) {
    if (i) o += ",";
    o += String(*bandAnt[i]);
  }
  return o + "]}";
}

void sendJson(const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}

// Every change must come as a POST with our own header: a page on another site cannot add it without a CORS preflight, which this server never answers.
bool webGuard() {
  if (server.method() != HTTP_POST || !server.hasHeader("X-AR")) {
    server.send(403, "text/plain", "forbidden");
    return false;
  }
  return true;
}

void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
}

// ************************************************ Firmware update *************************************************************************
void otaShowScreen(const char* msg, const char* sub) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(msg, 160, 120, 4);
  if (sub) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(sub, 160, 160, 2);
  }
  tft.setTextDatum(TL_DATUM);
}

void otaDrawProgress(unsigned long done, unsigned long total) {
  if (!total) return;
  int pct = (int)(done * 100UL / total);
  if (pct > 100) pct = 100;
  if (pct == otaPct) return;
  otaPct = pct;
  tft.drawRoundRect(40, 150, 240, 16, 3, TFT_WHITE);
  tft.fillRect(42, 152, (236 * pct) / 100, 12, TFT_GREEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextPadding(80);
  tft.drawString(String(pct) + "%", 160, 190, 4);
  tft.setTextPadding(0);
  tft.setTextDatum(TL_DATUM);
}

void otaFailScreen(const String& err) {
  otaShowScreen("Update failed", NULL);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(err, 160, 170, 2);
  tft.setTextDatum(TL_DATUM);
}

void handleUpdateDone() {
  if (otaDenied) {
    otaDenied = false;
    server.send(403, "text/plain", "forbidden");
    return;
  }
  server.sendHeader("Connection", "close");
  if (otaOk) {
    otaShowScreen("Update OK", "Restarting...");
    server.send(200, "text/plain", "ok");
    delay(800);
    ESP.restart();
  } else {
    if (!otaErr.length() || otaErr == "No Error") otaErr = "aborted";
    Update.abort();
    otaFailScreen(otaErr);
    server.send(500, "text/plain", otaErr);
    delay(2500);
    ESP.restart();
  }
}

void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaDenied = !server.hasHeader("X-AR");  // same rule as every other change
    if (otaDenied) return;
    otaUploadStarted = true;
    otaBusy = true;
    otaErr = "";
    otaOk = false;
    otaPct = -1;
    tuning = false;  // a tune request must not hang on while the loop is stopped
    digitalWrite(TUNE_REQ_pin, LOW);
    rigClient.stop();
    rigServer.stop();
    servers_started = false;
    img.deleteSprite();  // the antenna list sprite is the biggest block of RAM the update could use
    otaShowScreen("Updating...", NULL);
    WiFi.setSleep(false);
    server.client().setTimeout(60000);
    Update.abort();
    otaBeginOk = Update.begin(UPDATE_SIZE_UNKNOWN);
    if (!otaBeginOk) {
      otaErr = Update.getError() ? Update.errorString() : "start failed";
      Update.printError(Serial);
    }
  } else if (otaDenied) {
    return;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaBeginOk && !otaErr.length() && upload.currentSize) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        otaErr = Update.getError() ? Update.errorString() : "write failed";
        otaBeginOk = false;
        Update.printError(Serial);
      }
      otaDrawProgress(upload.totalSize, server.clientContentLength());
    }
    yield();
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaBeginOk && !otaErr.length()) {
      if (Update.end(true)) {
        otaOk = true;
      } else {
        otaErr = Update.getError() ? Update.errorString() : "finalize failed";
        Update.printError(Serial);
      }
    } else if (!otaErr.length()) {
      otaErr = "no image";
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (!otaOk && !otaErr.length()) otaErr = "connection lost";
  }
}

void handleState() {
  sendJson(stateJson());
}

void handleConfigGet() {
  sendJson(configJson());
}

void handleAnt() {
  if (!webGuard()) return;
  int n = server.arg("n").toInt();
  if (!b_automatic && n >= 1 && n <= constrain(n_ant_max, 1, 8)) {  // automatic mode would overrule it at once
    antenna_selected = n;
    sel_antenna();
  }
  sendJson(stateJson());
}

void handleAuto() {
  if (!webGuard()) return;
  setAutomatic(server.arg("v") == "1");
  sendJson(stateJson());
}

void handleTuner() {
  if (!webGuard()) return;
  if (!b_automatic && TRX_address != 0) setTuner(server.arg("v") == "1");
  sendJson(stateJson());
}

void handleTune() {
  if (!webGuard()) return;
  if (TRX_address != 0 && digitalRead(TUNE_EXT_pin)) setTuning(server.arg("v") == "1");
  sendJson(stateJson());
}

void handleConfigPost() {
  if (!webGuard()) return;
  String batch = "";
  int n = server.arg("n").toInt();
  if (n >= 1 && n <= 8) batch += "n_ant_max=" + String(n) + "\n";
  if (server.hasArg("trx")) {
    int t = server.arg("trx").toInt();
    if (t >= 0 && t <= 255) batch += "TRX_address=" + String(t) + "\n";
  }
  if (server.hasArg("tuneroff")) {
    int t = server.arg("tuneroff").toInt();
    if (t >= 0 && t <= 255) batch += "TUNEROFF=" + String(t) + "\n";
  }
  for (int i = 0; i < 8; i++) {
    String nm = cleanName(server.arg("nm" + String(i)));
    if (nm.length()) batch += "A" + String(i + 1) + "NAME=" + nm + "\n";
  }
  for (int i = 0; i < 12; i++) {
    int v = server.arg("b" + String(i)).toInt();
    if (v >= 1 && v <= 8) batch += String(BAND_KEY[i]) + "=" + String(v) + "\n";
  }
  writeConfigBatch(batch);
  if (antenna_selected > n_ant_max) antenna_selected = 1;  // fewer antennas than before
  sel_antenna();
  scrollposact = scrollposact - 1;  // makes the antenna list on the display redraw
  tft_update();
  sendJson(configJson());
}

void handleReboot() {
  if (!webGuard()) return;
  sendJson("{\"ok\":1}");
  delay(300);
  ESP.restart();
}

void handleReset() {  // factory values, but the unit stays in WLAN mode or it could not be reached again
  if (!webGuard()) return;
  SPIFFS.remove("/config.txt");
  createDefaultConfig();
  writeConfig("b_wifi_on=true");
  sendJson("{\"ok\":1}");
  delay(300);
  ESP.restart();
}

// All lines of the batch replace their old versions in a single write of the file.
void writeConfigBatch(const String& batch) {
  String updated = "";
  File file = SPIFFS.open("/config.txt", "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (line.length() == 0) continue;
      int eq = line.indexOf('=');
      String key = (eq >= 0) ? line.substring(0, eq) : line;
      if (batch.startsWith(key + "=") || batch.indexOf("\n" + key + "=") >= 0) continue;
      updated += line + "\n";
    }
    file.close();
  }
  updated += batch;
  file = SPIFFS.open("/config.txt", "w");
  if (file) {
    file.print(updated);
    file.close();
    readConfig();
  } else {
    Serial.println("Fehler beim Aktualisieren der Konfigurationsdatei");
  }
}

void handleADC() {
  float web_QRG = QRG / 10000;
  String response = "";
  if (BAND == 0) {
    response = String(web_QRG, 4) + ",Band not defined," + String(ANT);
  } else {
    response = String(web_QRG, 4) + "," + String(BAND) + "," + String(ANT);
  }

  server.send(200, "text/plain", response);  // Sende den kombinierten String zurück
}

void tft_update() {
  if (n_disp_page >= 3) return;  // the setup pages draw themselves
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  bool b_page_changed = false;
  if (n_disp_page != n_disp_page_old) {
    b_page_changed = true;
    scrollposact = scrollposact - 1;
    n_disp_page_old = n_disp_page;
    tft.fillScreen(COLOR_BG);
  }

  if (n_disp_page == 1) {
    if (b_page_changed == true) {
      b_page_changed = false;
      tft.drawString("QRG:", 0, 47, 4);
      tft.drawString("BAND:", 0, 78, 4);
      drawButtons_p1();
    }
    tft.setTextSize(1);
    // Print current selected QRG
    String QRG_txt = String(QRG / 10000, 4) + " MHz";
    if (QRG_txt.length() != QRG_len_old) {
      QRG_len_old = QRG_txt.length();
      tft.fillRect(80, 47, 190, 30, COLOR_BG);
    }
    tft.drawString(QRG_txt, 90, 47, 4);

    // Print current selected Band
    String BAND_txt = String(BAND) + "m";
    if (String(BAND).length() != BAND_len_old) {
      BAND_len_old = String(BAND).length();
      tft.fillRect(80, 76, 190, 28, COLOR_BG);
    }
    if (BAND == 0) {
      tft.drawString("-", 90, 78, 4);
    } else {
      tft.drawString(BAND_txt, 90, 78, 4);
    }
  }

  if (n_disp_page == 2) {  // This is display page 2: the connection
    if (b_page_changed == true) {
      b_page_changed = false;
      const char* labels[] = { "MODE:", "SSID:", "STATE:", "IP:", "RSSI:", "REMOTE:", "ANTENNA #:", "AUTOMODE:" };
      for (int i = 0; i < 8; i++) {
        tft.drawString(labels[i], 0, i * 20, 2);
        p2Last[i] = "\x01";  // forces the value to be drawn
      }
      drawButtons_p2();
    }
    bool up = b_wifi_on && WiFi.status() == WL_CONNECTED;
    p2Field(0, b_wifi_on ? "WLAN" : "Bluetooth");
    p2Field(1, b_wifi_on ? (wifi_ssid.length() ? wifi_ssid : String("-")) : String("-"));
    p2Field(2, wifiStateText());
    p2Field(3, up ? WiFi.localIP().toString() : String("-"));
    p2Field(4, up ? String(WiFi.RSSI()) + " dBm" : String("-"));
    p2Field(5, !up ? String("-") : (rigClient && rigClient.connected()) ? String("SDROxide connected") : "waiting, port " + String(RIG_PORT));
    p2Field(6, String(antenna_selected) + " " + ANT);
    p2Field(7, b_automatic ? "on" : "off");
    if (hintUntil != 0 && millis() > hintUntil) { hintUntil = 0; }
    tft.setTextColor(TFT_YELLOW, COLOR_BG);
    tft.setTextPadding(240);
    tft.drawString(hintUntil ? hintText : String(""), 0, 200, 2);
    tft.setTextPadding(0);
    tft.setTextColor(TFT_WHITE, COLOR_BG);
  }
}

void p2Field(int row, String text) {
  if (text.length() > 22) text = text.substring(0, 22);
  if (p2Last[row] == text) return;
  p2Last[row] = text;
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.setTextPadding(152);  // overwrites what the previous, longer value left behind
  tft.drawString(text, 88, row * 20, 2);
  tft.setTextPadding(0);
}

void scrollAntennas() {
  String AntArray[] = { A1NAME, A2NAME, A3NAME, A4NAME, A5NAME, A6NAME, A7NAME, A8NAME };
  scrolltarget = antenna_selected * textboxdiffy + textboxstartfromtop;
  if (scrolltarget != scrollposact) {
    img.fillSprite(COLOR_BG);
    for (int i = 0; i <= n_ant_max - 1; i++) {
      if (i + 1 == antenna_selected) {
        img.setTextColor(TFT_POPPY);
      } else {
        img.setTextColor(TFT_ORANGE);
      }
      img.drawString(AntArray[i], 10, scrollposact - i * textboxdiffy, 4);
    }
    img.drawRoundRect(0, 0, 260, 130, 8, TFT_BROWN);
    img.drawRoundRect(3, 45, 254, 40, 8, TFT_GOLD);
    img.pushSprite(0, 110);
    int scrolldiff = 5;
    int targeterror = abs(scrollposact - scrolltarget);
    scrolldiff = map(targeterror, 0, 15, 1, 5);
    if (scrollposact < scrolltarget) { scrollposact = scrollposact + scrolldiff; }
    if (scrollposact > scrolltarget) { scrollposact = scrollposact - scrolldiff; }
  }
}

void getsmeter() {      // Read S-meter level *(0000=S0, 0120=S9, 0241=S9+60dB)
  Serial2.write(0xFE);  // always twice 0xFE in the beginning
  Serial2.write(0xFE);
  Serial2.write(TRX_address);
  Serial2.write(0xE0);
  Serial2.write(0x15);  // command 15
  Serial2.write(0x02);  // sub command Read s-meter (02), command 15 02
  Serial2.write(0xFD);  // end sequence always 0xFD
  delay(20);
}

void getswr() {         // Read SWR meter level*( 0000=SWR1.0, 0048=SWR1.5,0080=SWR2.0, 0120=SWR3.0)
  Serial2.write(0xFE);  // always twice 0xFE in the beginning
  Serial2.write(0xFE);
  Serial2.write(TRX_address);
  Serial2.write(0xE0);
  Serial2.write(0x15);  // command 15
  Serial2.write(0x12);  // sub command Read swr (12), command 15 02
  Serial2.write(0xFD);  // end sequence always 0xFD
  delay(20);
}

void getqrg() {         // Read operating frequency
  Serial2.write(0xFE);  // always twice 0xFE in the beginning
  Serial2.write(0xFE);
  Serial2.write(TRX_address);
  Serial2.write(0xE0);
  Serial2.write(0x03);  // command 15
  Serial2.write(0xFD);  // end sequence always 0xFD
}

void sel_band(float t_QRG) {
  float b_QRG = t_QRG / 10;
  BAND = 0;
  if (b_QRG >= l160 && b_QRG <= h160) BAND = 160;
  if (b_QRG >= l80 && b_QRG <= h80) BAND = 80;
  if (b_QRG >= l60 && b_QRG <= h60) BAND = 60;
  if (b_QRG >= l40 && b_QRG <= h40) BAND = 40;
  if (b_QRG >= l30 && b_QRG <= h30) BAND = 30;
  if (b_QRG >= l20 && b_QRG <= h20) BAND = 20;
  if (b_QRG >= l17 && b_QRG <= h17) BAND = 17;
  if (b_QRG >= l15 && b_QRG <= h15) BAND = 15;
  if (b_QRG >= l12 && b_QRG <= h12) BAND = 12;
  if (b_QRG >= l10 && b_QRG <= h10) BAND = 10;
  if (b_QRG >= l6 && b_QRG <= h6) BAND = 6;
  if (b_QRG >= l4 && b_QRG <= h4) BAND = 4;
  sel_antenna();
}

void sel_antenna() {
  if (b_automatic == true) {
    if (BAND == 160) antenna_selected = a160m;
    if (BAND == 80) antenna_selected = a80m;
    if (BAND == 60) antenna_selected = a60m;
    if (BAND == 40) antenna_selected = a40m;
    if (BAND == 30) antenna_selected = a30m;
    if (BAND == 20) antenna_selected = a20m;
    if (BAND == 17) antenna_selected = a17m;
    if (BAND == 15) antenna_selected = a15m;
    if (BAND == 12) antenna_selected = a12m;
    if (BAND == 10) antenna_selected = a10m;
    if (BAND == 6) antenna_selected = a6m;
    if (BAND == 4) antenna_selected = a4m;
    // if (BAND == 0) antenna_selected = 0;
  }

  // Here we switch on/off the internal tuner of the icom TRX depending on the setting in the "Tuneroff" value
  int intValue = TUNEROFF.toInt();
  int bitPosition = antenna_selected - 1;
  int bitValue = (intValue >> bitPosition) & 1;
  if (bitValue == 1) {
    digitalWrite(TUNE_EXT_pin, HIGH);
    delay(1);
  }
  if (bitValue == 0) {
    digitalWrite(TUNE_EXT_pin, LOW);
    delay(1);
  }


  String AntArray[] = { A1NAME, A2NAME, A3NAME, A4NAME, A5NAME, A6NAME, A7NAME, A8NAME };
  ANT = AntArray[antenna_selected - 1];
  if (ANT != ANT_OLD) {
    ANT_OLD = ANT;
    sendalivemessage();
  }

  if (antenna_selected != antenna_driven) {  // a repeated clear-and-shift would blink every relay off
    antenna_driven = antenna_selected;
    switch (antenna_selected) {
      case 1:
        send2shftreg(1);
        break;
      case 2:
        send2shftreg(2);
        break;
      case 3:
        send2shftreg(4);
        break;
      case 4:
        send2shftreg(8);
        break;
      case 5:
        send2shftreg(16);
        break;
      case 6:
        send2shftreg(32);
        break;
      case 7:
        send2shftreg(64);
        break;
      case 8:
        send2shftreg(128);
        break;
    }
  }
}

void drawButtons_p1() {
  tft.setTextFont(4);
  key[1].initButton(&tft, KEY_X, 75, 50, 50, TFT_WHITE, COLOR_BG, TFT_WHITE, "", 1);  // ST Button
  key[1].setLabelDatum(0, 6, MC_DATUM);
  key[1].drawButton(false, b1_txt);

  key[0].initButton(&tft, KEY_X, 140, 50, 60, TFT_WHITE, COLOR_BG, TFT_WHITE, "", 1);  // UP Button
  key[0].setLabelDatum(0, 6, MC_DATUM);
  key[0].drawButton(false, b0_txt);

  key[2].initButton(&tft, KEY_X, 210, 50, 60, TFT_WHITE, COLOR_BG, TFT_WHITE, "", 1);  // DN Button
  key[2].setLabelDatum(0, 6, MC_DATUM);
  key[2].drawButton(false, b2_txt);

  tft.setTextFont(2);
  key[3].initButton(&tft, KEY_X, 20, 50, 30, TFT_WHITE, COLOR_BG, TFT_WHITE, "", 1);  // PAGE Button
  key[3].setLabelDatum(0, 5, MC_DATUM);
  key[3].drawButton(false, b3_p1_txt);

  tft.setTextFont(2);
  key[4].setLabelDatum(0, 5, MC_DATUM);
  if (b_automatic == true) {
    key[4].initButton(&tft, AUTO_CX, 20, AUTO_W, 30, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, "", 1);  // MODE button
    key[4].drawButton(false, "AUTOMATIC MODE ON");
  } else {
    key[4].initButton(&tft, AUTO_CX, 20, AUTO_W, 30, TFT_WHITE, TFT_GREY, TFT_WHITE, "", 1);  // MODE button
    key[4].drawButton(false, "AUTOMATIC MODE OFF");
  }

  drawLinkStatus(true);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
}

void drawButtons_p2() {
  tft.setTextFont(2);
  key[3].initButton(&tft, 280, 20, 80, 30, TFT_WHITE, COLOR_BG, TFT_WHITE, "", 1);
  key[3].setLabelDatum(0, 5, MC_DATUM);
  key[3].drawButton(false, b3_p2_txt);

  tft.setTextFont(1);
  key[6].initButton(&tft, 280, 70, 80, 50, TFT_WHITE, COLOR_BG, TFT_WHITE, "", 1);
  key[6].setLabelDatum(0, 5, MC_DATUM);
  if (b_wifi_on) {
    key[6].drawButton(false, b7_txt_on);
  } else {
    key[6].drawButton(false, b7_txt_off);
  }

  key[5].initButton(&tft, 280, 130, 80, 50, b_wifi_on ? TFT_WHITE : TFT_DARKGREY, COLOR_BG, b_wifi_on ? TFT_WHITE : TFT_DARKGREY, "", 1);  // greyed out unless in WLAN mode
  key[5].setLabelDatum(0, 5, MC_DATUM);
  key[5].drawButton(false, b5_txt);
}

void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin()) {
    Serial.println("Formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    } else {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char*)calData, 14) == 14)
          calDataOK = 1;
        Serial.println("Touchscreen calibration file found");
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(COLOR_BG);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, COLOR_BG);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, COLOR_BG);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, COLOR_BG, 15);

    tft.setTextColor(TFT_GREEN, COLOR_BG);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char*)calData, 14);
      f.close();
    }
  }
}

String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void send2shftreg(int data) {
  digitalWrite(CLR_pin, LOW);
  digitalWrite(CLR_pin, HIGH);
  shiftOut(AB_pin, CLK_pin, MSBFIRST, data);
}

// ************************************************ NETWORK ********************************************************************************
// ************************************************ Link icons (from RotorControl) ***********************************************
uint32_t lastLinkKey = 0xFFFFFFFF;

// Signal strength in three steps, the same thresholds as RotorControl.
int wifiRssiBars() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int r = WiFi.RSSI();
  if (r >= -55) return 3;
  if (r >= -70) return 2;
  if (r >= -85) return 1;
  return 0;
}

static void drawFanArc(int cx, int cy, int r, uint16_t color) {
  float t = -0.75f;
  int x0 = cx + (int)(r * sin(t));
  int y0 = cy - (int)(r * cos(t));
  for (int i = 1; i <= 8; i++) {
    t = -0.75f + (1.5f * i) / 8.0f;
    int x = cx + (int)(r * sin(t));
    int y = cy - (int)(r * cos(t));
    tft.drawLine(x0, y0, x, y, color);
    tft.drawLine(x0 + 1, y0, x + 1, y, color);
    x0 = x;
    y0 = y;
  }
}

static void drawBtIcon(int x, int y, uint16_t color) {
  int cx = x + 6;
  int y0 = y;
  int y1 = y + 16;
  int ym = y + 8;
  int r = x + 12;
  tft.drawLine(cx, y0, cx, y1, color);
  tft.drawLine(cx + 1, y0, cx + 1, y1, color);
  tft.drawLine(cx, y0, r, y0 + 4, color);
  tft.drawLine(r, y0 + 4, cx, ym, color);
  tft.drawLine(cx, ym, r, y1 - 4, color);
  tft.drawLine(r, y1 - 4, cx, y1, color);
  tft.drawLine(x, y0 + 4, r, y1 - 4, color);
  tft.drawLine(x, y1 - 4, r, y0 + 4, color);
}

static void drawWifiIcon(int cx, int cy, bool connected, int bars, uint16_t color) {
  uint16_t dim = TFT_DARKGREY;
  tft.fillCircle(cx, cy, 2, color);
  drawFanArc(cx, cy, 6, (connected && bars >= 1) ? color : dim);
  drawFanArc(cx, cy, 11, (connected && bars >= 2) ? color : dim);
  drawFanArc(cx, cy, 16, (connected && bars >= 3) ? color : dim);
}

static void drawRcIcon(int x, int y, bool connected) {  // small monitor: filled green while SDROxide is connected
  uint16_t col = connected ? TFT_GREEN : TFT_DARKGREY;
  tft.drawRoundRect(x, y, 18, 12, 2, col);
  if (connected) tft.fillRect(x + 3, y + 3, 12, 6, col);
  tft.drawFastVLine(x + 9, y + 12, 3, col);
  tft.drawFastHLine(x + 4, y + 15, 10, col);
}

// The two icons between the AUTOMATIC button and MISC on page 1: in Bluetooth mode the Bluetooth sign (green while a
// client is connected), in WLAN mode the signal strength and the monitor for SDROxide. Redrawn only when something changes.
void drawLinkStatus(bool force) {
  if (n_disp_page != 1) return;
  bool btCli = bt_active && SerialBT.hasClient();
  bool wifiOn = b_wifi_on;
  int st = wifiOn ? (int)WiFi.status() : (int)WL_DISCONNECTED;
  bool wifiOk = (st == WL_CONNECTED);
  int bars = wifiOk ? wifiRssiBars() : 0;
  bool rcCli = wifiOn && servers_started && rigClient && rigClient.connected();
  uint32_t key = (wifiOn ? 1u : 0u) | (wifiOk ? 2u : 0u) | ((uint32_t)(bars & 3) << 2) | (btCli ? 16u : 0u) | (rcCli ? 32u : 0u) | ((uint32_t)st << 8);
  if (!force && key == lastLinkKey) return;
  lastLinkKey = key;

  tft.fillRect(198, 2, 70, 36, COLOR_BG);
  if (wifiOn) {
    uint16_t col = TFT_CYAN;
    if (!wifiOk && (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL)) col = TFT_ORANGE;
    else if (!wifiOk) col = TFT_YELLOW;
    drawWifiIcon(214, 31, wifiOk, bars, col);
    drawRcIcon(240, 11, rcCli);
  } else {
    drawBtIcon(216, 11, btCli ? TFT_GREEN : TFT_CYAN);
  }
}

void loadWifiCreds() {
  netPrefs.begin("net", false);
  wifi_ssid = netPrefs.getString("ssid", "");
  wifi_pass = netPrefs.getString("pass", "");
  netPrefs.end();
}

void saveWifiCreds() {
  netPrefs.begin("net", false);
  netPrefs.putString("ssid", wifi_ssid);
  netPrefs.putString("pass", wifi_pass);
  netPrefs.end();
}

void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifi_reason = info.wifi_sta_disconnected.reason;
    Serial.println("WLAN disconnected, reason " + String(wifi_reason));
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    wifi_reason = 0;
    Serial.println("WLAN connected, IP " + WiFi.localIP().toString());
  }
}

void startWifi() {
  WiFi.persistent(false);  // credentials are ours, the WiFi library need not keep a copy
  WiFi.onEvent(onWifiEvent);
  WiFi.setHostname("AntennaRemote");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep makes a reply wait for the next beacon, and SDROxide gives up after a few seconds
  WiFi.setAutoReconnect(true);
  server.on("/", handleRoot);
  server.on("/readADC", handleADC);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/ant", HTTP_POST, handleAnt);
  server.on("/api/auto", HTTP_POST, handleAuto);
  server.on("/api/tuner", HTTP_POST, handleTuner);
  server.on("/api/tune", HTTP_POST, handleTune);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  static const char* wantedHeaders[] = { "X-AR" };
  server.collectHeaders(wantedHeaders, 1);
  wifiBegin();
}

void wifiBegin() {
  wifi_last_try = millis();
  if (wifi_ssid.length() == 0) return;
  Serial.println("WLAN: connecting to " + wifi_ssid);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.length() ? wifi_pass.c_str() : NULL);
}

void wifiConnectNow() {
  wifi_hold = false;
  wifi_reason = 0;
  WiFi.disconnect(false);
  wifiBegin();
}

String wifiStateText() {
  if (!b_wifi_on) return "WLAN off";
  if (WiFi.status() == WL_CONNECTED) return "Connected";
  if (wifi_ssid.length() == 0) return "No WLAN set up";
  if (wifi_reason == 201) return "SSID not found";
  if (wifi_reason == 202 || wifi_reason == 15 || wifi_reason == 204) return "Wrong password?";
  if (wifi_reason != 0) return "Failed, reason " + String(wifi_reason);
  return "Connecting...";
}

void startServers() {
  server.begin();
  rigServer.begin();
  rigServer.setNoDelay(true);
  servers_started = true;
  Serial.println("Servers up on " + WiFi.localIP().toString() + ": web on 80, AntennaRemote on " + String(RIG_PORT));
}

// Runs every loop: brings the servers up once connected and retries a connection that does not come.
void wifiTick() {
  if (!b_wifi_on) return;
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 500) return;
  lastCheck = millis();

  bool conn = (WiFi.status() == WL_CONNECTED);
  if (conn && !servers_started) startServers();
  if (!conn && !wifi_hold && wifi_ssid.length() && millis() - wifi_last_try > 20000) {
    Serial.println("WLAN: still not connected (" + wifiStateText() + "), trying again");
    WiFi.disconnect(false);
    wifiBegin();
  }
  if (conn != wifi_was_conn) {
    wifi_was_conn = conn;
    if (n_disp_page == 2) tft_update();
  }
}

// ******************************************** AntennaRemote 1 over TCP ******************************************************************
void rigPoll() {
  if (!servers_started) return;
  if (rigClient && !rigClient.connected()) rigClient.stop();
  if (rigClient && millis() - rigLastSeen > RIG_IDLE_MS) {
    Serial.println("Remote: client went quiet, dropped");
    rigClient.stop();
  }

  WiFiClient c = rigServer.available();
  if (c) {
    // SDROxide asks once a second, so a client quiet for two seconds is a dead connection that has not
    // been closed yet, and the one now knocking is the same program coming back: let it in.
    if (rigClient && rigClient.connected() && millis() - rigLastSeen < RIG_REPLACE_MS) {
      c.stop();  // a live client: one at a time, SDROxide reports this as "busy"
    } else {
      if (rigClient) rigClient.stop();
      rigClient = c;
      rigLine = "";
      rigLastSeen = millis();
      Serial.println("Remote: client connected");
      if (n_disp_page == 2) tft_update();
    }
  }

  while (rigClient && rigClient.connected() && rigClient.available()) {
    char ch = rigClient.read();
    rigLastSeen = millis();
    if (ch == '\n') {
      rigHandle(rigLine);
      rigLine = "";
    } else if (ch != '\r' && rigLine.length() < 64) {
      rigLine += ch;
    }
  }
}

void rigHandle(String line) {
  line.trim();
  if (line.length() == 0) return;
  String out;
  if (line == "v") {
    out = "AntennaRemote 1\nRPRT 0\n";
  } else if (line == "s") {
    out = "ant=" + String(antenna_selected) + " auto=" + String(b_automatic ? 1 : 0) + " band=" + String(BAND) + " n=" + String(constrain(n_ant_max, 1, 8)) + " name=" + ANT + "\nRPRT 0\n";
  } else if (line.startsWith("F ")) {
    unsigned long hz = strtoul(line.c_str() + 2, NULL, 10);
    if (hz == 0) {
      out = "RPRT -1\n";
    } else {
      rigSetFreq(hz);
      out = "RPRT 0\n";
    }
  } else if (line.startsWith("A ")) {  // choose an antenna by hand: manual mode with it
    int n = line.substring(2).toInt();
    if (n >= 1 && n <= constrain(n_ant_max, 1, 8)) {
      setAutomatic(false);
      antenna_selected = n;
      sel_antenna();
      out = "RPRT 0\n";
    } else {
      out = "RPRT -1\n";
    }
  } else if (line.startsWith("M ")) {  // automatic mode off / on
    String v = line.substring(2);
    v.trim();
    if (v == "0" || v == "1") {
      setAutomatic(v == "1");
      out = "RPRT 0\n";
    } else {
      out = "RPRT -1\n";
    }
  } else {
    out = "RPRT -1\n";
  }
  rigClient.print(out);
}

void rigSetFreq(unsigned long hz) {
  float q = hz / 100.0;  // the rest of this sketch counts in units of 100 Hz
  if (q == QRG) return;
  QRG = q;
  sel_band(q);
  tft_update();
}

// ************************************************ WLAN setup pages ***********************************************************************
const UiBtn nbBack = { 244, 4, 72, 34, "BACK" };
const UiBtn nbUp = { 244, 44, 72, 40, "UP" };
const UiBtn nbDn = { 244, 90, 72, 40, "DN" };
const UiBtn nbScan = { 244, 136, 72, 40, "RESCAN" };
const UiBtn nbHidden = { 244, 182, 72, 40, "HIDDEN" };
const uint16_t UI_FILL = 0x2104;

void drawBtn(const UiBtn& b, uint16_t fill) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 4, fill);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 4, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, fill);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2, 2);
  tft.setTextDatum(TL_DATUM);
}

bool inBtn(const UiBtn& b, uint16_t x, uint16_t y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

void setupTouch(bool pressed, uint16_t x, uint16_t y) {
  bool edge = pressed && !setupTouchWas;  // act once per tap
  setupTouchWas = pressed;
  if (!edge) return;
  if (n_disp_page == 3) netListTouch(x, y);
  else if (n_disp_page == 4) kbTouch(x, y);
}

void setupClose() {  // back to page 2, reconnect with what is saved
  n_disp_page = 2;
  wifiConnectNow();
  setupTouchWas = true;
  tft_update();
}

void connectTo(const String& s, const String& p) {
  wifi_ssid = s;
  wifi_pass = p;
  saveWifiCreds();
  n_disp_page = 2;
  wifiConnectNow();
  setupTouchWas = true;
  tft_update();
}

void netListOpen() {
  wifi_hold = true;
  n_disp_page = 3;
  n_disp_page_old = 3;
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString("Scanning for WLANs...", 10, 100, 4);
  WiFi.disconnect(false);
  delay(100);
  int n = WiFi.scanNetworks(false, true);
  netCount = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;  // hidden networks are entered by hand
    int dup = -1;
    for (int j = 0; j < netCount; j++) {
      if (netSsid[j] == s) dup = j;
    }
    if (dup >= 0) {
      if (WiFi.RSSI(i) > netRssi[dup]) netRssi[dup] = WiFi.RSSI(i);  // same name on several access points: keep the strongest
      continue;
    }
    if (netCount >= MAX_NETS) continue;
    netSsid[netCount] = s;
    netRssi[netCount] = WiFi.RSSI(i);
    netOpen[netCount] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    netCount++;
  }
  WiFi.scanDelete();
  for (int a = 0; a < netCount; a++) {  // strongest first
    for (int b = a + 1; b < netCount; b++) {
      if (netRssi[b] > netRssi[a]) {
        String ts = netSsid[a];
        netSsid[a] = netSsid[b];
        netSsid[b] = ts;
        int tr = netRssi[a];
        netRssi[a] = netRssi[b];
        netRssi[b] = tr;
        bool to = netOpen[a];
        netOpen[a] = netOpen[b];
        netOpen[b] = to;
      }
    }
  }
  Serial.println("WLAN scan: " + String(netCount) + " networks");
  netTop = 0;
  setupTouchWas = true;
  netListDraw();
}

void netListDraw() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString("Select your WLAN (" + String(netCount) + " found)", 4, 4, 2);
  if (netCount == 0) tft.drawString("No networks found", 10, 80, 2);
  for (int r = 0; r < 5; r++) {
    int i = netTop + r;
    if (i >= netCount) break;
    int y = 24 + r * 38;
    uint16_t fill = (netSsid[i] == wifi_ssid) ? TFT_DARKGREEN : UI_FILL;  // green: the one saved now
    tft.fillRoundRect(0, y, 238, 36, 4, fill);
    tft.setTextColor(TFT_WHITE, fill);
    String s = netSsid[i];
    if (s.length() > 19) s = s.substring(0, 19);
    tft.drawString(s, 6, y + 10, 2);
    tft.drawString((netOpen[i] ? String("open ") : String("")) + String(netRssi[i]), 172, y + 10, 2);
  }
  drawBtn(nbBack, UI_FILL);
  drawBtn(nbUp, UI_FILL);
  drawBtn(nbDn, UI_FILL);
  drawBtn(nbScan, UI_FILL);
  drawBtn(nbHidden, UI_FILL);
}

void netListTouch(uint16_t x, uint16_t y) {
  if (inBtn(nbBack, x, y)) {
    setupClose();
  } else if (inBtn(nbUp, x, y)) {
    netTop = (netTop >= 5) ? netTop - 5 : 0;
    netListDraw();
  } else if (inBtn(nbDn, x, y)) {
    if (netTop + 5 < netCount) netTop += 5;
    netListDraw();
  } else if (inBtn(nbScan, x, y)) {
    netListOpen();
  } else if (inBtn(nbHidden, x, y)) {
    kbOpen(0, "");
  } else if (x < 238 && y >= 24 && (y - 24) % 38 < 36) {
    int i = netTop + (y - 24) / 38;
    if (i < netCount && (y - 24) / 38 < 5) {
      pendingSsid = netSsid[i];
      if (netOpen[i]) {
        connectTo(pendingSsid, "");
      } else {
        kbOpen(1, "");
      }
    }
  }
}

void kbSet(KbKey& k, int x, int y, int w, int h, const char* label, int code) {
  k.x = x;
  k.y = y;
  k.w = w;
  k.h = h;
  strncpy(k.label, label, sizeof(k.label) - 1);
  k.label[sizeof(k.label) - 1] = 0;
  k.code = code;
}

int kbBuild(KbKey* k) {
  const char* L0[4] = { "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm" };
  const char* L2[4] = { "!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>/?", "~`.,'\"?" };
  const char** L = (kbLayout == 2) ? L2 : L0;
  int n = 0;
  for (int r = 0; r < 4; r++) {
    int len = strlen(L[r]);
    int y = 56 + r * 36;
    int x0 = (r == 3) ? 44 : (len == 9 ? 16 : 0);
    for (int i = 0; i < len; i++) {
      char c = L[r][i];
      if (kbLayout == 0 && kbShift && c >= 'a' && c <= 'z') c = c - 32;
      char lab[2] = { c, 0 };
      kbSet(k[n++], x0 + i * 32, y, 30, 34, lab, c);
    }
  }
  if (kbLayout == 0) kbSet(k[n++], 0, 164, 42, 34, "Shift", KC_SHIFT);
  kbSet(k[n++], 270, 164, 50, 34, "DEL", KC_DEL);
  kbSet(k[n++], 0, 200, 58, 34, kbLayout == 2 ? "abc" : "?123", KC_SYM);
  kbSet(k[n++], 60, 200, 58, 34, kbShow ? "Hide" : "Show", KC_SHOW);
  kbSet(k[n++], 120, 200, 98, 34, "SPACE", ' ');
  kbSet(k[n++], 220, 200, 48, 34, "Esc", KC_CANCEL);
  kbSet(k[n++], 270, 200, 50, 34, "OK", KC_OK);
  return n;
}

void kbDrawKeys() {
  KbKey k[48];
  int n = kbBuild(k);
  tft.fillRect(0, 54, 320, 186, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < n; i++) {
    uint16_t fill = UI_FILL;
    if (k[i].code == KC_OK) fill = TFT_DARKGREEN;
    if (k[i].code == KC_CANCEL || k[i].code == KC_DEL) fill = TFT_MAROON;
    if (k[i].code == KC_SHIFT && kbShift) fill = TFT_ORANGE;
    tft.fillRoundRect(k[i].x, k[i].y, k[i].w, k[i].h, 4, fill);
    tft.setTextColor(TFT_WHITE, fill);
    tft.drawString(k[i].label, k[i].x + k[i].w / 2, k[i].y + k[i].h / 2, (strlen(k[i].label) > 4 && k[i].w < 60) ? 1 : 2);
  }
  tft.setTextDatum(TL_DATUM);
}

void kbDrawField() {
  String shown = "";
  if (kbShow) {
    shown = kbText;
  } else {
    for (unsigned int i = 0; i < kbText.length(); i++) shown += '*';
  }
  if (shown.length() > 34) shown = shown.substring(shown.length() - 34);  // the end is what is being typed
  tft.drawRect(0, 22, 320, 30, TFT_WHITE);
  tft.fillRect(1, 23, 318, 28, COLOR_BG);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString(shown + "_", 6, 29, 2);
}

void kbDraw() {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  String title = (kbTarget == 0) ? String("Name of the WLAN (SSID)") : "Password for " + pendingSsid;
  if (title.length() > 36) title = title.substring(0, 36);
  tft.drawString(title, 4, 3, 2);
  kbDrawField();
  kbDrawKeys();
}

void kbOpen(int target, const String& initial) {
  kbTarget = target;
  kbText = initial;
  kbLayout = 0;
  kbShift = false;
  kbShow = (target == 0);
  n_disp_page = 4;
  n_disp_page_old = 4;
  setupTouchWas = true;
  kbDraw();
}

void kbTouch(uint16_t x, uint16_t y) {
  KbKey k[48];
  int n = kbBuild(k);
  int maxLen = (kbTarget == 0) ? 32 : 63;
  for (int i = 0; i < n; i++) {
    if (x < k[i].x || x >= k[i].x + k[i].w || y < k[i].y || y >= k[i].y + k[i].h) continue;
    int c = k[i].code;
    if (c > 0) {
      if ((int)kbText.length() < maxLen) kbText += (char)c;
      if (kbShift) {
        kbShift = false;  // shift is for one letter
        kbDrawKeys();
      }
      kbDrawField();
    } else if (c == KC_SHIFT) {
      kbShift = !kbShift;
      kbDrawKeys();
    } else if (c == KC_DEL) {
      if (kbText.length()) kbText.remove(kbText.length() - 1);
      kbDrawField();
    } else if (c == KC_SYM) {
      kbLayout = (kbLayout == 2) ? 0 : 2;
      kbShift = false;
      kbDrawKeys();
    } else if (c == KC_SHOW) {
      kbShow = !kbShow;
      kbDrawKeys();
      kbDrawField();
    } else if (c == KC_CANCEL) {
      n_disp_page = 3;
      n_disp_page_old = 3;
      netListDraw();
    } else if (c == KC_OK) {
      if (kbTarget == 0) {
        if (kbText.length() == 0) return;
        pendingSsid = kbText;
        kbOpen(1, "");
      } else {
        connectTo(pendingSsid, kbText);
      }
    }
    return;
  }
}