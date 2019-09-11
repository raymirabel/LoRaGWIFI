

void commandLineInit(void){
  cmd.begin(Serial, F("LoraGWIFI>"), welcome);

  // Add commands...
  cmd.addCommand(command_ssid,  F("ssid"),      F("[32 char]  Wifi SSID"));
  cmd.addCommand(command_pass,  F("pass"),      F("[32 char]  Wifi password"));
  cmd.addCommand(command_key ,  F("key"),       F("[32 char]  Emoncms APIKEY"));
  cmd.addCommand(command_pow,   F("pow"),       F("[5...23]   RF power (5:min...23:max)"));
  cmd.addCommand(command_pan,   F("pan"),       F("[0...250]  PAN id"));
  cmd.addCommand(command_list,  F("list"),      F("List wifi AP"));
  cmd.addCommand(command_clear, F("clear"),     F("Clear Wifi configuration"));
  cmd.addCommand(command_sta,   F("sta"),       F("Read Wifi status"));
  cmd.addCommand(command_param, F("all"),       F("Read all parameters"));
  cmd.addCommand(command_help,  F("?"),         F("Command list help"));  
}

// Welcome...
void welcome(void){
  cmd.println(F("Ray Ingenieria Electronica, S.L."));
  cmd.print  (F("LoRaGWIFI - Firm.: v"));
  cmd.print  (F(FIRMWARE_VERSION));
  cmd.print  (F(" Hard.: "));
  cmd.println(F(HARDWARE_VERSION));  
  cmd.println(F("Press '?' for help")); 
}

/**************************************************************************
 * COMMAND FUNCTIONS
 *************************************************************************/ 

// Comando pow...
int command_pow(int argc, char** argv){
  bool ok = false;
  long temp;

  temp = config.rfPower;
  if(argc == 1) ok = true;
  if(argc >= 2){
    if(cmd.testLong(argv[1], 5, 23, &temp)){
      config.rfPower = (byte)temp;
      rf95.setTxPower(config.rfPower, false);
      EEPROM.put(DIR_EEPROM_CFG, config);
      EEPROM.commit();
      ok = true;
    }
  }  
  if(ok) cmd.printf(F("pow = %ld\r\n"),(long)config.rfPower);
  return(ok);
}

// Comando ssid...
int command_ssid(int argc, char** argv){
  bool ok = false;

  if(argc == 1) ok = true;
  if(argc >= 2){
    if(cmd.testString(argv[1], 32)){
      strcpy(config.ssid, argv[1]);
      EEPROM.put(DIR_EEPROM_CFG, config);
      EEPROM.commit();
      WiFi.disconnect();
      ok = true;
    }
  } 
  if(ok) cmd.printf(F("ssid = %s\r\n"),config.ssid);
  return(ok);
}

// Comando pass...
int command_pass(int argc, char** argv){
  bool ok = false;

  if(argc == 1) ok = true;
  if(argc >= 2){
    if(cmd.testString(argv[1], 32)){
      strcpy(config.pass, argv[1]);
      EEPROM.put(DIR_EEPROM_CFG, config);
      EEPROM.commit();
      WiFi.disconnect();
      ok = true;
    }
  } 
  if(ok) cmd.printf(F("pass = %s\r\n"),config.pass);
  return(ok);
}

// Comando key...
int command_key(int argc, char** argv){
  bool ok = false;

  if(argc == 1) ok = true;
  if(argc >= 2){
    if(cmd.testString(argv[1], 32)){
      strcpy(config.key, argv[1]);
      EEPROM.put(DIR_EEPROM_CFG, config);
      EEPROM.commit();
      ok = true;
    }
  } 
  if(ok) cmd.printf(F("key = %s\r\n"),config.key);
  return(ok);
}


// Comando pan...
int command_pan(int argc, char** argv){
  bool ok = false;
  long temp;

  temp = config.rfPan;
  if(argc == 1) ok = true;
  if(argc >= 2){
    if(cmd.testLong(argv[1], 0, 250, &temp)){
      config.rfPan = (byte)temp;
      theData.pan_id = config.rfPan;  
      EEPROM.put(DIR_EEPROM_CFG, config);  
      EEPROM.commit();
      ok = true;
    }
  }  
  if(ok) cmd.printf(F("pan = %ld\r\n"),(long)config.rfPan);
  return(ok);   
}

// Comando list...
int command_list(int argc, char** argv){
  // scan for nearby networks
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    cmd.println("Couldn't get a wifi connection");
    while (true);
  }

  // print the list of networks seen
  cmd.print("Number of available networks:");
  cmd.printf("%ld\r\n",numSsid);

  // print the network number and name for each network found
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") ");
    Serial.print(WiFi.SSID(thisNet));
    Serial.print("\tSignal: ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.print(" dBm");
    Serial.print("\tEncryption: ");
    printEncryptionType(WiFi.encryptionType(thisNet));
  }

  return(true);
}

// Comando param...
int command_param(int argc, char** argv){
  cmd.printf(F("ssid  = %s\r\n"),config.ssid);
  cmd.printf(F("pass  = %s\r\n"),config.pass);
  cmd.printf(F("key   = %s\r\n"),config.key);
  cmd.printf(F("pow   = %d\r\n"), (long)config.rfPower);  
  cmd.printf(F("pan   = %d\r\n"), (long)config.rfPan);
  return(true);
}

// Comando status...
int command_sta(int argc, char** argv){

  if(WiFi.status() == WL_CONNECTED){ 
    Serial.println("You're connected to the network");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI):");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    WiFi.printDiag(Serial);
  }else{
    Serial.println("You are not connected");
    WiFi.printDiag(Serial);
  }
  return(true);
}

// Comando clear...
int command_clear(int argc, char** argv){
  strcpy(config.pass, "");
  strcpy(config.ssid, "");
  strcpy(config.key, "");
  EEPROM.put(DIR_EEPROM_CFG, config);
  EEPROM.commit();
  WiFi.disconnect();
  return(true);
}

// Comando help...
int command_help(int argc, char** argv){
  cmd.println(F("Command list:"));
  cmd.printCommands();
  return(false);
}



void printEncryptionType(int thisType) {
  // read the encryption type and print out the name
  switch (thisType) {
    case ENC_TYPE_WEP:
      Serial.print("WEP");
      break;
    case ENC_TYPE_TKIP:
      Serial.print("WPA_PSK");
      break;
    case ENC_TYPE_CCMP:
      Serial.print("WPA2_PSK");
      break;
    case ENC_TYPE_AUTO:
      Serial.print("WPA_WPA2_PSK");
      break;
    case ENC_TYPE_NONE:
      Serial.print("None");
      break;
  }
  Serial.println();
}



