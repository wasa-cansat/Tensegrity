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
