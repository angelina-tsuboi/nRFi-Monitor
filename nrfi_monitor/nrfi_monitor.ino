/*
 
    _  _    ____ ____ _    _  _ ____ _  _ _ ___ ____ ____ 
    |\ | __ |__/ |___ |    |\/| |  | |\ | |  |  |  | |__/ 
    | \|    |  \ |    |    |  | |__| | \| |  |  |__| |  \ 
                                                      
   A 2.4GHz band and WiFi analyzer toolkit made with the D1 Mini and NRF24L01
   https://github.com/angelina-tsuboi/nRFi-Monitor

   By Angelina Tsuboi (angelinatsuboi.net)

*/

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "SH1106Wire.h"
#include <ESP8266WiFi.h>
#include "graphics.h"

//
// Hardware configuration
//
#define CSN D8
#define CE D3
#define BUTTON_INPUT D4
#define BACK_BUTTON_INPUT 1

RF24 radio(CE, CSN);
SH1106Wire display(0x3C, D2, D1);

const unsigned long eventTime_1_Button = 0;
const unsigned long eventTime_2_Scan = 10;

unsigned long previousTime_1 = 0;
unsigned long previousTime_2 = 0;

byte count;
byte sensorArray[128];
byte drawHeight;

char filled = 'F';
char drawDirection = 'R';
char slope = 'W';

int lState = 0;
int rState = 0;
int menuPointer = 0;
int scannerMenuPointer = 0;
int wifiScannedNumber = 0;
int wifiScannedIndex = 0;
bool wifiScanComplete = false;
bool vicinityScanComplete = false;
int vicinityAPAmount = 0;

const char *options[3] = {
  "Traffic Analyzer",
  "WiFi Scanner",
  "Vicinity Detector"
};


const char *scannerOptions[3] = {
  "Automatic",
  "Start Scan",
  "   Back"
};

int displayState = 0;
bool wifiScanStarted = false;
bool wifiVicinityScanStarted = false;
int vicinityMenuSelector = 0;

int selectedChannel = 0;

//
// Define Channel Addresses
//
const uint64_t pipes[2] = { 0xABCDABCD71LL, 0x544d52687CLL };   // Radio pipe addresses for the 2 nodes to communicate.

//
// Define Constants
//
const uint8_t num_channels = 126;
const int num_reps = 100;

static const char rf24_datarate_e_str_0[] PROGMEM = "1MBPS";
static const char rf24_datarate_e_str_1[] PROGMEM = "2MBPS";
static const char rf24_datarate_e_str_2[] PROGMEM = "250KBPS";
static const char * const rf24_datarate_e_str_P[] PROGMEM = {
  rf24_datarate_e_str_0,
  rf24_datarate_e_str_1,
  rf24_datarate_e_str_2,
};

static const char rf24_model_e_str_0[] PROGMEM = "nRF24L01";
static const char rf24_model_e_str_1[] PROGMEM = "nRF24L01+";
static const char * const rf24_model_e_str_P[] PROGMEM = {
  rf24_model_e_str_0,
  rf24_model_e_str_1,
};

static const char rf24_crclength_e_str_0[] PROGMEM = "Disabled";
static const char rf24_crclength_e_str_1[] PROGMEM = "8 bits";
static const char rf24_crclength_e_str_2[] PROGMEM = "16 bits" ;
static const char * const rf24_crclength_e_str_P[] PROGMEM = {
  rf24_crclength_e_str_0,
  rf24_crclength_e_str_1,
  rf24_crclength_e_str_2,
};

static const char rf24_pa_dbm_e_str_0[] PROGMEM = "PA_MIN";
static const char rf24_pa_dbm_e_str_1[] PROGMEM = "PA_LOW";
static const char rf24_pa_dbm_e_str_2[] PROGMEM = "PA_HIGH";
static const char rf24_pa_dbm_e_str_3[] PROGMEM = "PA_MAX";
static const char * const rf24_pa_dbm_e_str_P[] PROGMEM = {
  rf24_pa_dbm_e_str_0,
  rf24_pa_dbm_e_str_1,
  rf24_pa_dbm_e_str_2,
  rf24_pa_dbm_e_str_3,
};

