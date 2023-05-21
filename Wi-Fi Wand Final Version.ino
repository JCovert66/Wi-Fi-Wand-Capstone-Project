#include <TinyGPS++.h>
#include <wiring_private.h>
#include"TFT_eSPI.h"
#include "rpcWiFi.h"
#include <map>
#include "seeed_line_chart.h"
#include <SPI.h>
#include <Seeed_FS.h>
#include "SD/Seeed_SD.h"

//Display Variables
TFT_eSPI tft; //Initializing LCD screen
TFT_eSprite spr = TFT_eSprite(&tft); //Initializing screen buffer
const int textsize = 2;
int screenHeight = 240;

//WiFi Variables
std::map<String, long> networks; //Map that holds network names and correlated signal strengths
String networkNames[50]; //Array of network names that can be sorted
String net; // Holds network name in options where network choice is made

//Line Chart Variables
#define MAX_SIZE 30 // Maximum size of data for line chart
doubles data; //Holds data used for line chart

//GUI Variables
const int numOfOptions = 5; //Update with number of menu options
String menuOptions[numOfOptions] = { //Holds all options for main menu
  "WiFi Scan", 
  "GPS Coordinates",
  "WiFi Time Chart",
  "Simple Heat Map",
  "Strength & GPS Data Logger"
};
int spot = 1; //keeps track of cursor for menu selection
int guiSection = 0;
int refreshCount = 0;
int maxDown = numOfOptions;

//GPS Variables
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps; // The TinyGPS++ object
// The serial connection to the GPS device - Left side Grove connector.
// Left side Grove connector shares pins with I2C1 of 40 pin connector.
static Uart Serial3(&sercom4, D1, D0, SERCOM_RX_PAD_1, UART_TX_PAD_0);
bool isConnected;
double gpsLng;
double gpsLat;
String gpsMonth;
String gpsDay;
String gpsYear;
String gpsHour;
String gpsMinute;
String gpsSecond;
String gpsCentisecond;

//Simple Heat Map Variables
struct StrengthCoord{
   long strength;
   double lat;
   double lng;
};
StrengthCoord coordA;
StrengthCoord coordB;
StrengthCoord coordC;
StrengthCoord coordD;
int Pa, Pb, Pc, Pd; //Pa = NW (Top-left corner), Pb = NE (Top-right corner), Pc = SW (Bottom-left corner), Pd = SE (Bottom-right corner)
int color;
bool wait = false;
int r = 0, g = 150, b = 200;
int sam, eq;
int r2, b2, g2;

//SDCard variables
File dataLog;
int cycles = 10;


void setup() {

  Serial.begin(115200);

  Serial3.begin(GPSBaud);
  pinPeripheral(D0, PIO_SERCOM_ALT);
  pinPeripheral(D1, PIO_SERCOM_ALT);

  tft.begin();
  tft.setRotation(3); 
  //spr.createSprite(TFT_HEIGHT,TFT_WIDTH);      
  tft.setTextSize(textsize);
  tft.fillScreen(TFT_BLACK); //Black background
  //spr.createSprite(320,180); //try half screen

  // pin stuff
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  //while (!Serial) {
  //}
  Serial.print("Initializing SD card...");
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");
  

  tft.fillScreen(TFT_BLACK); //White background
  tft.drawString("Starting...", 10, 10);

  delay(1000);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  //Scans for networks
  int n = scanForNetworks();
  tft.fillScreen(TFT_BLACK); //White background
  tft.drawString(String(n) + " networks found!", 10, 10);
  delay(1000);

  drawOptions();
}

