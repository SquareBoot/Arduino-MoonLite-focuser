/*
  Moonlite-compatible focuser controller

  ** Version 2.3 **
    Hand control support

  ** Version 2.2 **
    Bugs fixed, general changes.

  ** Version 2.1 **
    Added function to set pin value.

  ** Version 2.0 **
    Modified to be used with the cheaper DRV8825 (or A4988) driver and almost completely rewritten by SquareBoot

  ** Version 1.0 **
    Inspired by (http://orlygoingthirty.blogspot.co.nz/2014/04/arduino-based-motor-focuser-controller.html)
    Modified for INDI, easydriver by Cees Lensink
    Added sleep function by Daniel Franzén
*/

// Firmware version - 2.3
const String VERSION = "23";

// Configuration
#include "Config.h"

// AVR libraries
#if ENABLE_SW_RS == true
#include <avr/io.h>                                       // AVR library
#include <avr/wdt.h>                                      // Watchdog timer
#define reboot() wdt_enable(WDTO_30MS); while(1) {}       // To reset the board, call reboot();
#endif

#if ENABLE_HC == true
#include <ButtonDebounce.h>
#include <SevenSeg.h>
#endif

// Stepper driver libraries
// AccelStepper, used to provide motor acceleration
#include <AccelStepper.h>
#if STEPPER_TYPE == 0
#include <BasicStepperDriver.h>
#elif STEPPER_TYPE == 1
#include <DRV8825.h>
#elif STEPPER_TYPE == 2
#include <A4988.h>
#elif STEPPER_TYPE == 3
#include <DRV8834.h>
#endif

// The period to wait before turning off the driver (in milliseconds)
boolean isRunning = false;
#ifdef DRIVER_EN
#define TIMER_DELAY 30000
boolean isPowerOn = false;
#endif
#if STEPPER_TYPE != 0
boolean isHalfStep = false;
#endif

#ifdef DRIVER_EN
#if STEPPER_TYPE == 0
BasicStepperDriver driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP, DRIVER_EN);
#elif STEPPER_TYPE == 1
DRV8825 driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP, MODE0, MODE1, MODE2, DRIVER_EN);
#elif STEPPER_TYPE == 2
A4988 driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP, MODE0, MODE1, MODE2, DRIVER_EN);
#elif STEPPER_TYPE == 3
DRV8834 driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP, MODE0, MODE1, DRIVER_EN);
#endif
#else
#if STEPPER_TYPE == 0
BasicStepperDriver driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP);
#elif STEPPER_TYPE == 1
DRV8825 driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP, MODE0, MODE1, MODE2);
#elif STEPPER_TYPE == 2
A4988 driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP, MODE0, MODE1, MODE2);
#elif STEPPER_TYPE == 3
DRV8834 driver(STEPS_REV, DRIVER_DIR, DRIVER_STEP, MODE0, MODE1);
#endif
#endif

// ----- Motor control wrappers -----
// Forward step
void goForward() {
  int n = SINGLE_STEP;
#if STEPPER_TYPE != 0
  if (isHalfStep) {
    n *= HALF_STEP;

  } else {
    n *= FULL_STEP;
  }
#endif
  driver.move(n);
}
// Backward step
void goBackward() {
  int n = SINGLE_STEP;
#if STEPPER_TYPE != 0
  if (isHalfStep) {
    n *= HALF_STEP;

  } else {
    n *= FULL_STEP;
  }
#endif
  driver.move(-n);
}
#if REVERSE_DIR == false
AccelStepper stepper(goForward, goBackward);
#else
AccelStepper stepper(goBackward, goForward);
#endif

#define CMD_LENGHT 8
// Used to interpret the MoonLite protocol
char inChar;
char cmd[CMD_LENGHT];
char param[CMD_LENGHT];
char line[CMD_LENGHT];
boolean eoc = false;
int idx = 0;
long timerStartTime = 0;
char tempString[10];

int customPins[] = CUSTOMIZABLE_PINS;
#define BLINK_PERIOD 300
boolean ledState = false;
long blinkStartTime = 0;

#if ENABLE_HC == true
#define BUTTONS_DEBOUNCE 60
ButtonDebounce buttonUp(BUTTON_UP, BUTTONS_DEBOUNCE);
ButtonDebounce buttonDown(BUTTON_DOWN, BUTTONS_DEBOUNCE);
ButtonDebounce buttonMode(BUTTON_MODE, BUTTONS_DEBOUNCE);
int hcSpeed = 1;
boolean hcMode = true;
SevenSeg hcDisp(SEVEN_SEGMENT_DISP);
long lastScreenBlink = 0;
boolean screenBlinkState = true;

void buttonModeChanged(int state) {
  if (state == 1) {
    if (hcMode) {
      hcDisp.writeDigit(hcSpeed);
      hcMode = false;

    } else {
      hcMode = true;
    }
  }
}

