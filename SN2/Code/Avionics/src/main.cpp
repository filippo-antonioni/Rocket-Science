#include <Arduino.h>
#include <Wire.h>  //per I2C
#include <Adafruit_BMP280.h>  //per barometro, correggi con 380/390 eventualmente 
#include <Adafruit_MPU6050.h> //accelerometro
#include <LittleFS.h> // per memoria esp

#define SDA_PIN 8
#define SCL_PIN 9
#define p0 1013.25
/* Codice di controllo per vedere se ho fatto porcate collegando i fili
// Definisco pin per sensori



void setup() {
  
  Serial.begin(115200);
  
  
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("\nInizio scansione I2C...");
}


void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scansiono...");

  // Conto gli indirizzi I2C
  for(address = 1; address < 127; address++ ) {
    
    
    Wire.beginTransmission(address); // restituisce 0 se c'è un sensore a quell'indirizzo
    
    error = Wire.endTransmission();

    if (!error) {
      Serial.print("Sensore I2C trovato all'indirizzo: 0x");
      
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      nDevices++;
    }
  }

  if (!nDevices) {
    Serial.println("Nessun dispositivo I2C trovato. Controlla i cavi!");
  } else {
    Serial.println("Scansione terminata.\n");
  }

   

  delay(5000); 
}
*/
       


Adafruit_BMP280 bmp;        //eventualmente cambia con 380/390
Adafruit_MPU6050 mpu;

float altitudineZero = 0;
File fileLog;

void setup() {
    Serial.begin(115200);
    
  
    Wire.begin(8, 9); 

    
    if (!LittleFS.begin(true)) {
        Serial.println("Errore: Memoria Flash non montata!");
        return;
    }

    // --- SETUP BMP280 ---
    
    if (!bmp.begin()) { 
        Serial.println("ERRORE: BMP280 non trovato!"); 
        while(1) delay(10); 
    }
    
    
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                    Adafruit_BMP280::SAMPLING_X16,    // Temp. oversampling
                    Adafruit_BMP280::SAMPLING_X16,    // Press. oversampling
                    Adafruit_BMP280::FILTER_X16,      // Filtro hardware interno
                    Adafruit_BMP280::STANDBY_MS_1);   // Tempo tra le letture

    if (!mpu.begin()) { 
        Serial.println("ERRORE: MPU6050 non trovato!"); 
        while(1) delay(10);
    }

    // --- CALIBRAZIONE ZERO ---
    Serial.println("Calibrazione altimetro in corso...");
    delay(1000); // tempo per far assestare il sensore
    
    
    altitudineZero = bmp.readAltitude(p0);
    
    Serial.printf("Quota terra letta (relativa al livello mare std): %.2f",altitudineZero); 
    

    // file per missione
    fileLog = LittleFS.open("/dati_volo.csv", FILE_WRITE); // Sovrascrive il file precedente
    if(fileLog) {
        fileLog.println("tempo_ms,alt_relativa,acc_x,acc_y,acc_z"); 
        fileLog.close();
    }
}

void loop() {
    unsigned long tempo = millis(); 

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp); 
    
    // Calcoliamo l'altitudine relativa (rispetto al suolo da dove decolla)
    float altitudineRelativa = bmp.readAltitude(p0) - altitudineZero;

    // --- LOGGING ---
    // Usiamo FILE_APPEND per aggiungere righe in fondo al file
    fileLog = LittleFS.open("/volo.csv", FILE_APPEND);
    if(fileLog) {
        fileLog.print(tempo); fileLog.print(",");
        fileLog.print(altitudineRelativa); fileLog.print(",");
        fileLog.print(a.acceleration.x); fileLog.print(",");
        fileLog.print(a.acceleration.y); fileLog.print(",");
        fileLog.println(a.acceleration.z);
        fileLog.close(); 
    }

    Serial.print("Quota: ");
    Serial.print(altitudineRelativa);
    Serial.println(" m");

    
    delay(50); 
}