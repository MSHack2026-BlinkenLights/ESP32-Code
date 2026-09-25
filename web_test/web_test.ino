#include <SPI.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
/////// WiFi Settings ///////
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

char serverAddress[] = "mshack26.0123456789abcdef.dev/ws";  // server address
int port = 443;

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
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    // unsuccessful, retry in 4 seconds
    Serial.println("failed ... ");
    delay(4000);
    Serial.print("retrying ... ");
  }

  Serial.println("connected");
}

void loop() {
  Serial.println("starting WebSocket client");
  client.begin();

  while (client.connected()) {
    Serial.print("Sending hello ");
    Serial.println(count);

    // send a hello #
    client.beginMessage(TYPE_TEXT);
    client.print("hello ");
    client.print(count);
    client.endMessage();

    // increment count for next message
    count++;

    // check if a message is available to be received
    int messageSize = client.parseMessage();

    if (messageSize > 0) {
      Serial.println("Received a message:");
      Serial.println(client.readString());
    }

    // wait 5 seconds
    delay(5000);
  }

  Serial.println("disconnected");
}