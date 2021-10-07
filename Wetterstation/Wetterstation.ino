// ESP8266-Bibliothek
#include <ESP8266WiFi.h>
// MQTT-Bibliothek
#include <PubSubClient.h>
// Sensor-Bibliothek
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
// Web-Server-Bibliothek
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
// LC-Display-Bibliothek für I2C
#include <LiquidCrystal_I2C.h>
#include <map>
#include <string>

//#define Seehoehendruck (1013.25); // Angabe des Seehöhendrucks für Berechnung der Höhe

const char* ssid = "aFRITZ!Box 7530"; // SSID des WiFi-Netzwerks
const char* password = "77736ImBuchenfeld16"; // Passwort des WiFi-Netzwerks
const char* mqtt_server = "broker.mqtt-dashboard.com";
// vector aus Themen, die man Publishen oder Subscriben will. Iteration möglich durch for(const char* i : topics){...}
std::vector<const char*> subscribe_topics = {"Temperatur_Innen", "Luftfeuchte_Innen", "Luftdruck_Innen", "Hoehe_Innen"};
std::vector<const char*> publish_topics = {"Temperatur", "Luftfeuchte", "Luftdruck", "Hoehe"};
std::vector<float> messwerte = {0.0, 0.0, 0.0, 0.0};
std::map<const char*, String> value_bib;

// Setup des Wifi-Clients
WiFiClient sensorClient;
// Setup des MQTT-Clients
PubSubClient mqtt_client(mqtt_server, 1883, sensorClient);
// Sensor-Instanz erstellen
Adafruit_BME280 bme;
// AsyncWebServer Objekt an Port 80
AsyncWebServer server(80);
// LC-Display an I2C-Adresse 0x27 mit 20 Zeichen auf 4 Zeilen
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Callback wird aufgerufen, wenn eine Nachricht von einem abonnierten Kanal empfangen wird.
// Entweder im vollständigen Konstruktor angeben oder mit client.setCallback(callback).
// Übernommen von makesmart unter: https://makesmart.net/esp8266-d1-mini-mqtt/ und angepasst

void callback(char* topic, byte * payload, unsigned int length) {
  Serial.println("Nachricht angekommen");
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  String var = String(topic);
  // Schaue, welches Topic ankam und speichere Nachricht in _value_bib
  for (const char* i : subscribe_topics) {
    if (var == i) {
      value_bib.at(i) = message;
    }
  }
}

void setup_wifi() {

  // Funktion, die die Verbindung mit dem Wifi herstellt.

  Serial.print("Verbinde zu Netzwerk: ");
  Serial.println(ssid);
  // WiFi Verbindung mit Router
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Warte, bis Verbindung hergestellt und gebe so lange alle 0,5 Sekunden Punkte aus
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Erzeuge anhand der Arduino-Laufzeit einen anderen Seed für die erzeugen der Pseudo-Zufalls-Zahlen.
  // Ohne randomSeed() wäre die folge der Zufallszahlen immer gleich.
  // Hier wird gehofft, dass der WiFi-Verbindungsaufbau nicht immer gleich lange dauert.
  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi verbunden");
  Serial.println("IP Addresse: ");
  Serial.println(WiFi.localIP()); // WiFI.localIP() gibt die IP-Adresse des ESP8266 im Netzwerk an.
}

void mqtt_connect() {
  // Funktion, die eine Verbindung zum MQTT Broker aufbaut oder wiederaufbaut.

  String str_server(mqtt_server); // Konvertiere const char* zu String für Ausgabe
  // Schleife, solange nicht verbunden
  while (!mqtt_client.connected()) {
    Serial.println("Versuche MQTT Verbindungsaufbau zu Server" + str_server + "...");
    // Erstellen einer zufälligen Client-ID
    String clientId = "Wetterstation_BME280_";
    clientId += String(random(0xffff), HEX);
    // Versuch des Verbindungsaufbaus
    if (mqtt_client.connect(clientId.c_str())) { // Konvertiere string clientID zu const char*, weil client.connect(const char*)
      Serial.println("connected");
      for (const char* i : subscribe_topics) {
        Serial.println(i);
        mqtt_client.subscribe(i);
        delay(100); // Lasse genug Zeit zum Verarbeiten
      }
      // Nach Verbindungsaufbau eine Retained Nachricht an jedes Topic schicken
      for (const char* i : publish_topics) {
        Serial.println(i);
        mqtt_client.publish(i, "Erwarte Sensorwert in den nächsten 5 Sec", true); // true am Ende für Retained
        delay(100);
      }
    } else {
      Serial.print("Verbindung zu MQTT-Server fehlgeschlagen. Grund =");
      Serial.print(mqtt_client.state());
      Serial.println("Erneuter Versuch in 5 Sekunden");
      // Warte 5 Sekunden vor dem nächsten Verbindungsaufbau
      delay(5000);
    }
  }
}


