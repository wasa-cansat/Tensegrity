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

class Message {
 public:
    uint8_t seq;
    uint8_t type;
    uint8_t index;
    /* uint32_t     payload; */
    byte    payload[4];

    Message* next = nullptr;

    /* Message(uintt8_t type, uint8_t index, uint32_t payload): */
    /*   type(type), index(index), payload(payload), next(nullptr) {}; */
    Message(byte *bytes);
};

class MessageList {
 public:
    Message* first = nullptr;
    Message* last  = nullptr;
    unsigned count = 0;
    uint8_t id;

    MessageList(uint8_t id): id(id) {};
    bool parseHexAndAdd(String str);
    int remove(Message *message);
};

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

    uint8_t seq = 0;

    MessageList messages;

 public:

    Comm(uint8_t id): id(id), messages(MessageList(id)) {};

    void setMode(Mode m) {mode = m;}
    void setSerialDestination(Stream& s) {serial = s;}
    void setTelemetryDestination(Stream& s) {telemetry = s;}

    void nextSequence() {seq++;};

    // Sending

    void log(const char* str);
    void log(String &str);
    void error(uint8_t code, const char* info, const char* str = "");
    void return_();

    void send(uint8_t type, uint8_t index);
    template <typename T>
        void send(uint8_t type, uint8_t index, const T& payload) {
        return send(type, index, reinterpret_cast<const byte*>(&payload));
    }

    // Receiving

    int receive();
    unsigned messageCount();
    template <typename T>
        bool expect(uint8_t type, uint8_t& index, T& payload) {
        return expect(type, index, reinterpret_cast<byte*>(&payload));
    }

    String readLine();
    bool inputAvailable();

 private:
    void send(uint8_t type, uint8_t index, const byte* payload);
    bool expect(uint8_t type, uint8_t& index, byte* payload);
};
