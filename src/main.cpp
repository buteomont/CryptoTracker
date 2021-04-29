#include <Arduino.h>

/*
 * Program to track and display the spot price for cryptocurrencies.
Below is a list of the supported cryptos. It's frustrating how
hard it is to find the name that goes with the ticker symbol.

1INCH - (ETH exchange)
AAVE
ADA - Cardano
AED
AFN
ALGO
ALL
AMD
ANG
ANKR
AOA
ARS
ATOM
AUD
AWG
AZN
BAL
BAM
BAND
BAT - Basic Attent.
BBD
BCH - Bitcoin Cash
BDT
BGN
BHD
BIF
BMD
BND
BNT - Bancor
BOB
BRL
BSD
BSV
BTC - Bitcoin
BTN
BWP
BYN
BYR
BZD
CAD
CDF
CGLD
CHF
CLF
CLP
CNH
CNY
COMP
COP
CRC
CRV
CUC
CVC - Civic
CVE
CZK
DAI
DASH - Dash
DJF
DKK
DNT - district0x
DOP
DZD
EGP
ENJ - Enjin Coin
EOS - EOS
ERN
ETB
ETC - Ethereum Classic
ETH - Ethereum
ETH2
EUR
FIL
FJD
FKP
FORTH
GBP
GBX
GEL
GGP
GHS
GIP
GMD
GNF
GRT
GTQ
GYD
HKD
HNL
HRK
HTG
HUF
IDR
ILS
IMP
INR
IQD
ISK
JEP
JMD
JOD
JPY
KES
KGS
KHR
KMF
KNC - Kyber Network
KRW
KWD
KYD
KZT
LAK
LBP
LINK - ChainLink
LKR
LRC - Loopring
LRD
LSL
LTC - Litecoin
LYD
MAD
MANA - Decentraland
MATIC
MDL
MGA
MKD
MKR - Maker
MMK
MNT
MOP
MRO
MTL - Metal
MUR
MVR
MWK
MXN
MYR
MZN
NAD
NGN
NIO
NKN
NMR
NOK
NPR
NU
NZD
OGN
OMG - OmiseGO
OMR
OXT
PAB
PEN
PGK
PHP
PKR
PLN
PYG
QAR
REN
REP - Augur
RON
RSD
RUB
RWF
SAR
SBD
SCR
SEK
SGD
SHP
SKL
SLL
SNX
SOS
SRD
SSP
STD
STORJ - Storj
SUSHI
SVC
SZL
THB
TJS
TMT
TND
TOP
TRY
TTD
TWD
TZS
UAH
UGX
UMA
UNI
USD
USDC
UYU
UZS
VES
VND
VUV
WBTC
WST
XAF
XAG
XAU
XCD
XDR
XLM - Stellar
XOF
XPD
XPF
XPT
XTZ
YER
YFI
ZAR
ZEC - Zcash
ZMW
ZRX - 0x
ZWL
 
*/
#include <Arduino.h>
/**
 * This is an ESP8266 program to display the time and weather on an
 * OLED display.
 *
 * Configuration is done via serial connection.  Enter:
 *  ssid=<wifi ssid>
 *  wifipass=<wifi password>
 *  scrollDelay=<seconds to display each coin> 
 */ 
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <pgmspace.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <oled.h>
#include "ESPAsyncWebServer.h"
#include "cryptoTracker.h"

AsyncWebServer server(80);
OLED display=OLED(SDA,SCL,NO_RESET_PIN,0x3C,128,32,true);

WiFiClientSecure wifiClient;
HTTPClient http;

// These are the settings that get stored in EEPROM.  They are all in one struct which
// makes it easier to store and retrieve.
typedef struct 
  {
  unsigned int validConfig=0; 
  char ssid[SSID_SIZE] = "";
  char wifiPassword[PASSWORD_SIZE] = "";
  unsigned int scrollDelay=3; //seconds to pause scrolling for each crypto
  char myCoins[250]={}; //the coins that I'm interested in
  boolean debug=false;
  } conf;

