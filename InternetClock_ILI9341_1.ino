/*
 An example analogue clock using a TFT LCD screen to show the time
 use of some of the drawing commands with the modified Adafruit_TFT_AS library.
 Based on a sketch by Gilchrist 6/2/2014 1.0
 
 Original sketch from https://embedded-lab.com/blog/tutorial-8-esp8266-internet-clock/
 Nicu FLORICA (niq_ro) made this changes:
 v.0 - move to TFT_eSPI library from https://github.com/Bodmer/TFT_eSPI
 v.1 - added DST switch (summer/winter time)
 */

#include <SPI.h>
//#include <TFT_ILI9341_ESP.h> // Hardware-specific library
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define DSTpin  12 // GPIO12 = D6, see https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

// WiFi connection
const char ssid[] = "niq_ro";      // your wifi SSID
const char pass[] = "berelaburtica";  //  Your WiFi password
int hours_Offset_From_GMT = 2;      // -4;  // Change it according to your location
const char tzone[] = "Craiova, RO"; //"New York";

WiFiServer server(80);
WiFiClient client;
unsigned int localPort = 2390;      // local port to listen for UDP packets

IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

//Unix time in seconds
unsigned long epoch = 0;
unsigned long LastNTP = 0;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

int hour, hour1;
int minute;
int blink=0, PM=0;

int RequestedTime = 0;
int TimeCheckLoop = 0;

#define TFT_GREY 0x5AEB

//TFT_ILI9341_ESP tft = TFT_ILI9341_ESP();       // Invoke custom library
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;    // Saved H, M, S x & y multipliers
float sdeg=0, mdeg=0, hdeg=0;
uint16_t osx=120, osy=120, omx=120, omy=120, ohx=120, ohy=120;  // Saved H, M, S x & y coords
uint16_t x0=0, x1=0, yy0=0, yy1=0;
uint32_t targetTime = 0;                    // for next 1 second timeout
uint8_t hh, mm, ss;
static uint8_t conv2d(const char* p); // Forward declaration needed for IDE 1.6.x
//uint8_t hh=conv2d(__TIME__), mm=conv2d(__TIME__+3), ss=conv2d(__TIME__+6);  // Get H, M, S from compile time

boolean initial = 1;
byte DST = 0;
byte DST0;

void setup(void) {
  Serial.begin(112500);
  Serial.println(" ");
  pinMode (DSTpin, INPUT);
  if (digitalRead(DSTpin) == LOW)
   DST = 0;
  else
   DST = 1;
  Serial.print("DST = ");
  Serial.println(DST );
  DST0 = DST+3;
  
  tft.init();
//   tft.setRotation(2);  // original
  tft.setRotation(0);
  
  ConnectToAP();
  udp.begin(localPort);
  Request_Time();
  delay(2000);
  while (!Check_Time())
  {
    delay(2000);
    TimeCheckLoop++;
    if (TimeCheckLoop > 5)
    {
      Request_Time();
    }
  }
  
  //tft.fillScreen(TFT_BLACK);
  //tft.fillScreen(TFT_RED);
  //tft.fillScreen(TFT_GREEN);
  //tft.fillScreen(TFT_BLUE);
  //tft.fillScreen(TFT_BLACK);
  tft.fillScreen(TFT_GREY);
  
  tft.setTextColor(TFT_WHITE, TFT_GREY);  // Adding a background colour erases previous text automatically
  
  // Draw clock face
  tft.fillCircle(120, 120, 118, TFT_GREEN);
  tft.fillCircle(120, 120, 110, TFT_BLACK);

  // Draw 12 lines
  for(int i = 0; i<360; i+= 30) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*114+120;
    yy0 = sy*114+120;
    x1 = sx*100+120;
    yy1 = sy*100+120;

    tft.drawLine(x0, yy0, x1, yy1, TFT_GREEN);
  }

  // Draw 60 dots
  for(int i = 0; i<360; i+= 6) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*102+120;
    yy0 = sy*102+120;
    // Draw minute markers
    tft.drawPixel(x0, yy0, TFT_WHITE);
    
    // Draw main quadrant dots
    if(i==0 || i==180) tft.fillCircle(x0, yy0, 2, TFT_WHITE);
    if(i==90 || i==270) tft.fillCircle(x0, yy0, 2, TFT_WHITE);
  }

  tft.fillCircle(120, 121, 3, TFT_WHITE);

  // Draw text at position 120,260 using fonts 4
  // Only font numbers 2,4,6,7 are valid. Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : . - a p m
  // Font 7 is a 7 segment font and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : .
 

  targetTime = millis() + 1000; 
  
}

void ConnectToAP()
{
 // Serial.println("Attempting to Connect");
  
  // Serial.print("Connecting to ");
  // Serial.println(ssid);
   WiFi.begin(ssid, pass);
   while (WiFi.status() != WL_CONNECTED) {
    delay(300);
   }
}

