#include <Arduino.h>
#include <Wire.h>

namespace diag {
constexpr int sda=8,scl=9;
constexpr unsigned scanCount=5;
unsigned scans=0;
uint32_t lastScanMs=0;

bool readRegister(uint8_t address,uint8_t reg,uint8_t& value) {
    Wire.beginTransmission(address);Wire.write(reg);
    if(Wire.endTransmission(false)!=0) return false;
    if(Wire.requestFrom(address,uint8_t(1))!=1) return false;
    value=uint8_t(Wire.read());return true;
}

void scan() {
    ++scans;
    int sdaLevel=digitalRead(sda),sclLevel=digitalRead(scl);
    Serial.printf("\nSCAN %u/%u SDA_GPIO=8 SCL_GPIO=9 SDA_IDLE=%d SCL_IDLE=%d\n",
                  scans,scanCount,sdaLevel,sclLevel);
    if(sdaLevel==LOW || sclLevel==LOW) {
        Serial.println("RESULT BUS_LOW: una linea resta bassa; controllare alimentazione, ponti di stagno e collegamenti");
        return;
    }
    unsigned found=0,busErrors=0;
    bool imu=false,imuAlt=false,baro=false;
    // Only ordinary 7-bit addresses: avoid 0x00..0x07 and 0x78..0x7F.
    // In particular, 0x7E is also the I3C broadcast address: probing it can
    // disturb dual I2C/I3C sensors such as the ICM42670P before initialization.
    for(uint8_t address=0x08;address<=0x77;++address) {
        Wire.beginTransmission(address);uint8_t error=Wire.endTransmission();
        if(error==0) {
            ++found;Serial.printf("FOUND 0x%02X\n",unsigned(address));
            if(address==0x68 || address==0x69) {
                uint8_t id=0;
                if(readRegister(address,0x75,id)) {
                    Serial.printf("IMU addr=0x%02X WHO_AM_I=0x%02X expected=0x67 %s\n",
                        unsigned(address),unsigned(id),id==0x67?"OK":"ID_DIVERSO");
                    if(id==0x67) {if(address==0x68) imu=true;else imuAlt=true;}
                } else Serial.println("IMU IDENTIFICAZIONE_NON_LEGGIBILE");
            }
            if(address==0x76 || address==0x77) {
                uint8_t id=0;
                if(readRegister(address,0xD0,id)) {
                    Serial.printf("BARO addr=0x%02X CHIP_ID=0x%02X expected=0x58 %s\n",
                        unsigned(address),unsigned(id),id==0x58?"OK":"ID_DIVERSO");
                    if(id==0x58) baro=true;
                    if(id==0x60) Serial.println("NOTA: ID 0x60 indica BME280, diverso dal BMP280 previsto");
                } else Serial.println("BARO IDENTIFICAZIONE_NON_LEGGIBILE");
            }
        } else if(error!=2) {
            ++busErrors;Serial.printf("BUS_ERROR addr=0x%02X code=%u\n",unsigned(address),unsigned(error));
        }
        delay(1);
    }
    if(imuAlt) Serial.println("NOTA: IMU risponde a 0x69; il firmware attuale usa 0x68. Verificare AD0 -> GND");
    Serial.printf("RESULT found=%u bus_errors=%u IMU_0x68=%s IMU_0x69=%s BMP280=%s\n",
        found,busErrors,imu?"OK":"NO",imuAlt?"OK":"NO",baro?"OK":"NO");
    if(imu && baro && busErrors==0) {
        Serial.println("ENTRAMBI_RICONOSCIUTI: indirizzi e identificativi corretti; non e ancora una prova delle misure");
    }
}
}

void setup() {
    Serial.begin(115200);
    pinMode(4,OUTPUT);digitalWrite(4,LOW);
    Wire.begin(diag::sda,diag::scl);Wire.setClock(100000);Wire.setTimeOut(10);
    diag::scans=0;diag::lastScanMs=millis();
    Serial.println("READY DIAGNOSI_I2C revision=2: SDA=8 SCL=9; solo indirizzi 0x08..0x77; 5 scansioni, nessun servo e nessun file su flash");
}

void loop() {
    if(diag::scans<diag::scanCount && uint32_t(millis()-diag::lastScanMs)>=3000) {
        diag::lastScanMs=millis();diag::scan();
        if(diag::scans==diag::scanCount) Serial.println("END_DIAGNOSI: premere RESET per ripetere");
    }
    delay(1);
}