void loop() {

  getGPSCoordinates();

  // wait for input

    //If UP is pressed
   if (digitalRead(WIO_5S_UP) == LOW && (guiSection == 0 || guiSection > 2)) {
    spot--;
    refreshCount = 0;
    if(spot == 0) {
      spot = 1;
      //return;
      goto breakif;
      // DONT MOVE UP AND DOWN ON guiSection 3 OR 4
    }
   }

    //If DOWN is pressed
   else if (digitalRead(WIO_5S_DOWN) == LOW && (guiSection == 0 || guiSection > 2)) {
    spot++;
    refreshCount = 0;
    if(spot > maxDown) {
      spot = maxDown;
      goto breakif;
      // DONT MOVE UP AND DOWN ON guiSection 3 OR 4
    }
   }

    //If LEFT is pressed
   else if (digitalRead(WIO_5S_LEFT) == LOW) {
    tft.setTextSize(textsize);
    refreshCount = 0;
    spot = 1;
    guiSection = 0;
    if(guiSection < 0) {
      guiSection = 0;
      goto breakif;
    } //else if (guiSection == 1) {
      // reset the spot pointer fore the api values
      //spot = 0;
    //}
   }

    //If RIGHT is pressed
   else if (digitalRead(WIO_5S_RIGHT) == LOW && (guiSection == 0 || guiSection > 2)) {
    if (guiSection == 0){
      guiSection = spot;
      Serial.println(guiSection);
      spot = 1;
      if(guiSection >= 5) {
        guiSection = 5;
        refreshCount = 0;
        goto breakif;
      }
    }
    else if (guiSection == 3 || (guiSection == 4 && refreshCount == 1)){
      net = networkNames[spot - 1];
      if(guiSection > 5) {
        guiSection = 5;
        goto breakif;
      }
      refreshCount = 2;
      tft.fillScreen(TFT_WHITE);
    }
   }

    //If SELECT is pressed
   else if (digitalRead(WIO_5S_PRESS) == LOW && (guiSection == 0 || guiSection > 2)) {
    if (guiSection == 0){
      guiSection = spot;
      Serial.println(guiSection);
      spot = 1;
      if(guiSection == 2) {
        tft.fillScreen(TFT_BLACK);
      }
      if(guiSection >= 5) {
        guiSection = 5;
        refreshCount = 0;
        goto breakif;
      }
    }
    else if (guiSection == 3 || (guiSection >= 4 && refreshCount == 1)){
      net = networkNames[spot - 1];
      if(guiSection > 5) {
        guiSection = 5;
        goto breakif;
      }
      refreshCount = 2;
      tft.fillScreen(TFT_WHITE);
    }
   } 
   
   else {
    // NOTHING PRESSED SO DON'T DRAW
    if(guiSection == 2) {
      if(refreshCount == 10) {
        refreshCount = 0;
        drawing();
      }
      //refreshCount++;
    }
    //If waiting for button press
    if (guiSection == 0 || (guiSection == 3 && refreshCount == 1)){
      goto breakif;
    }
   }
   
   drawing();

   breakif:
   delay(200);

}

void drawing() {
  tft.setTextColor(TFT_WHITE);
  if (guiSection == 0) {
    maxDown = numOfOptions;
    drawOptions();
  } else if (guiSection == 1) {
    drawWiFiScan();
    refreshCount++;
  } else if (guiSection == 2) {   
    displayGPSInfo();
  } else if (guiSection == 3) {
    maxDown = sizeof(networkNames);
    drawWiFiChart();
    refreshCount++;
  } else if (guiSection == 4) {
    maxDown = sizeof(networkNames);
    drawSimpleHeatMap();
  } else if (guiSection == 5) {
    drawDataLogger();
    maxDown = sizeof(networkNames);
  }

}

void drawOptions() {
  tft.fillScreen(TFT_BLACK);
  int drawline = 0;
  int nextline = 10 * textsize;
  int start = ((spot * nextline) / screenHeight) * (screenHeight / nextline);
  for (int i = start; i < start + (screenHeight / nextline); i++) {
    if (i >= numOfOptions) {
      break;
    }
    if(i == (spot - 1)) {
      tft.drawString("- " + menuOptions[i], 0, drawline);
    } else {
      tft.drawString(menuOptions[i], 0, drawline);
    }    
    drawline += nextline;
  }
}

