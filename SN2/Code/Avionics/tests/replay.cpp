#include "../firmware/banco/flight_core.h"
#include <iostream>
#include <iomanip>
#include <string>

int main() {
    volo::Parametri p;
    volo::Kalman k;
    volo::MacchinaStati m;
    std::string acc, baro;
    unsigned fresh;
    double dt;
    uint32_t ms;
    std::cout << std::setprecision(17);
    while (std::cin >> acc >> baro >> fresh >> dt >> ms) {
        const double a = std::stod(acc), h = std::stod(baro);
        auto c = k.step(a, h, fresh != 0, dt, p);
        auto e = m.step(k.x[1], a, k.x[0], ms, p);
        for (double v : k.x) std::cout << v << ' ';
        for (auto& row : k.P) for (double v : row) std::cout << v << ' ';
        std::cout << k.scarti << ' ' << int(c) << ' ' << int(m.fase) << ' '
                  << e.decollo << ' ' << e.apertura << ' ' << m.deploy << '\n';
    }
}
