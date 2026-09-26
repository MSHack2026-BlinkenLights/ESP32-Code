#include "Freenove_WS2812_Lib_for_ESP32.h"

#include <SPI.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"
#include <Arduino_JSON.h>


// ============================================================
// LED CONFIGURATION
// ============================================================

#define LEDS_COUNT 256
#define LEDS_PIN   23
#define CHANNEL    0

int iRowSize = 8;
int iStandardDelay = 10;


// ============================================================
// WIFI / WEBSOCKET CONFIGURATION
// ============================================================

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

char serverAddress[] = "158.220.127.154";
int port = 3400;

WiFiClient c;
WebSocketClient client = WebSocketClient(c, serverAddress, port);

const char* DEVICE_ID = "55";


// ============================================================
// LED STATE
// ============================================================

int iLEDStates[LEDS_COUNT] = {0};
int iLED_R[LEDS_COUNT] = {0};
int iLED_G[LEDS_COUNT] = {0};
int iLED_B[LEDS_COUNT] = {0};


// ============================================================
// LED GROUPS
// ============================================================

// Each Tic-Tac-Toe square consists of four physical LEDs.
uint8_t mLEDGroups[9][4] =
{
  {7, 6, 8, 9},
  {5, 4, 10, 11},
  {3, 2, 12, 13},

  {23, 22, 24, 25},
  {21, 20, 26, 27},
  {19, 18, 28, 29},

  {39, 38, 40, 41},
  {37, 36, 42, 43},
  {35, 34, 44, 45}
};

int iLEDGroupStatus[9] = {0};


// ============================================================
// TIC-TAC-TOE GAME DATA
// ============================================================

uint8_t mWins[8][3] =
{
  {0, 1, 2},
  {3, 4, 5},
  {6, 7, 8},
  {0, 3, 6},
  {1, 4, 7},
  {2, 5, 8},
  {0, 4, 8},
  {2, 4, 6}
};

// Player 0 = Red
// Player 1 = Blue
uint8_t mPlayerClrs[2][3] =
{
  {255, 0, 0},
  {0, 0, 255}
};

int iActivePlayer = 0;
int iGameActive = 0;


// ============================================================
// LED STRIP
// ============================================================

Freenove_ESP32_WS2812 strip =
  Freenove_ESP32_WS2812(
    LEDS_COUNT,
    LEDS_PIN,
    CHANNEL,
    TYPE_GRB
  );


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(9600);

  while (!Serial)
  {
    ;
  }

  // ----------------------------------------------------------
  // LED setup
  // ----------------------------------------------------------

  strip.begin();
  strip.setBrightness(5);

  resetAll();

  // ----------------------------------------------------------
  // WiFi setup
  // ----------------------------------------------------------

  connectWiFi();

  // ----------------------------------------------------------
  // WebSocket setup
  // ----------------------------------------------------------

  connectWebSocket();

  // Tell server who we are and our grid size.
  JSONVar helloJson;

  helloJson["msgType"] = "hello";
  helloJson["id"] = DEVICE_ID;
  helloJson["x"] = "3";
  helloJson["y"] = "3";

  sendMessage(helloJson);
  sendStartMessage();


  Serial.println("Setup complete.");
}

void sendStartMessage() {
  // Tell server which game we are running.
  JSONVar startJson;

  startJson["msgType"] = "gameStart";
  startJson["game"] = "tic-tac-toe";

  sendMessage(startJson);
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // Keep WiFi alive.
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  // Keep WebSocket alive.
  if (!client.connected())
  {
    reconnectWebSocket();
  }

  // ----------------------------------------------------------
  // Check for incoming WebSocket messages.
  // ----------------------------------------------------------

  int messageSize = client.parseMessage();

  if (messageSize > 0)
  {
    String message = client.readString();

    onMessageReceived(message);
  }

  // ----------------------------------------------------------
  // Start a new local game after a previous game finished.
  // ----------------------------------------------------------

  if (!iGameActive)
  {
    setupTicTacToe();
  }

  /*
    IMPORTANT:

    The original sketch automatically played random moves:

        if(pickGroup(getRandomGroup()))
          delay(iStandardDelay * 100);

    That has been removed here.

    The board is now controlled by WebSocket "change"
    messages from the server.

    If you want the ESP32 to remain a standalone random
    Tic-Tac-Toe player, uncomment this:

        if (pickGroup(getRandomGroup()))
          delay(iStandardDelay * 100);
  */
          //if (pickGroup(getRandomGroup()))
          //delay(iStandardDelay * 1000);
}


// ============================================================
// WIFI
// ============================================================