conf settings; //all settings in one struct makes it easier to store in EEPROM
boolean settingsAreValid=false;

String commandString = "";     // a String to hold incoming commands from serial
bool commandComplete = false;  // goes true when enter is pressed

char properURL[sizeof CRYPTO_URL];
IPAddress cryptoAddr; //IP address of CRYPTO_HOST

StaticJsonDocument<COINBASE_JSON_SIZE> jDoc; //this will hold the weather query results

IPAddress myAPip(192,168,4,1);
IPAddress myAPmask(255,255,255,0);

float previous[]={0,0,0};
float prices[]={0,0,0};
char* tickers[]={"BTC","ETH","1INCH"};
unsigned int displayCount=sizeof(prices)/sizeof(prices[0]);
unsigned int displayIndex=0; //moving message
char* name1=tickers[0];
char* name2=tickers[1];
float price1=prices[0];
float price2=prices[1];

char webBuffer[20000];

char* fixup(char* rawString, const char* field, const char* value)
  {
  String rs=String(rawString);
  rs.replace(field,String(value));
  strcpy(rawString,rs.c_str());
  }

// Scroll the crypto prices
void scrollCrypto()
  {
  if (displayIndex>=displayCount)
    displayIndex=0;
  price1=price2;
  name1=name2;
  displayIndex++;
  if (displayIndex>=displayCount)
    displayIndex=0;
  price2=prices[displayIndex];
  name2=tickers[displayIndex];

  char message1[11];
  dtostrf(price1,-9,4,message1);
  char message2[11];
  dtostrf(price2,-9,4,message2);


  for (int x=128;x>=0;x-=8)
    {
    checkForCommand(); // Check for input in case something needs to be changed to work

    display.clear();
    display.draw_string(0,0,name1,OLED::NORMAL_SIZE);
    display.draw_string(0,15,message1,OLED::DOUBLE_SIZE);
    display.draw_rectangle(x,0,DISPLAY_WIDTH,31,OLED::SOLID,OLED::BLACK);
    display.draw_string(x,0,name2,OLED::NORMAL_SIZE);
    display.draw_string(x,15,message2,OLED::DOUBLE_SIZE);

    //draw the up or down arrow
    if (x==0)
      {
      if (prices[displayIndex]>previous[displayIndex])
        {
        display.draw_bitmap_P(DISPLAY_WIDTH-13,0,13,16,upArrow,OLED::WHITE);
        }
      else if (prices[displayIndex]<previous[displayIndex])
        {
        display.draw_bitmap_P(DISPLAY_WIDTH-13,0,13,16,downArrow,OLED::WHITE);
        }
      }
    display.display();
    }
  }