void loop() { 
  int SecondsSinceLastNTP = (millis() - LastNTP) / 1000;
  //Update NTP every 2 minutes
  if (SecondsSinceLastNTP > 120 and RequestedTime == 0) {
    Request_Time();
    RequestedTime = 1;
  }

  if ((RequestedTime == 1) or (DST != DST0))
  {
    Check_Time();
    TimeCheckLoop++;
    if (TimeCheckLoop > 5)
    {
      RequestedTime = 0;
    }
      // Erase hour and minute hand positions every minute
      tft.drawLine(ohx, ohy, 120, 121, TFT_BLACK);
      ohx = hx*62+121;    
      ohy = hy*62+121;
      tft.drawLine(omx, omy, 120, 121, TFT_BLACK);
      omx = mx*84+120;    
      omy = my*84+121;
    DST0 = DST;
  }

 //Only Check_Time() should update epoch
  DecodeEpoch(epoch + SecondsSinceLastNTP);
  
    if (ss==60) {
      ss=0;
      mm++;            // Advance minute
      if(mm>59) {
        mm=0;
        hh++;          // Advance hour
        if (hh>23) {
          hh=0;
        }
      }
    }
    
    // Pre-compute hand degrees, x & y coords for a fast screen update
    sdeg = ss*6;                  // 0-59 -> 0-354
    mdeg = mm*6+sdeg*0.01666667;  // 0-59 -> 0-360 - includes seconds
    hdeg = hh*30+mdeg*0.0833333;  // 0-11 -> 0-360 - includes minutes and seconds
    hx = cos((hdeg-90)*0.0174532925);    
    hy = sin((hdeg-90)*0.0174532925);
    mx = cos((mdeg-90)*0.0174532925);    
    my = sin((mdeg-90)*0.0174532925);
    sx = cos((sdeg-90)*0.0174532925);    
    sy = sin((sdeg-90)*0.0174532925);

    if (ss==0 || initial) {
      
      initial = 0;
      // Erase hour and minute hand positions every minute
      tft.drawLine(ohx, ohy, 120, 121, TFT_BLACK);
      ohx = hx*62+121;    
      ohy = hy*62+121;
      tft.drawLine(omx, omy, 120, 121, TFT_BLACK);
      omx = mx*84+120;    
      omy = my*84+121;
    }

      // Redraw new hand positions, hour and minute hands not erased here to avoid flicker
      tft.drawLine(osx, osy, 120, 121, TFT_BLACK);
      osx = sx*90+121;    
      osy = sy*90+121;
      tft.drawLine(osx, osy, 120, 121, TFT_RED);
      tft.drawLine(ohx, ohy, 120, 121, TFT_WHITE);
      tft.drawLine(omx, omy, 120, 121, TFT_WHITE);
      tft.drawLine(osx, osy, 120, 121, TFT_RED);

      tft.fillCircle(120, 121, 3, TFT_RED);
      tft.setCursor(25, 245, 6);
      if(hh <10) tft.print(0);
      tft.print(hh);
      tft.print(':');
      if(mm <10) tft.print(0);
      tft.print(mm);
      tft.print(':');
      if(ss <10) tft.print(0);
      tft.print(ss);
      tft.setCursor(65, 285, 4);
      tft.print(tzone);
    //tft.drawCentreString("Time flies",120,260,4);
  delay(1000);
  ss++;
  
}  // end main loop

static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

void DecodeEpoch(unsigned long currentTime)
{
  pinMode(DSTpin, INPUT);
  if (digitalRead(DSTpin) == LOW)
   DST = 0;
  else
   DST = 1;
  Serial.print("DST = ");
  Serial.println(DST);
  // print the hour, minute and second:
  //Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
  //Serial.println(epoch);
  //Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
 // Serial.print(':');
  if ( ((epoch % 3600) / 60) < 10 ) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
  //  Serial.print('0');
  }
 // Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
//  Serial.print(':');
  if ( (epoch % 60) < 10 ) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
  //  Serial.print('0');
  }
  //Serial.println(epoch % 60); // print the second

  //Update for local zone
  currentTime = currentTime + ((DST+hours_Offset_From_GMT) * 60 * 60);
  //Serial.print("The current local time is ");
  //Serial.print((currentTime  % 86400L) / 3600); // print the hour (86400 equals secs per day)
  //Serial.print(':');
  if ( ((currentTime % 3600) / 60) < 10 ) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
 //   Serial.print('0');
  }
 // Serial.print((currentTime  % 3600) / 60); // print the minute (3600 equals secs per minute)
 // Serial.print(':');
  if ( (currentTime % 60) < 10 ) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
  //  Serial.print('0');
  }
  //Serial.println(currentTime % 60); // print the second

  hour = (currentTime  % 86400L) / 3600;
  hh=hour;
  if(hour >= 12) {
    hour = hour-12;
    PM=1;
  }
  else {
    PM=0;
  }
  if(hour == 0) hour = 12;
  
  minute = (currentTime % 3600) / 60;
  mm=minute;
  ss=currentTime % 60;

  Serial.print(hour);
  Serial.print(":");
  if (mm < 10) Serial.print("0");
  Serial.print(mm);
  Serial.print(":");
  if (ss < 10) Serial.print("0");
  Serial.print(ss);
  Serial.print(" -> "); 
  if (PM == 0) Serial.println("AM"); 
  else Serial.println("PM");   
}

void Request_Time()
{
 // Serial.println("Getting Time");
  sendNTPpacket(timeServer); // send an NTP packet to a time server
}

bool Check_Time()
{
  int cb = udp.parsePacket();
  if (!cb) {
   // Serial.println("no packet yet");
    return false;
  }
  else {
   // Serial.print("packet received, length=");
   // Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //Serial.print("Seconds since Jan 1 1900 = " );
    // Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    epoch = secsSince1900 - seventyYears;
    LastNTP = millis();
    RequestedTime = 0;
    TimeCheckLoop = 0;
    return true;
  }
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress & address)
{
 // Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