void connectWiFi()
{
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("failed ...");
    delay(4000);

    Serial.println("retrying ...");
  }

  Serial.println("connected");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


// ============================================================
// WEBSOCKET
// ============================================================

void connectWebSocket()
{
  Serial.println("Starting WebSocket client");

  client.begin("/ws");

  Serial.println("WebSocket connected");
}


void reconnectWebSocket()
{
  Serial.println("WebSocket disconnected, trying again...");

  delay(1000);

  client.begin("/ws");

  JSONVar reconnectJson;

  reconnectJson["msgType"] = "reconnect";
  reconnectJson["id"] = DEVICE_ID;

  sendMessage(reconnectJson);

  Serial.println("WebSocket reconnected");
}


// ============================================================
// WEBSOCKET MESSAGING
// ============================================================

void sendMessage(JSONVar msg)
{
  if (!client.connected())
  {
    Serial.println("Cannot send message - WebSocket disconnected");
    return;
  }

  client.beginMessage(TYPE_TEXT);

  client.print(JSON.stringify(msg));

  client.endMessage();
}


void onMessageReceived(String answer)
{
  Serial.println();
  Serial.println("Received:");
  Serial.println(answer);

  JSONVar answerObject = JSON.parse(answer);

  if (JSON.typeof(answerObject) == "undefined")
  {
    Serial.println("Invalid JSON received.");
    return;
  }

  String msgType = answerObject["msgType"];

  Serial.print("Message type: ");
  Serial.println(msgType);


  // ----------------------------------------------------------
  // HELLO
  // ----------------------------------------------------------

  if (msgType == "hello")
  {
    Serial.print("Said hello with id ");
    Serial.println((const char*)answerObject["id"]);

    Serial.print("Grid size is ");
    Serial.print((const char*)answerObject["x"]);
    Serial.print("x");
    Serial.println((const char*)answerObject["y"]);
  }


  // ----------------------------------------------------------
  // GAME START
  // ----------------------------------------------------------

  else if (msgType == "gameStart")
  {
    Serial.print("Beginning game ");
    Serial.println((const char*)answerObject["game"]);

    setupTicTacToe();
  }


  // ----------------------------------------------------------
  // CHANGE
  // ----------------------------------------------------------

  else if (msgType == "change")
  {
    handleChangeMessage(answerObject);
  }

  else if(msgType == "buttonPress") {
    handleButtonPress(answerObject);
  }
}

void handleButtonPress(JSONVar answerObject) {
  int x = answerObject["x"];
  int y = answerObject["y"];
  int group = ((y - 1) * 3) + (x - 1);
  pickGroup(group);
}



// ============================================================
// HANDLE REMOTE CHANGE
// ============================================================

void handleChangeMessage(JSONVar answerObject)
{
  int x = answerObject["x"];
  int y = answerObject["y"];
  String colorString = answerObject["color"];

  Serial.print("Changed tile at ");
  Serial.print(x);
  Serial.print("x");
  Serial.print(y);
  Serial.print(" to color ");
  Serial.println(colorString);


  // ----------------------------------------------------------
  // Convert server coordinates (1-3) to group index (0-8)
  // ----------------------------------------------------------

  if (x < 1 || x > 3 || y < 1 || y > 3)
  {
    Serial.println("Invalid board coordinates.");
    return;
  }

  int group = ((y - 1) * 3) + (x - 1);


  // ----------------------------------------------------------
  // Parse RGB color
  // Expected format:
  //
  // rgb(255, 0, 0)
  // ----------------------------------------------------------

  int r;
  int g;
  int b;

  if (!parseRGB(colorString, r, g, b))
  {
    Serial.println("Could not parse RGB color.");
    return;
  }


  // ----------------------------------------------------------
  // Update physical LED group
  // ----------------------------------------------------------

  setGroupClr(group, r, g, b);


  // ----------------------------------------------------------
  // Update local game state
  //
  // Non-black = occupied.
  // ----------------------------------------------------------

  if ((r + g + b) > 0)
  {
    if (isSameColor(r, g, b, 255, 0, 0))
    {
      iLEDGroupStatus[group] = 1;
    }
    else if (isSameColor(r, g, b, 0, 0, 255))
    {
      iLEDGroupStatus[group] = 2;
    }
    else
    {
      // Any other non-black color means occupied.
      iLEDGroupStatus[group] = 1;
    }
  }
  else
  {
    iLEDGroupStatus[group] = 0;
  }
}


// ============================================================
// RGB PARSER
// ============================================================

