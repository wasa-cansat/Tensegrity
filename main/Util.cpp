#include "Util.h"

Vec3 vec3(float x, float y, float z) {
    Quaternion v(x, y, z);
    return v;
}

Vec3 zero() {
    return vec3(0, 0, 0);
}

float getX(Vec3 v) { return v.b; };
float getY(Vec3 v) { return v.c; };
float getZ(Vec3 v) { return v.d; };

void printQuaternion(Quaternion q, Print &S) {
    S.print(",p:"); S.println(q.a, 4);  //q0が特別な奴
    S.print(",q:"); S.println(q.b, 4);
    S.print(",r:"); S.println(q.c, 4);
    S.print(",s:"); S.println(q.d, 4);
}

void printVec3(Vec3 v, char prefix, Print &S) {
    S.print(","); if (prefix != '\0') S.print(prefix);
    S.print("x:"); S.print(v.b, 4);
    S.print(","); if (prefix != '\0') S.print(prefix);
    S.print("y:"); S.print(v.c, 4);
    S.print(","); if (prefix != '\0') S.print(prefix);
    S.print("z:"); S.print(v.d, 4);
}

void Comm::log(const char* str) {
    serial.print("# ");
    serial.println(str);
}
void Comm::log(String &str) {
    serial.print("# ");
    serial.println(str);
}

void Comm::error(uint8_t code, const char* info, const char* str) {
    serial.print("#! ");
    serial.print(info);
    serial.print(' ');
    serial.println(str);
    send('E', code, info);
}

void Comm::send(uint8_t type, uint8_t index) {
    send(type, index, 0);
}

void Comm::send(uint8_t type, uint8_t index, const byte payload[4]) {
    byte data[8] = {id, seq, type, index};
    for (int i = 0; i < 4; i++) data[4+i] = payload[i];

    switch (mode) {
    case Hex:
        for (int i = 0; i < 8; i++) {
            if (data[i] < 16) telemetry.print('0');
            telemetry.print(data[i], HEX);
        }
        telemetry.print('\n');
        break;
    case Binary:
        for (int i = 0; i < 8; i++) telemetry.write(data[i]);
        break;
    default:
        break;
    }
}
void Comm::send(uint8_t type, uint8_t index, const void* payload) {
    send(type, index, static_cast<const byte*>(payload));
}
void Comm::send(uint8_t type, uint8_t index, const char payload[4]) {
    send(type, index, static_cast<const void*>(payload));
}
void Comm::send(uint8_t type, uint8_t index, int32_t payload) {
    send(type, index, static_cast<void*>(&payload));
}
void Comm::send(uint8_t type, uint8_t index, uint32_t payload) {
    send(type, index, static_cast<void*>(&payload));
}
void Comm::send(uint8_t type, uint8_t index, float payload) {
    send(type, index, static_cast<void*>(&payload));
}
void Comm::send(uint8_t type, uint8_t index, double payload) {
    send(type, index, (float)payload);
}

String Comm::readLine() {
    if (serial.available() > 0)    return serial.readStringUntil('\n');
    if (telemetry.available() > 0) return telemetry.readStringUntil('\n');
    return "";
}

bool Comm::inputAvailable() {
    return serial.available() > 0 || telemetry.available();
}

void Comm::return_() {
    telemetry.print('\n');
}
