/*The purpose of this sketch is to relay (receive and transmit) LoRa, 433MHz 
and serve Wi-Fi for viewing weather data on a browser.*/
#include <SSD1306.h> 
#include <WebServer.h>
#include <WiFiClient.h>
#include <AcuriteDecoder.h>

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  

//#include "images.h"
#include <WiFi.h>


#include <ESPmDNS.h>
#include <RTClib.h>
#include <Ticker.h>
#include <math.h>

#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     14   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND    433E6

unsigned int counter = 0;

SSD1306 display(0x3c, 4, 15);
String rssi = "RSSI --";
String packSize = "--";
String packet ;

WebServer server;
//Server variables
const char *ssidSTA = "Woolley_Annex";
const char *ssidAP = "WeatherAP-STA";
const char *password = "sedrowoolley";
byte channel = 6; //WiFi channel 1-13, default 1, if ommitted; 1, 6 and 11 non-overlapping

IPAddress ip(192,168,11,4);
IPAddress gateway(192,168,11,1);
IPAddress subnet(255,255,255,0);

//WiFiClient client;
const char* filename = "ESP32WeatherAP_STA";
//const int sleepTimeS = 30;

AcuriteDecoder AD;

//433mhz receiver variables
//byte pin = 3; // from receiver, GPIO3 RX
//byte ar[116];
//byte pos=0; 

//Weather variables
//double realtemp; double realtempF;                                  
//unsigned char hum;
//double windspeed;
//String rtfTrim;
//String rtTrim;
//String wsTrim;

//Time variables
char* t = __TIME__;
char* d = __DATE__;
const char* uploaddate = d;
byte rh;
byte rm;
byte rmonth=02;
byte rday=11;
byte hh;
byte mm;
RTC_Millis rtc;
DateTime now;
byte i;//index to determine presence of time in URL

//Watchdog variables
Ticker secondTick;
volatile int watchdogCount = 0;


void setup() {

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssidSTA,password);
  
  pinMode(16,OUTPUT);
  pinMode(25,OUTPUT);
  
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
  
  Serial.begin(9600);

  delay(10);
  Serial.println();
  Serial.println(d);
  Serial.println(filename);
  
  currentTime();
   
  //433mhz setup
  //pinMode(pin, INPUT);
  Serial.flush();
  
  while (!Serial);
  Serial.println();
  Serial.println("LoRa Sender Weather");
  
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  //server paths
  server.on("/", handleRootPath);
  server.begin();

  WiFi.softAPConfig(ip, gateway, subnet);  
  WiFi.softAP(ssidAP, password);   
  
  serialMonitor();//diagnostics
    
  //LoRa.onReceive(cbk);
  //LoRa.receive();
  Serial.println("init ok");
  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_10);
  
  delay(1500);
}

void serialMonitor()
{//diagnostics 
  Serial.print("ssidSTA: ");Serial.println(ssidSTA); 
  Serial.print("ssidAP: ");Serial.println(ssidAP);
  Serial.print("password: ");Serial.println(password);
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());//
  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());//the default is 192.168.4.1; static set with WiFi.softAPConfig(local_IP, gateway, subnet); 
}