bool parseRGB(String color, int &r, int &g, int &b)
{
  color.trim();

  if (!color.startsWith("rgb("))
  {
    return false;
  }

  if (!color.endsWith(")"))
  {
    return false;
  }

  String values = color.substring(4, color.length() - 1);

  int firstComma = values.indexOf(',');
  int secondComma = values.indexOf(',', firstComma + 1);

  if (firstComma < 0 || secondComma < 0)
  {
    return false;
  }

  String rString = values.substring(0, firstComma);
  String gString = values.substring(
    firstComma + 1,
    secondComma
  );
  String bString = values.substring(secondComma + 1);

  r = rString.toInt();
  g = gString.toInt();
  b = bString.toInt();

  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);

  return true;
}


bool isSameColor(
  int r1,
  int g1,
  int b1,
  int r2,
  int g2,
  int b2
)
{
  return
    r1 == r2 &&
    g1 == g2 &&
    b1 == b2;
}


// ============================================================
// SEND BOARD CHANGE
// ============================================================

void sendChange(String x, String y, String col)
{
  JSONVar changeJson;

  changeJson["msgType"] = "change";
  changeJson["x"] = x;
  changeJson["y"] = y;
  changeJson["color"] = col;

  sendMessage(changeJson);
}


void sendChange(int x, int y, int r, int g, int b)
{
  String rgb = "rgb(";

  rgb += String(r);
  rgb += ",";
  rgb += String(g);
  rgb += ",";
  rgb += String(b);
  rgb += ")";

  sendChange(
    String(x),
    String(y),
    rgb
  );
}


// ============================================================
// LED SET FUNCTIONS
// ============================================================

int setLEDClr(
  int iLED,
  int iR,
  int iG,
  int iB
)
{
  int iStatus = 0;

  if ((iR + iG + iB) > 0)
  {
    iStatus = 1;
  }

  strip.setLedColorData(
    iLED,
    iR,
    iG,
    iB
  );

  strip.show();

  iLEDStates[iLED] = iStatus;

  if (!iStatus)
  {
    iLED_R[iLED] = 0;
    iLED_G[iLED] = 0;
    iLED_B[iLED] = 0;

    return iStatus;
  }

  iLED_R[iLED] = iR;
  iLED_G[iLED] = iG;
  iLED_B[iLED] = iB;

  return iStatus;
}


void setGroupClr(
  int iGroup,
  int iR,
  int iG,
  int iB
)
{
  for (int i = 0; i < 4; i++)
  {
    setLEDClr(
      mLEDGroups[iGroup][i],
      iR,
      iG,
      iB
    );
  }
}


int toggleLED(int iLED)
{
  if (getLEDStatus(iLED))
  {
    return setLEDClr(
      iLED,
      0,
      0,
      0
    );
  }

  int r = iLED_R[iLED];
  int g = iLED_G[iLED];
  int b = iLED_B[iLED];

  if ((r + g + b) <= 0)
  {
    r = 255;
    g = 255;
    b = 255;
  }

  setLEDClr(
    iLED,
    r,
    g,
    b
  );

  return getLEDStatus(iLED);
}


// ============================================================
// RESET
// ============================================================

void resetAll()
{
  wipeBoard();
  resetLEDGroups();
  resetPlayerdata();
  sendGameEnd();
}

void sendGameEnd() {
  JSONVar endJson;
  endJson["msgType"] = "gameEnd";
  sendMessage(endJson);
}

void wipeBoard()
{
  for (int i = 0; i < LEDS_COUNT; i++)
  {
    iLED_R[i] = 0;
    iLED_G[i] = 255;
    iLED_B[i] = 0;

    setLEDClr(
      i,
      0,
      0,
      0
    );
  }
}


void resetLEDGroups()
{
  for (int j = 0; j < 9; j++)
  {
    iLEDGroupStatus[j] = 0;

    for (int i = 0; i < 4; i++)
    {
      int iLED = mLEDGroups[j][i];

      iLED_R[iLED] = 0;
      iLED_G[iLED] = 255;
      iLED_B[iLED] = 0;

      setLEDClr(
        iLED,
        0,
        0,
        0
      );

      sendChange(j,i,0,0,0);
    }
  }
}


void resetPlayerdata()
{
  iActivePlayer = 0;
}


// ============================================================
// GETTERS
// ============================================================

int getLEDStatus(int iLED)
{
  return iLEDStates[iLED];
}


int getColumn(int iLED)
{
  return iLED % iRowSize;
}


int getRow(int iLED)
{
  return iLED / iRowSize;
}


int getLEDGroupStatus(int iGroup)
{
  return iLEDGroupStatus[iGroup];
}


int getRandomGroup()
{
  return random(9);
}


// ============================================================
// TIC-TAC-TOE
// ============================================================

