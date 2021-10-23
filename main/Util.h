#pragma once

#include <Arduino.h>
#include <Quaternion.h>

typedef Quaternion Vec3;
typedef byte       Pin;
typedef byte       I2CAddr;

Vec3 vec3(float x, float y, float z);
Vec3 zero();
void printVec3(Vec3 v, char prefix = '\0', Print &S = Serial);
void printQuaternion(Quaternion q, Print &S = Serial);

float getX(Vec3 v);
float getY(Vec3 v);
float getZ(Vec3 v);

class Comm {
 public:
    enum Mode {
        Binary, Hex,
    };
 private:
    uint8_t id;
    Mode mode = Hex;

    Stream &serial    = Serial;
    Stream &telemetry = Serial;

    uint8_t seq;

 public:


    Comm(uint8_t id): id(id) {};

    void setMode(Mode m) {mode = m;}
    void setSerialDestination(Stream &s) {serial = s;}
    void setTelemetryDestination(Stream &s) {telemetry = s;}

    void nextSequence() {seq++;};

    void log(const char* str);
    void log(String &str);
    void error(uint8_t code, const char* info, const char* str = "");
    void send(uint8_t type, uint8_t index);
    void send(uint8_t type, uint8_t index, const void* payload);
    void send(uint8_t type, uint8_t index, const byte payload[4]);
    void send(uint8_t type, uint8_t index, const char payload[4]);
    void send(uint8_t type, uint8_t index, int32_t  payload);
    void send(uint8_t type, uint8_t index, uint32_t payload);
    void send(uint8_t type, uint8_t index, float    payload);
    void send(uint8_t type, uint8_t index, double   payload);

    String readLine();
    bool inputAvailable();

    void return_();
};
