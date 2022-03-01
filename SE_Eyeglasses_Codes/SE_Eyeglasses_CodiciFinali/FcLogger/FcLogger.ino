/*
Descrizione: 
Codice per effettuare un log della lettura della frequenza cardiaca del sensore MAX30102.
Il log avviene attraverso l'utilizzo di un modulo SD card e di una connessione ad Internet
per inizzializzare l'RTC (real time clock) utile al salvataggio del tempo della lettura.
Autore: Caciolo Valerio
Data: 3/02/2022

Si ringrazia SparkFun per l'utilizzo della libreria "SparkFun_MAX3010x_Sensor_Library"
https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
*/

//librerie incluse
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

#include <RTCZero.h>

#include <SD.h>
#include <SPI.h>

#include <SPI.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h" 

//dichiarazioni e variabili globali del Real Time Clock
RTCZero rtc;

//dichiarazioni e variabili globali del lettore di schede SD
File dataFile;
int pinCS = 10;

//dichiarazioni e variabili globali del MAX30102
MAX30105 heartSensor;
const byte heatAvgArraySize = 15; //dimensione dell'array della media aritmetica dei battiti
byte heartAvgArraySize[heatAvgArraySize]; //array della media aritmetica dei battiti
byte indexArrayAvg = 0;
long exBeatsTime = 0;
long delta = 0; //variabile dove verrà salvato il tempo trascorso tra una battito e l'altro
float beatsPerMinute = 0;
float temperature;
int beatAvg = 0;
const int buzzer = 9; 

//dichiarazioni e variabili globali dei dati sensibili nel secret tab arduino_secrets.h
char ssid[] = SECRET_SSID;        
char pass[] = SECRET_PASS; 
int status = WL_IDLE_STATUS;


void setup() {
    //Init della porta seriale
    Serial.begin(115200);
    /*
    Se gli occhiali non sono collegati al pc questa parte va lasciata commentata
    while (!Serial) {
    ;                 
    }
    */
    pinMode(buzzer, OUTPUT);

//-----------------------------Wifi e RTC------------------------------------
    rtc.begin(); // Inizializzo l'RTC (vedere dopo)

    // Controllo di integrità del modulo wifi dell'Arduino nano 33 ioT
    if (WiFi.status() == WL_NO_MODULE) 
    {
    Serial.println("Problema hardware con il modulo wifi!");
    while (true); //in caso di problemi, loop infinito
    }
    // Controllo di integrità del firmware del modulo wifi dell'Arduino nano 33 ioT
    String versioneWifi = WiFi.firmwareVersion();
    if (versioneWifi < WIFI_FIRMWARE_LATEST_VERSION) 
    {
    Serial.println("Firmware del modulo non aggiornato");
    }

    // Connessione ad internet
    while (status != WL_CONNECTED)
    {
    Serial.print("Tentativo di connessione al wifi: ");
    Serial.println(ssid);
    //Connessione riuscita:
    status = WiFi.begin(ssid, pass);
    //5 secondi di attesa per stabilità secondo guida della libreria wifiNina:
    delay(5000);
    }

    Serial.print("Connessione riuscita e stabile, adesso stamperò qualche info sulla connessione: ");
    printCurrentNet();
    printWifiData();

    //aspetto un secondo prima di iniziare il calcolo dell'ora e il setup dell'RTC
    delay(1000);
    /*Ora il metodo getTime della libreria WiFiNina mi consente di ottenere il tempo passato dal 1 Gennaio 1970
    in secondi. Perchè? Boh. Quindi ora faccio una sequela di calcoli per trasfromare quei secondi nell'orario corrente*/
    unsigned long secFrom1970 = WiFi.getTime();
    Serial.print("secondi passati dal 1 Gennaio 1970: ");
    Serial.println(secFrom1970);

    /*Ora printo nel monitor seriale il tempo, nel mentre imposto l'RTC (Real Time Clock) che mi sarà utile nel log per 
    salvarmi la data e sopratutto l'ora corretta di ogni misurazione*/

    // Stanpa di ore, secondi e minuti e secondi:
    Serial.print("Ora corrente: ");
    Serial.print(((secFrom1970  % 86400L) / 3600)+1); // in un giorno ci sono 86400 secondi.
    //Il +1 è perchè i secondo dal 1970 sono con GMT+0
    //Ora imposto l'ora appena calcolata per l'RTC
    rtc.setHours(((secFrom1970  % 86400L) / 3600)+1);
    Serial.print(':');
    if (((secFrom1970 % 3600) / 60) < 10) 
    {
    // Se i minuti sono < 10 mi printo uno 0
    Serial.print('0');
    }
    Serial.print((secFrom1970  % 3600) / 60); // calcolo i minuti
    //Ora imposto i minuti appena calcolati per l'RTC
    rtc.setMinutes((secFrom1970  % 3600) / 60);
    Serial.print(':');
    if ((secFrom1970 % 60) < 10) 
    {
    // Se i secondo sono <10 aggiungo uno 0 alla stampa, stesso discorso di prima
    Serial.print('0');
    }
    Serial.println(secFrom1970 % 60); // stampo i secondi 
    //Ora imposto i secondi appena calcolati per l'RTC
    rtc.setSeconds(secFrom1970 % 60);
//-----------------------------Wifi e RTC------------------------------------

//-------------------------Sensore di battiti--------------------------------
    if (!heartSensor.begin(Wire, I2C_SPEED_FAST)) //porta fast di default per I2C, 400kHz
    {
    Serial.println("MAX30105 non trovato. Problema hardware. Blocco sistema");
    while (1);
    }
    Serial.println("Sensore MAX30102 correttamente riconosciuto");

    heartSensor.setup(); //metodo per configurare il sensore con i valori di default del produttore
    heartSensor.setPulseAmplitudeRed(0x0A); //accensione led rosso
    heartSensor.setPulseAmplitudeGreen(0); //spegnimento led verde
    heartSensor.enableDIETEMPRDY(); // att temperatura
//-------------------------Sensore di battiti--------------------------------

//------------------------------Scheda SD------------------------------------
    pinMode(pinCS, OUTPUT);

    while (!SD.begin(pinCS))
    {
    Serial.println("Scheda SD non trovata. Problema hardware? ci riprovo.");
    } 
    Serial.println("Scheda SD trovata");
//------------------------------Scheda SD------------------------------------
    tone(buzzer, 2000);
    delay(1000);
    noTone(buzzer);
}


