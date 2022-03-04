/*
Descrizione: 
Codice per effettuare un log dei dati inerenti al rilevamento delle cadute
Il log avviene attraverso l'utilizzo di un modulo SD card e di una connessione ad Internet
per inizializzare l'RTC (real time clock) utile al salvataggio del tempo della lettura.
Autore: Caciolo Valerio
Data: 23/12/2021

Si ricorda di configurare il file arduino_secrets.h

Si ringraziano gli autori dello studio "Optimization of an Accelerometer and Gyroscope-Based Fall Detection Algorithm"
https://www.hindawi.com/journals/js/2015/452078/
*/

//librerie incluse
#include <Arduino_LSM6DS3.h>

#include <RTCZero.h>

#include <SPI.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h" 

#include <SD.h>
#include <SPI.h>

//dichiarazioni e variabili globali per IMU
const int buzzer = 9;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;

boolean fall = false; //true se si è rilevata una caduta
boolean trigger1=false; //trigger per la soglia critica inferiore dell'accelerazione totale
boolean trigger2=false; //trigger per la soglia critica superiore dell'accelerazione totale
boolean trigger3=false; //trigger per la soglia critica superiore della velocità angolare
/*
i seguenti contatori servono ad impostare un tempo a partire dal quale ogni trigger attende all'incirca
500ms che si verifichi il trigger successivo
*/
byte trigger1count = 0; //conta i cicli in cui trigger1 = true
byte trigger2count = 0; //conta i cicli in cui trigger2 = true
byte trigger3count = 0; //conta i cicli in cui trigger3 = true

int omega = 0;
float acc = 0;

//Dichiarazioni per il Real Time Clock
RTCZero rtc;

//Dichiarazioni per il lettore di schede SD
File dataFile;
int pinCS = 10;

//Dichiarazioni dei dati sensibili nel secret tab arduino_secrets.h
char ssid[] = SECRET_SSID;        
char pass[] = SECRET_PASS; 
int status = WL_IDLE_STATUS;

void setup(){
    Serial.begin(9600);
    //while (!Serial);
    pinMode(buzzer, OUTPUT);
    rtc.begin(); // Inizializzo l'RTC (vedere dopo)

    //Inizializzazione e controllo dell'hardware del LSM6DS3
    if (!IMU.begin()) 
    {
        Serial.println("Failed to initialize IMU!");
        while (1);
    }

    Serial.print("Gyroscope sample rate = ");
    Serial.print(IMU.gyroscopeSampleRate());
    Serial.println(" Hz");  
    Serial.println();
    Serial.println("Gyroscope in degrees/second");

    Serial.print("Accelerometer sample rate = ");
    Serial.print(IMU.accelerationSampleRate());
    Serial.println(" Hz");
    Serial.println();
    Serial.println("Acceleration in G's");
    Serial.println("X\tY\tZ");
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
    //aspetto un secondo prima di iniziare il calcolo dell'ora e il setup dell'RTC
    delay(1000);

    unsigned long secFrom1970 = WiFi.getTime();
    Serial.print("secondi passati dal 1 Gennaio 1970: ");
    Serial.println(secFrom1970);
    //Ora imposto l'ora appena calcolata per l'RTC
    rtc.setHours(((secFrom1970  % 86400L) / 3600)+1);
    //Ora imposto i minuti appena calcolati per l'RTC
    rtc.setMinutes((secFrom1970  % 3600) / 60);
    //Ora imposto i secondi appena calcolati per l'RTC
    rtc.setSeconds(secFrom1970 % 60);

    //------------------------------Scheda SD---------------------------------
    pinMode(pinCS, OUTPUT);
    
    while (!SD.begin(pinCS))
    {
        Serial.println("Scheda SD non trovata. Problema hardware? ci riprovo.");
    } 
    Serial.println("Scheda SD trovata");
    //myFile = SD.open("test.txt", FILE_WRITE); //creo il file test.txt
    
    //------------------------------Scheda SD---------------------------------
    tone(buzzer, 2000);
    delay(1000);
    noTone(buzzer);
}