float fetchPrice(char* coin)
  {
//  digitalWrite(LED_BUILTIN,LOW); //turn on the LED
  digitalWrite(LED_BUILTIN,HIGH); //turn off the LED
  float retval=-1;
  if (settings.debug)
    {
    Serial.print(F("Fetching price of "));
    Serial.print(String(coin)+"...");
    }
  connectToWiFi(); //may need to connect to the wifi

  ESP.wdtFeed();

  //Fetch the coin price JSON object
  wifiClient.setInsecure();
  wifiClient.setBufferSizes(1500, 128);
  if(!wifiClient.connect(CRYPTO_HOST, 443))
    {
    if (settings.debug)
      {
      Serial.print(F("connection failed: "));
      char errmsg[200];
      wifiClient.getLastSSLError(errmsg,200);
      Serial.println(errmsg);
      }
    }
  else 
    {
    if (settings.debug)
      {
      Serial.println(F("connected to server..."));
      }

    checkForCommand(); // Check for input in case something needs to be changed to work

    //insert the coin ticker symbol into the URL
    strcpy(properURL,CRYPTO_URL);
    fixup(properURL,"{crypto}",coin);
    String url=String(properURL);

    if (settings.debug)
      Serial.println("Fetching \""+url+"\"");

    wifiClient.println("GET "+url+" HTTP/1.0");
    wifiClient.println("Host: "+String(CRYPTO_HOST));
    wifiClient.println("Connection: close");
    wifiClient.println();

    //discard the headers
    while (wifiClient.connected()) 
      {
      String line = wifiClient.readStringUntil('\n');
      if (line == "\r") 
        {
        if (settings.debug)
          Serial.println(F("headers discarded"));
        break;
        }
      }

    //Now for the payload
    String response=wifiClient.readString();
    if (settings.debug)
      Serial.println(response);

    wifiClient.stop();

    //Decode the JSON response
    DeserializationError de=deserializeJson(jDoc, response);
    if (de.code()==de.Ok)
      {
      if (settings.debug)
        Serial.println("...done.");
      JsonObject docRoot = jDoc.as<JsonObject>();
      if (docRoot.containsKey("errors"))
        {
        Serial.print(F("Error returned for "));
        Serial.println(coin);
        }
      else  
        {
        retval=docRoot["data"]["amount"].as<float>();
        if (settings.debug)
          Serial.println(coin+String("'s value is ")+String(retval));
        }
      }
    else
      {
      Serial.print(F("...Error: "));
      Serial.println(de.code());
      }
    }
//  digitalWrite(LED_BUILTIN,HIGH); //turn off the LED
  return retval;
  }
 
void setup() 
  {
//  ESP.wdtEnable(WDTO_8S);
  pinMode(LED_BUILTIN,OUTPUT);// The blue light on the board shows WiFi activity

  Serial.begin(115200);
  Serial.setTimeout(10000);
  Serial.println();
  
  yield();
  while (!Serial); // wait here for serial port to connect.
  
  yield();

  if (settings.debug)
    {
    Serial.print(F("*********************** Setting size is "));
    Serial.println(sizeof(settings));
    }
  EEPROM.begin(sizeof(settings)); //fire up the eeprom section of flash
  commandString.reserve(200); // reserve 200 bytes of serial buffer space for incoming command string

  display.begin();

  display.set_contrast(100);
  display.clear();

  display.draw_string(0,0,"Initializing...");
  if (settings.debug)
    Serial.println(F("Loading settings"));
  loadSettings(); //set the values from eeprom

  // if (settingsAreValid) 
  //   {
    if (settings.debug)
      Serial.println(F("Connecting to WiFi"));
    if (!connectToWiFi()); //connect to the wifi
//    }
  display.display();
  }

void loop()
  {
  if (WiFi.status() == WL_CONNECTED)
    {
    static unsigned long nextScroll=millis()+settings.scrollDelay*1000;
    ESP.wdtFeed();  

    if (millis()>nextScroll)
      {
      scrollCrypto();
      nextScroll=millis()+settings.scrollDelay*1000;
      previous[displayIndex]=prices[displayIndex];
      prices[displayIndex]=fetchPrice(tickers[displayIndex]); //update this coin price
      }
    }
  checkForCommand(); // Check for input in case something needs to be changed to work
  }

void processSettings(AsyncWebServerRequest* request)
  {
  Serial.println("Listing post data");
  for (int i=0;i<request->params();i++)
    {
    Serial.println(request->getParam(i)->name());
    }
//  if (request.hasParam())
  
  }