void buttonUpChanged(int state) {
  if (state == 1) {
    if (hcMode) {
      if (hcSpeed < 6) {
        hcSpeed++;
        hcDisp.writeDigit(hcSpeed);
      }

    } else {
      //TODO: should we tell INDI that the position is changed by the HC?
      stepper.move(hcSpeedToSteps());
    }
  }
}

void buttonDownChanged(int state) {
  if (state == 1) {
    if (hcMode) {
      if (hcSpeed > 1) {
        hcSpeed--;
        hcDisp.writeDigit(hcSpeed);
      }

    } else {
      //TODO: should we tell INDI that the position is changed by the HC?
      stepper.move(-hcSpeedToSteps());
    }
  }
}
#endif

void setup() {
  // Serial connection
  Serial.begin(SERIAL_SPEED);
  // Status LED
  pinMode(LED, OUTPUT);
  // Customizable pins
  for (int i = 0; i < (sizeof(customPins) / sizeof(*customPins)); i++) {
    pinMode(customPins[i], OUTPUT);
  }
  // Polar finder light
#if ENABLE_POLAR_LIGHT == true
  pinMode(POLAR_LIGHT_LED, OUTPUT);
#endif

#if ENABLE_HC == true
  hcDisp.setCommonCathode();
  hcDisp.changeDigit(0);
  hcDisp.writeDigit(hcSpeed);
  buttonMode.setCallback(buttonModeChanged);
  buttonUp.setCallback(buttonUpChanged);
  buttonDown.setCallback(buttonDownChanged);
#endif

  // ----- Motor driver -----
  // Ignore Moonlite speed
  driver.setRPM(MOTOR_RPM);
  //stepper.setSpeed(MOTOR_PPS);
  //stepper.setMaxSpeed(MOTOR_PPS);
  stepper.setAcceleration(MOTOR_ACCEL);
#if STEPPER_TYPE != 0
  driver.setMicrostep(FULL_STEP);
#endif
  turnOff();

  blinkStartTime = millis();
  memset(line, 0, CMD_LENGHT);
}

