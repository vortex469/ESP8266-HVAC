/**************************************************************
 * WiFiManager is a library for the ESP8266/Arduino platform
 * (https://github.com/esp8266/Arduino) to enable easy
 * configuration and reconfiguration of WiFi credentials and
 * store them in EEPROM.
 * inspired by http://www.esp8266.com/viewtopic.php?f=29&t=2520
 * https://github.com/chriscook8/esp-arduino-apboot
 * Built by AlexT https://github.com/tzapu
 * Licensed under MIT license
 **************************************************************/

// Nextion added for onscreen SSID selection and password entry while server running

#include "WiFiManager.h"
#include "Nextion.h"
#include <TimeLib.h>

extern Nextion nex;

WiFiServer server_s ( 80 );

WiFiManager::WiFiManager(int eepromStart)
{
    _eepromStart = eepromStart;
}

void WiFiManager::begin() {
    begin("NoNetESP");
}

void WiFiManager::begin(char const *apName) {
    _apName = apName;

    EEPROM.begin(512);
    delay(10);
}

boolean WiFiManager::autoConnect() {
    autoConnect("NoNetESP");
}

boolean WiFiManager::autoConnect(char const *apName) {
    begin(apName);

//  DEBUG_PRINT("");
//    DEBUG_PRINT("AutoConnect");
    // read eeprom for ssid and pass
    String ssid = getSSID();
    String pass = getPassword();

    if ( ssid.length() > 1 ) {
        DEBUG_PRINT("Waiting for Wifi to connect");

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        if ( hasConnected() ) {
            return true;
        }
    }
    //setup AP
    beginConfigMode();
    //start portal and loop
    startWebConfig(ssid);
    return false;
}

String WiFiManager::getSSID() {
    if(_ssid == "") {
//        DEBUG_PRINT("Reading EEPROM SSID");
        _ssid = getEEPROMString(0, 32);
//        DEBUG_PRINT("SSID: ");
//        DEBUG_PRINT(_ssid);
    }
    return _ssid;
}

String WiFiManager::getPassword() {
    if(_pass == "") {
//        DEBUG_PRINT("Reading EEPROM Password");
        _pass = getEEPROMString(32, 64);
//        DEBUG_PRINT("Password: ");
//        DEBUG_PRINT(_pass);
    }
    return _pass;
}

String WiFiManager::getEEPROMString(int start, int len) {
    String string = "";
    for (int i = _eepromStart + start; i < _eepromStart + start + len; i++) {
        //DEBUG_PRINT(i);
        char c = char(EEPROM.read(i));
        if(c == 0 ||c == 255) break; // fix for 2.0.0
        string += c;
    }
    return string;
}

void WiFiManager::setEEPROMString(int start, int len, String string) {
    int si = 0;
    for (int i = _eepromStart + start; i < _eepromStart + start + len; i++) {
        char c;
        if(si < string.length()) {
            c = string[si];
//            DEBUG_PRINT("Wrote: ");
//            DEBUG_PRINT(c);
        } else {
            c = 0;
        }
        EEPROM.write(i, c);
        si++;
    }
}

void WiFiManager::eeReadData(uint8_t *data, int size)
{
  for(int i = 0, addr = 64; i < size; i++, addr++)
  {
    data[i] = EEPROM.read( addr );
  }
}

void WiFiManager::eeWriteData(uint8_t *data, int size)
{
  for(int i = 0, addr = 64; i < size; i++, addr++)
  {
    EEPROM.write(addr, data[i] );
  }
  EEPROM.commit();
}

boolean WiFiManager::hasConnected(void)
{
  for(int c = 0; c < 50; c++)
  {
    if (WiFi.status() == WL_CONNECTED)
      return true;
    delay(200);
    Serial.print(".");
  }
  DEBUG_PRINT("");
  DEBUG_PRINT("Could not connect to WiFi");
  return false;
}

