/*
Descrizione: 
Codice per il rilevamento di una caduta
Autore: Caciolo Valerio
Data: 17/12/2021

Si ringraziano gli autori dello studio "Optimization of an Accelerometer and Gyroscope-Based Fall Detection Algorithm"
https://www.hindawi.com/journals/js/2015/452078/
*/

//librerie incluse
#include <Arduino_LSM6DS3.h>

//dichiarazioni e variabili globali
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

void setup(){
    Serial.begin(9600);
    //while (!Serial);
    pinMode(buzzer, OUTPUT);
    
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
            trigger1=false; trigger1count=0;
        }
    }
    if (accMod<=2 && trigger2==false)
    { //SOGLIA CRITICA INFERIORE ACCMOD
        trigger1=true;
        Serial.println("TRIGGER 1 ACTIVATED");
    }
    delay(100);
}

void leggiLSM6DS3(){
 IMU.readGyroscope(gx, gy, gz);
 IMU.readAcceleration(ax, ay, az);
 }