//
// Define Variables
//
uint8_t values[num_channels];
uint8_t zeroVal = 0xf;
uint8_t addr_width = 5;
bool p_variant = false;

// WiFi Scanner Config
const int RSSI_MAX = -50; // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

const int displayEnc = 1; // set to 1 to dispaly Encryption or 0 not to display

//
// Setup
//

void setup(void)
{
  // Setup and configure rf radio
  radio.begin();
  radio.setAutoAck(false);

  // Get into standby mode
  radio.startListening();
  delayMicroseconds(130);
  radio.stopListening();

  pinMode(BUTTON_INPUT, INPUT_PULLUP);
  //  pinMode(BACK_BUTTON_INPUT, INPUT);
  pinMode(BACK_BUTTON_INPUT, FUNCTION_3);  // TX
  pinMode(BACK_BUTTON_INPUT, INPUT_PULLUP);

  //  Set up OLED
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.drawXbm(5, 5, logo_width, logo_height, logo);
  display.drawString(17, 50, "By Angelina Tsuboi");
  display.display();

  // Display Radio Configuration
  getRadioDetails();
  radio.setDataRate(RF24_1MBPS);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(2000);
}

//
// Main Loop
//

void scannerMenuInput () {
  lState, rState = HIGH;
  lState = digitalRead(BUTTON_INPUT);
  rState = digitalRead(BACK_BUTTON_INPUT);
  digitalWrite(BACK_BUTTON_INPUT, LOW);

  // uni-directional menu scroller (left = navigation, right = selection)
  if (lState == LOW && scannerMenuPointer == 2) {
    scannerMenuPointer = 0;
    delay(200);
  } else if (lState == LOW) {
    scannerMenuPointer += 1;
    delay(200);
  }


  if (rState == LOW && displayState == 2) {
    if (scannerMenuPointer == 2) {
      displayState = 0;
    } else if (scannerMenuPointer == 0) {
      if (scannerOptions[0] == "Automatic") {
        scannerOptions[0] = " Manual";
      } else {
        scannerOptions[0] = "Automatic";
      }
    } else if (scannerMenuPointer == 1) {
      wifiScanComplete = false;
      if (scannerOptions[0] == " Manual") {
        displayState = 4;
      } else {
        displayState = 5;
      }

    }
    delay(500);
  }
}

void manualScannerInput() {
  lState = digitalRead(BUTTON_INPUT);
  rState = digitalRead(BACK_BUTTON_INPUT);

  if (lState == LOW) {
    displayState = 2;
    delay(300);
  }

  if (rState == LOW) {
    wifiScannedIndex++;
    if (wifiScannedIndex == wifiScannedNumber) {
      wifiScannedIndex = 0;
    }
    delay(300);
  }
}

void autoScannerInput() {
  lState = digitalRead(BUTTON_INPUT);
  rState = digitalRead(BACK_BUTTON_INPUT);

  if (lState == LOW) {
    displayState = 2;
    delay(300);
  }
}

void vicinityDisplayInput() {
  lState = digitalRead(BUTTON_INPUT);

  if (lState == LOW) {
    displayState = 3;
    wifiVicinityScanStarted = false;
    delay(300);
  }
}

void vicinityInput() {
  lState = digitalRead(BUTTON_INPUT);
  rState = digitalRead(BACK_BUTTON_INPUT);

  if (lState == LOW) {
    vicinityMenuSelector++;
    if (vicinityMenuSelector > vicinityAPAmount) {
      vicinityMenuSelector = 0;
    }
    delay(300);
  }

   if (rState == LOW) {

    if (vicinityMenuSelector == vicinityAPAmount) {
       displayState = 0;
    } else {
       displayState = 6;
    }
   
    delay(300);
  }
}

