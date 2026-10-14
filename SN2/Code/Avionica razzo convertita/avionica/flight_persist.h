#pragma once
// =====================================================================
//  Persistenza in RTC RAM (nessuna scrittura su flash in volo).
//  Sopravvive a panic / watchdog / (di norma) brownout, NON a una perdita totale di alimentazione.
// =====================================================================
#include <Arduino.h>
#include <esp_system.h>
#include "flight_core.h"

RTC_NOINIT_ATTR static FlightSnapshot g_snap;

static uint32_t persistCrc(const FlightSnapshot& s) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&s);
    uint32_t h = 2166136261u;                                  // FNV-1a su tutti i byte prima del campo crc
    for (size_t i = 0; i < offsetof(FlightSnapshot, crc); i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static inline void persistSave(FlightSnapshot s) {
    s.magic = SNAPSHOT_MAGIC;
    s.crc = persistCrc(s);
    g_snap = s;
}

static inline void persistInvalidate() { g_snap.magic = 0; }

// true solo se: reset da guasto (non power-on / non reset manuale), snapshot integro, volo non concluso.
// I reset "volontari" (USB, EN, ESP.restart) NON innescano il recupero: sul pad non deve mai partire una sequenza di volo.
static inline bool persistLoadForRecovery(FlightSnapshot& out) {
#if FLIGHT_RECOVERY
    switch (esp_reset_reason()) {
    case ESP_RST_PANIC: case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:   case ESP_RST_BROWNOUT: break;
    default: return false;
    }
    if (g_snap.magic != SNAPSHOT_MAGIC || g_snap.crc != persistCrc(g_snap)) return false;
    if (g_snap.fase >= (uint8_t)Fase::LANDED) return false;
    out = g_snap;
    return true;
#else
    (void)out; return false;
#endif
}
