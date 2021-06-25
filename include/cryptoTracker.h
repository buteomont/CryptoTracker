
#define LED_ON LOW
#define LED_OFF HIGH

#define VALID_SETTINGS_FLAG 0xDAB0
#define SSID_SIZE 100
#define PASSWORD_SIZE 50
#define ADDRESS_SIZE 30
#define USERNAME_SIZE 50
#define COORDINATE_SIZE 10

#define CRYPTO_HOST "api.coinbase.com"
#define CRYPTO_URL "/v2/prices/{crypto}-USD/{priceType}"
#define COINBASE_JSON_SIZE 500
#define PRICE_BUF_LENGTH 100

#define MAX_HEADER_TIME 5000 //milliseconds to wait for server response

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32

static const uint8_t upArrow[] PROGMEM =
  {
  0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80,
  0x00, 0x00, 0x00, 0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0x00, 0x00

  };
static const uint8_t downArrow[] PROGMEM =
  {
  0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01
  };

const char* const allCoins[] PROGMEM =
	{
	"1INCH", "AAVE", "ADA", "ALGO", "ANKR", "ATOM", "BAL", "BAND", "BAT", "BCH",
	"BNT", "BTC", "CGLD", "COMP", "CRV", "DAI", "DASH", "DOGE","ENJ", "EOS", "ETC",
	"ETH", "FIL", "FORTH", "GRT", "KNC", "LINK", "LRC", "LTC", "MANA", "MATIC",
	"MKR", "NKN", "NMR", "NU", "OGN", "OMG", "OXT", "REN", "REP", "SKL",
	"SNX", "STORJ", "SUSHI", "UMA", "UNI", "USDT", "WBTC", "XLM", "XTZ", "YFI",
	"ZEC", "ZRX"	
	};
	// {"1INCH", "AAVE", "ADA", "AED", "AFN", "ALGO", "ALL", "AMD", "ANG", "ANKR",
	// "AOA", "ARS", "ATOM", "AUD", "AWG", "AZN", "BAL", "BAM", "BAND", "BAT",
	// "BBD", "BCH", "BDT", "BGN", "BHD", "BIF", "BMD", "BND", "BNT", "BOB",
	// "BRL", "BSD", "BSV", "BTC", "BTN", "BWP", "BYN", "BYR", "BZD", "CAD",
	// "CDF", "CGLD", "CHF", "CLF", "CLP", "CNH", "CNY", "COMP", "COP", "CRC",
	// "CRV", "CUC", "CVC", "CVE", "CZK", "DAI", "DASH", "DJF", "DKK", "DNT",
	// "DOGE","DOP", "DZD", "EGP", "ENJ", "EOS", "ERN", "ETB", "ETC", "ETH", "ETH2",
	// "EUR", "FIL", "FJD", "FKP", "FORTH", "GBP", "GBX", "GEL", "GGP", "GHS",
	// "GIP", "GMD", "GNF", "GRT", "GTQ", "GYD", "HKD", "HNL", "HRK", "HTG",
	// "HUF", "IDR", "ILS", "IMP", "INR", "IQD", "ISK", "JEP", "JMD", "JOD",
	// "JPY", "KES", "KGS", "KHR", "KMF", "KNC", "KRW", "KWD", "KYD", "KZT",
	// "LAK", "LBP", "LINK", "LKR", "LRC", "LRD", "LSL", "LTC", "LYD", "MAD",
	// "MANA", "MATIC", "MDL", "MGA", "MKD", "MKR", "MMK", "MNT", "MOP", "MRO",
	// "MTL", "MUR", "MVR", "MWK", "MXN", "MYR", "MZN", "NAD", "NGN", "NIO",
	// "NKN", "NMR", "NOK", "NPR", "NU", "NZD", "OGN", "OMG", "OMR", "OXT",
	// "PAB", "PEN", "PGK", "PHP", "PKR", "PLN", "PYG", "QAR", "REN", "REP",
	// "RON", "RSD", "RUB", "RWF", "SAR", "SBD", "SCR", "SEK", "SGD", "SHP",
	// "SKL", "SLL", "SNX", "SOS", "SRD", "SSP", "STD", "STORJ", "SUSHI", "SVC",
	// "SZL", "THB", "TJS", "TMT", "TND", "TOP", "TRY", "TTD", "TWD", "TZS",
	// "UAH", "UGX", "UMA", "UNI", "USD", "USDC", "UYU", "UZS", "VES", "VND",
	// "VUV", "WBTC", "WST", "XAF", "XAG", "XAU", "XCD", "XDR", "XLM", "XOF",
	// "XPD", "XPF", "XPT", "XTZ", "YER", "YFI", "ZAR", "ZEC", "ZMW", "ZRX",
	// "ZWL"};

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

//Settings web page

const char ssidString[] PROGMEM ="<td>WiFi SSID:</td><td><input type=text name=SSID value={ssid}></td>";
const char passwordString[] PROGMEM ="<td>WiFi Password:</td><td><input type=text name=wifiPassword value={wifiPassword}></td>";
const char scrollDelayOptionString[] PROGMEM ="<option value={optNum} {selected}>{optNum}</option>";
const char priceTypeString[] PROGMEM = "<input type=radio id={ptype} name=pricetype value={ptypeNum}{checked}><label for={ptype}>{Ptype}</label>&nbsp;&nbsp;&nbsp;";
const char checkboxString[] PROGMEM ="<td><input type=checkbox name={coin} id={coin} value={coin}{checked}><label for={coin}>{coin}</label></td>";
const char newRowString[] PROGMEM ="</tr><tr>";
const char settingsPart1[] PROGMEM = 
"<html>"
"  <body>"
"    <center>Luke's CryptoTracker Settings</center>"
"    <br><br>"
"    <form method=POST action=/set>"
"      <table>"
"	<tr>";
//SSID string goes here
//Next row string goes here
//WiFi password string goes here
//Next row string goes here
const char settingsPart2[] PROGMEM = 
"	  <td>Scroll Delay (seconds):</td><td><select id=scrollDelay name=scrollDelay>";
//scrollDelayOptionString goes here (x10)
const char settingsPart3[] PROGMEM = 
"	    </select>"
"	    </td>"
"	  </tr>"
"	<tr>"
"		<td>Price type:</td>"
"		<td valign=top>";
//price types go here
const char settingsPart4[] PROGMEM =
"		</td>"
"	</tr>"
"	<tr>"
"	  <td valign=top>Currencies:</td><td>"
"	    <table>"
"	      <tr>";
//checkboxString goes here (once for each coin, newRowString every 5 rows)
const char settingsPartEnd[] PROGMEM = 
"		</tr>"
"	    <tr><td colspan=6 align=center><hr>You may need to restart the device if you change the network settings<hr></td></tr>"
"	    <tr><td colspan=6 align=center><input type=submit></td></tr>"
"	    </table>"
"	    </td>"
"	  </tr>"
"	</table>"
"      </form>"
"    </body>"
"  </html>" ;