void menuButtonPress() {
  lState = digitalRead(BUTTON_INPUT);
  rState = digitalRead(BACK_BUTTON_INPUT);

  // uni-directional menu scroller (left = navigation, right = selection)
  if (lState == LOW && menuPointer == 2) {
    menuPointer = 0;
    delay(300);
  } else if (lState == LOW) {
    menuPointer += 1;
    delay(300);
  }

  if (rState == LOW && displayState == 0) {
    displayState = menuPointer + 1;
    if (displayState == 2) {
      scannerMenuPointer = 0;
    }
    if (displayState == 3) {
      wifiVicinityScanStarted = false;
    }
    //Serial.print(displayState);
    delay(300);
  }
}

void displayMenu() {
  for (int i = 0; i < 3; i++) {
    if (menuPointer == i) {
      char buf[2048];
      display.fillRect(0, 16 + (17 * i), 127, 13);
      display.setColor(BLACK);
      display.drawString(28, 16 + (17 * i), options[i]);
      display.setColor(WHITE);
    } else {
      display.drawString(28, 16 + (17 * i), options[i]);
    }
  }
}

void displayScannerMenu() {
  display.clear();
  display.drawString(33, 0, "WiFi Scanner");
  display.drawLine(0, 12, 127, 12);
  for (int i = 0; i < 3; i++) {
    if (scannerMenuPointer == i) {
      char buf[2048];
      if (i == 0) {
        display.fillRect(20, 16 + (17 * i), 87, 14);
        display.drawLine(20, 16, 20, 29);
        display.fillTriangle(8, 22, 11, 18, 11, 26);
        display.fillTriangle(119, 22, 116, 18, 116, 26);
        display.drawLine(107, 16, 107, 29);
      } else {
        display.fillRect(0, 16 + (17 * i), 127, 13);
      }
      display.setColor(BLACK);
      display.drawString(38, 16 + (17 * i), scannerOptions[i]);
      display.setColor(WHITE);
    } else {
      display.drawString(38, 16 + (17 * i), scannerOptions[i]);
    }

    if (scannerMenuPointer != 0) {
      display.drawLine(20, 16, 20, 29);
      display.fillTriangle(8, 22, 11, 18, 11, 26);
      display.fillTriangle(119, 22, 116, 18, 116, 26);
      display.drawLine(107, 16, 107, 29);
    }
  }
  display.display();
  delay(0);
}

void printMenuScreen() {
  display.drawString(35, 0, "n-RFi Monitor");
  displayMenu();
}

int roundNum(int num)
{
     int rem = num % 10;
     return rem >= 5 ? (num - rem + 10) : (num - rem);
}

void displayVicinityView() {
    display.drawLine(0, 12, 127, 12);
    display.drawLine(20, 0, 20, 12);
    display.fillTriangle(8, 5, 11, 2, 11, 8);
    display.drawString(30, 0, "Vicinity Display");
    display.drawString(0, 14, "SSID: "); display.drawString(30, 14, WiFi.SSID(vicinityMenuSelector));
    display.drawString(0, 26, "RSSI: "); display.drawString(30, 26, String(WiFi.RSSI(vicinityMenuSelector)) + " dBm");
    display.drawString(0, 38, "RSSI: "); display.drawString(30, 38, String(dBmtoPercentage(WiFi.RSSI(vicinityMenuSelector))) + "%");
    int rounded = roundNum(dBmtoPercentage(WiFi.RSSI(vicinityMenuSelector))) / 10;
    String meter = rssiMeter(rounded);
    display.drawString(0, 50, meter); // [#####-----]   
}

String rssiMeter (int amount) {
  String meter = "[";
  for (int i = 0; i < amount; i++) {
     meter += "#";
  }
  for (int i = 0; i < 10 - amount; i++) {
     meter += "-";
  }
  meter += "]";
  return meter;
}

