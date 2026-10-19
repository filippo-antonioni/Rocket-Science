#include <Arduino.h>
#include <driver/ledc.h>

namespace diag {
constexpr int outputPin = 17, inputPin = 16;
constexpr uint32_t periodUs = 20000;
constexpr uint32_t ticks = 1u << 14;
constexpr uint32_t startupMs = 10000, durationMs = 60000;
bool ready = false, started = false, finished = false;
uint32_t bootMs = 0, startMs = 0, lastChangeMs = 0;
unsigned angle = 130;

uint32_t pulseForAngle(unsigned degrees) {
    return 500u + degrees * 1900u / 180u;
}
uint32_t dutyForPulse(uint32_t pulseUs) {
    return (pulseUs * ticks + periodUs / 2u) / periodUs;
}
bool nearValue(uint32_t value, uint32_t target, uint32_t tolerance) {
    return value >= target - tolerance && value <= target + tolerance;
}
void stopSignal() {
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    finished = true;
}
void setAngleAndMeasure(unsigned degrees) {
    const uint32_t pulseUs = pulseForAngle(degrees);
    const uint32_t duty = dutyForPulse(pulseUs);
    const esp_err_t setResult = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    const esp_err_t updateResult = setResult == ESP_OK
        ? ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0) : setResult;
    if (updateResult != ESP_OK) {
        Serial.printf("ERR PWM_WRITE code=%d\n", int(updateResult));
        stopSignal();
        return;
    }
    // Attende piu' di un periodo per misurare la nuova posizione comandata.
    delay(60);
    const uint32_t highUs = pulseIn(inputPin, HIGH, 30000);
    const uint32_t lowUs = pulseIn(inputPin, LOW, 30000);
    const uint32_t hz = ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0);
    const bool signalOk = highUs && lowUs && nearValue(highUs, pulseUs, 100)
        && nearValue(highUs + lowUs, periodUs, 400);
    const char* feedback = (!highUs || !lowUs) ? "NO_SIGNAL" : signalOk ? "PASS" : "CHECK";
    Serial.printf("PWM pin=17 input=16 angle=%u hz=%lu pulse_us=%lu high_us=%lu low_us=%lu feedback=%s\n",
        degrees, (unsigned long)hz, (unsigned long)pulseUs,
        (unsigned long)highUs, (unsigned long)lowUs, feedback);
}
}

void setup() {
    Serial.begin(115200);
    pinMode(diag::inputPin, INPUT_PULLDOWN);
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_14_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 50;
    timer.clk_cfg = LEDC_AUTO_CLK;
    const esp_err_t timerResult = ledc_timer_config(&timer);
    if (timerResult != ESP_OK) {
        Serial.printf("ERR PWM_TIMER code=%d\n", int(timerResult));
        return;
    }
    ledc_channel_config_t channel = {};
    channel.gpio_num = diag::outputPin;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_0;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.timer_sel = LEDC_TIMER_0;
    channel.duty = 0;
    channel.hpoint = 0;
    const esp_err_t channelResult = ledc_channel_config(&channel);
    if (channelResult != ESP_OK) {
        Serial.printf("ERR PWM_CHANNEL code=%d\n", int(channelResult));
        return;
    }
    diag::ready = true;
    diag::bootMs = millis();
    Serial.println("DIAGNOSI_SERVO: avvio tra 10 s; IO17 uscita, IO16 ingresso di verifica");
}

void loop() {
    if (!diag::ready || diag::finished) {delay(20);return;}
    const uint32_t now = millis();
    if (!diag::started) {
        if (uint32_t(now - diag::bootMs) < diag::startupMs) {delay(1);return;}
        diag::started = true;
        diag::startMs = diag::lastChangeMs = now;
        diag::setAngleAndMeasure(diag::angle);
        return;
    }
    if (uint32_t(now - diag::startMs) >= diag::durationMs) {
        diag::stopSignal();
        Serial.println("TEST_FINITO: impulsi disattivati; alimentazione servo ancora presente");
        return;
    }
    if (uint32_t(now - diag::lastChangeMs) >= 3000) {
        diag::lastChangeMs = now;
        diag::angle = diag::angle == 130 ? 90 : 130;
        diag::setAngleAndMeasure(diag::angle);
    }
    delay(1);
}