void setupTicTacToe()
{
  FadeOver();

  resetAll();

  sendStartMessage();

  iGameActive = 1;

  delay(iStandardDelay * 100);
}


int pickGroup(int iGroup)
{
  if (getLEDGroupStatus(iGroup))
  {
    return 0;
  }

  setGroupClr(
    iGroup,
    mPlayerClrs[iActivePlayer][0],
    mPlayerClrs[iActivePlayer][1],
    mPlayerClrs[iActivePlayer][2]
  );

  iLEDGroupStatus[iGroup] = iActivePlayer + 1;


  // ----------------------------------------------------------
  // Tell WebSocket server about the move.
  // ----------------------------------------------------------

  int x = (iGroup % 3) + 1;
  int y = (iGroup / 3) + 1;

  sendChange(
    x,
    y,
    mPlayerClrs[iActivePlayer][0],
    mPlayerClrs[iActivePlayer][1],
    mPlayerClrs[iActivePlayer][2]
  );


  // ----------------------------------------------------------
  // Check win
  // ----------------------------------------------------------

  int iWinIndex = check4Win(iActivePlayer);

  if (iWinIndex >= 0)
  {
    return win(iWinIndex);
  }


  // ----------------------------------------------------------
  // Check tie
  // ----------------------------------------------------------

  int iTie = check4Tie();

  if (iTie)
  {
    return tie();
  }


  // ----------------------------------------------------------
  // Switch player
  // ----------------------------------------------------------

  if (iActivePlayer)
  {
    iActivePlayer = 0;
  }
  else
  {
    iActivePlayer = 1;
  }

  return 1;
}


int check4Win(int iPlr)
{
  int iWinningGroup = -1;

  for (int j = 0; j < 8; j++)
  {
    int iCount = 0;

    for (int i = 0; i < 3; i++)
    {
      if (
        getLEDGroupStatus(
          mWins[j][i]
        ) - 1 == iPlr
      )
      {
        iCount++;
      }
    }

    if (iCount == 3)
    {
      iWinningGroup = j;
      break;
    }
  }

  return iWinningGroup;
}


int win(int iIndex)
{
  int iR = mPlayerClrs[iActivePlayer][0];
  int iG = mPlayerClrs[iActivePlayer][1];
  int iB = mPlayerClrs[iActivePlayer][2];

  for (int iCount = 0; iCount < 3; iCount++)
  {
    for (int i = 0; i < 3; i++)
    {
      int group = mWins[iIndex][i];

      // Green blink
      setGroupClr(
        group,
        0,
        255,
        0
      );

      // Send green state to server
      int x = (group % 3) + 1;
      int y = (group / 3) + 1;

      sendChange(
        x,
        y,
        0,
        255,
        0
      );

      delay(iStandardDelay * 50);

      // Restore player's color
      setGroupClr(
        group,
        iR,
        iG,
        iB
      );

      // Send player's color to server
      sendChange(
        x,
        y,
        iR,
        iG,
        iB
      );
    }
  }

  iGameActive = 0;

  return 1;
}


int check4Tie()
{
  int iTie = 1;

  for (int i = 0; i < 9; i++)
  {
    if (!getLEDGroupStatus(i))
    {
      iTie = 0;
      break;
    }
  }

  return iTie;
}


int tie()
{
  iGameActive = 0;

  return 1;
}


// ============================================================
// DECORATION
// ============================================================

void FadeOver()
{
  int iR = 0;
  int iG = 255;
  int iB = 0;

  setGroupClr(0, iR, iG, iB);

  delay(iStandardDelay);

  setGroupClr(1, iR, iG, iB);
  setGroupClr(3, iR, iG, iB);

  delay(iStandardDelay);

  setGroupClr(2, iR, iG, iB);
  setGroupClr(4, iR, iG, iB);
  setGroupClr(6, iR, iG, iB);

  delay(iStandardDelay);

  setGroupClr(5, iR, iG, iB);
  setGroupClr(7, iR, iG, iB);

  delay(iStandardDelay);

  setGroupClr(8, iR, iG, iB);

  delay(iStandardDelay * 2);


  setGroupClr(0, 0, 0, 0);

  delay(iStandardDelay);

  setGroupClr(1, 0, 0, 0);
  setGroupClr(3, 0, 0, 0);

  delay(iStandardDelay);

  setGroupClr(2, 0, 0, 0);
  setGroupClr(4, 0, 0, 0);
  setGroupClr(6, 0, 0, 0);

  delay(iStandardDelay);

  setGroupClr(5, 0, 0, 0);
  setGroupClr(7, 0, 0, 0);

  delay(iStandardDelay);

  setGroupClr(8, 0, 0, 0);
}