void loop()
{
  if (displayState == 0) {
    menuButtonPress();
    display.clear();
    printMenuScreen();
    display.display();
    delay(0);
  } else if (displayState == 1) { //TODO: either make concurrent processes or restyle menu to be synchronous
    // Clear measurement values
    checkTrafficAnalyzerInput();
    memset(values, 0, sizeof(values));

    // Scan all channels num_reps times
    int rep_counter = num_reps;
    while (rep_counter--)
    {
      int i = num_channels;
      while (i--)
      {
        // Select this channel
        if (selectedChannel == 0) {
          radio.setChannel(i);
        } else {
          radio.setChannel(selectedChannel + 1);
        }

        radio.startListening();
        delayMicroseconds(130);
        radio.stopListening();

        // Did we get a carrier?
        if ( radio.testCarrier() ) {
          ++values[i];
        }
      }
      checkTrafficAnalyzerInput();
      yield();
    }

    outputChannels();

  } else if (displayState == 2) { 
    scannerMenuInput();
    displayScannerMenu();
  } else if (displayState == 3) { // vicinity detector
    display.clear();
    vicinityInput();
    updateVicinityToolbar();
    displayVicinityWiFiList();
    display.display();
  } else if (displayState == 4) { // WiFi scanner manual display
    display.clear();
    manualScannerInput();
    updateWifiScannerManualToolbar();
    scanWiFi();
    displayScannedWiFi();
    display.display();
    delay(0);
  } else if (displayState == 5) { // WiFi scanner automatic display
    display.clear();
    autoScannerInput();
    displayAutoScannedWiFi();
    delay(0);
  } else if (displayState == 6) { // WiFi vicinity RSSID display
      display.clear();
      vicinityDisplayInput();
      displayVicinityView();
      display.display();
      delay(0);
   }
}

void scanWiFi() {
  if (wifiScanComplete == false) {
    int n = WiFi.scanNetworks();
    wifiScannedNumber = n;
    wifiScanComplete = true;
  }
}

void displayAutoScannedWiFi() {
  updateWifiScannerAutoToolbar();
  int n = WiFi.scanNetworks();
  if (n == 0) {
    display.clear();
    display.drawString(35, 35, "No Networks Found");
    display.display();
  } else {
    n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
      display.clear();
      updateWifiScannerAutoToolbar();
      String bssid;
      if (WiFi.BSSIDstr(i).length() < 18) {
        bssid = WiFi.BSSIDstr(i);
      }
      else if (WiFi.BSSIDstr(i).length() > 1) {
        bssid = WiFi.BSSIDstr(i).substring(0, 17 ) + "...";
      }
      display.drawString(0, 14, "SSID: "); display.drawString(30, 14, WiFi.SSID(i));
      display.drawString(0, 22, "CH: "); display.drawString(30, 22, String(WiFi.channel(i)));
      display.drawString(0, 30, "RSSI: "); display.drawString(30, 30, String(WiFi.RSSI(i)) + " dBm");
      display.drawString(0, 38, "RSSI: "); display.drawString(30, 38, String(dBmtoPercentage(WiFi.RSSI(i))) + "%");
      display.drawString(0, 46, "MAC: "); display.drawString(30, 46, bssid);
      display.drawString(0, 54, "ENC: "); display.drawString(30, 54, encType(i));
      delay(100); // delay to see packets longer
      autoScannerInput();
      display.display();
    }
    WiFi.scanDelete();
  }
}

