/* #include <BluetoothSerial.h> */
#include <math.h>
#include <SPI.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <SparkFunLSM9DS1.h>
#include <Quaternion.h>
#include "MadgwickAHRSQ.h"
#include <SPI.h>
#include <SD.h>
#include "Util.h"
#include "Runner.h"



/* BluetoothSerial BS; */

Comm comm('T');

// Wire ======================================--================================
#define SDA 21
#define SCL 22

// IMU =========================================================================
#define LSM9DS1_M  0x1C
#define LSM9DS1_AG 0x6A
LSM9DS1 imu;

// GPS =========================================================================
static const uint32_t GPS_BAUD = 9600;
TinyGPSPlus gps;
HardwareSerial ss(2);
#define latgoal 100;
#define lnggoal 100;

// Madgwick Filter =============================================================
Madgwick MadgwickFilter;


// Runner ======================================================================
Runner runners[6] = {
    Runner(comm, 0, 0x60, A0, vec3(0, 0, 1)),
    Runner(comm, 1, 0x61, A3, vec3(0, 0, 1)),
    Runner(comm, 2, 0x64, A4, vec3(0, 0, 1)),
    Runner(comm, 3, 0x63, A5, vec3(0, 0, 1)),
    Runner(comm, 4, 0x67, A6, vec3(0, 0, 1)),
    Runner(comm, 5, 0x65, A7, vec3(0, 0, 1)),
};

// General =====================================================================
#define FREQ 20

#define SERIAL_BAUD 112500

#define PRINT_FREQ 5
static unsigned long lastPrint = 0; // Keep track of print time

// Command =====================================================================
enum class Command {
  None,
  Jog,
};
Command command = Command::None;
int command_target = -1;
int command_value = 0;

enum class CommandState {
  StandBy,
    Target,
    Value,
    };
CommandState command_state = CommandState::StandBy;

#define DECLINATION 8 // Declination (degrees)

Quaternion dir_correction =
  Quaternion::from_euler_rotation(0, 0, (-DECLINATION + 90) / -180.0 * M_PI);

int calibrating = 0;
Vec3 mag_center  = vec3(0.0199, -0.4695, -0.0373);
float mag_radius = 1;

//Function definitions
float calibrate(Vec3 &mag, Vec3 &center, float &radius, float gain = 0.0001);
bool scanI2C();
void printGyro();
void printAccel();
void printMag();
void printAttitude();

void setup() {
  Serial.begin(SERIAL_BAUD);
  /* BS.begin("Tensegrity"); */
  comm.setMode(Comm::Hex);
  comm.setSerialDestination(Serial);
  comm.setTelemetryDestination(Serial);

  Wire.begin(SDA, SCL);
  ss.begin(GPS_BAUD);

  comm.log("Initializing");

  // Initialize and diagnose
  while (true) {
    bool i2c_ok = scanI2C();
    bool imu_ok = imu.begin(LSM9DS1_AG, LSM9DS1_M);
    if (!imu_ok) comm.error('I', "IMU", "Failed to communicate with LSM9DS1.");
    imu_ok = true;

    bool runners_ok = true;
    for (int i = 0; i < 6; i++) runners_ok &= runners[i].init();
    runners_ok = true;

    bool gps_ok = true;

    if (i2c_ok && imu_ok && runners_ok && gps_ok) break; // All OK

    delay(5000);
  }

  delay(100);

  MadgwickFilter.begin(FREQ);

  /* runners[1].jog(1000); */
  /* runners[1].test(); */

}
void loop()
{

  int start_millis = millis();



  // Update the sensor values whenever new data is available
  if (imu.gyroAvailable())  imu.readGyro();
  if (imu.accelAvailable()) imu.readAccel();
  if (imu.magAvailable())   imu.readMag();

  Vec3 gyro =
    vec3(imu.calcGyro(imu.gx), imu.calcGyro(-imu.gy), imu.calcGyro(imu.gz));
  Vec3 accel =
    vec3(0.80665 * imu.calcAccel(-imu.ax),
         0.80665 * imu.calcAccel(imu.ay),
         0.80665 * imu.calcAccel(-imu.az));
  Vec3 mag =
    vec3(imu.calcMag(-imu.mx) - getX(mag_center),
         imu.calcMag(-imu.my) - getY(mag_center),
         imu.calcMag(imu.mz) - getZ(mag_center));

  float epsilon = 0;

  if (calibrating > 0) {
    epsilon = calibrate(mag, mag_center, mag_radius, 0.01);
    calibrating--;
    if (calibrating == 0) {
      comm.log("End Calibration");
    }
  }


  MadgwickFilter.update(getX(gyro),  getY(gyro),  getZ(gyro),
                        -getX(accel), -getY(accel), -getZ(accel),
                        -getX(mag),  -getY(mag),    -getZ(mag));
  /* MadgwickFilter.updateIMU(getX(gyro),  getY(gyro),  getZ(gyro), */
  /*                          getX(accel), getY(accel), getZ(accel)); */

  float q[4];
  MadgwickFilter.getQuaternion(q);

  Quaternion att;
  att.a=q[0];
  att.b=q[1];
  att.c=q[2];
  att.d=q[3];

  /* att *= dir_correction; */
  att.normalize();

  Quaternion att_inv = att.conj();

  while(ss.available() > 0){
    char c = ss.read();
    gps.encode(c);
    /* if(gps.location.isUpdated()){ */
    /*   latitude  = gps.location.lat(); */
    /*   longitude = gps.location.lng(); */
    /* } */
  }

  float xg = latgoal - gps.location.lat();
  float yg = lnggoal - gps.location.lng();

  xg = 1;
  yg = 0;

  Vec3 goal_abs(xg, yg, 0);
  goal_abs.normalize();
  Vec3 goal = att_inv.rotate(goal_abs);

  /* Vec3 bodyZ = att_inv.rotate(vec3(0, 0, 1)); */
  /* Vec3 bodyX = att_inv.rotate(vec3(1, 0, 0)); */

  for (uint8_t i = 0; i < 6; i++) {
    /* if (runners[i].enable) runners[i].headTowards(goal_body); */
  }

  for (uint8_t i = 0; i < 6; i++) {
    if (runners[i].enable) runners[i].update();
  }

  // Print for debug
  if (millis() - lastPrint > 1000 / PRINT_FREQ) {

    // Receive command
    if (comm.receive() > 0) {
      uint8_t index;
      int   ivalue;
      float fvalue;
      if (comm.expect('j', index, ivalue)) {
        String msg = "Jog #";
        msg.concat(String(index));
        msg.concat(" ");
        msg.concat(String(ivalue));
        comm.log(msg);

        if (index >= 0) runners[index].jog(ivalue / 1000.0 * FREQ);
      }
      if (comm.expect('c', index, ivalue)) {
        comm.log("Start Calibration");
        calibrating = ivalue * FREQ;
      }
    }

    comm.send('L', 0, (float)gps.location.lat());
    comm.send('L', 1, (float)gps.location.lng());

    comm.send('Q', 0, (float)att.a);
    comm.send('Q', 1, (float)att.b);
    comm.send('Q', 2, (float)att.c);
    comm.send('Q', 3, (float)att.d);

    comm.send('A', 0, (float)getX(accel));
    comm.send('A', 1, (float)getY(accel));
    comm.send('A', 2, (float)getZ(accel));

    comm.send('G', 0, (float)getX(goal));
    comm.send('G', 1, (float)getY(goal));
    comm.send('G', 2, (float)getZ(goal));

    /* printQuaternion(att); */

    /* printVec3(gyro,  'G'); */
    /* printVec3(accel, 'A'); */
    /* printVec3(bodyX, 'X'); */
    /* printVec3(bodyZ, 'Z'); */

    /* printVec3(mag,   'M'); */
    /* printVec3(mag_center, 'C'); */

    /* if (calibrating > 0) { */
    /*   Serial.print(",e:"); */
    /*   Serial.print(epsilon); */
    /* } */

    /* Serial.println(""); */


    /* printAttitude(); */


    for (uint8_t i = 0; i < 6; i++) {
      if (runners[i].enable) runners[i].send();
    }

    comm.send('T', 0, (float)(start_millis / 1000.0));
    comm.nextSequence();

    lastPrint = millis(); // Update lastPrint time
  }

  unsigned past_millis = millis() - start_millis;
  int wait = 1000 / FREQ - past_millis;
  if (wait > 0) delay(wait);
  else comm.error('F', "FOT", "FLAMEOUT");
}