//Converts RSSI values to Percentage of signal strength
long DbmToPercent(long Dbm){
  long percent;
  if (Dbm == 0){
    percent = 0;
  }
  else if (Dbm >= -20){
    percent = 100;
  }
  else if (Dbm <= -93){
    percent = 1;
  }
  else {
    //Quadratic scaling equation 
    percent = -0.0134 * sq(Dbm) - 0.2228 * Dbm + 100.2;
  }
  return percent;
}
//Sorts WiFi networks by network strength
void sortStrengths(String networkNames[50], std::map<String, long> networks){
  String temp;
  for(int i=0; i<sizeof(networkNames)-1; i++){
    for(int j=i+1; j<sizeof(networkNames); j++){
      if(networks[networkNames[i]] < networks[networkNames[j]]){
        temp = networkNames[i];
        networkNames[i] = networkNames[j];
        networkNames[j] = temp;
      }
    }
  }
}
//Displays list of WiFi networks available with both RSSI
//and percentage displayed
void drawWiFiScan() {
  if (refreshCount == 0){
    tft.fillScreen(TFT_BLACK); //Black background
      tft.drawString("Scanning...", 10, 10);
  }
  int n = scanForNetworks();
  sortStrengths(networkNames, networks);
  
  displayNetworks(n);
}

bool networkExists(String network){
  for (int i = 0; i < 50; i++){
    if (networkNames[i] == network){
      return true;
    }
  }
  return false;
}

void clearArray(String networkNames[50]) {
  for (int i = 0; i < 50; i++){
    networkNames[i] = "";
  }
}

int scanForNetworks() {
  clearArray(networkNames);
  int n = WiFi.scanNetworks();
  if (n <= 50){
    for (int i = 0; i < n; i++){
      if (!networkExists(WiFi.SSID(i))){
        //Stores network names into array
        networkNames[i] = WiFi.SSID(i);
      }
      networks[WiFi.SSID(i)] = WiFi.RSSI(i);
    }  
    for (int i = n; i < 50; i++){
      networkNames[i] = "";
    }
  }
  else {
    for (int i = 0; i < 50; i++){
      if (!networkExists(WiFi.SSID(i)) && WiFi.SSID(i) != ""){
        //Stores network names into array
        networkNames[i] = WiFi.SSID(i);
      }
      if (WiFi.SSID(i) != "") {
        networks[WiFi.SSID(i)] = WiFi.RSSI(i);
      }
    }
  }
  return n;
}

void displayNetworks(int n) {
  //Refresh screen in order to make sure text doesn't overlap
  tft.fillScreen(TFT_BLACK); //Black background
  int line = 0;
  for (int i=0; i<n; i++){
    String key = networkNames[i];
    if (key.length() > 13){
      tft.drawString((key.substring(0,13) + "..: " + networks[key] + " (" + DbmToPercent(networks[key]) + "%)"), 10, ((line*20) + 10));
      line++;
    }
    else if (key != ""){
      tft.drawString((key + ": " + networks[key] + " (" + DbmToPercent(networks[key]) + "%)"), 10, ((line*20) + 10));
      line++;
    }
  }
}

void getGPSCoordinates() {
  // This sketch displays information every time a new sentence is correctly encoded.
  while (Serial3.available() > 0)
    if (gps.encode(Serial3.read()))
      gpsLat = gps.location.lat();
      gpsLng = gps.location.lng();
      gpsMonth = String(gps.date.month());
      gpsDay = String(gps.date.day());
      gpsYear = String(gps.date.year());
      gpsHour = String(gps.time.hour());
      gpsMinute = String(gps.time.minute());
      gpsSecond = String(gps.time.second());
      gpsCentisecond = String(gps.time.centisecond());
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }
}

void displayGPSInfo() {
  isConnected = gps.location.isValid();
  if (isConnected)
  {

    spr.createSprite(320, 90);
    spr.fillSprite(TFT_BLACK);
    spr.setFreeFont(&FreeSansBoldOblique12pt7b);
    spr.setTextColor(TFT_WHITE);
    spr.drawString("Longitude: " + String(gpsLng, 6), 5, 5, 1);
    spr.drawString("Latitude: " + String(gpsLat, 6), 5, 30, 1);
    spr.pushSprite(0, 0);
    spr.deleteSprite();
  }
  else {
    spr.createSprite(320, 90);
    spr.fillSprite(TFT_BLACK);
    spr.setFreeFont(&FreeSansBoldOblique12pt7b);
    spr.setTextColor(TFT_WHITE);
    spr.drawString("Searching for satelite...", 5, 5, 1);
    spr.pushSprite(0, 0);
    spr.deleteSprite();
  }
}

void SERCOM4_0_Handler()
{
  Serial3.IrqHandler();
}
void SERCOM4_1_Handler()
{
  Serial3.IrqHandler();
}
void SERCOM4_2_Handler()
{
  Serial3.IrqHandler();
}
void SERCOM4_3_Handler()
{
  Serial3.IrqHandler();
}