void loop() {
    //printCurrentNet(); //Se voglio accertarmi che l'arduino sia ancora connesso
    if(mytimer(2000))
    {
        temperature = heartSensor.readTemperature();
    }

    long irValue = heartSensor.getIR(); //valore ir direttamente dal Max30102

    if (irValue >= 50000)
    {
        if (checkForBeat(irValue) == true)
        {
            //rilevato un battito riproduco un bip dal buzzer
            tone(buzzer, 2000);
            delay(10);
            noTone(buzzer);
            
            long tempTime = millis(); //salvo tempo corrente
            delta = tempTime - exBeatsTime; // tempo trascorso tra battito corrente e precedente
            exBeatsTime = tempTime;
        
            beatsPerMinute = 60 / (delta / 1000.0);
        
            if (beatsPerMinute < 255 && beatsPerMinute > 35)
            {
                heartAvgArraySize[indexArrayAvg++] = (byte)beatsPerMinute; //aggiungo la lettura nell'array
                indexArrayAvg %= heatAvgArraySize; //Wrap variable
                //media delle letture
                beatAvg = 0;
                for (byte x = 0 ; x < heatAvgArraySize ; x++)
                beatAvg += heartAvgArraySize[x];
                beatAvg /= heatAvgArraySize;
            }
            /*
            Serial.print("Body Temperature °C = ");
            Serial.print(temperature+1.4 , 4);
            Serial.print(", IR=");
            Serial.print(irValue);
            Serial.print(", BPM=");
            Serial.print(beatsPerMinute);
            Serial.print(", Avg BPM=");
            Serial.println(beatAvg);
            */

            //Mi salvo ore, secondi e minuti dall'RTC
            int h = rtc.getHours();
            int m = rtc.getMinutes();
            int s = rtc.getSeconds();

            // Costruisco la stringa per il log
            String logRow = "";
            if (h < 10) logRow += "0";
            logRow += h;
            logRow += ":";
            if (m < 10) logRow += "0";
            logRow += m;
            logRow += ":";
            if (s < 10) logRow += "0";
            logRow += s;
            logRow += ", ";
            logRow += irValue;
            logRow += ", ";
            logRow += beatsPerMinute;
            logRow += ", ";
            logRow += beatAvg;
            //Serial.println(logRow);

            if(mytimer(1000))
            {  
                dataFile = SD.open("datalog.txt", FILE_WRITE);
                if (dataFile) 
                {
                    dataFile.println(logRow);
                    dataFile.close();
                }
                else 
                {
                    Serial.println("errore di lettura del file datalog.txt");
                }
            }
        }
    }else 
    {
        Serial.println("Occhiali non indossati!");
        delay(2000);
    }
}

int mytimer(int timerdurata){
  static unsigned long t1, dt;
  int ret = 0;
  dt = millis() - t1;
  if(dt >= timerdurata)
  {
    t1 = millis();
    ret = 1;
  }
  return ret;
}

void printWifiData() {
  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}
