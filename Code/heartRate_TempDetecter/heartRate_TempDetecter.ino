/*
Descrizione: 
Codice per la lettura della frequenza cardiaca del sensore MAX30102
Autore: Caciolo Valerio
Data: 10/01/2022

Si ringrazia SparkFun per l'utilizzo della libreria "SparkFun_MAX3010x_Sensor_Library"
https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
*/

//librerie incluse
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

//dichiarazioni e variabili globali
MAX30105 heartSensor;
const byte heartAvgArraySize = 15; //dimensione dell'array della media aritmetica dei battiti
byte heartAvgArray[heartAvgArraySize]; //array della media aritmetica dei battiti
byte indexArrayAvg = 0;
long exBeatsTime = 0;
long delta = 0; //variabile dove verrà salvato il tempo trascorso tra una battito e l'altro
float beatsPerMinute = 0;
float exBeatsPerMinute = 0;
float temperature;
int beatAvg = 0;

const int buzzer = 9; 


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

    //Inizializzazione e controllo dell'hardware del MAX30102
    if (!heartSensor.begin(Wire, I2C_SPEED_FAST)) //porta fast di default per I2C, 400kHz
    {
        Serial.println("MAX30105 non trovato. Problema hardware. Blocco sistema");
        while (1);
    }
    Serial.println("Sensore MAX30102 correttamente riconosciuto");

    heartSensor.setup(); //metodo per configurare il sensore con i valori di default del produttore
    heartSensor.setPulseAmplitudeRed(0x0A); //accensione led rosso
    heartSensor.setPulseAmplitudeGreen(0); //spegnimento led verde
    //heartSensor.enableDIETEMPRDY(); // att temperatura

    tone(buzzer, 2000);
    delay(500);
    noTone(buzzer);
}


void loop() {
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
            if(beatAvg <= 60)
            {
                if (beatsPerMinute < 200 && beatsPerMinute > 35)
                {
                    heartAvgArray[indexArrayAvg++] = (byte)beatsPerMinute; //aggiungo la lettura nell'array
                    indexArrayAvg %= heartAvgArraySize; //Wrap variable
                    //media delle letture
                    beatAvg = 0;
                    for (byte x = 0 ; x < heartAvgArraySize ; x++)
                    beatAvg += heartAvgArray[x];
                    beatAvg /= heartAvgArraySize;
                    exBeatsPerMinute = beatsPerMinute;
                }
            }else
            {
                if (checkForTrueBeat())
                {
                    heartAvgArray[indexArrayAvg++] = (byte)beatsPerMinute; //aggiungo la lettura nell'array
                    indexArrayAvg %= heartAvgArraySize; //Wrap variable
                    //media delle letture
                    beatAvg = 0;
                    for (byte x = 0 ; x < heartAvgArraySize ; x++)
                    beatAvg += heartAvgArray[x];
                    beatAvg /= heartAvgArraySize;
                    exBeatsPerMinute = beatsPerMinute;
                }

            }
        Serial.print("Body Temperature °C = ");
        Serial.print(temperature+1.4 , 4);
        Serial.print("IR=");
        Serial.print(irValue);
        Serial.print(", BPM=");
        Serial.print(beatsPerMinute);
        Serial.print(", Avg BPM=");
        Serial.println(beatAvg);
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

bool checkForTrueBeat(){
    int suspiciousHeartRateCount = 0;
    if (beatsPerMinute >= 235 && beatsPerMinute <= 35) return false;
    if ((beatsPerMinute <= exBeatsPerMinute + 20 && beatsPerMinute >= exBeatsPerMinute - 20) && (beatsPerMinute <= beatAvg + 20 && beatAvg >= exBeatsPerMinute - 20))
    {
        for (byte c = 0 ; c < heartAvgArraySize ; c++)
        {
        if(beatsPerMinute >= heartAvgArray[c] +20 && beatsPerMinute <= heartAvgArray[c] - 20) suspiciousHeartRateCount++;
        }
        if(suspiciousHeartRateCount >= 10)return false;
        return true;
    }
    return false;
}
