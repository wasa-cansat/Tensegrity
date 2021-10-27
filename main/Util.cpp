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

void Comm::send(uint8_t type, uint8_t index, const byte* payload) {
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
void Comm::send(uint8_t type, uint8_t index) {
    send(type, index, 0);
}

int Comm::receive() {
    while (inputAvailable()) {
        String str = readLine();
        if (str == "") continue;
        messages.parseHexAndAdd(str);
    }
    return messages.count;
}

bool Comm::expect(uint8_t type, uint8_t& index, byte* payload) {
    Message* message = messages.first;
    while (message != nullptr) {
        if (message->type == type) {
            index = message->index;
            for (int i = 0; i < 4; i++) payload[i] = message->payload[i];
            messages.remove(message);
            return true;
        }
        message = message->next;
    }
    return false;
}

String Comm::readLine() {
    if (serial.available() > 0)    return serial.readStringUntil('\n');
    if (telemetry.available() > 0) return telemetry.readStringUntil('\n');
    return "";
}

bool Comm::inputAvailable() {
    return serial.available() > 0 || telemetry.available() > 0;
}

void Comm::return_() {
    telemetry.print('\n');
}


Message::Message(byte *bytes) {
    seq        = bytes[1];
    type       = bytes[2];
    index      = bytes[3];
    payload[0] = bytes[4];
    payload[1] = bytes[5];
    payload[2] = bytes[6];
    payload[3] = bytes[7];
}

bool MessageList::parseHexAndAdd(String str) {
    char *err = NULL;
    byte bytes[8];
    for (int i = 0; i < 8; i++) {
        char hex[3];
        hex[0] = str.charAt(2 * i );
        hex[1] = str.charAt(2 * i + 1);
        hex[2] = '\0';
        bytes[i] = strtol(hex, &err, 16);
    }
    if (*err != '\0')   return false;
    if (bytes[0] != id) return false;

    Message* message = new Message(bytes);

    if (first == nullptr) first = message;
    if (last  != nullptr) last->next = message;
    last = message;
    count++;

    return true;
}

int MessageList::remove(Message *message) {
    Message* m = first;
    Message* p = nullptr;
    unsigned position = 0;
    while (m != nullptr) {
        if (m == message) {
            if (p == nullptr) first = message->next;
            else              p->next = message->next;
            if (message == last) last = p;
            delete message;
            return position;
        }
        p = m;
        m = m->next;
        position++;
    }
    return -1;
}
