#include <Arduino.h>
/*
 * Program to track and display the buy,spot,or sell price for cryptocurrencies.
 * By David E. Powell 
 *
 * Configuration is done via serial connection or by browsing to
 * the device. If the device can't connect to the local WiFi, it
 * creates its own access point for configuration.
 *  
 * Note: The case I made for this had to have the display rotated
 * 180 degrees. To do this, I had to customize the arduino-lib-oled
 * library.
 * In oled.cpp lines 248 and 249 I changed
 * i2c_send(0xA1); // segment remapping mode
 * and
 * i2c_send(0xC8); // COM output scan direction
 * to
 * i2c_send(0xA0); // segment remapping mode
 * and
 * i2c_send(0xC0); // COM output scan direction,
 * respectively.
 * 
 * **** to erase the entire flash chip in PlatformIO open
 * **** a terminal and type "pio run -t erase"
 */ 
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <oled.h>
#include "ESP8266WebServer.h"
#include "cryptoTracker.h"

char *stack_start;// initial stack size

ESP8266WebServer server(80);
OLED display=OLED(SDA,SCL,NO_RESET_PIN,0x3C,128,32,true);
//Adafruit_SSD1306 display(128, 32, &Wire, -1);

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
  char* myCoins[250]={}; //the coins that I'm interested in
  unsigned int myCoinsIndex=0;
  int priceType=2;  //1="buy", 2="spot", 3="sell"
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

unsigned int allCoinCount=sizeof(allCoins)/sizeof(allCoins[0]);
float previous[sizeof(allCoins)/sizeof(allCoins[0])];
float prices[sizeof(allCoins)/sizeof(allCoins[0])];
unsigned int displayIndex=0; //moving message

char webBuffer[21000];

void printStackSize(char id)
  {
  if (settings.debug)
    {
    char stack;
    Serial.print(id);
    Serial.print (F(": stack size "));
    Serial.println (stack_start - &stack);
    }
  }

char* fixup(char* rawString, const char* field, const char* value)
  {
  String rs=String(rawString);
  rs.replace(field,String(value));
  strcpy(rawString,rs.c_str());
  printStackSize('F');
  return rawString;
  }

// Scroll the crypto prices
void scrollCrypto()
  {
  if (settings.debug)
    Serial.println(F("Preparing to scroll"));

  static char* name1=settings.myCoins[0];
  static char* name2=settings.myCoins[1];
  static float price1=prices[0];
  static float price2=prices[1];

  //make sure we have something to display
  if (settings.myCoinsIndex==0)
    return;
  if (settings.myCoinsIndex==1)
    {
    settings.myCoins[1]=settings.myCoins[0];
    name2=settings.myCoins[1];
    }

  if (displayIndex>=settings.myCoinsIndex)
    displayIndex=0;
  price1=price2;
  name1=name2;
  displayIndex++;
  if (displayIndex>=settings.myCoinsIndex)
    displayIndex=0;
  price2=prices[displayIndex];
  name2=settings.myCoins[displayIndex];

  char message1[11];
  dtostrf(price1,-9,4,message1);
  char message2[11];
  dtostrf(price2,-9,4,message2);

  if (settings.debug)
    {
    Serial.print(F("Displaying "));
    Serial.print(name1);
    Serial.print("=");
    Serial.println(message1);
    Serial.print(F("Scrolling "));
    Serial.print(name2);
    Serial.print("=");
    Serial.println(message2);
    }

  for (int x=128;x>=0;x-=8)
    {
    display.clear();
    display.draw_string(0,0,name1,OLED::NORMAL_SIZE);
    display.draw_string(0,15,message1,OLED::DOUBLE_SIZE);
    display.draw_rectangle(x,0,DISPLAY_WIDTH,31,OLED::SOLID,OLED::BLACK);
    display.draw_string(x,0,name2,OLED::NORMAL_SIZE);
    display.draw_string(x,15,message2,OLED::DOUBLE_SIZE);


    checkForCommand(); // Check for input in case something needs to be changed to work

    //draw the up or down arrow
    if (x==0)
      {
      if (prices[displayIndex]>previous[displayIndex])
        {
        display.draw_bitmap_P(DISPLAY_WIDTH-13,0,13,16,(uint8_t*)FPSTR(upArrow),OLED::WHITE);
        }
      else if (prices[displayIndex]<previous[displayIndex])
        {
        display.draw_bitmap_P(DISPLAY_WIDTH-13,0,13,16,(uint8_t*)FPSTR(downArrow),OLED::WHITE);
        }
      }
    display.display();
    }
  printStackSize('S');
  if (settings.debug)
    Serial.println(F("Finished scroll"));
  }