void displayVicinityWiFiList() {
  if (wifiVicinityScanStarted == false) {
    vicinityAPAmount = WiFi.scanNetworks();
    if (vicinityAPAmount > 4) {
      vicinityAPAmount = 4;
    }
    wifiVicinityScanStarted = true;
  }

  for (int i = 0; i < vicinityAPAmount + 1; ++i) {
    if (vicinityMenuSelector == i) {
      display.fillRect(0, 14 + (11 * i), 127, 13);
      display.setColor(BLACK);
      if (i == vicinityAPAmount) {
        display.drawString(5, (14 + (11 * i)), "BACK");
      } else {
        display.drawString(5, (14 + (11 * i)), WiFi.SSID(i));
      }
      display.setColor(WHITE);
    } else {
      if (i == vicinityAPAmount) {
        display.drawString(5, (14 + (11 * i)), "BACK");
      } else {
        display.drawString(5, (14 + (11 * i)), WiFi.SSID(i));
      }
    }
  }
  // WiFi.scanDelete(); in back button
}

void displayScannedWiFi() { //wifiScannedNumber
  String bssid;
  if (WiFi.BSSIDstr(wifiScannedIndex).length() < 18) {
    bssid = WiFi.BSSIDstr(wifiScannedIndex);
  }
  else if (WiFi.BSSIDstr(wifiScannedIndex).length() > 1) {
    bssid = WiFi.BSSIDstr(wifiScannedIndex).substring(0, 17 ) + "...";
  }


  if (wifiScannedNumber == 0) {
    display.drawString(35, 35, "No Networks Found");
  } else {
    display.drawString(0, 14, "SSID: "); display.drawString(30, 14, WiFi.SSID(wifiScannedIndex));
    display.drawString(0, 22, "CH: "); display.drawString(30, 22, String(WiFi.channel(wifiScannedIndex)));
    display.drawString(0, 30, "RSSI: "); display.drawString(30, 30, String(WiFi.RSSI(wifiScannedIndex)) + " dBm");
    display.drawString(0, 38, "RSSI: "); display.drawString(30, 38, String(dBmtoPercentage(WiFi.RSSI(wifiScannedIndex))) + "%");
    display.drawString(0, 46, "MAC: "); display.drawString(30, 46, bssid);
    display.drawString(0, 54, "ENC: "); display.drawString(30, 54, encType(wifiScannedIndex));
  }
}

// TODO: LOADING SCREEN

void checkTrafficAnalyzerInput() {
  pinMode(BACK_BUTTON_INPUT, INPUT_PULLUP);
  int increase = digitalRead(BACK_BUTTON_INPUT);
  int back = digitalRead(BUTTON_INPUT);

  if (back == LOW) {
    displayState = 0;
    delay(300);
  }

  if (increase == LOW) {
    selectedChannel++;
    if (selectedChannel >= 13) selectedChannel = 0;
    delay(300);
  }
}

void updateTrafficAnalyzerToolbar() {
  display.drawLine(0, 12, 127, 12);
  display.drawLine(20, 0, 20, 12);
  display.fillTriangle(8, 5, 11, 2, 11, 8);
  display.drawLine(107, 0, 107, 12);
  display.drawString(116, 0, "+");
  //display.drawString(45, 0, (String)channelOptions[selectedChannel]);
  if (selectedChannel == 0) {
    display.drawString(55, 0, "ALL");
  } else {
    display.drawString(45, 0, "Channel: " + String(selectedChannel));
  }
}

void updateVicinityToolbar() {
  display.drawLine(0, 12, 127, 12);
  display.drawString(33, 0, "Select AP");
}

void updateWifiScannerManualToolbar() {
  display.drawLine(0, 12, 127, 12);
  display.drawLine(20, 0, 20, 12);
  display.fillTriangle(8, 5, 11, 2, 11, 8);
  display.drawLine(107, 0, 107, 12);
  display.drawString(116, 0, "+");
  display.drawString(50, 0, String(wifiScannedIndex + 1) + " / " + String(wifiScannedNumber));
}

void updateWifiScannerAutoToolbar() {
  display.drawLine(0, 12, 127, 12);
  display.drawLine(20, 0, 20, 12);
  display.fillTriangle(8, 5, 11, 2, 11, 8);
  display.drawString(35, 0, "WiFi Scanner");
}


