/*
  Based on WiFiTelnetToSerial - Example Transparent UART to Telnet Server for esp8266

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WiFi library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>   // Include the Wi-Fi-Multi library*/
#include <algorithm> // std::min
#include "Secrets.h"
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#define VERSION "Nodemcu Wifi USB Bridge - 2023"

/*
    SWAP_PINS:
   0: use Serial1 for logging (legacy example)
   1: configure Hardware Serial port on RX:GPIO13 TX:GPIO15
      and use SoftwareSerial for logging on
      standard Serial pins RX:GPIO3 and TX:GPIO1
*/

ESP8266WiFiMulti wifiMulti;
#include<SoftwareSerial.h> //Included SoftwareSerial Library
//Started SoftwareSerial at RX and TX pin of ESP8266/NodeMCU
SoftwareSerial s(3, 1);

/*
    Network listener şablonu

*/
//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 2
#define USE_UPPER_CASE true
const int port = 23;
WiFiServer server(port);
WiFiClient serverClients[MAX_SRV_CLIENTS];
String strTopla[MAX_SRV_CLIENTS]; //Clientlardan gelen komut biriktirme
String versionCmd = "VERSION";
String resetCmd = "RESET";
String quitCmd = "QUIT";

String sendBack = "";
String readData = "";

int restarting;
int disconnecting;

/*
   Reset fonksiyonu: 0 a git
*/
void(* resetFunc) (void) = 0;

void resetStrings(int i) {
  strTopla[i] = "";
}

void setupNetworkListener() {
  restarting = false;
  server.begin();
  server.setNoDelay(true);
}

void loopNetworkHandle() {
  if (server.hasClient()) {
    //find free/disconnected spot
    int i;
    for (i = 0; i < MAX_SRV_CLIENTS; i++)
      if (!serverClients[i]) { // equivalent to !serverClients[i].connected()
        serverClients[i] = server.available();
        resetStrings(i);
        break;
      }

    //no free/disconnected spot so reject
    if (i == MAX_SRV_CLIENTS) {
      WiFiClient tempClient;
      tempClient = server.available();
      tempClient.println("busy");
      //tempClient.flush();
      // hints: server.available() is a WiFiClient with short-term scope
      // when out of scope, a WiFiClient will
      // - flush() - all data will be sent
      // - stop() - automatically too
    }
  }

  for (int i = 0; i < MAX_SRV_CLIENTS; i++) {
    if (serverClients[i].connected()) {
      if (readData != "") {
        serverClients[i].println(readData);
        serverClients[i].flush();
      }
    }

    while (serverClients[i].available()) {
      if (readData != "") {
        serverClients[i].print(readData);
        serverClients[i].flush();
      }

      char a = serverClients[i].read();
      if (a != '\n')
        strTopla[i] += a;
      else {
        disconnecting = false;
        if (USE_UPPER_CASE) {
          strTopla[i].toUpperCase();
        }
        processNetworkCommand(strTopla[i]);
        if (sendBack != "") {
          serverClients[i].println(sendBack);
        }
        if (restarting) {
          delay(500);
          serverClients[i].flush();
          serverClients[i].stop();
          delay(500);
          resetFunc();
        }

        if (disconnecting) {
          delay(500);
          serverClients[i].flush();
          serverClients[i].stop();
          disconnecting = false;
        }

        resetStrings(i);
      }
    }
  }
  readData = "";

  // determine maximum output size "fair TCP use"
  // client.availableForWrite() returns 0 when !client.connected()
  size_t maxToTcp = 0;
  for (int i = 0; i < MAX_SRV_CLIENTS; i++)
    if (serverClients[i]) {
      size_t afw = serverClients[i].availableForWrite();
      if (afw) {
        if (!maxToTcp) {
          maxToTcp = afw;
        } else {
          maxToTcp = std::min(maxToTcp, afw);
        }
      } else {
        // warn but ignore congested clients
        //logger->println("one client is congested");
      }
    }
}

void addSendBack(String newHeader, String newValue) {
  sendBack = sendBack + newHeader + " : " + newValue + "\n";
}

void addSendBack(String newHeader, int newValue) {
  sendBack = sendBack + newHeader + " : " + newValue + "\n";
}

void processNetworkCommand(String currentCommand) {
  sendBack = "";
  if (currentCommand.startsWith(versionCmd)) {
    addSendBack("VERSION", VERSION);
  }
  else if (currentCommand.startsWith(resetCmd)) {
    restarting = true;
    addSendBack("RESTARTING", "NOW");
  }
  else if (currentCommand.startsWith(quitCmd)) {
    disconnecting = true;
    addSendBack("DISCONNECTING", "NOW");
  }
  else {
    /*
       Bu şablonun kullanıldığı kodlarda bu fonksiyon tanımlanacak
        bool customProcessNetworkCommand(String currentCommand)
    */
    if (!customProcessNetworkCommand(currentCommand)) {
      addSendBack("INVALID COMMAND", currentCommand);
    }
  }
}
/*
    Network listener şablonu sonu
    Uygulama özelinde processNetwork(String currentCommand) yeniden yazılmalı
*/

/*
   Bu fonksiyon şablonun kullanıldığı yerlerde düzenlenecek
*/
bool customProcessNetworkCommand(String currentCommand) {
  s.print("STRING READ: ");
  s.println(currentCommand);
  return true;
}

void setupSerial() {
  s.begin(57600);
}


void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;


    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
  });
  ArduinoOTA.begin();
}

void loopSerial() {
  while (s.available())
  {
    String reading = s.readString();
    readData += reading;
  }
}

void setup() {
  WiFi.mode(WIFI_STA);
  for ( int i = 0; i <  WIFISIZE; i++)
    wifiMulti.addAP(SECRET_SSIDS[i], SECRET_PSKS[i]);   // add Wi-Fi networks you want to connect to

  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(1000);
  }
  setupOTA();
  setupNetworkListener();
  setupSerial();
}

void loop() {
  //check if there are any new clients
  //setSpeed(50);
  ArduinoOTA.handle();
  loopSerial();
  loopNetworkHandle();
}