void loop(){
    if(IMU.accelerationAvailable() && IMU.gyroscopeAvailable())
    {
        leggiLSM6DS3();
    }
    acc = pow(pow(ax,2)+pow(ay,2)+pow(az,2),0.5);
    int accMod = acc * 10; //visto che i valori di equilibrio sono da 0 a 1
    Serial.println(accMod);

    if (trigger3==true)
    {
        omega = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5);
        trigger3count++;
        //Serial.println(trigger3count);
        if (trigger3count>=10)
        { 
            omega = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5);
            Serial.println(omega); 
            if ((omega>=0) && (omega<=10))
            { //ritorno a situazione di equilibrio quindi impatto
                fall=true; trigger3=false; trigger3count=0;
                Serial.println(omega);
            }
            else
            { //probabilmente era un movimento 
                trigger3=false; trigger3count=0;
                Serial.println("TRIGGER 3 DEACTIVATED");
            }
        }
    }
    if (fall==true)
    { //se è stata rilevata una caduta faccio suonare il buzzer
        Serial.println("FALL DETECTED");
        tone(buzzer, 1000);
        dataFile = SD.open("datalog.txt", FILE_WRITE);
        if (dataFile) 
        {
            String logRowFall = "\nFALL DETECTED\n";
            dataFile.println(logRowFall);
            dataFile.close();
        }
        else 
        {
            Serial.println("errore di lettura del file datalog.txt");
        }
        delay(5000);
        noTone(buzzer);
        fall=false;
        // exit(1);
    }
    if (trigger2count>=6)
    { //nei prossimi 0.5 secondi si cercherà il superamento della soglia critica superiore di omega
        trigger2=false; trigger2count=0;
        Serial.println("TRIGGER 2 DECACTIVATED");
    }
    if (trigger1count>=6)
    { //nei prossimi 0.5 secondi si cercherà il superamento della soglia critica superiore di accMod
        trigger1=false; trigger1count=0;
        Serial.println("TRIGGER 1 DECACTIVATED");
    }
    if (trigger2==true)
    {
        trigger2count++;
        omega = pow(pow(gx,2)+pow(gy,2)+pow(gz,2),0.5); Serial.println(omega);
        if (omega>=61 && omega<=400)
        { //SOGLIA CRITICA SUPERIORE OMEGA
            trigger3=true; trigger2=false; trigger2count=0;
            Serial.println(omega);
            dataFile = SD.open("datalog.txt", FILE_WRITE);
            if (dataFile) 
            {
                String logRowFallTrigger3 = "\nTRIGGER 3 ACTIVATED\n";
                dataFile.println(logRowFallTrigger3);
                dataFile.close();
            }
            else 
            {
                Serial.println("errore di lettura del file datalog.txt");
            }
            Serial.println("TRIGGER 3 ACTIVATED");
        }
    }
    if (trigger1==true)
    {
        trigger1count++;
        if (accMod>=14)
        { //SOGLIA CRITICA SUPERIORE ACCMOD
            trigger2=true;
            Serial.println("TRIGGER 2 ACTIVATED");
            dataFile = SD.open("datalog.txt", FILE_WRITE);
            if (dataFile) 
            {
                String logRowFallTrigger2 = "\nTRIGGER 2 ACTIVATED\n";
                dataFile.println(logRowFallTrigger2);
                dataFile.close();
            }
            else 
            {
                Serial.println("errore di lettura del file datalog.txt");
            }
            trigger1=false; trigger1count=0;
        }
    }
    if (accMod<=2 && trigger2==false)
    { //SOGLIA CRITICA INFERIORE ACCMOD
        trigger1=true;
        Serial.println("TRIGGER 1 ACTIVATED");
        dataFile = SD.open("datalog.txt", FILE_WRITE);
        if (dataFile) 
        {
            String logRowFallTrigger1 = "\nTRIGGER 1 ACTIVATED\n";
            dataFile.println(logRowFallTrigger1);
            dataFile.close();
        }
        else 
        {
            Serial.println("errore di lettura del file datalog.txt");
        }
    }
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
    logRow += accMod;
    logRow += ", ";
    logRow += omega;
    //Serial.println(logRow);
    
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

void leggiLSM6DS3(){
 IMU.readGyroscope(gx, gy, gz);
 IMU.readAcceleration(ax, ay, az);
 }
