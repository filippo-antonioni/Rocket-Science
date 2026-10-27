#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

// Formato little-endian, float IEEE754 a 32 bit. v3: un record = 88 byte.
#pragma pack(push, 1)
struct LogHeader {
    char magic[8] = {'R','K','T','L','O','G','3','\0'};
    uint16_t version = 3, recordSize = 88;
    uint32_t periodoUs = 10000, preCampioni = 300, sorgente = 0, asse = 2;
    int32_t segno = 1;
    uint32_t logMode = 1, coreMode = 0, clockMode = 0, durataMs = 35000;
};
struct Record {
    uint32_t tMs, dtUs, sequenza;
    float forza, baro, h, v, bias, pressione;
    int16_t raw[6];
    uint8_t fase, flags, motivo, correzione;
    uint32_t ritardi, persi;
    uint32_t wallUs, realDtUs, coreUs;
    float trueH, trueV; // NaN sui sensori reali
    uint32_t queueDepth, crc;
};
#pragma pack(pop)
static_assert(sizeof(Record) == 88, "Formato record modificato");
static_assert(sizeof(LogHeader) == 48, "Formato header modificato");
// flags: 1 IMU valida, 2 baro nuovo, 4 deploy, 8 PWM servo comandato,
//        16 simulazione, 32 errore numerico Kalman.
inline uint32_t crc32(const void* data, size_t size) {
    const auto* b = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t n = 0; n < size; ++n) {
        crc ^= b[n];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
template <typename T, size_t N> class Ring {
    T dati[N]; size_t next = 0, used = 0;
public:
    void clear() { next = used = 0; }
    void push(const T& value) { dati[next] = value; next = (next + 1) % N; if (used < N) ++used; }
    size_t size() const { return used; }
    const T& oldest(size_t i) const { return dati[(next + N - used + i) % N]; }
};