void drawWiFiChart(){
  if (refreshCount == 0){
    int drawline = 0;
    int nextline = 10 * textsize;
    int start = ((spot * nextline) / screenHeight) * (screenHeight / nextline);
    tft.fillScreen(TFT_BLACK);
    for (int i = start; i < start + (screenHeight / nextline); i++) {
      if (networkNames[i] != ""){
        if (i == (spot - 1)) {
          tft.drawString("- " + networkNames[i], 0, drawline);
        } else {
          tft.drawString(networkNames[i], 0, drawline);
        }    
        drawline += nextline;
      }
    }
    refreshCount == 1;
  }
  else if (refreshCount > 1){
    if (data.size() > MAX_SIZE) // keep the old line chart front
    {
        data.pop(); // this is used to remove the first read variable
    }
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++){
      if (WiFi.SSID(i) == net){
        data.push(WiFi.RSSI(i));
        goto breakif2;
      }
    }
    breakif2:
    char char_array[net.length() + 1];
    net.toCharArray(char_array, net.length());

    

    // Settings for the line graph title
    auto header = text(0, 0)
                      .value(char_array)
                      .align(center)
                      .valign(vcenter)
                      .width(tft.width())
                      .thickness(2);

    header.height(header.font_height(&tft) * 2);
    header.draw(&tft); // Header height is the twice the height of the font

    // Settings for the line graph
    auto content = line_chart(20, header.height()); //(x,y) where the line graph begins
    content
        .height(tft.height() - header.height() * 1.5) // actual height of the line chart
        .width(tft.width() - content.x() * 2)         // actual width of the line chart
        .based_on(-30.0)                                // Starting point of y-axis, must be a float
        .show_circle(true)                           // drawing a cirle at each point, default is on.
        .value(data)                                  // passing through the data to line graph
        .max_size(MAX_SIZE)
        .color(TFT_RED)                               // Setting the color for the line
        .backgroud(TFT_WHITE)                         // Setting the color for the backgroud
        .draw(&tft);
    
    delay(200);
  }
}

