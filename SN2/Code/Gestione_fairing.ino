#include <ESP32Servo.h>

// --- CONFIGURAZIONE ---
const int servoPin = 18;       // Sostituisci con il pin a cui hai collegato il servo
const int POS_RIPOSO = 130;    // Posizione di sportello chiuso
const int POS_APERTURA = 90;   // Posizione di scatto per aprire il vano

Servo paracaduteServo;

void setup() {
  Serial.begin(115200);
  
  // Configurazione per ESP32Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  paracaduteServo.setPeriodHertz(50); // Frequenza standard per i servo (50Hz)
  paracaduteServo.attach(servoPin, 500, 2400); // Attach con ampiezze standard del segnale
  
  // All'avvio, metti il servo in posizione di riposo (chiuso)
  paracaduteServo.write(POS_RIPOSO);
  
  Serial.println("--- Test Vano Paracadute ESP32 ---");
  Serial.println("Invia 'A' per APRIRE (90 gradi)");
  Serial.println("Invia 'C' per CHIUDERE (130 gradi)");
}

void loop() {
  if (Serial.available() > 0) {
    char comando = Serial.read();
    
    // Ignora i caratteri di fine riga
    if (comando == '\n' || comando == '\r') return; 

    if (comando == 'A' || comando == 'a') {
      paracaduteServo.write(POS_APERTURA);
      Serial.println("Stato: APERTO (90 gradi). Molla rilasciata.");
    } 
    else if (comando == 'C' || comando == 'c') {
      paracaduteServo.write(POS_RIPOSO);
      Serial.println("Stato: CHIUSO (130 gradi). Pronto al lancio.");
    } 
    else {
      Serial.println("Comando non riconosciuto. Usa 'A' o 'C'.");
    }
  }
}