char* buildSettingsPage()
  {
  char temp[150];

  strcpy_P(webBuffer,settingsPart1);

  strcpy_P(temp,ssidString);
  fixup(temp,"{ssid}",settings.ssid);
  strcat(webBuffer,temp);
  strcat_P(webBuffer,newRowString);

  strcpy_P(temp,passwordString);
  fixup(temp,"{wifiPassword}",settings.wifiPassword);
  strcat(webBuffer,temp);
  strcat_P(webBuffer,newRowString);

  //add the scroll delay
  strcat_P(webBuffer,settingsPart2);
  //these are the scroll delay options
  for (unsigned int i=3;i<13;i++)
    {
    strcpy_P(temp,scrollDelayOptionString);
    char num[3];
    char sel[9]=" ";
    if (i==settings.scrollDelay)
      strcpy(sel,"selected");
    fixup(temp,"{optNum}",itoa(i,num,10));
    fixup(temp,"{selected}",sel);
    strcat(webBuffer,temp);
    }

  //this is the currency 
  strcat_P(webBuffer,settingsPart3);
  //and the individual coin names
  unsigned int coinCount=sizeof(allCoins)/sizeof(allCoins[0]);
  for (unsigned int i=0;i<coinCount;i++)
    {
    if (i>0 && i%6==0)
      strcat_P(webBuffer,newRowString);
    strcpy_P(temp,checkboxString);
    fixup(temp,"{coin}",allCoins[i]);
    strcat(webBuffer,temp);
    }

  strcat_P(webBuffer,settingsPartEnd);
   
  return webBuffer;
  }

/*
 * If not connected to wifi, connect.
 */
boolean connectToWiFi()
  {
  yield();
  static boolean retval=true; //assume connection to wifi is ok
  if (WiFi.status() != WL_CONNECTED)
    {
    if (settings.debug)
      {
      Serial.print(F("Attempting to connect to WPA SSID \""));
      Serial.print(settings.ssid);
      Serial.println("\"");
      }
    WiFi.mode(WIFI_STA); //station mode, we are only a client in the wifi world
    WiFi.begin(settings.ssid, settings.wifiPassword);

    //try for 5 seconds to connect to existing wifi
    for (int i=0;i<15;i++)  
      {
      if (WiFi.status() == WL_CONNECTED)
        break;  // not yet connected
      if (settings.debug)
        Serial.print(".");
      checkForCommand(); // Check for input in case something needs to be changed to work
      ESP.wdtFeed(); //feed the watchdog timers.
      delay(500);
      }
    char addr[16];  //used to display address on OLED

    if (WiFi.status() == WL_CONNECTED)
      {
      if (settings.debug)
        {
        Serial.println(F("Connected to network."));
        Serial.println();
        }
      strcpy(addr,WiFi.localIP().toString().c_str());
      retval=true;
      }
    else //can't connect to wifi, let's make our own
      {
      retval=false;
      Serial.println("Connection unsuccessful. Creating AP.");
      WiFi.disconnect(); 
      ESP.eraseConfig();
      WiFi.softAPConfig(myAPip,myAPip,myAPmask);  
      if (WiFi.softAP("cryptoWatcher"))
        {
        Serial.println("AP started.  Use address 192.168.4.1");
        strcpy(addr,WiFi.softAPIP().toString().c_str());
        }
      else
        {
        Serial.println("AP failed to start.");
        strcpy(addr,"Wifi Fail");
        }
      }
    //show the IP address
    Serial.println(addr);
    display.clear();
    display.draw_string(0,DISPLAY_HEIGHT/2-8,addr);
    display.display();
    delay(5000);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
      {
      request->send_P(200, "text/html", buildSettingsPage());
      });
    server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request)
      {
      Serial.println("Processing POST");
      processSettings(request);
      request->send_P(200, "text/html", buildSettingsPage());
      });
  
    ESP.wdtFeed();
    server.begin();
    }
  return retval;
  }

void showSettings()
  {
  Serial.print("ssid=<wifi ssid> (");
  Serial.print(settings.ssid);
  Serial.println(")");
  Serial.print("wifipass=<wifi password> (");
  Serial.print(settings.wifiPassword);
  Serial.println(")");
  Serial.print("scrollDelay=<number of seconds to show each crypto> (");
  Serial.print(settings.scrollDelay);
  Serial.println(")");
  Serial.print("debug=<print debug messages to serial port> (");
  Serial.print(settings.debug?"true":"false");
  Serial.println(")");
  Serial.println("\n*** Use \"factorydefaults=yes\" to reset all settings ***\n");
  }