void drawSimpleHeatMap(){
  bool isConnected;
  if (refreshCount == 0){
    int drawline = 0;
    int nextline = 10 * textsize;
    int start = ((spot * nextline) / screenHeight) * (screenHeight / nextline);
    tft.fillScreen(TFT_BLACK);
    for (int i = start; i < start + (screenHeight / nextline); i++) {
      if (networkNames[i] != ""){
        if (i == (spot - 1)) {
          tft.drawString("- " + networkNames[i], 0, drawline);
        } else {
          tft.drawString(networkNames[i], 0, drawline);
        }    
        drawline += nextline;
      }
    }
    refreshCount++;
    isConnected = gps.location.isValid();
  }
  if (refreshCount == 2){
    if (wait == false){
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Press button to record ", 10, 10);
      tft.drawString("first signal strength ", 10, 30);
      tft.drawString("for " + net, 10, 50);
    }
    wait = true;
    if (digitalRead(WIO_5S_PRESS) == LOW){
      tft.fillScreen(TFT_BLACK);
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++){
        if (WiFi.SSID(i) == net){
          coordA.strength = WiFi.RSSI(i);
          coordA.lat = gpsLat;
          coordA.lng = gpsLng;
        }
      }
      refreshCount++;
      wait = false;
    }
  }   
  else if (refreshCount == 3){
    if (wait == false){
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Press button to record ", 10, 10);
      tft.drawString("second signal strength ", 10, 30);
      tft.drawString("for " + net, 10, 50);
    }
    wait = true;
    if (digitalRead(WIO_5S_PRESS) == LOW){
      tft.fillScreen(TFT_BLACK);
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++){
        if (WiFi.SSID(i) == net){
          coordB.strength = WiFi.RSSI(i);
          coordB.lat = gpsLat;
          coordB.lng = gpsLng;
        }
      }
      refreshCount++;
      wait = false;
    }
  }
  else if (refreshCount == 4){
    if (wait == false){
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Press button to record ", 10, 10);
      tft.drawString("third signal strength ", 10, 30);
      tft.drawString("for " + net, 10, 50);
    }
    wait = true;
    if (digitalRead(WIO_5S_PRESS) == LOW){
      tft.fillScreen(TFT_BLACK);
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++){
        if (WiFi.SSID(i) == net){
          coordC.strength = WiFi.RSSI(i);
          coordC.lat = gpsLat;
          coordC.lng = gpsLng;
        }
      }
      refreshCount++;
      wait = false;
    }
  }
  else if (refreshCount == 5){
    if (wait == false){
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Press button to record ", 10, 10);
      tft.drawString("fourth signal strength ", 10, 30);
      tft.drawString("for " + net, 10, 50);
    }
    wait = true;
    if (digitalRead(WIO_5S_PRESS) == LOW){
      tft.fillScreen(TFT_BLACK);
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++){
        if (WiFi.SSID(i) == net){
          coordD.strength = WiFi.RSSI(i);
          coordD.lat = gpsLat;
          coordD.lng = gpsLng;
        }
      }
      refreshCount++;
      wait = false;
    }
  }
  else if (refreshCount == 6){
    if (wait == false){
      double avgLng = (coordA.lng + coordB.lng + coordC.lng + coordD.lng) / 4;
      double avgLat = (coordA.lat + coordB.lat + coordC.lat + coordD.lat) / 4;
      Serial.println("Average latitude: " + String(avgLat, 6));
      sortCoordinates(coordA.lng, coordA.lat, coordA.strength, avgLng, avgLat);
      sortCoordinates(coordB.lng, coordB.lat, coordB.strength, avgLng, avgLat);
      sortCoordinates(coordC.lng, coordC.lat, coordC.strength, avgLng, avgLat);
      sortCoordinates(coordD.lng, coordD.lat, coordD.strength, avgLng, avgLat);
      sam =  (( sqrt(pow(240, 2) + pow(160, 2)) * Pa + sqrt(pow(480 - 240, 2) + pow(160, 2)) * Pb + sqrt(pow(240, 2) + pow(320 - 160, 2)) * Pc + sqrt(pow(480 - 240, 2) + pow(320 - 160, 2)) * Pd ) / ( sqrt(pow(240, 2) + pow(160, 2)) + sqrt(pow(480 - 240, 2) + pow(160, 2)) + sqrt(pow(240, 2) + pow(320 - 160, 2)) + sqrt(pow(480 - 240, 2) + pow(320 - 160, 2)) ) );
      for (int y = 0; y < 320; y++) { 
        for (int x = 0; x < 480; x++) {
          eq = (( sqrt(pow(x, 2) + pow(y, 2)) * Pa + sqrt(pow(480 - x, 2) + pow(y, 2)) * Pb + sqrt(pow(x, 2) + pow(320 - y, 2)) * Pc + sqrt(pow(480 - x, 2) + pow(320 - y, 2)) * Pd ) / ( sqrt(pow(x, 2) + pow(y, 2)) + sqrt(pow(480 - x, 2) + pow(y, 2)) + sqrt(pow(x, 2) + pow(320 - y, 2)) + sqrt(pow(480 - x, 2) + pow(320 - y, 2)) ) ); 
          // Serial.println(eq); 
          // delay(20); 
          eq = (eq - sam) * -1; 
          //Serial.println(eq); 
          r2 = r + (30 * eq); 
          b2 = b + (30 * eq); 
          g2 = g + (30 * eq); 
          if (r2 > 255)r2 = 255;
          if (r2 < 0)r2 = 0; 
          if (b2 > 255)b2 = 0;
          if (b2 < 0)b2 = 0; 
          if (g2 > 255)g2 = 0;
          if (g2 < 0)g2 = 0;
          color = tft.color565(r2, g2, b2);
          tft.drawPixel(x, y, color);
        }
      }   
    }
    wait = true;
    if (digitalRead(WIO_5S_PRESS) == LOW){
      refreshCount = 0;
      wait = false;
    }
  }
}

void sortCoordinates(double lng, double lat, long strength, double avgLng, double avgLat){
  if (lng > avgLng && lat > avgLat){
    Pb = int(strength);
  }
  else if (lng > avgLng && lat < avgLat){
    Pa = int(strength);
  }
  else if (lng < avgLng && lat > avgLat){
    Pd = int(strength);
  }
  else if (lng < avgLng && lat < avgLat){
    Pc = int(strength);
  }
}

