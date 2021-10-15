#include <BluetoothSerial.h>
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


/* #define LOG_VIA_BLUETOOTH */

#ifdef LOG_VIA_BLUETOOTH
BluetoothSerial S;
#else
#define S Serial
#endif


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
Runner runners[6];

// General =====================================================================
#define FREQ 20

#define SERIAL_BAUD 115200
#define PRINT_FREQ 1
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

// Earth's magnetic field varies by location. Add or subtract
// a declination to get a more accurate heading. Calculate
// your's here:
// http://www.ngdc.noaa.gov/geomag-web/#declination
#define DECLINATION -8.58 // Declination (degrees) in Boulder, CO.



//Function definitions
void calibrateIMU();
bool scanI2C();
void printGyro();
void printAccel();
void printMag();
void printAttitude();

void setup()
{
#ifdef LOG_VIA_BLUETOOTH
  S.begin("Tensegrity");
#else
  S.begin(SERIAL_BAUD);
#endif

  Wire.begin(SDA, SCL);
  ss.begin(GPS_BAUD);

  S.println("Initializing");

  // Initialize and diagnose
  while (true) {
    bool i2c_ok = scanI2C();
    bool imu_ok = imu.begin(LSM9DS1_AG, LSM9DS1_M);
    if (!imu_ok) S.println("Failed to communicate with LSM9DS1.");
    imu_ok = true;

    /* uint8_t LIMITS[6] = { A0, A3, A4, A5, A6, A7 }; */
    /* Pin LIMITS[6] = { 36, 39, 32, 33, 34, 35 }; */

    bool runners_ok = true;
    runners_ok &= runners[0].init(0, 0x60, A0, vec3(0, 0, 1));
    runners_ok &= runners[1].init(1, 0x61, A3, vec3(0, 0, 1));
    runners_ok &= runners[2].init(2, 0x62, A4, vec3(0, 0, 1));
    runners_ok &= runners[3].init(3, 0x63, A5, vec3(0, 0, 1));
    runners_ok &= runners[4].init(4, 0x64, A6, vec3(0, 0, 1));
    runners_ok &= runners[5].init(5, 0x65, A7, vec3(0, 0, 1));

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

  MadgwickFilter
    .update(imu.calcGyro(imu.gx), imu.calcGyro(imu.gy), imu.calcGyro(imu.gz),
            imu.calcAccel(imu.ax),imu.calcAccel(imu.ay),imu.calcAccel(imu.az),
            imu.calcMag(-imu.my), imu.calcMag(-imu.mx), imu.calcMag(imu.mz));

  float q[4];
  MadgwickFilter.getQuaternion(q);

  Quaternion att;
  att.a=q[0];
  att.b=q[1];
  att.c=q[2];
  att.d=q[3];

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

  Vec3 goal_abs(xg, yg, 0);
  goal_abs.normalize();

  Quaternion goal_body = att.rotate(goal_abs);

  for (uint8_t i = 0; i < 6; i++) {
    /* if (runners[i].enable) runners[i].headTowards(goal_body); */
  }

  for (uint8_t i = 0; i < 6; i++) {
    if (runners[i].enable) runners[i].update();
  }

  // Print for debug
  if (millis() - lastPrint > 1000 / PRINT_FREQ) {

    S.print(",Lat:"); S.print(gps.location.lat(), 6);
    S.print(",Lng:"); S.print(gps.location.lng(), 6);

    /* printGyro(); */
    /* printAccel(); */
    /* printMag(); */
    /* printAttitude(); */


    /* printVec3(goal_abs, 'G'); */

    /* printQuaternion(att); */

    for (uint8_t i = 0; i < 6; i++) {
      if (runners[i].enable) runners[i].print();
    }

    S.println("");

    lastPrint = millis(); // Update lastPrint time
  }

  // Receive command
  if (S.available() > 0) {
    /* String cmd = ""; */
    /* while (S.available() > 0) { */
    /*   cmd.concat(S.read()); */
    /* } */
    String cmd = S.readStringUntil('\n');
    switch (command_state) {
    case CommandState::StandBy:
      if (cmd == "j" || (cmd == "" && command == Command::Jog)) {
        command = Command::Jog;
        command_state = CommandState::Target;
        S.print("Jog: Specify the runnner (#");
        S.print(command_target);
        S.println(")");
      }
      else {
        S.println("Wrong command");
        command_state = CommandState::StandBy;
      }
      break;
    case CommandState::Target:
      if (cmd == "x") {

      }
      else if (cmd == "") {
        command_state = CommandState::Value;
        S.print("Input the value (");
        S.print(command_value);
        S.println(")");
      }
      else {
        int number = cmd.toInt();
        if (0 <= number && number <= 5) {
          command_target = number;
          command_state = CommandState::Value;
          S.print("Input the value (");
          S.print(command_value);
          S.println(")");
        }
        else {
          S.println("Wrong number");
          command_state = CommandState::StandBy;
        }
      }
      break;
    case CommandState::Value:
      if (cmd != "") command_value = cmd.toInt();
      switch (command) {
      case Command::Jog:
        S.print("Jog #");
        S.print(command_target);
        S.print(" ");
        S.println(command_value);

        if (command_target >= 0) {
          runners[command_target].jog(command_value / 1000.0 * FREQ);
        }

        command_state = CommandState::StandBy;
        break;
      }
      break;
    }
  }




  unsigned past_millis = millis() - start_millis;
  int wait = 1000 / FREQ - past_millis;
  if (wait > 0) delay(wait);
  else S.println("FLAMEOUT");
}


void calibrateIMU() {
  S.println("Calibration started...");
  imu.calibrateMag(true);
  S.println("Calculation finished");
}

void printGyro() {
  S.print(",Gx:"); S.print(imu.calcGyro(imu.gx), 2);
  S.print(",Gy:"); S.print(imu.calcGyro(imu.gy), 2);
  S.print(",Gz:"); S.print(imu.calcGyro(imu.gz), 2);
}
void printAccel() {
  S.print(",Ax:"); S.print(imu.calcAccel(imu.ax), 2);
  S.print(",Ay:"); S.print(imu.calcAccel(imu.ay), 2);
  S.print(",Az:"); S.print(imu.calcAccel(imu.az), 2);
}
void printMag() {
  S.print(",Mx:"); S.print(imu.calcMag(imu.mx), 2);
  S.print(",My:"); S.print(imu.calcMag(imu.my), 2);
  S.print(",Mz:"); S.print(imu.calcMag(imu.mz), 2);
}

void printAttitude() {
  float roll  = MadgwickFilter.getRoll();
  float pitch = MadgwickFilter.getPitch();
  float yaw   = MadgwickFilter.getYaw();

  S.print(",Pitch:"); S.print(pitch, 2);
  S.print(",Roll:");  S.print(roll, 2);
  S.print(",Yaw:");   S.print(yaw, 2);
}

bool scanI2C()
{
  byte error, address;
  int nDevices;

  S.println("Scanning...");

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
          S.print("I2C device found at address 0x");
          if (address<16)
            S.print("0");
          S.print(address,HEX);
          S.println("  !");

          nDevices++;
        }
      else if (error==4)
        {
          S.print("Unknown error at address 0x");
          if (address<16)
            S.print("0");
          S.println(address,HEX);
        }
    }
  if (nDevices == 0) {
    S.println("No I2C devices found\n");
    return false;
  }
  else if (nDevices > 10) {
    S.println("Too many I2C devices found\n");
    return false;
  }
  else {
    S.println("done\n");
    return true;
  }
}
