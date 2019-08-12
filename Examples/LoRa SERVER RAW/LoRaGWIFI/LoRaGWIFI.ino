/**************************************************************************
 @FILE:         LoRaGWIFI.ino
 @AUTHOR:       Raimundo Alfonso
 @COMPANY:      Ray Ingeniería Electronica, S.L.
 @DESCRIPTION:  Ejemplo de uso para el gateway LoRaGWIFI y emoncms (https://emoncms.org)
                Example of use for the LoRaGWIFI gateway and emoncms (https://emoncms.org)
  
 @LICENCE DETAILS:
  Este sketch está basada en software libre. Tu puedes redistribuir
  y/o modificar esta sketch bajo los términos de licencia GNU.

  Esta programa se distribuye con la esperanza de que sea útil,
  pero SIN NINGUNA GARANTÍA, incluso sin la garantía implícita de
  COMERCIALIZACIÓN O PARA UN PROPÓSITO PARTICULAR.
  Consulte los términos de licencia GNU para más detalles:
                                                                       
  http://www.gnu.org/licenses/gpl-3.0.txt

  This sketch is based on free software. You can redistribute
  and/or modify this library under the terms of the GNU license.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY, even without the implied warranty of
  MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU license terms for more details:   
  
  http://www.gnu.org/licenses/gpl-3.0.txtç

 @BOARD_TESTED:
  - NodeMCU 0.9 (ESP-12 MODULE)

 @VERSIONS:
  20-07-2019 - v1.00 : Primera versión
  
**************************************************************************/

#define  FIRMWARE_VERSION "1.00"
#define  HARDWARE_VERSION "190511"

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <EEPROM.h>
#include <RHReliableDatagram.h>       // https://www.airspayce.com/mikem/arduino/RadioHead/index.html
#include <RH_RF95.h>                  // https://www.airspayce.com/mikem/arduino/RadioHead/index.html
#include <SerialCMD.h>                // https://github.com/raymirabel/SerialCMD.git

//Check these values in the SerialCMD.h file of the SerialCMD library:
/*
#define CMD_MAX_COMMANDS        20    // Máximo numero de comandos permitidos
#define CMD_MAX_COMMANDS_LENGHT 10    // Longitud máxima de caracteres por cada comando
#define CMD_MAX_COMMANDS_ARGS   3     // Número máximo de argumentos por comando
#define CMD_MAX_INPUT_LENGHT    50    // Tamaño del buffer de entrada de comandos y argumentos
*/


// Command line class instance...
SerialCMD cmd;  


//#define DEVICE_TIMEOUT      1   // hours

#define HOST_NAME           "www.emoncms.org"
#define HOST_PORT           (80)


// Structure of default configuration parameters. This parameters are stored in internal eeprom...
typedef struct {
  char    ssid[33]        = "";     // [32 char]  Wifi SSID
  char    pass[33]        = "";     // [32 char]  Wifi PASSWORD
  char    key[33]         = "";     // [32 char]  Emoncms APIKEY
  byte    rfPower         = 23;     // [5...23]   RF power (5:min...23:max)
  byte    rfPan           = 210;    // [0...250]  PAN id (Only nodes with this same number are visible)
} stConfig;
stConfig config;


// Hardware definitions...
#define RFM95_CS        15
#define RFM95_INT       4
#define LED             16
#define DIR_EEPROM_CFG  10

// Registers and devices configuration...
#define SERVER_ADDRESS      250
#define MAX_REGS_BY_DEVICE  8
#define MAX_DEVICES         16
#define MAX_REGS            (MAX_REGS_BY_DEVICE * (MAX_DEVICES+1))  // 136 = 8 registros x 16+1 dispositivos

// RFM95 transceiver configuration and instances...
#define RF95_FREQ 868.0
RH_RF95 rf95(RFM95_CS, RFM95_INT);
RHReliableDatagram manager(rf95, SERVER_ADDRESS);
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];

        
int status    = WL_IDLE_STATUS;     // the Wifi radio's status
char server[] = HOST_NAME;


// Payload structures...  
enum {        
        MB_STATUS,
        MB_RSSI,
        MB_DATA1,
        MB_DATA2,
        MB_DATA3,
        MB_DATA4,
        MB_DATA5,
        MB_DATA6
};
typedef struct {
  uint8_t     pan_id;
  uint8_t     device_id;
  int16_t     data[MAX_REGS_BY_DEVICE];
} Payload;
Payload theData;

// wifi client class instance...
WiFiClient client;

/**************************************************************************
 * SETUP
 *************************************************************************/  
