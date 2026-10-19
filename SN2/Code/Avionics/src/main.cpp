#include <LittleFS.h>

// Compila una sola modalità alla volta.
#if MODE_SENSORI
#include "../firmware/sensori/main.cpp"
#else
#include "../firmware/banco/main.cpp"
#endif