// WiFi Scanner methods
String encType(int id) {

  String type;
  if (WiFi.encryptionType(id) == ENC_TYPE_WEP) {
    type = " WEP";
  } else if (WiFi.encryptionType(id) == ENC_TYPE_TKIP) {
    type = "WPA / PSK";
  } else if (WiFi.encryptionType(id) == ENC_TYPE_CCMP) {
    type = "WPA2 / PSK";
  } else if (WiFi.encryptionType(id) == ENC_TYPE_AUTO) {
    type = "WPA / WPA2 / PSK";
  } else if (WiFi.encryptionType(id) == ENC_TYPE_NONE) {
    type = "<<OPEN>>";
  }
  return type;
  //1:  ENC_TYPE_WEP – WEP
  //2 : ENC_TYPE_TKIP – WPA / PSK
  //4 : ENC_TYPE_CCMP – WPA2 / PSK
  //7 : ENC_TYPE_NONE – open network
  //8 : ENC_TYPE_AUTO – WPA / WPA2 / PSK
}

int dBmtoPercentage(int dBm)
{
  int quality;
  if (dBm <= RSSI_MIN)
  {
    quality = 0;
  }
  else if (dBm >= RSSI_MAX)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (dBm + 100);
  }

  return quality;
}//dBmtoPercentage

void outputChannels()
{
  display.clear();
  updateTrafficAnalyzerToolbar();
  checkTrafficAnalyzerInput();

  for (int i = 0; i < 64; i++) {
    //Serial.println(values[i]);
    display.fillRect((1 + (i * 2)), (60 - (values[i] * 5)), 2, (values[i] * 5) + 7); // adjust scaling as needed
    display.print(60 - values[i]);
  }
  display.display();
}

///////////////////////////////////////////////////////////////////////////////////////
//   These functions have been extracted from the RF24 library
//   and modified to work on the ESP8266 under the Arduino framework

void getRadioDetails()
{
  //Serial.println(); Serial.println();
  //printStatus(spiTrans(NOP));
  printAddressRegister("RX_ADDR_P0-1\t", RX_ADDR_P0, 2);
  printByteRegister("RX_ADDR_P2-5\t", RX_ADDR_P2, 4);
  printAddressRegister("TX_ADDR\t\t", TX_ADDR, 1);
  printByteRegister("RX_PW_P0-6\t\t", RX_PW_P0, 6);
  printByteRegister("EN_AA\t\t", EN_AA, 1);
  printByteRegister("EN_RXADDR\t\t", EN_RXADDR, 1);
  printByteRegister("RF_CH\t\t", RF_CH, 1);
  printByteRegister("RF_SETUP\t\t", RF_SETUP, 1);
  printByteRegister("CONFIG\t\t", NRF_CONFIG, 1);
  printByteRegister("DYNPD/FEATURE\t", DYNPD, 2);
  String statTemp = rf24_datarate_e_str_P[getDataRate()];
  //Serial.printf("Data Rate\t\t= %s\r\n", statTemp.c_str());
  statTemp = rf24_model_e_str_P[isPVariant()];
  //Serial.printf("Model\t\t= %s\r\n", statTemp.c_str());
  statTemp = rf24_crclength_e_str_P[getCRCLength()];
  //Serial.printf("CRC Length\t\t= %s\r\n", statTemp.c_str());
  statTemp = rf24_pa_dbm_e_str_P[getPALevel()];
  //Serial.printf("PA Power\t\t= %s\r\n", statTemp.c_str());
  //Serial.println(); Serial.println();
}

uint8_t spiTrans(uint8_t cmd)
{
  uint8_t status;
  beginTransaction();
  status = SPI.transfer( cmd );
  endTransaction();
  return status;
}