void loop() {
  // Only have to do this if stepper is on
  if (isRunning) {
    stepper.run();
    if (stepper.distanceToGo() == 0) {
      // Start timer to decide when to power off the board
      timerStartTime = millis();
      isRunning = false;
    }
  }
#ifdef DRIVER_EN
  else if (isPowerOn) {
    // Turn power off if active time period has passed
    if (millis() - timerStartTime >= TIMER_DELAY) {
      turnOff();
    }
  }
#endif
  else {
    unsigned long currentMillis = millis();
    if (currentMillis - blinkStartTime >= BLINK_PERIOD) {
      blinkStartTime = currentMillis;
      ledState = !ledState;
      digitalWrite(LED, ledState);
    }
  }

#if ENABLE_POLAR_LIGHT == true
  int val = analogRead(POT_ANALOG_PIN);
  if (val > 20) {
    analogWrite(POLAR_LIGHT_LED, map(val, 20, 1023, 0, 250));

  } else {
    analogWrite(POLAR_LIGHT_LED, 0);
  }
#endif

#if ENABLE_HC == true
  buttonMode.update();
  buttonUp.update();
  buttonDown.update();
  if (hcMode == false) {
    if (millis() - lastScreenBlink > 300) {
      lastScreenBlink = millis();
      screenBlinkState = !screenBlinkState;
      if (screenBlinkState) {
        hcDisp.clearDisp();

      } else {
        hcDisp.writeDigit(hcSpeed);
      }
    }
  }
#endif

  // Read the command until the terminating # character
  while (Serial.available() && !eoc) {
    inChar = Serial.read();
    if (inChar != '#' && inChar != ':') {
      line[idx++] = inChar;
      if (idx >= CMD_LENGHT) {
        idx = CMD_LENGHT - 1;
      }

    } else {
      if (inChar == '#') {
        eoc = true;
      }
    }
  }

  // We may not have a complete command set but there is no character coming in for now and mightas well loop in case stepper needs updating
  // eoc will flag if a full command is there to act upon
  // Process the command we got
  if (eoc) {
    memset(cmd, 0, CMD_LENGHT);
    memset(param, 0, CMD_LENGHT);

    // Set the value of a pin
    if ((line[0] == 'A') && (line[1] == 'V')) {
      String pin = "";
      pin.concat(line[2]);
      pin.concat(line[3]);
      String value = "";
      value.concat(line[4]);
      value.concat(line[5]);
      value.concat(line[6]);
      int p = pin.toInt();
      for (int i = 0; i < (sizeof(customPins) / sizeof(*customPins)); i++) {
        if (customPins[i] == p) {
          analogWrite(p, value.toInt());
          break;
        }
      }
    }

    int len = strlen(line);
    if (len >= 2) {
      strncpy(cmd, line, 2);
    }
    if (len > 2) {
      strncpy(param, line + 2, len - 2);
    }

    memset(line, 0, CMD_LENGHT);
    eoc = false;
    idx = 0;

    // Execute the command

    if (!strcasecmp(cmd, "RS")) {
#if ENABLE_SW_RS == true
      reboot();
#else
      for (int i = 0; i < (sizeof(customPins) / sizeof(*customPins)); i++) {
        analogWrite(customPins[i], 0);
      }
#endif
    }

    // Immediately stop any focus motor movement. Returns nothing
    if (!strcasecmp(cmd, "FQ")) {
      if (!isRunning) {
        turnOn();
      }
      // Stop as fast as possible
      stepper.stop();
      // Blocks until the target position is reached and stopped
      stepper.runToPosition();
    }

    // Go to the new position as set by the ":SNYYYY#" command. Returns nothing.
    // Turn stepper on and flag it is running
    // is this the only command that should actually make the stepper run ?
    if (!strcasecmp(cmd, "FG")) {
      if (!isRunning) {
        turnOn();
      }
    }

    // Returns the temperature coefficient where XX is a two-digit signed (2’s complement)  number.
    if (!strcasecmp(cmd, "GC")) {
      Serial.print("02#");
    }

    // Returns the current stepping delay where XX is a two-digit unsigned  number. See the :SD# command for a list of possible return values.
    // Might turn this into AccelStepper acceleration at some point
    if (!strcasecmp(cmd, "GD")) {
      Serial.print("02#");
    }

    // Returns "FF#" if the focus motor is half-stepped otherwise return "00#"
    if (!strcasecmp(cmd, "GH")) {
#if STEPPER_TYPE != 0
      if (isHalfStep) {
        Serial.print("FF#");

      } else {
        Serial.print("00#");
      }
#else
      Serial.print("00#");
#endif
    }

    // Returns "00#" if the focus motor is not moving, otherwise return "01#",
    // AccelStepper returns Positive as clockwise
    if (!strcasecmp(cmd, "GI")) {
      if (stepper.distanceToGo() == 0) {
        Serial.print("00#");

      } else {
        Serial.print("01#");
      }
    }

    // Returns the new position previously set by a ":SNYYYY" command where YYYY is a four-digit unsigned hex number.
    if (!strcasecmp(cmd, "GN")) {
      sprintf(tempString, "%04X", stepper.targetPosition());
      Serial.print(tempString);
      Serial.print("#");
    }

    // Returns the current position where YYYY is a four-digit unsigned hex number.
    if (!strcasecmp(cmd, "GP")) {
      sprintf(tempString, "%04X", stepper.currentPosition());
      Serial.print(tempString);
      Serial.print("#");
    }

    // Returns the current temperature where YYYY is a four-digit signed (2’s complement) hex number.
    if (!strcasecmp(cmd, "GT")) {
      Serial.print("0020#");
    }

    // Get the version of the firmware as a two-digit decimal number where the first digit is the major version number, and the second digit is the minor version number.
    if (!strcasecmp(cmd, "GV")) {
      Serial.print(VERSION + '#');
    }

    // Set the new temperature coefficient where XX is a two-digit, signed (2’s complement) hex number.
    if (!strcasecmp(cmd, "SC")) {

    }

    // Set the new stepping delay where XX is a two-digit,unsigned hex number.
    if (!strcasecmp(cmd, "SD")) {

    }

    // Set full-step mode.
#if STEPPER_TYPE != 0
    if (!strcasecmp(cmd, "SF")) {
      driver.setMicrostep(FULL_STEP);
      isHalfStep = false;
    }
#endif

    // Set half-step mode.
#if STEPPER_TYPE != 0
    if (!strcasecmp(cmd, "SH")) {
      driver.setMicrostep(HALF_STEP);
      isHalfStep = true;
    }
#endif

    // Set the new position where YYYY is a four-digit
    if (!strcasecmp(cmd, "SN")) {
      // stepper.enableOutputs(); // Turn the motor on here?
      if (!isRunning) {
        turnOn();
      }
      stepper.moveTo(hexToLong(param));
    }

    // Set the current position, where YYYY is a four-digit unsigned hex number.
    if (!strcasecmp(cmd, "SP")) {
      stepper.setCurrentPosition(hexToLong(param));
    }
  }
}

int hcSpeedToSteps() {
  switch (hcSpeed) {
  case 1:
    return 1;
  case 2:
    return 5;
  case 3:
    return 10;
  case 4:
    return 50;
  case 5:
    return 100;
  case 6:
    return 500;
}
}

long hexToLong(char *line) {
  return strtol(line, NULL, 16);
}

void turnOn() {
#ifdef DRIVER_EN
  driver.enable();
  isPowerOn = true;
#endif
  digitalWrite(LED, HIGH);
  isRunning = true;
}

void turnOff() {
#ifdef DRIVER_EN
  driver.disable();
  isPowerOn = false;
#endif
  isRunning = false;
}
