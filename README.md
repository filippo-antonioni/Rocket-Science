# Rocket Science

This repository documents a collaborative amateur rocketry project through
successive vehicle versions, combining vehicle modelling, flight simulation
and experimental development.

**SN2 identifies the second version of the rocket.** The first version, SN1,
had no onboard computer. SN2 introduces embedded avionics, with development
work spanning the OpenRocket model, MATLAB simulations, ESP32-S3 firmware
and experimental bench data.

The repository currently contains the SN2 development files and is intended
to accommodate subsequent versions, including a future SN3. The technical
overview below describes the work on SN2.

The avionics work centres on estimating altitude and vertical velocity,
identifying flight phases and commanding a servo-operated parachute release.
Simulation, embedded implementation and hardware testing are kept together
to make the development process and its results traceable.

## Flight modelling and simulation

The MATLAB simulator models vertical flight using a motor thrust curve,
changing vehicle mass, gravity and aerodynamic drag. A sensor model adds
measurement noise, bias and disturbances to the simulated trajectory.

A Kalman filter estimates altitude, vertical velocity and accelerometer bias
from acceleration and barometric measurements. A finite-state machine tracks
the PAD, BOOST, COAST, DESCENT and LANDED phases and generates the deployment
command. The simulation also includes Monte Carlo scripts for exploring
sensitivity to variations in vehicle and sensor parameters.

The [OpenRocket model](SN2/Design/SN2.ork) provides the vehicle design reference;
the [MATLAB sources](SN2/Code/Sim/) contain the flight and sensor models,
estimation logic and analysis scripts.

## Embedded avionics

The embedded implementation uses C++ with the Arduino framework and PlatformIO
on an **ESP32-S3 N16R8**. The hardware includes an **ICM42670P IMU**, a
**BMP280 barometer** and an **SG90 servo** for the recovery mechanism.

The firmware and accompanying Python tools support:

- Replay of synthetic flight datasets, including noise, measurement gaps,
  barometric spikes and timeout scenarios.
- Acquisition and analysis of real sensor data.
- Servo command testing, I2C diagnostics and combined sensor/servo power tests.
- Binary logging with record integrity checks, CSV export and diagnostic plots.

Sensor data are buffered in PSRAM during acquisition and saved to internal
flash afterwards. This approach was introduced to keep flash-write delays
outside the sampling loop. The firmware retains separate configurations for
LED replay, servo replay and real-sensor acquisition.

Implementation details and hardware documentation are collected under
[Avionics](SN2/Code/Avionics/README.md).

## Testing and experimental data

Software checks cover the estimator, state transitions, simulated deployment
commands, logging and binary decoding. Numerical comparisons check the C++
estimator against a Python transcription of the MATLAB equations, using both
fixed and variable sampling intervals.

Hardware bench tests include stationary and moving sensor acquisitions,
PSRAM logging and timed servo commands while the sensors are being recorded.
The [test records](SN2/Code/Avionics/logs/README.md) retain raw measurements,
serial transcripts, analysis results and plots, including incomplete runs
and earlier experiments.

SN2 is currently at the development and bench-validation stage.
The available configurations are test firmware; flight validation of the
complete recovery system remains a separate development milestone.

## Repository structure

Files are grouped by vehicle version. The current `SN2/` directory contains
the second version; later versions can be documented alongside it as the
project develops.

| Directory | Contents |
|---|---|
| `SN2/Design/` | OpenRocket vehicle model |
| `SN2/Code/Sim/` | MATLAB flight simulation, sensor models and analysis |
| `SN2/Code/Avionics/firmware/` | Embedded estimation, state logic, acquisition and logging |
| `SN2/Code/Avionics/tools/` | Python tools for serial communication and data analysis |
| `SN2/Code/Avionics/tests/` | Software tests and simulated hardware interfaces |
| `SN2/Code/Avionics/DiagnosiI2C/` | Sensor bus diagnostic firmware |
| `SN2/Code/Avionics/DiagnosiServo/` | Servo PWM diagnostic firmware |
| `SN2/Code/Avionics/TestAlimentazione/` | Combined sensor, logging and servo test |
| `SN2/Code/Avionics/logs/` | Experimental datasets and reports |
| `SN2/Code/Avionics/docs/` | Hardware documentation and development notes |
| `SN2/Code/Avionics/archive/` | Earlier source versions retained for reference |
| `SN2/Code/Gestione_fairing/` | Original standalone servo sketch |
| `SN2/docs/` | Supporting project material |
| `tools/` | Repository-level software verification |

**Main tools:** MATLAB · OpenRocket · C++ · Arduino · PlatformIO · Python
