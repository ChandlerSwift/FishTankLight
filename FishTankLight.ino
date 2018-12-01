#include <TimeLib.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>

ESP8266WiFiMulti wifiMulti;

static const char ntpServerName[] = "us.pool.ntp.org";

const int timeZone = -6; // TODO: does this make sense?

int currentBrightness = 0;

static const int LIGHT_PIN = 2;

WiFiUDP Udp;

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

// For this to work, desiredBrightness much be > delay_ms (int divide otherwise)
void fadeTo(int desiredBrightness, unsigned long delay_ms) {
  Serial.println(desiredBrightness);
  long brightnessDifference = desiredBrightness - currentBrightness;
  if (brightnessDifference == 0) {
    Serial.println("No Change!");
    return;
  }

  unsigned long delayTime = delay_ms / abs(brightnessDifference); // TODO: non-int division?

  if (brightnessDifference < 0) { // TODO: combine loops
    for (int i = currentBrightness; i >= desiredBrightness; i--) {
      analogWrite(LIGHT_PIN, i);
      delay(delayTime);
    }
  } else {
    for (int i = currentBrightness; i <= desiredBrightness; i++) {
      analogWrite(LIGHT_PIN, i);
      delay(delayTime);
    }
  }
  currentBrightness = desiredBrightness;
}

void setup() {
  pinMode(LIGHT_PIN, OUTPUT);

  fadeTo(128, 0); // Dim

  Serial.begin(115200);
  delay(500); // todo: unnecessary?
  Serial.println("Starting");

#include "/home/chandler/Arduino/wifi-creds.h" // TODO: relative

  Serial.print('WiFi connecting');
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Udp.begin(8888);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(600);

  // wait for time
  while (!timeSet) {
    delay(100);
  }

  // initial fade out if nighttime
  if (hour() < 7 || hour() >= 21) {
    fadeTo(0, 5000);
  } else {
    fadeTo(1023, 5000);
  }
}

void loop() {
  if (hour() >= 7 && hour() < 21) { // Day: 7am - 9pm
    Serial.println("Brightening...");
    fadeTo(1023, 1200000ul); // 20 minutes = 20 min * 60 sec/min * 1000 ms/sec
    delay(1000);
  } else { // Night: 9pm - 7am
    Serial.println("Dimming...");
    fadeTo(0, 1200000ul);
    delay(1000);
  }

}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println('NTP updated time');
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