void WiFiManager::startWebConfig(String ssid) {
    DEBUG_PRINT("");
    DEBUG_PRINT("WiFi connected");
    DEBUG_PRINT(WiFi.localIP());
    DEBUG_PRINT(WiFi.softAPIP());
    if (!MDNS.begin(_apName)) {
        DEBUG_PRINT("Error setting up MDNS responder!");
        while(1) {
            delay(1000);
        }
    }
    DEBUG_PRINT("mDNS responder started");
    // Start the server
    server_s.begin();
    DEBUG_PRINT("Server started");

    uint8_t s;
    uint8_t m = minute();


    int n = WiFi.scanNetworks();
    if(n)
    {
      nex.refreshItem("t0"); // Just to terminate any debug strings in the Nextion
      nex.setPage("SSID");
  
      for (int i = 0; i < n && n < 16; i++)
      {
        nex.btnText(i, ssidList[i] = WiFi.SSID(i));
      }
    }
    
    _timeout = true;
    char cBuf[64];
    String sSsid;
    String sPass;
  
    while(serverLoop() == WM_WAIT) {      //looping
      if(nex.service(cBuf))
      {
          switch(cBuf[0])  // code
          {
            case 0x65: // button
              switch(cBuf[1]) // page
              {
                  case 2: // Selection page t1=ID 2 ~ t16=ID 17
                    sSsid = ssidList[cBuf[2] - 2];
                    nex.refreshItem("t0"); // Just to terminate any debug strings in the Nextion
                    nex.setPage("keyboard"); // go to keyboard
                    nex.itemText(1, "Enter Password");
                    _timeout = false; // user detected
                    break;
              }
              break;
            case 0x70:// string return from keyboard
              sPass = String(cBuf + 1);
  //            Serial.print("Pass ");
  //            Serial.println(sPass);
              setEEPROMString(0, 32, sSsid);
              setEEPROMString(32, 64, sPass);
              EEPROM.commit();
//              DEBUG_PRINT("Setup done");
              nex.setPage("Thermostat");
              delay(1000);
              ESP.reset();
              break;
          }
      }
      if(s != second())
      {
        s = second();
        digitalWrite(2, !digitalRead(2)); // Toggle blue LED (also SCL)
      }
      if(_timeout)
      {
        if(m != minute())
        {
          m = minute();
          int n = WiFi.scanNetworks();
          if(n){
            for (int i = 0; i < n; ++i)
            {
                if(WiFi.SSID(i) == ssid)
                {
                  nex.setPage("Thermostat"); // set back to normal while restarting
                  delay(500);
                  ESP.reset();
                }
            }
          }
        }
      }
    }

//    DEBUG_PRINT("Setup done");
    nex.setPage("Thermostat"); // set back to normal while restarting
    delay(5000);
    ESP.reset();
}

void WiFiManager::beginConfigMode(void) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apName);
    DEBUG_PRINT("Started Soft Access Point");
//    IPAddress apIp = WiFi.softAPIP();
//    char ip[24];
//    sprintf(ip, "%d.%d.%d.%d", apIp[0], apIp[1], apIp[2], apIp[3]);
//    display.print(String(ip));
}

int WiFiManager::serverLoop()
{
    // Check for any mDNS queries and send responses
    MDNS.update();
    String s;

    WiFiClient client = server_s.available();
    if (!client) {
        return(WM_WAIT);
    }

    DEBUG_PRINT("New client");
    
    // Wait for data from client to become available
    while(client.connected() && !client.available()){
        delay(1);
    }
    
    // Read the first line of HTTP request
    String req = client.readStringUntil('\r');
    
    // First line of HTTP request looks like "GET /path HTTP/1.1"
    // Retrieve the "/path" part by finding the spaces
    int addr_start = req.indexOf(' ');
    int addr_end = req.indexOf(' ', addr_start + 1);
    if (addr_start == -1 || addr_end == -1) {
        DEBUG_PRINT("Invalid request: ");
        DEBUG_PRINT(req);
        return(WM_WAIT);
    }
    req = req.substring(addr_start + 1, addr_end);
    DEBUG_PRINT("Request: ");
    DEBUG_PRINT(req);
    client.flush();

    if (req == "/")
    {
        s = HTTP_200;
        String head = HTTP_HEAD;
        head.replace("{v}", "Config ESP");
        s += head;
        s += HTTP_SCRIPT;
        s += HTTP_STYLE;
        s += HTTP_HEAD_END;

        int n = WiFi.scanNetworks();
        DEBUG_PRINT("scan done");
        if (n == 0) {
            DEBUG_PRINT("no networks found");
            s += "<div>No networks found. Refresh to scan again.</div>";
        }
        else {
            for (int i = 0; i < n; ++i)
            {
                DEBUG_PRINT(WiFi.SSID(i));
                DEBUG_PRINT(WiFi.RSSI(i));
                String item = HTTP_ITEM;
                item.replace("{v}", WiFi.SSID(i));
                s += item;
                delay(10);
            }
        }
        
        s += HTTP_FORM;
        s += HTTP_END;
        
        DEBUG_PRINT("Sending config page");
        _timeout = false;
    }
    else if ( req.startsWith("/s") ) {
        String qssid;
        qssid = urldecode(req.substring(8,req.indexOf('&')).c_str());
        DEBUG_PRINT(qssid);
        DEBUG_PRINT("");
        req = req.substring( req.indexOf('&') + 1);
        String qpass;
        qpass = urldecode(req.substring(req.lastIndexOf('=')+1).c_str());

        setEEPROMString(0, 32, qssid);
        setEEPROMString(32, 64, qpass);

        EEPROM.commit();

        s = HTTP_200;
        String head = HTTP_HEAD;
        head.replace("{v}", "Saved config");
        s += HTTP_STYLE;
        s += HTTP_HEAD_END;
        s += "saved to eeprom...<br/>resetting in 5 seconds";
        s += HTTP_END;
        client.print(s);
        client.flush();

        DEBUG_PRINT("Saved WiFiConfig...restarting.");
        return WM_DONE;
    }
    else
    {
        s = HTTP_404;
        DEBUG_PRINT("Sending 404");
    }
    
    client.print(s);
    DEBUG_PRINT("Done with client");
    return(WM_WAIT);
}

