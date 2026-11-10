#ifndef CPP_H
#define CPP_H
#include <3ds.h>

// CPP coordinates are uncalibrated 12-bit samples, not HID's signed units.
typedef struct {
    bool connected;
    circlePosition position;
    u32 keys;
    u8 battery;
    Result error;
} CppInput;

Result cppInit(void);
void cppPoll(CppInput *input);
void cppExit(void);
void cppSuspend(bool suspended);
#endif