void loop() {
  
  currentTime(); 
  parseURL();
  AD.getWeather(3,3619);  
  delay(1);
  server.handleClient();
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "Sending packet: ");
  display.drawString(90, 0, String(counter));
  display.display();

  // send packet
  LoRa.beginPacket();
  LoRa.print(AD.weather.wind);
  //LoRa.print(AD.weather.wind); //windspeed converted to string for printing
  LoRa.endPacket();

  counter++;
  digitalWrite(25, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(25, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
}


void handleRootPath()
{
  const char* css = "<style>"
    "body { font-size:30pt; color:#A0A0A0; background-color:#080808; }"
    "div.hidden { display:none; }"
    "h1 { font-size:40pt; }"
    //"input  { width:300px; font-size:30pt; }"
    "textarea  { border:none; width:700px; font-size:30pt; color:#A0A0A0; background-color:#080808; overflow:auto;  }"
    "button   { color:#2020FF; background-color:#A0A0A0; font-size: 14pt; margin-left:10px; } "
    "input  { width: 150px; color:#2020FF; background-color:#A0A0A0; font-size: 14pt;}"
    "a  { font-size: 20pt; color: #2020FF; padding: 7px 15px; float: right; cursor:hand;}"
  "</style>";
  
  const char* js = "<script type='text/JavaScript'>"
    "function forceRefresh(){"
    "location.assign(location.host);"
    "setTimeout('forceRefresh();',300000);"
    //"showTime();"
    "}"  

    "function toggleDiv(){"
    "var x = document.getElementById('setTimeDiv');"
    "if (x.style.display === 'none'){"
    "x.style.display = 'block';"
    "} else {"
    "x.style.display = 'none';}"
    "startClock();"
    "}"
    
    "function startClock() {"  
      "var now = new Date();"
      "var hours = now.getHours();"
      "if( hours < 10 ){ hours = '0' + hours };"  //format with two digits 
      "var minutes = now.getMinutes();"
      "if( minutes < 10 ){ minutes= '0' + minutes };"
      "var timeValue = ( hours + ':' + minutes );"
      "hh=hours;"
      "mm=minutes;"
      "var x = document.getElementById( 'time' );"  
      "x.innerHTML=timeValue;"//this shows on the page, but not in view source text.
      "var y = document.getElementById( 'timeInputTxt' ).value;" 
      "y.innerHTML=timeValue;"//  this value does not seem to show on page, nor in view source text.
      
      "timerID = setTimeout('startClock()',1000);"
      "timerRunning = true;"
    "}"
  
    "var timerID = null;"
    "var timerRunning = false;"    
    "function stopClock (){"
    "if(timerRunning)"
    "clearTimeout(timerID);"
    "timerRunning = false;"
    "}"

   "function calladdUrl(){" 
      "var y = document.getElementById( 'timeInputTxt' ).value;"  
      "document.location.search=addUrlParam(document.location.search, 'time', y);" //add user input time to URL
     "}" 
  
     "var addUrlParam = function(search, key, val){"  
     "var newParam = key + '=' + val,"  
     "params = '?' + newParam;"  
     // If the "search" string exists, then build params from it
     "if (search) {"  
     // Try to replace an existing instance
     "params = search.replace(new RegExp('([?&])' + key + '[^&]*'), '$1' + newParam);"  
     // If nothing was replaced, then add the new param to the end
     "if (params === search) {"  
     "params += '&' + newParam;"  
     "}"  
     "}"  
     "return params;"  
   "}" 
  "</script>";
    
  //HTML
  String s = "<!DOCTYPE html>";
  s+= "<html>";
  s+= "<head>";
  s+= "<title>Weather Server</title>";
  s+= "<meta http-equiv=\"Refresh\" content=\"300\"/>";
  s+= css;//stylesheet 
  s+= js;  
  s+= "</head>";
  
  s+= "<body onload=startClock()>";  
  s+= "<a id='time' >"; s+=hh; s+=":"; s+=mm; s+= "</a>";
  s+= "<h1>Weather Server</h1>"; 
  s+= "<div id='setTimeDiv' class='hidden' >";
  s+= "<a >";s+= rh;s+= ':';s+= rm;  s+= "</a>";//these, rh, rm variables don't seem to show on webpage
  s+= "Set Clock: <input type='text' id='timeInputTxt' >";
  s+= "<button id='addUrlBtn' onclick=calladdUrl()>Set</button>";
  s+= "</div>";
  s+= "<p>"; 
    s+= AD.weather.wind;
    s+= "<br/>";  
    s+= AD.weather.temperature;
    s+= "<br/>";     
    s+= AD.weather.humidity;
  s+= "</p>";
  s+= "<a href=''>Refresh </a>";
  s+= "<a onclick=toggleDiv();>Set Time</a>";
  s+= "</body>";
  s+= "</html>";

  server.send(200,"text/HTML", s);
  Serial.println("");Serial.print("server rootpath: "); Serial.println(s);
}

void softRTC () {
  now = rtc.now();    
  int hour = now.hour();
  int min = now.minute();
  int sec= now.second();
  char formattedTime[9]="";
  sprintf(formattedTime, "%02d:%02d:%02d", hour, min, sec); 
}

void adjustTime(){    
  if(i != 0){//test for querystring
  //Serial.println("adjustTime called");  
  currentTime();
  rtc.adjust(DateTime(2017, rmonth, rday, rh, rm, 0)); 
  now = rtc.now();
  hh=now.hour();
  mm=now.minute();
  Serial.print("Adjusted RTC Time: ");
  char formattedTime[6]="";
  sprintf(formattedTime, "%02d:%02d", hh, mm);   
  //Serial.print(hh);Serial.print(":");Serial.println(mm);
  currentTime();
  i=0;
  }
}  

void currentTime(){//the RTC time now  
  now = rtc.now();
  hh=now.hour();
  mm=now.minute();
  //Serial.print("rtc.now Time: ");Serial.print(hh);Serial.print(":");Serial.println(mm);
}

void parseURL(){
  String request = server.arg("time"); //this lets you access a query param (http://x.x.x.x/action1?value=1) 
  i=request.length();
  if (i != 0)  {
    String tt=request;//.substring(i+6,i+11);//JavaScript URI encoding adds %3A, (3) character to replace a colon, :, (1 character)
    String h= tt.substring(0,2); 
    rh=h.toInt();
    String m= tt.substring(3,5);
    rm=m.toInt();
    adjustTime(); 
    i=0;   
  }
}

//String roundNumber(float x)
//{
//  String roundNum="";
//  float y=(x*10);
//  int z=(int)(y+0.5);
//  float zf=(float)z;
//  roundNum=String(zf/10).substring(0,String(zf/10).indexOf('.')+2);
//  return roundNum;
//}
//
//void getWeather(){
//  b: pos=0;
//  unsigned long dur; //array, position in array, pulse duration
//  unsigned long zeroL=600;
//  unsigned long zeroS=301;
//  unsigned long oneL=300;
//  unsigned long oneS=100;
//    dur = pulseIn(pin, LOW); if ((dur>zeroS)&&(dur<zeroL)) {ar[pos] = 0; pos++;
//      dur = pulseIn(pin, LOW); if ((dur>zeroS)&&(dur<zeroL)) {ar[pos] = 0; pos++;
//        dur = pulseIn(pin, LOW); if ((dur>zeroS)&&(dur<zeroL)) {ar[pos] = 0; pos++;
//        }else goto b;
//      }else goto b;
//    }else goto b;
//
//    //save the input RF pulse stream, pulse width of low;
//    for (int i=1; i <= 61; i++){
//      dur = pulseIn(pin, LOW);
//      if ((dur>zeroS)&&(dur<zeroL)){ar[pos] = 0; pos++; }
//      if ((dur>oneS)&&(dur<oneL)){ar[pos] = 1; pos++; }
//    }
//    int cc=0; for (int i=0; i <= 14; i++) {cc=cc << 1; cc=cc+ar[1+i]; 
//    //Serial.print(ar[1+i]); 
//    }
//    //Serial.print(".");
//    Serial.print("ChannelCode: "); Serial.print(cc);Serial.println("");
//
//  if (cc==3619) //print data for valid channel code, old value was 3619
//    {
//    //Serial.println(" ");
//    int batt=0; for (int i=0; i <= 6; i++) {batt=batt << 1; batt=batt+ar[21+i]; Serial.print(ar[21+i]); }//if battery is a byte then p1100100=100%
//    Serial.print(" ");Serial.print("Battery21: "); Serial.print(batt);Serial.println("%");
//
//    int hum5=0; for (int i=0; i <= 6; i++) {hum5=hum5 << 1; hum5=hum5+ar[29+i]; Serial.print(ar[29+i]); }
//    Serial.print(" ");Serial.print("Humidity29-6: "); Serial.print(hum5);Serial.println("%");
//    hum=hum5;
//    //char* x;
//    //hum = dtostrf(hum5,4,3,x);
//    
//    int temp=0; for (int i=0; i <= 11; i++) {if (i==7){i++;}temp=temp << 1; temp=temp+ar[37+i]; Serial.print(ar[37+i]); } //2 bytes, (skip 1st nibble) skip parity bit of each byte
//    realtemp = (float(0.457)*float(temp))-float(103.20);
//    rtTrim=roundNumber(realtemp);
//    //rtTrim=String(realtemp).substring(0,String(realtemp).indexOf('.')+2);Serial.println(rtTrim);
//    //int realt=(int)(realtemp+0.5);
//    //Serial.print(" Raw Temperature: ");Serial.println(temp);
//    Serial.printf(" Raw Temperature: %d\n",temp);
//    //realtemp = (float(1)*float(temp))-float(0);
//    if(realtemp<43){
//      realtempF=32+realtemp*1.8;
//      rtfTrim=roundNumber(realtempF);
//      //rtfTrim=String(realtempF).substring(0,String(realtempF).indexOf('.')+2);Serial.println(rtfTrim);
//      Serial.print("Temperature37-12: "); Serial.print(realtemp,1);Serial.println("C");
//    }
//
//    int wind2=0; for (int i=0; i <= 6; i++) {wind2=wind2 << 1; wind2=wind2+ar[56+i]; Serial.print(ar[56+i]);}
//    float realwind2 = float(wind2) / float(50.1); //this might not be correct, need to see the values in higher wind conditions.
//    //int tempW = (int)(realwind2+0.5);
//    
//    if(windspeed-realwind2!=0){;
//      windspeed=realwind2;
//      wsTrim=roundNumber(windspeed);
//      //wsTrim=String(windspeed).substring(0,String(windspeed).indexOf('.')+2);Serial.println(wsTrim);
//      Serial.print(" ");Serial.print("wind56: "); Serial.print(realwind2);Serial.println("mph");//wind is averaged over an unknown period
//    }
//    Serial.println(" ");
//    }
//  pos=0;
//  yield();  
//}
//




