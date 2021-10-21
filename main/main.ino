/* #include <BluetoothSerial.h> */
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
#define PRINT_FREQ 4
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

    comm.send('T', 0, (float)start_millis);

    comm.send('L', 0, gps.location.lat());
    comm.send('L', 1, gps.location.lng());

    comm.send('Q', 0, (float)att.a);
    comm.send('Q', 1, (float)att.b);
    comm.send('Q', 2, (float)att.c);
    comm.send('Q', 3, (float)att.d);

    comm.send('A', 0, imu.calcAccel(imu.ax));
    comm.send('A', 1, imu.calcAccel(imu.ay));
    comm.send('A', 2, imu.calcAccel(imu.az));

    /* printQuaternion(att); */



    comm.nextSequence();

    for (uint8_t i = 0; i < 6; i++) {
      /* if (runners[i].enable) runners[i].print(); */
    }

    /* Serial.print(",Lat:"); Serial.print(gps.location.lat(), 6); */
    /* Serial.print(",Lng:"); Serial.print(gps.location.lng(), 6); */

    /* printGyro(); */
    /* printAccel(); */
    /* printMag(); */
    /* printAttitude(); */

    /* Vec3 v = vec3(0, 0, -1); */
    /* printVec3(att.rotate(v)); */
    /* Serial.println(""); */


    /* printVec3(goal_abs, 'G'); */

    /* printQuaternion(att); */

    for (uint8_t i = 0; i < 6; i++) {
      if (runners[i].enable) runners[i].send();
    }

    lastPrint = millis(); // Update lastPrint time
  }

  // Receive command
  String cmd = comm.readLine();
  if (cmd.length() > 0) {
    switch (command_state) {
    case CommandState::StandBy:
      if (cmd == "j" || (cmd == "" && command == Command::Jog)) {
        command = Command::Jog;
        command_state = CommandState::Target;
        comm.log("Jog: Specify the runnner");
      }
      else if (cmd == "c") {
        comm.log("Calibrate");
        calibrateIMU();
      }
      else {
        comm.log("Wrong command");
        command_state = CommandState::StandBy;
      }
      break;
    case CommandState::Target:
      if (cmd == "x") {

      }
      else if (cmd == "") {
        command_state = CommandState::Value;
        comm.log("Input the value");
      }
      else {
        int number = cmd.toInt();
        if (0 <= number && number <= 5) {
          command_target = number;
          command_state = CommandState::Value;
          comm.log("Input the value");
        }
        else {
          comm.log("Wrong number");
          command_state = CommandState::StandBy;
        }
      }
      break;
    case CommandState::Value:
      if (cmd != "") command_value = cmd.toInt();
      switch (command) {
      case Command::Jog:
        String msg = "Jog #";
        msg.concat(String(command_target));
        msg.concat(" ");
        msg.concat(String(command_value));
        comm.log(msg);

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
  else comm.error('F', "FOT", "FLAMEOUT");
}


void calibrateIMU() {
  comm.log("Calibration started...");
  imu.calibrateMag(true);
  comm.log("Calculation finished");
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