void drawDataLogger(){
  if (refreshCount == 0){
    int drawline = 0;
    int nextline = 10 * textsize;
    int start = ((spot * nextline) / screenHeight) * (screenHeight / nextline);
    tft.fillScreen(TFT_BLACK);
    for (int i = start; i < start + (screenHeight / nextline); i++) {
      if (networkNames[i] != ""){
        if (i == (spot - 1)) {
          tft.drawString("- " + networkNames[i], 0, drawline);
        } else {
          tft.drawString(networkNames[i], 0, drawline);
        }    
        drawline += nextline;
      }
    }
    refreshCount++;
  }
  else if (refreshCount == 2){
    bool ready = false;
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Number of cycles: " + String(cycles), 50, 80);
    while (!ready){
      // wait for input
      if (digitalRead(WIO_KEY_A) == LOW) {
        Serial.println("Key A");
        cycles++;  
      }
      else if (digitalRead(WIO_KEY_B) == LOW) {
        Serial.println("Key B");
        cycles--;
        if(cycles <= 0) {
          cycles=0;
          goto breakif;
          // DONT MOVE UP AND DOWN ON guiSection 3 OR 4
        } 
      }
      else if (digitalRead(WIO_5S_PRESS) == LOW) {
        Serial.println("5 Way Press");
        refreshCount++;
        ready = true;
      } 
      else {
        // NOTHING PRESSED SO DON'T DRAW
        goto breakif;
      }
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Number of cycles: " + String(cycles), 50, 80);

      breakif:
      delay(200); 
    }
  }
  else if (refreshCount == 3){
    dataLog = SD.open("test.csv", FILE_WRITE);
    String tempStrength;
    if(dataLog){
      dataLog.println("Network Name,Network Strength,Longitude,Latitude,Date,Time");
      for (int j = 0; j < cycles; j++){
        int n = WiFi.scanNetworks();
        getGPSCoordinates();
        for (int i = 0; i < n; i++){
          if (WiFi.SSID(i) == net){
            tft.fillScreen(TFT_BLACK);
            tempStrength = String(WiFi.RSSI(i));
            dataLog.println(net+","+String(WiFi.RSSI(i))+","+String(gpsLng, 6)+","+String(gpsLat, 6)+","+gpsMonth+"/"+gpsDay+"/"+gpsYear+","+gpsHour+":"+gpsMinute+":"+gpsSecond+":"+gpsCentisecond);
            Serial.println(net+','+String(WiFi.RSSI(i))+','+String(gpsLng, 6)+','+String(gpsLat, 6)+','+gpsMonth+'/'+gpsDay+'/'+gpsYear+','+gpsHour+':'+gpsMinute+':'+gpsSecond+':'+gpsCentisecond);
            tft.drawString("Network Name: " + net, 10, 10);
            tft.drawString("Network Strength: " + String(WiFi.RSSI(i)), 10, 30);
            tft.drawString("Longitude: "+ String(gpsLng, 6), 10, 50);
            tft.drawString("Latitude: "+ String(gpsLat, 6), 10, 70);
            tft.drawString("Date: " + gpsMonth + "/" + gpsDay + "/" + gpsYear, 10, 90);
            tft.drawString("Time: " + gpsHour + ":" + gpsMinute + ":" + gpsSecond + ":" + gpsCentisecond, 10, 110);
            tft.drawString("Recording... " + String(j) + "/" + String(cycles), 10, 130);
          } 
        }
      }
      dataLog.close();
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Network Name: " + net, 10, 10);
      tft.drawString("Network Strength: " + tempStrength, 10, 30);
      tft.drawString("Longitude: "+ String(gpsLng, 6), 10, 50);
      tft.drawString("Latitude: "+ String(gpsLat, 6), 10, 70);
      tft.drawString("Date: " + gpsMonth + "/" + gpsDay + "/" + gpsYear, 10, 90);
      tft.drawString("Time: " + gpsHour + ":" + gpsMinute + ":" + gpsSecond + ":" + gpsCentisecond, 10, 110);
      tft.drawString("Recording finished!", 10, 130);
    }
    refreshCount++;
  }
}