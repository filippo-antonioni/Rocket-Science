#pragma once
#include "Wire.h"
#include <cstdint>
enum {INV_SENSOR_ACCEL};
struct inv_imu_sensor_event_t {int sensor_mask;int16_t accel[3],gyro[3];};
struct ICM42670 {
 ICM42670(TwoWire&,int);int begin();int startAccel(int,int);int startGyro(int,int);
 int getDataFromRegisters(inv_imu_sensor_event_t&);
};
