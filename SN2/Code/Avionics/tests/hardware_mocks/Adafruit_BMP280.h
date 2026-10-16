#pragma once
struct Adafruit_BMP280 {
 enum {MODE_SLEEP,SAMPLING_X1,SAMPLING_X8,FILTER_X4,STANDBY_MS_1};
 bool begin(int);void setSampling(int,int,int,int,int);float readPressure();
};