// Platzhalter der HTML-Datei mit Werten ersetzen
// Wird in der AsyncWebServer-Bibliothek gefordert
String processor(const String& var) {
  Serial.println(var);
  if (var == "TEMPERATUR_AUSSEN") {
    return value_bib["Temperatur"];
  }
  else if (var == "TEMPERATUR_INNEN") {
    return value_bib["Temperatur_Innen"];
  }
  else if (var == "FEUCHTE_INNEN") {
    return value_bib["Luftfeuchte_Innen"];
  }
  else if (var == "FEUCHTE_AUSSEN") {
    return value_bib["Luftfeuchte"];
  }
  else if (var == "DRUCK") {
    return value_bib["Luftdruck"];
  }
}

void setup_server() {
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    // request->send(SPIFFS, "/index.html");
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });
  // Verarbeiten der "xhttp.open("GET", "/temperatur_aussen", true);" XML/Javascript-Befehle aus der HTML-Datei
  // 
  server.on("/temperatur_aussen", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", value_bib["Temperatur"].c_str());
  });
  server.on("/temperatur_innen", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", value_bib["Temperatur_Innen"].c_str());
  });
  server.on("/feuchte_innen", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", value_bib["Luftfeuchte_Innen"].c_str());
  });
  server.on("/feuchte_aussen", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", value_bib["Luftfeuchte"].c_str());
  });
  server.on("/druck", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", value_bib["Luftdruck"].c_str());
  });
  // Starte Server
  server.begin();
}

void do_publish() {
  for (const char* i : publish_topics) { // Iteration durch den Topic-Vector
    mqtt_client.publish(i, value_bib[i].c_str()); // MQTT-Publish
  }
}

void get_messwerte() {
  for (const char* i : publish_topics) {
    // Arduino-Funktion strstr(const char* s1, const char* s2) findet das erste Vorkommen des substrings s2 in s1.
    // Falls s2 in s1 gibt die Funktion den Pointer auf den Beginn des substrings. 
    // Falls s2 nicht in s1, dann gibt sie Null zurück.
    if (strstr(i, "Temperatur") != NULL) {
      value_bib.at(i) = String(bme.readTemperature());
    }
    else if (strstr(i, "Luftdruck") != NULL) {
      value_bib.at(i) = String(bme.readPressure() / 100.0F);
    }
    else if (strstr(i, "Luftfeuchte") != NULL) {
      value_bib.at(i) = String(bme.readHumidity());
    }
    else if (strstr(i, "Hoehe") != NULL) {
      value_bib.at(i) = String(bme.readAltitude(1013.25));
    }
  }
}


void visualisiere() {
  int x = 0;
  for (const char* i : publish_topics) {
    lcd.setCursor(0, x); // Schreibe an Stellen 0-11
    lcd.print(String(i));
    lcd.setCursor(12,x); // Schreibe an Stellen 12-19
    lcd.print(": " + String(value_bib[i]));
    x++;
  }
}

void setup() {
  // initalisiere die Value-Bibliothek
  for (const char* i : subscribe_topics) {
    value_bib[i] = "0.0";
  }
  for (const char* i : publish_topics) {
    value_bib[i] = "0.0";
  }
  Serial.begin(115200); // Baud-Rate einstellen

  setup_wifi(); // Die WiFi-Setup Funktion aufrufen
  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Fehler beim iniziieren des SPIFFS");
    return;
  }
  // Den BME280 Sensor konfigurieren, Methode aus der Bibliothek. 0x76 ist die Adresse für den i2c Bus
  while (!bme.begin(0x76)) {
    Serial.println("Sensorfehler");
    while (1);
  }

  lcd.init(); //Im Setup wird der LCD gestartet
  lcd.backlight(); //Hintergrundbeleuchtung einschalten (0 schaltet die Beleuchtung aus).

  mqtt_client.setCallback(callback);
  mqtt_connect(); // Versuche Verbindung zum MQTT-Broker aufzubauen
  setup_server(); // Server initialisieren
}

void loop() {
  // PubSubClient-Methode, die eingehende Nachrichten und die Verbindung zum Server verwaltet
  mqtt_client.loop();
  // Werte des Sensors in die std::map value_bib schreiben. Get-Methoden aus der BME280-Bibliothek.
  get_messwerte();
  // Messwerte an MQTT-Broker senden.
  do_publish();
  lcd.clear();
  visualisiere();
  delay(1000);
  std::map<const char*, String>::iterator it;
  for (it = value_bib.begin(); it != value_bib.end(); ++it) {
    Serial.print(it->first);
    Serial.print(" = ");
    Serial.print(it->second);
    Serial.print(" \n ");
  }
}