void printStatus(uint8_t status)
{
  printf_P(PSTR("STATUS\t\t= 0x%02x RX_DR=%x TX_DS=%x MAX_RT=%x RX_P_NO=%x TX_FULL=%x\r\n"),
           status,
           (status & _BV(RX_DR)) ? 1 : 0,
           (status & _BV(TX_DS)) ? 1 : 0,
           (status & _BV(MAX_RT)) ? 1 : 0,
           ((status >> RX_P_NO) & 0x07),
           (status & _BV(TX_FULL)) ? 1 : 0
          );
}

void beginTransaction()
{
  digitalWrite(CSN, LOW);
}

void endTransaction()
{
  digitalWrite(CSN, HIGH);
}

void printAddressRegister(const char* name, uint8_t reg, uint8_t qty)
{
  char tempBuff[20];
  String valueBuf;
  valueBuf += name;
  valueBuf += "=";
  while (qty--)
  {
    uint8_t buffer[addr_width];
    readRegister(reg++, buffer, sizeof buffer);
    valueBuf += " 0x";
    uint8_t* bufptr = buffer + sizeof buffer;
    while ( --bufptr >= buffer ) {
      sprintf(tempBuff, "%02x" , *bufptr);
      valueBuf += tempBuff;
    }
  }
  valueBuf += "\r\n";
  //Serial.print(valueBuf);
}

uint8_t readRegister(uint8_t reg, uint8_t* buf, uint8_t len)
{
  uint8_t status;
  beginTransaction();
  status = SPI.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  while ( len-- ) {
    *buf++ = SPI.transfer(0xff);
  }
  endTransaction();
  return status;
}

void printByteRegister(const char* name, uint8_t reg, uint8_t qty)
{
  char tempBuff[20];
  String valueBuf;
  valueBuf += name;
  valueBuf += "=";
  while (qty--) {
    sprintf(tempBuff , " 0x%02x" , readRegister(reg++));
    valueBuf += tempBuff;
  }
  valueBuf += "\r\n";
  //Serial.print(valueBuf);
}

uint8_t readRegister(uint8_t reg)
{
  uint8_t result;
  beginTransaction();
  SPI.transfer( R_REGISTER | ( REGISTER_MASK & reg ) );
  result = SPI.transfer(0xff);
  endTransaction();
  return result;
}

rf24_datarate_e getDataRate( void )
{
  rf24_datarate_e result ;
  uint8_t dr = readRegister(RF_SETUP) & (_BV(RF_DR_LOW) | _BV(RF_DR_HIGH));

  // switch uses RAM (evil!)
  // Order matters in our case below
  if ( dr == _BV(RF_DR_LOW) )
  {
    // '10' = 250KBPS
    result = RF24_250KBPS ;
  }
  else if ( dr == _BV(RF_DR_HIGH) )
  {
    // '01' = 2MBPS
    result = RF24_2MBPS ;
  }
  else
  {
    // '00' = 1MBPS
    result = RF24_1MBPS ;
  }
  return result ;
}

bool isPVariant(void)
{
  p_variant = false ;
  if ( radio.setDataRate( RF24_250KBPS ) )
  {
    p_variant = true ;
  }
  byte regRead = readRegister(RF_SETUP);
  if ( regRead == 0b00001110 )    // register default for nRF24L01P
  {
    p_variant = true ;
  }
  return p_variant ;
}

rf24_crclength_e getCRCLength(void)
{
  rf24_crclength_e result = RF24_CRC_DISABLED;

  uint8_t config = readRegister(NRF_CONFIG) & ( _BV(CRCO) | _BV(EN_CRC)) ;
  uint8_t AA = readRegister(EN_AA);

  if ( config & _BV(EN_CRC ) || AA)
  {
    if ( config & _BV(CRCO) )
      result = RF24_CRC_16;
    else
      result = RF24_CRC_8;
  }

  return result;
}

uint8_t getPALevel(void)
{

  return (readRegister(RF_SETUP) & (_BV(RF_PWR_LOW) | _BV(RF_PWR_HIGH))) >> 1 ;
}
