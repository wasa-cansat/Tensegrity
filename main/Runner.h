#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "Util.h"



class Motor {
  static const uint8_t CONTROL = 0x00;
  static const uint8_t FAULT   = 0x01;

public:
  I2CAddr addr;

  byte i2c_error = 0;

  Motor(I2CAddr addr): addr(addr) {}
  bool init();

  void clear_control();
  void control(float voltage, bool brake = false);
  byte read_control();
  void clear_fault();
  byte read_fault();
};


class Runner {
  uint8_t number;
  Motor   motor;
  Pin     limitSW;
  Vec3    axis;
  float   maxVoltage = 3.0;
  float   accel = 0.1;

  static const int LIMIT_A_TH    = 1000;
  static const int LIMIT_B_TH    = 2500;

  float voltage = 0;
  int jog_count = 0;

  Comm &comm;

public:
  bool enable = false;
  bool reverse;

  int target = 0;

 Runner(Comm &comm,
        uint8_t number,
        uint8_t motorAddr,
        Pin limitSW,
        Vec3 axis,
        bool reverse = false):
  comm(comm),
      number(number),
      motor(Motor(motorAddr)),
      limitSW(limitSW),
      axis(axis),
      reverse(reverse) {};

  /* bool init(uint8_t number, uint8_t motorAddr, Pin limitSW, Vec3 axis, bool reverse = false); */
  bool init();
  void update();
  void headTowards(Vec3 goal);

  // Limit switches ----------------------------------------------------------
  bool moveCompleted();
  bool onEndA();
  bool onEndB();
  int  position();

  // For debug ---------------------------------------------------------------
  void jog(int count);
  void test();
  bool checkLimitSwitches();
  void print();
  void send();
};

