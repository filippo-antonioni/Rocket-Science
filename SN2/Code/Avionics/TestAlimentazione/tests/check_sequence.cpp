#include <cassert>
#include <iostream>
#include "../src/power_sequence.h"
#include "../src/power_servo.h"

int timerResult=ESP_OK,channelResult=ESP_OK,setResult=ESP_OK,updateResult=ESP_OK;
uint32_t lastDuty=0;unsigned updates=0;
esp_err_t ledc_timer_config(const ledc_timer_config_t* t) {
    assert(t->freq_hz==50 && t->duty_resolution==14);return timerResult;
}
esp_err_t ledc_channel_config(const ledc_channel_config_t* c) {
    assert(c->gpio_num==17 && c->duty==0);return channelResult;
}
esp_err_t ledc_set_duty(int,int,uint32_t duty) {lastDuty=duty;return setResult;}
esp_err_t ledc_update_duty(int,int) {++updates;return updateResult;}
int main() {
    using namespace powerTest;
    Sequence seq;
    assert(!seq.due(0) && !seq.due(59999) && !seq.isOpen());
    assert(seq.due(60000)->open && seq.completed()==0);
    // A failed command must not be recorded as executed.
    assert(seq.due(60001)->open && seq.completed()==0);
    seq.acknowledge();assert(seq.isOpen() && !seq.due(89999));
    assert(!seq.due(90000)->open);seq.acknowledge();assert(!seq.isOpen());
    assert(!seq.due(119999) && seq.due(120000)->open);
    seq.acknowledge();assert(seq.isOpen() && seq.completed()==3);
    assert(!seq.due(180000) && !seq.due(0xffffffff));
    seq.acknowledge();assert(seq.completed()==3);
    PowerServo servo;
    assert(servo.begin());assert(servo.write(false));
    assert(lastDuty==(1872u*16384u+10000u)/20000u);
    assert(servo.write(true));assert(lastDuty==(1450u*16384u+10000u)/20000u);
    unsigned count=updates;setResult=-1;assert(!servo.write(true));assert(updates==count);
    setResult=ESP_OK;updateResult=-1;assert(!servo.write(false));
    timerResult=-1;assert(!servo.begin());timerResult=ESP_OK;channelResult=-1;assert(!servo.begin());
    std::cout<<"PASS: timer 60/90/120 s, nessun duplicato, GPIO17, PWM e propagazione errori\n";
}