float fetchPrice(char* coin)
  {
  printStackSize('A');
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
  wifiClient.setBufferSizes(2500, 128);
  if(!wifiClient.connect(CRYPTO_HOST, 443))
    {
    if (settings.debug)
      {
      Serial.print(F("connection failed: "));
      char errmsg[200];
      wifiClient.getLastSSLError(errmsg,200);
      Serial.println(errmsg);
      wifiClient.stop();
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
    switch (settings.priceType)
    {
    case 1:
      fixup(properURL,"{priceType}","buy");
      break;
    case 2:
      fixup(properURL,"{priceType}","spot");
      break;
    case 3:
      fixup(properURL,"{priceType}","sell");
      break;
    default:
      Serial.println(F("Invalid price type setting!"));
      fixup(properURL,"{priceType}","spot"); //default if bogus
      break;
    }
    String url=String(properURL);

    if (settings.debug)
      Serial.println("Fetching \""+url+"\"");

    wifiClient.println("GET "+url+" HTTP/1.0");
    wifiClient.println("Host: "+String(CRYPTO_HOST));
    wifiClient.println("Connection: close");
    wifiClient.println();

    //discard the headers
    boolean contacted=false; 
    unsigned long abandon=millis()+MAX_HEADER_TIME;
    while (wifiClient.connected() && millis()<abandon) 
      {
      String line = wifiClient.readStringUntil('\n');
      if (line == "\r") 
        {
        printStackSize('B');
        if (settings.debug)
          Serial.println(F("headers discarded"));
        contacted=true; //we're in touch with the server
        break;
        }
      }

    //Now for the payload
    if (wifiClient.connected() && contacted)
      {
      char response[PRICE_BUF_LENGTH]="";
      wifiClient.readBytes(response,PRICE_BUF_LENGTH);
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
          Serial.println(docRoot);
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
    }
//  digitalWrite(LED_BUILTIN,HIGH); //turn off the LED
  return retval;
  }
 
void setup() 
  {
  //init record of stack
  char stack;
  stack_start = &stack;  
  
  pinMode(LED_BUILTIN,OUTPUT);// The blue light on the board shows WiFi activity

  Serial.begin(115200);
  Serial.setTimeout(10000);
  Serial.println();
  
  while (!Serial); // wait here for serial port to connect.
  Serial.println(F("Running."));

  //reset the wifi
  WiFi.disconnect(true); 
  WiFi.softAPdisconnect(true);
  ESP.eraseConfig();

  size_t settingsSize=sizeof(settings);
  EEPROM.begin(settingsSize+settingsSize%4); //must be on a 4-byte boundary
  commandString.reserve(200); // reserve 200 bytes of serial buffer space for incoming command string

  display.begin();
  
  display.set_contrast(100);
  display.clear();

  display.draw_string(0,0,"Initializing...");
  if (settings.debug)
    Serial.println(F("Loading settings"));
  loadSettings(); //set the values from eeprom

  if (settings.scrollDelay<0 ||
      settings.scrollDelay>15 ||
      settings.priceType<1 ||
      settings.priceType>3) 
    {
    initializeSettings(); //must be a new board or flash was erased
    }

  if (settings.debug)
    Serial.println(F("Connecting to WiFi"));
  connectToWiFi(); //connect to the wifi

  display.display();
  }


void loop()
  {
  server.handleClient(); //get any incoming requests
  if (WiFi.status() == WL_CONNECTED)
    {

    static unsigned long nextScroll=millis()+settings.scrollDelay*1000;
    ESP.wdtFeed();  

    if (millis()>nextScroll)
      {
      scrollCrypto(); //show this one

      nextScroll=millis()+settings.scrollDelay*1000;
      
      static int aveCount=0;//keep a pseudo-running average of prices. Reset every 100 reads
      
      if (aveCount<100 && previous[displayIndex]>0) //is zero on startup
        {
        previous[displayIndex]=(previous[displayIndex]+prices[displayIndex])/2; //pseudo-running average
        aveCount++;
        }
      else
        {
        previous[displayIndex]=prices[displayIndex]; //reset average
        aveCount=0;
        }
      if (settings.myCoinsIndex>0 && displayIndex<settings.myCoinsIndex)
        {
        float price=fetchPrice(settings.myCoins[displayIndex]); //update this coin price
        if (price>0) //only update it if no errors
          prices[displayIndex]=price;
        }
      if (settings.debug)
        {
        Serial.print(F("Heap size is "));
        Serial.println(system_get_free_heap_size());
        printStackSize('L');
        }
      }
    }
  checkForCommand(); // Check for input in case something needs to be changed to work
  }

void processSettings()
  {
  if (settings.debug)
    {
    Serial.println("Processing settings...");
    }
  settings.myCoinsIndex=0;
  for (int i=0;i<server.args();i++)
    {
    const String value=server.arg(i);
    const String name=server.argName(i);

    if (settings.debug)
      Serial.println(name+String("=")+value);
    if (name.equals("plain"))
      continue;
    else if (name.equals("SSID"))
      strcpy(settings.ssid,value.c_str());
    else if (name.equals("wifiPassword"))
      strcpy(settings.wifiPassword,value.c_str());
    else if (name.equals("scrollDelay"))
      settings.scrollDelay=atoi(value.c_str());
    else if (name.equals("pricetype"))
      settings.priceType=atoi(value.c_str());
    else
      {
      for (unsigned int i=0;i<allCoinCount;i++)
        {
        if (name.equals((const char*)FPSTR(allCoins[i])))
          {
          Serial.print(settings.myCoinsIndex);
          Serial.print("=");
          Serial.println((const char*)FPSTR(allCoins[i]));
          previous[settings.myCoinsIndex]=0;
          prices[settings.myCoinsIndex]=0;
          settings.myCoins[settings.myCoinsIndex++]=(char*)FPSTR(allCoins[i]);
          break;
          }
        }
      }
    }
  saveSettings();
  }


char* buildSettingsPage()
  {
  char temp[150];
  char sel[10];

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
    if (i==settings.scrollDelay)
      strcpy(sel," selected");
    else
      strcpy(sel," ");
    fixup(temp,"{optNum}",itoa(i,num,10));
    fixup(temp,"{selected}",sel);
    strcat(webBuffer,temp);
    }

  //add the price type lines
  strcat_P(webBuffer,settingsPart3);
  strcpy_P(temp,priceTypeString);
  fixup(temp,"{ptype}","buy");
  fixup(temp,"{Ptype}","Buy");
  fixup(temp,"{ptypeNum}","1");
  fixup(temp,"{checked}",settings.priceType==1?" checked":" ");
  strcat(webBuffer,temp);
  strcpy_P(temp,priceTypeString);
  fixup(temp,"{ptype}","spot");
  fixup(temp,"{Ptype}","Spot");
  fixup(temp,"{ptypeNum}","2");
  fixup(temp,"{checked}",settings.priceType==2?" checked":" ");
  strcat(webBuffer,temp);
  strcpy_P(temp,priceTypeString);
  fixup(temp,"{ptype}","sell");
  fixup(temp,"{Ptype}","Sell");
  fixup(temp,"{ptypeNum}","3");
  fixup(temp,"{checked}",settings.priceType==3?" checked":" ");
  strcat(webBuffer,temp);

  //this is the currency 
  strcat_P(webBuffer,settingsPart4);
  //and the individual coin names
  for (unsigned int i=0;i<allCoinCount;i++)
    {
    if (i>0 && i%6==0)
      strcat_P(webBuffer,newRowString);
    strcpy_P(temp,checkboxString);
    fixup(temp,"{coin}",(const char*)FPSTR(allCoins[i]));
        
    char checked[10]="";
    for (unsigned int j=0;j<settings.myCoinsIndex;j++)
      {
      if (strcmp_P(settings.myCoins[j],allCoins[i])==0)
        {
        strcpy(checked," checked");
        break;
        }
      }
    fixup(temp,"{checked}",checked);
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
  static boolean wasConnected=false;
  static boolean retval=true; //assume connection to wifi is ok

  if (WiFi.status() != WL_CONNECTED)
    {
    if (settings.debug)
      {
      Serial.print(F("Attempting to connect to WPA SSID \""));
      Serial.print(settings.ssid);
      Serial.println("\"");
      }

    //notifiy the user
    char connmsg[120]="";
    strcat(connmsg,settings.ssid);
    strcat(connmsg,"...");
    display.clear();
    display.draw_string(0,10,"Connecting to ");
    display.draw_string(0,20,connmsg);
    display.display();

    WiFi.mode(WIFI_STA); //station mode, we are only a client in the wifi world
    WiFi.begin(settings.ssid, settings.wifiPassword);

    //If we were not previously connected, then
    //try for 60 seconds to connect to existing wifi.
    //If we were previously connected, keep trying.
    for (int i=0;i<120||wasConnected;i++)  
      {
      if (WiFi.status() == WL_CONNECTED)
        break;  // got it
      if (settings.debug)
        Serial.print(".");
      checkForCommand(); // Check for input in case something needs to be changed to work
      ESP.wdtFeed(); //feed the watchdog timers.
      delay(500);
      yield(); //not sure this is necessary here
      }
    char addr[18];  //used to display address on OLED

    if (WiFi.status() == WL_CONNECTED)
      {
      wasConnected=true; //We will only go into AP mode if rebooted
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
      Serial.print("Wifi status is ");
      Serial.println(WiFi.status());
      Serial.println(F("WiFi connection unsuccessful. Creating AP."));
      WiFi.disconnect(); 
      ESP.eraseConfig();
      WiFi.softAPConfig(myAPip,myAPip,myAPmask);  
      if (WiFi.softAP(F("cryptoTracker")))
        {
        Serial.println(F("AP started.  Use address 192.168.4.1"));
        strcpy(addr,WiFi.softAPIP().toString().c_str());
        }
      else
        {
        Serial.println(F("AP failed to start."));
        strcpy_P(addr,"Wifi Fail");
        }
      }
    //show the IP address
    Serial.println(addr);
    display.clear();
    display.draw_string(0,20,addr);
    if (WiFi.status() == WL_CONNECTED)
      {
      char buff[20]="Price type: ";
      strcat(buff,settings.priceType==1?"buy":settings.priceType==2?"spot":"sell");
      display.draw_string(0,10,buff);
      }
    else
      {
      display.draw_string(0,0,"WiFi not in range");
      display.draw_string(0,10,"Use \"cryptoTracker\"");
      }
    display.display();
    delay(5000);

    server.on("/", HTTP_GET, []()
      {
      server.send(200, "text/html", buildSettingsPage());
      });
    server.on("/set", HTTP_POST, []()
      {
      Serial.println("Processing POST");
      processSettings();
      server.sendHeader("Location", String("/"), true);
      server.send (302, "text/plain", "");
      });
  
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
  Serial.print("priceType=<1=buy, 2=spot, 3=sell> (");
  Serial.print(settings.priceType);
  Serial.println(")");
  Serial.print("myCoins=<crypto to display (use addCoin= to add more)> (");
  for (unsigned int i=0;i<settings.myCoinsIndex;i++)
    {
    Serial.print(settings.myCoins[i]);
    Serial.print(",");
    }
  Serial.println(")");
  Serial.print("debug=<print debug messages to serial port> (");
  Serial.print(settings.debug?"true":"false");
  Serial.println(")");
  Serial.println("\n*** Use \"factorydefaults=yes\" to reset all settings ***");
  Serial.println("*** Use \"reset=yes\" to reboot the device ***\n");
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
  else if (strcmp(nme,"addCoin")==0)
    {
    for (unsigned int i=0;i<allCoinCount;i++)
        {
        if (strcmp_P(val,allCoins[i])==0)
          {
          settings.myCoins[settings.myCoinsIndex++]=(char*)FPSTR(allCoins[i]);
          break;
          }
        }
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
  else if ((strcmp(nme,"reset")==0) && (strcmp(val,"yes")==0)) //reset the device
    {
    Serial.println("\n*********************** Resetting Device ************************");
    delay(1000);
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
  settings.myCoinsIndex=0;
  settings.priceType=2;
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
    strlen(settings.ssid)<=SSID_SIZE &&
    strlen(settings.wifiPassword)>0 &&
    strlen(settings.wifiPassword)<=PASSWORD_SIZE &&
    settings.priceType>0 && settings.priceType<4)
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