float calibrate(Vec3& mag, Vec3& center, float& radius, float gain) {
  float f = mag.b * mag.b + mag.c * mag.c + mag.d * mag.d - radius*radius;
  center.b += 4 * gain * f * mag.b;
  center.c += 4 * gain * f * mag.c;
  center.d += 4 * gain * f * mag.d;
  radius   += 4 * gain * f * radius;
  if (f > 1E30) {
    center.b = 0;
    center.c = 0;
    center.d = 0;
    radius   = 1;
  }
  return f;
}

void printGyro() {
  Serial.print(",Gx:"); Serial.print(imu.calcGyro(imu.gx), 2);
  Serial.print(",Gy:"); Serial.print(imu.calcGyro(imu.gy), 2);
  Serial.print(",Gz:"); Serial.print(imu.calcGyro(imu.gz), 2);
}
void printAccel() {
  Serial.print(",Ax:"); Serial.print(imu.calcAccel(imu.ax), 2);
  Serial.print(",Ay:"); Serial.print(imu.calcAccel(imu.ay), 2);
  Serial.print(",Az:"); Serial.print(imu.calcAccel(imu.az), 2);
}
void printMag() {
  Serial.print(",Mx:"); Serial.print(imu.calcMag(imu.mx), 2);
  Serial.print(",My:"); Serial.print(imu.calcMag(imu.my), 2);
  Serial.print(",Mz:"); Serial.print(imu.calcMag(imu.mz), 2);
}

void printAttitude() {
  float roll  = MadgwickFilter.getRoll();
  float pitch = MadgwickFilter.getPitch();
  float yaw   = MadgwickFilter.getYaw();

  Serial.print(",Pitch:"); Serial.print(pitch, 2);
  Serial.print(",Roll:");  Serial.print(roll, 2);
  Serial.print(",Yaw:");   Serial.print(yaw, 2);
}

bool scanI2C()
{
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ )
    {
      // The i2c_scanner uses the return value of
      // the Write.endTransmisstion to see if
      // a device did acknowledge to the address.
      Wire.beginTransmission(address);
      error = Wire.endTransmission();

      if (error == 0)
        {
          Serial.print("I2C device found at address 0x");
          if (address<16)
            Serial.print("0");
          Serial.print(address,HEX);
          Serial.println("  !");

          nDevices++;
        }
      else if (error==4)
        {
          Serial.print("Unknown error at address 0x");
          if (address<16)
            Serial.print("0");
          Serial.println(address,HEX);
        }
    }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
    return false;
  }
  else if (nDevices > 10) {
    Serial.println("Too many I2C devices found\n");
    return false;
  }
  else {
    Serial.println("done\n");
    return true;
  }
}