/*
 * Check for configuration input via the serial port.  Return a null string 
 * if no input is available or return the complete line otherwise.
 */
String getConfigCommand()
  {
  if (commandComplete) 
    {
    String newCommand=commandString;

    commandString = "";
    commandComplete = false;
    return newCommand;
    }
  else return "";
  }

bool processCommand(String cmd)
  {
  const char *str=cmd.c_str();
  char *val=NULL;
  char *nme=strtok((char *)str,"=");
  if (nme!=NULL)
    val=strtok(NULL,"=");

  //Get rid of the carriage return
  if (val!=NULL && strlen(val)>0 && val[strlen(val)-1]==13)
    val[strlen(val)-1]=0; 

  if (nme==NULL || val==NULL || strlen(nme)==0 || strlen(val)==0)
    {
    showSettings();
    return false;   //not a valid command, or it's missing
    }
  else if (strcmp(nme,"ssid")==0)
    {
    strcpy(settings.ssid,val);
    saveSettings();
    }
  else if (strcmp(nme,"wifipass")==0)
    {
    strcpy(settings.wifiPassword,val);
    saveSettings();
    }
  else if (strcmp(nme,"scrollDelay")==0)
    {
    settings.scrollDelay=atoi(val);
    saveSettings();
    }
  else if (strcmp(nme,"debug")==0)
    {
    settings.debug=strcmp(val,"false")==0?false:true;
    saveSettings();
    }
 else if ((strcmp(nme,"factorydefaults")==0) && (strcmp(val,"yes")==0)) //reset all eeprom settings
    {
    Serial.println("\n*********************** Resetting EEPROM Values ************************");
    initializeSettings();
    saveSettings();
    delay(2000);
    ESP.restart();
    }
  else
    {
    showSettings();
    return false; //command not found
    }
  return true;
  }

void initializeSettings()
  {
  settings.validConfig=0; 
  strcpy(settings.ssid,"");
  strcpy(settings.wifiPassword,"");
  settings.scrollDelay=3;
  settings.debug=false;
  }

void checkForCommand()
  {
  if (Serial.available())
    {
    serialEvent();
    String cmd=getConfigCommand();
    if (cmd.length()>0)
      {
      processCommand(cmd);
      }
    }
  }
  
/*
*  Initialize the settings from eeprom and determine if they are valid
*/
void loadSettings()
  {
  EEPROM.get(0,settings);
  if (settings.validConfig==VALID_SETTINGS_FLAG)    //skip loading stuff if it's never been written
    {
    settingsAreValid=true;
    if (settings.debug)
      Serial.println("Loaded configuration values from EEPROM");
    }
  else
    {
    Serial.println("Skipping load from EEPROM, device not configured.");    
    settingsAreValid=false;
    }
  }

/*
 * Save the settings to EEPROM. Set the valid flag if everything is filled in.
 */
boolean saveSettings()
  {
  if (strlen(settings.ssid)>0 &&
    strlen(settings.wifiPassword)>0)
    {
    Serial.println("Settings deemed complete");
    settings.validConfig=VALID_SETTINGS_FLAG;
    settingsAreValid=true;
    }
  else
    {
    Serial.println("Settings still incomplete");
    settings.validConfig=0;
    settingsAreValid=false;
    }
    
  EEPROM.put(0,settings);
  return EEPROM.commit();
  }

/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
void serialEvent() 
  {
  while (Serial.available()) 
    {
    // get the new byte
    char inChar = (char)Serial.read();
    Serial.print(inChar);

    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it 
    if (inChar == '\n') 
      {
      commandComplete = true;
      }
    else
      {
      // add it to the inputString 
      commandString += inChar;
      }
    }
  }

