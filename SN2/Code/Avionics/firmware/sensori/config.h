#pragma once
#include <cstdint>
#include <cstddef>
#ifndef FIRMWARE_VOLO
#define FIRMWARE_VOLO 0
#endif
#ifndef RGB_PIN
#define RGB_PIN 48
#endif
namespace cfg {
constexpr int sda = 8, scl = 9;
constexpr int ledDeploy = 4; // LED esterno con resistenza in serie
constexpr int servoPin = 18;
constexpr int servoChiuso = 130, servoAperto = 90;
constexpr int asse = 0; // 0=X, 1=Y, 2=Z
constexpr int segno = -1;
constexpr uint32_t periodoUs = 10000;
constexpr unsigned preCampioni = 300; // ultimi 3 s a 100 Hz
constexpr unsigned codaCampioni = 512;
constexpr uint32_t maxLogMs = 180000; // dopo BOOST; algoritmo continua dopo il limite
constexpr uint32_t riservaByte = 1800000; // 180 s di record v3 + prebuffer + margine
constexpr size_t ramBufferBytes = 2u*1024u*1024u;
constexpr uint32_t ramRiservaByte = ramBufferBytes + 65536u;
// VOLO puo' comandare il servo solo se impostato esplicitamente true.
// BANCO non collega MAI il PWM, nemmeno con questa opzione true.
constexpr bool abilitaServo = false;
static_assert(asse >= 0 && asse < 3, "asse deve essere 0,1,2");
static_assert(segno == 1 || segno == -1, "segno deve essere +1 o -1");
static_assert(ledDeploy != servoPin && ledDeploy != sda && ledDeploy != scl, "GPIO in conflitto");
}
