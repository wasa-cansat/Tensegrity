#include "Runner.h"


bool Motor::init(I2CAddr addr) {
    this->addr = addr;
    Wire.beginTransmission(addr);
    i2c_error = Wire.endTransmission();
    return i2c_error == 0;
}
void Motor::clear_control() {
    Wire.beginTransmission(addr);
    Wire.write(CONTROL);
    Wire.write(0x00 << 2 | 0x00);
    i2c_error = Wire.endTransmission();
}
void Motor::control(float voltage, bool brake) {
    uint8_t speed = 0;
    uint8_t direction = 0x00;
    if (abs(voltage) >= 0.48) {
        speed = abs(voltage) * 16.0 / 1.285;
        if (voltage > 0) direction = 0x01;
        else             direction = 0x02;
    }
    if (brake) direction = 0x03;
    Wire.beginTransmission(addr);
    Wire.write(CONTROL);
    Wire.write(speed << 2 | direction);
    i2c_error = Wire.endTransmission();
}
byte Motor::read_control() {
    Wire.beginTransmission(addr);
    Wire.write(CONTROL);
    i2c_error = Wire.endTransmission();
    Wire.requestFrom(addr, (uint8_t)1);
    if (Wire.available() > 0)
        return (uint8_t)Wire.read();
    return 0;
}
void Motor::clear_fault() {
    Wire.beginTransmission(addr);
    Wire.write(FAULT);
    Wire.write(0x80);
    i2c_error = Wire.endTransmission();
}
byte Motor::read_fault() {
    Wire.beginTransmission(addr);
    Wire.write(FAULT);
    i2c_error = Wire.endTransmission();
    Wire.requestFrom(addr, (uint8_t)1);
    if (Wire.available() > 0)
        return (uint8_t)Wire.read();
    return 0;
}


bool Runner::init(uint8_t number, uint8_t motorAddr, Pin limitSW, Vec3 axis, bool reverse) {
    this->number  = number;
    this->limitSW = limitSW;
    this->axis    = axis;
    this->reverse = reverse;

    voltage   = 0;
    jog_count = 0;

    analogSetPinAttenuation(limitSW, ADC_11db);
    pinMode(limitSW, ANALOG);

    if (!motor.init(motorAddr)) {
      S.print("Failed to communicate with Motor #");
      S.print(number);
      S.print(" (Address: ");
      S.print(motorAddr, HEX);
      S.println(")");
      enable = false;
      return false;
    }
    delay(10);
    motor.clear_control();
    delay(10);
    motor.clear_fault();
    delay(10);

    enable = true;
    return true;
}

void Runner::update() {
    int limit = analogRead(limitSW);
    /* S.println(motor.read_fault()); */
    /* motor.clear_fault(); */

    float target_voltage = 0;

    if (jog_count > 0) {
      target_voltage = reverse ? -maxVoltage : maxVoltage;
      jog_count--;
    }
    else if (jog_count < 0) {
      target_voltage = reverse ? maxVoltage : -maxVoltage;
      jog_count++;
    }
    else {
      if (moveCompleted()) target_voltage = 0;
      else if (target > 0) target_voltage = reverse ? -maxVoltage :  maxVoltage;
      else if (target < 0) target_voltage = reverse ?  maxVoltage : -maxVoltage;
      else                 target_voltage = 0;
    }

    if      (voltage <= target_voltage - accel) voltage += accel;
    else if (voltage >= target_voltage + accel) voltage -= accel;
    else                                        voltage =  target_voltage;

    motor.control(voltage);
  }

void Runner::headTowards(Vec3 goal) {
    float dp = axis.dot_product(goal);
    if      (dp > 0) target =  1;
    else if (dp < 0) target = -1;
    else             target =  0;
  }

  // Limit switches ----------------------------------------------------------

bool Runner::moveCompleted() {
    if      (target == 0) return true;
    else if (target > 0)  return reverse ? onEndB() : onEndA();
    else                  return reverse ? onEndA() : onEndB();
  }

bool Runner::onEndA() {
    int limit = analogRead(limitSW);
    return limit < LIMIT_A_TH;
  }
bool Runner::onEndB() {
    int limit = analogRead(limitSW);
    return LIMIT_A_TH < limit && limit < LIMIT_B_TH;
  }

int Runner::position() {
    if      (onEndA()) return  1;
    else if (onEndB()) return -1;
    else               return  0;
  }


  // For debug ---------------------------------------------------------------

void Runner::jog(int count) {
    S.println(count);
    jog_count = count;
  }

void Runner::test() {
    S.println("Testing runner...");
    delay(500);
    target = 1;

    for (int i = 0; i < 500; i++) {
    /* while (true) { */
      if (moveCompleted()) {
        int t = target;
        target = 0;
        update();
        delay(1000);
        target = -t;
      }
      update();
      delay(50);
    }

    target = 0;
  }

bool Runner::checkLimitSwitches() {
    S.println("Checking limit switches...");
    bool rev = false;
    int i = 0;
    target = 1;
    while (true) {
      update();
      if (onEndA()) {
        S.println("Detected the end A");
        break;
      }
      else if (onEndB()) {
        rev = true;
        break;
        S.println("Detected the end B");
      }
      delay(50);
      i++;
      if (i > 500 || S.available() > 0) {
        S.println("Couldn't detect any ends");
        return false;
      }
    }

    i = 0;
    target = -1;
    while (true) {
      update();
      if (rev ? onEndA() : onEndB()) {
        S.println("Detected another end");
        break;
      }
      delay(50);
      i++;
      if (i > 500 || S.available() > 0) {
        S.println("Couldn't detect another end");
        return false;
      }
    }

    S.println(rev ? "reverse = true" : "reverse = false");

    return true;
  }

void Runner::print() {
    S.print(",T"); S.print(number); S.print(":");
    S.print(target);
    S.print(",P"); S.print(number); S.print(":");
    S.print(position());
    S.print(",F"); S.print(number); S.print(":");
    S.print(motor.read_fault());
    /* S.print(",L"); S.print(number); S.print(":"); */
    /* S.print(analogRead(limitSW) / 1000.0); */
}
