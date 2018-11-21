/*The purpose of this sketch is to relay LoRa, 433MHz radio data.
 * Reciever sensor data and send it moderate distance.
 */
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  
#include "SSD1306.h" 
//#include "images.h"
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


//433mhz receiver variables
byte pin = 3; // from receiver, GPIO3 RX
byte ar[116];
byte pos=0; 

//Weather variables
double realtemp; double realtempF;                                  
unsigned char hum;
double windspeed;
String rtfTrim;
String rtTrim;
String wsTrim;


void setup() {
  pinMode(16,OUTPUT);
  pinMode(25,OUTPUT);
  
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
  
  Serial.begin(9600);
  while (!Serial);
  Serial.println();
  Serial.print("LoRa Sender Wind Speed ");
  Serial.println(wsTrim); 
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  //LoRa.onReceive(cbk);
  //LoRa.receive();
  Serial.println("init ok");
  display.init();
  display.flipScreenVertically();  
  display.setFont(ArialMT_Plain_10);
  
  delay(1500);
}

void loop() {
  getWeather();  
  delay(1);
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  
  display.drawString(0, 0, "Sending packet: ");
  display.drawString(90, 0, String(counter));
  display.drawString(0, 10, "Sending wind: ");
  display.drawString(90, 10, wsTrim);
  display.display();

  //send packet
  LoRa.beginPacket();
  LoRa.print("Wind ");
  LoRa.println(wsTrim);
  LoRa.print(counter);
  LoRa.endPacket();

  counter++;
  digitalWrite(25, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(25, LOW);    // turn the LED off by making the voltage LOW
  delay(1000);                       // wait for a second
}

String roundNumber(float x)
{
  String roundNum="";
  float y=(x*10);
  int z=(int)(y+0.5);
  float zf=(float)z;
  roundNum=String(zf/10).substring(0,String(zf/10).indexOf('.')+2);
  return roundNum;
}

void getWeather(){
  Serial.print(".");
  b: pos=0;
  //Serial.print(".");
  unsigned long dur; //array, position in array, pulse duration
  unsigned long zeroL=600;
  unsigned long zeroS=301;
  unsigned long oneL=300;
  unsigned long oneS=100;
    dur = pulseIn(pin, LOW); if ((dur>zeroS)&&(dur<zeroL)) {ar[pos] = 0; pos++;
      dur = pulseIn(pin, LOW); if ((dur>zeroS)&&(dur<zeroL)) {ar[pos] = 0; pos++;
        dur = pulseIn(pin, LOW); if ((dur>zeroS)&&(dur<zeroL)) {ar[pos] = 0; pos++;
        }else goto b;
      }else goto b;
    }else goto b;

    //save the input RF pulse stream, pulse width of low;
    for (int i=1; i <= 61; i++){
      dur = pulseIn(pin, LOW);
      if ((dur>zeroS)&&(dur<zeroL)){ar[pos] = 0; pos++; }
      if ((dur>oneS)&&(dur<oneL)){ar[pos] = 1; pos++; }
    }
    int cc=0; for (int i=0; i <= 14; i++) {cc=cc << 1; cc=cc+ar[1+i]; 
    //Serial.print(ar[1+i]); 
    }
    //Serial.print(".");
    Serial.print("ChannelCode: "); Serial.print(cc);Serial.println("");

  if (cc==3619) //print data for valid channel code, old value was 3619
    {
    int batt=0; for (int i=0; i <= 6; i++) {batt=batt << 1; batt=batt+ar[21+i]; Serial.print(ar[21+i]); }//if battery is a byte then p1100100=100%
    Serial.print(" ");Serial.print("Battery21: "); Serial.print(batt);Serial.println("%");

    int hum5=0; for (int i=0; i <= 6; i++) {hum5=hum5 << 1; hum5=hum5+ar[29+i]; Serial.print(ar[29+i]); }
    Serial.print(" ");Serial.print("Humidity29-6: "); Serial.print(hum5);Serial.println("%");
    hum=hum5;

    int temp=0; for (int i=0; i <= 11; i++) {if (i==7){i++;}temp=temp << 1; temp=temp+ar[37+i]; Serial.print(ar[37+i]); } //2 bytes, (skip 1st nibble) skip parity bit of each byte
    realtemp = (float(0.457)*float(temp))-float(103.20);
    rtTrim=roundNumber(realtemp);
  
    Serial.printf(" Raw Temperature: %d\n",temp);
 
    if(realtemp<43){
      realtempF=32+realtemp*1.8;
      rtfTrim=roundNumber(realtempF);
 
      Serial.print("Temperature37-12: "); Serial.print(realtemp,1);Serial.println("C");
    }

    int wind2=0; for (int i=0; i <= 6; i++) {wind2=wind2 << 1; wind2=wind2+ar[56+i]; Serial.print(ar[56+i]);}
    float realwind2 = float(wind2) / float(50.1); //this might not be correct, need to see the values in higher wind conditions.

    //realwind2=1.23;
    Serial.print(String(realwind2));
    if(windspeed-realwind2!=0){;
      windspeed=realwind2;
      wsTrim=roundNumber(windspeed);
      wsTrim=String(windspeed).substring(0,String(windspeed).indexOf('.')+2);Serial.println(wsTrim);
      Serial.print(" ");Serial.print("wind56: "); Serial.print(realwind2);Serial.println("mph");//wind is averaged over an unknown period
    }
    Serial.println(" ");
    }
  pos=0;
  yield();  
}


