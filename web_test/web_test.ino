#include <SPI.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"
#include <Arduino_JSON.h>

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
/////// WiFi Settings ///////
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

char serverAddress[] = "158.220.127.154";  // server address
int port = 3400;

WiFiClient c;
WebSocketClient client = WebSocketClient(c, serverAddress, port);

int count = 0;

void setup()
{
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); //Optional
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    // unsuccessful, retry in 4 seconds
    Serial.println("failed ... ");
    delay(4000);
    Serial.print("retrying ... ");
  }

  Serial.println("connected");

  Serial.println("starting WebSocket client");
  client.begin("/ws");

  JSONVar helloJson;
  helloJson["msgType"] = "hello";
  helloJson["id"] = "55";
  helloJson["x"] = "3";
  helloJson["y"] = "3";
  sendMessage(helloJson);

  JSONVar startJson;
  startJson["msgType"] = "gameStart";
  startJson["game"] = "tic-tac-toe";
  sendMessage(startJson);
}

void sendMessage(JSONVar msg) {
    client.beginMessage(TYPE_TEXT);
    client.print(JSON.stringify(msg));
    client.endMessage();
}

void onMessageRecieved(String answer) {
  JSONVar answerObject = JSON.parse(answer);
  Serial.println("Received a message:");
  String msgType = answerObject["msgType"];
  Serial.println("Type: " + msgType);
  if(msgType == "hello") {
    Serial.print("Said hello with id ");
    Serial.print(answerObject["id"]);
    Serial.println();
    Serial.print("Grid size is ");
    Serial.print(answerObject["x"]);
    Serial.print("x");
    Serial.print(answerObject["y"]);
    Serial.println();
  } else if(msgType == "gameStart") {
    Serial.print("Beginning game ");
    Serial.print(answerObject["game"]);
    Serial.println();
  } else if(msgType == "change") {
    Serial.print("Changed tile at ");
    Serial.print(answerObject["x"]);
    Serial.print("x");
    Serial.print(answerObject["y"]);
    Serial.print(" to color ");
    Serial.print(answerObject["color"]);
    Serial.println();
  }
}

void loop() {
  while (client.connected()) {
    Serial.print("Messaging!");
    JSONVar changeJson;
    changeJson["msgType"] = "change";
    changeJson["x"] = "1";
    changeJson["y"] = "1";
    changeJson["color"] = "rgb(255, 255, 255)";
    sendMessage(changeJson);

    // increment count for next message
    count++;

    // check if a message is available to be received
    int messageSize = client.parseMessage();

    if (messageSize > 0) {
      onMessageRecieved(client.readString());
    }

    // wait 5 seconds
    delay(5000);
  }

  Serial.println("disconnected, trying again");
  client.begin("/ws");

  JSONVar helloJson;
  helloJson["msgType"] = "hello";
  helloJson["id"] = "55";
  helloJson["x"] = "3";
  helloJson["y"] = "3";
  sendMessage(helloJson);
}