void setup(void){
  pinMode(LED, OUTPUT);
  led_test_off();

  EEPROM.begin(512);

  // Check if the eeprom is empty...
  if(EEPROM.read(0) == 123){
    // Read the configuration parameters...
    EEPROM.get(DIR_EEPROM_CFG, config);
  }else{
    // If it is empty, it saves the configuration parameters by default...
    EEPROM.write(0,123);
    EEPROM.put(DIR_EEPROM_CFG, config);
  }
  EEPROM.commit();

  // Init serial port...
  Serial.begin(9600);
  while (!Serial);
  delay(1000);

  // Init RFM95 module...
  radioInit();

  Serial.println();
  commandLineInit();
}


/**************************************************************************
 * LOOP
 *************************************************************************/  
void loop(void){
  uint8_t n;

  join_ap(); 

  cmd.task();

  while (manager.available()){
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    uint8_t to;
    uint8_t id;
    
    led_test_on();
    
    if (manager.recvfrom((uint8_t* )(&theData), &len, &from, &to, &id)){
      // Check PAN ID and ID...
      if((theData.pan_id == config.rfPan) && (theData.device_id < MAX_DEVICES)){     
        // Send ACK...
        manager.setHeaderId(id);
        manager.setHeaderFlags(RH_FLAGS_ACK);
        uint8_t ack = '!';
        manager.sendto(&ack, sizeof(ack), from); 
        manager.waitPacketSent();
        
        // Copy data to modbus map...
        theData.data[MB_RSSI] = rf95.lastRssi();        
//        timeOutDevices[from] = millis();
        Serial.println(F("payload received:"));
        Serial.print(F("PAN_ID:    "));
        Serial.println(theData.pan_id);
        Serial.print(F("DEVICE_ID: "));
        Serial.println(theData.device_id);
        for(n=0;n<MAX_REGS_BY_DEVICE;n++){
          Serial.print(F("DATA["));
          Serial.print(n);
          Serial.print(F("]: "));
          Serial.println(theData.data[n]);
        }
        Serial.println();

        unsigned long a = millis();
        enviaDatosEmon(from);      
        Serial.print(F("Spent time: "));
        Serial.print(millis() - a);  
        Serial.println(F(" mS"));
      }
    }
    
    led_test_off();
  }
 // compruebaTimeOuts();
}

void enviaDatosEmon(uint8_t node){
  static unsigned long ap_reconect_time = millis();
  String GET;
  uint8_t n;  


  if (client.connect(HOST_NAME, HOST_PORT)) {
    Serial.print(F("create tcp ok\r\n"));
    GET = "GET /emoncms/input/post.json?apikey=";  
    GET += config.key;
    GET += "&node=";
    GET += node + theData.pan_id;
    GET += "&csv=";
    for(n=0;n<MAX_REGS_BY_DEVICE;n++){ 
      GET += theData.data[n];
      if(n < (MAX_REGS_BY_DEVICE-1)) GET += ",";
    }
    GET += "\r\n";
    GET +="HTTP/1.1\r\n";
  
    Serial.println(F("Send..."));
    client.print(GET);
    Serial.println(F("...end"));
    ap_reconect_time = millis();
  } else {
    Serial.print(F("Connection failed\r\n"));
    if((millis() - ap_reconect_time) > (5*60000L)){
      ap_reconect_time = millis();
       WiFi.disconnect();
    }
  }
}


void join_ap(void){
  unsigned long t = millis();

  if(WiFi.status() != WL_CONNECTED){
    // Config wifi and connect to AP...
    WiFi.mode(WIFI_STA);
    // attempt to connect to WiFi network 
    WiFi.begin(config.ssid, config.pass);  
    while ( WiFi.status() != WL_CONNECTED) {
      if((millis() - t) >= 3000){
        t = millis();
        Serial.println();
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(config.ssid);
      }
      cmd.task();
    }
  
    // you're connected now, so print out the data
    Serial.println();
    Serial.println("You're connected to the network");
    Serial.println(WiFi.localIP());
    
    // print the SSID of the network you're attached to
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
  
    // print your WiFi shield's IP address
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);
  
    // print the received signal strength
    long rssi = WiFi.RSSI();
    Serial.print("Signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    Serial.println();
  }
}


/*
void compruebaTimeOuts(void){
  unsigned long t = millis();
  byte n;

  for(n=0;n<MAX_DEVICES;n++){
    if(t - timeOutDevices[n] > (DEVICE_TIMEOUT*60*60*1000L)){
      regs[((n+1)*MAX_REGS_BY_DEVICE) + MB_STATUS] |= 0x8000;
    }else{
      regs[((n+1)*MAX_REGS_BY_DEVICE) + MB_STATUS] &= ~0x8000;
    }
  }
}
*/


// RFM95 radio init...
void radioInit(void){
  while (!manager.init()) {
    Serial.println(F("LoRa radio init failed"));
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println(F("setFrequency failed"));
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(config.rfPower, false);
}

void led_test_on(void){
  digitalWrite(LED, LOW);
}

void led_test_off(void){
  digitalWrite(LED, HIGH);
}

