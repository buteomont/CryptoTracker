
#define LED_ON LOW
#define LED_OFF HIGH

#define VALID_SETTINGS_FLAG 0xDAB0
#define SSID_SIZE 100
#define PASSWORD_SIZE 50
#define ADDRESS_SIZE 30
#define USERNAME_SIZE 50
#define COORDINATE_SIZE 10

#define CRYPTO_HOST "api.coinbase.com"
#define CRYPTO_URL "/v2/prices/{crypto}-USD/spot"
#define COINBASE_JSON_SIZE 500

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32

// Error codes copied from the MQTT library
// #define MQTT_CONNECTION_REFUSED            -2
// #define MQTT_CONNECTION_TIMEOUT            -1
// #define MQTT_SUCCESS                        0
// #define MQTT_UNACCEPTABLE_PROTOCOL_VERSION  1
// #define MQTT_IDENTIFIER_REJECTED            2
// #define MQTT_SERVER_UNAVAILABLE             3
// #define MQTT_BAD_USER_NAME_OR_PASSWORD      4
// #define MQTT_NOT_AUTHORIZED                 5

//WiFi status codes
//0 : WL_IDLE_STATUS when Wi-Fi is in process of changing between statuses
//1 : WL_NO_SSID_AVAILin case configured SSID cannot be reached
//3 : WL_CONNECTED after successful connection is established
//4 : WL_CONNECT_FAILED if password is incorrect
//6 : WL_DISCONNECTED if module is not configured in station mode

//prototypes
void incomingMqttHandler(char* reqTopic, byte* payload, unsigned int length);;
unsigned long myMillis();
bool processCommand(String cmd);
void checkForCommand();
boolean connectToWiFi();
void showSettings();
void reconnect(); 
void showSub(char* topic, bool subgood);
void initializeSettings();
void loadSettings();
boolean saveSettings();
void serialEvent(); 
void setup(); 
void loop();

//Flash constants
char webPage[] PROGMEM = {
  "<html><body>"\
  "This is a web page."\
  "</body></html>"
};