String WiFiManager::urldecode(const char *src)
{
    String decoded = "";
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            
            decoded += char(16*a+b);
            src+=3;
        } else if (*src == '+') {
            decoded += ' ';
            *src++;
        } else {
            decoded += *src;
            *src++;
        }
    }
    decoded += '\0';
    
    return decoded;
}

// Scan APs, use configured if found, otherwise try each open AP to get through, finally open soft AP with config page.
boolean WiFiManager::findOpenAP(const char *szUrl)
{
    int nOpen = 0;
    begin("ESP8266");
    int nScan = WiFi.scanNetworks();
    bool bFound = false;
    Serial.println("scan done");
    String sSSID = getSSID();
    int ind = 0;

    Serial.print("Cfg SSID: ");
    Serial.print(sSSID);
    Serial.print(" ");
    Serial.println(sSSID.length());

    if (nScan == 0) {
        Serial.println( "No APs found" );
    }
    else {
        for (int i = 0; i < nScan; ++i) // Print each AP, count public ones
        {
            Serial.print(WiFi.SSID(i));
            Serial.print(" ");

            if(WiFi.encryptionType(i) == 7 /*&& strncmp(WiFi.SSID(i),"Chromecast",6) != 0*/)
            {
              nOpen++;
            }
            else if( sSSID == WiFi.SSID(i) ){ // The saved AP was found
              bFound  = true;
              Serial.print("(Cfg) ");
            }

            Serial.println(WiFi.encryptionType(i));
        }
  }

  delay(2000); // delay for reading

  if(nOpen && !bFound)
  {
    WiFi.mode(WIFI_STA);
    int counter = 0;
    for (int i = 0; i < nScan; ++i)
    {
      if(WiFi.encryptionType(i) == 7)   // run through open APs and try to connect
      {
        Serial.print("Attempting ");
        Serial.print(WiFi.SSID(i));
        char szSSID[64];
        WiFi.SSID(i).toCharArray(szSSID, 64); // fix for 2.0.0
        WiFi.begin(szSSID);
        for(int n = 0; n < 50 && WiFi.status() != WL_CONNECTED; n++)
        {
          delay(200);
          Serial.print(".");
        }
        Serial.println("");
        if(WiFi.status() == WL_CONNECTED)
        {
          Serial.println("Connected");
          if(attemptClient(szUrl))    // attemp port 80 and 8080
          {
            break;
          }else{
            WiFi.disconnect();
          }
        }
        counter++;
      }
    }
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("Open WiFi failed");
      Serial.println("Switch to SoftAP");
      autoConnect("ESP8266");
    }
  }
  else
  {
    autoConnect("ESP8266");
  }
  return true;
}

// See if port 80 is accessable.
boolean WiFiManager::attemptClient(const char *szUrl)
{
  WiFiClient client;

  int i;
  Serial.print("Probe port 80");
  for(i = 0; i < 5; i++)
  {
    if (client.connect(szUrl, 80)) {
      client.stop();
      Serial.println("Port 80 success");
      return true;
    }
    Serial.print(".");
    delay(20);
  }
  Serial.println("");
  Serial.println("No joy");
  return false;  
}
