// ----- General config -----
// Status LED pin
#define LED 13
#define SERIAL_SPEED 115200
#define ENABLE_SW_RS false

// ----- Stepper driver configuration -----
// Motor driver type: BasicStepperDriver (0), DRV8825 (1), A4988 (2) or DRV8834 (3)
#define STEPPER_TYPE 1
// Steps per revolution
#define STEPS_REV 200
#define SINGLE_STEP 1
// Ignored for BasicStepperDriver. Can be 1, 2, 4, 8, 16 or 32
#define FULL_STEP 16
// Ignored for BasicStepperDriver. Can be 1, 2, 4, 8, 16 or 32
#define HALF_STEP 32
// Reverses the direction of the motor
#define REVERSE_DIR false
// RPM. Decreasing this value should decrease telescope shaking.
#define MOTOR_RPM 120
// Acceleration
#define MOTOR_ACCEL 1300
// Motor pulses per second (pps). See full formula here: https://electronics.stackexchange.com/a/232679
// #define MOTOR_PPS (MOTOR_RPM * STEPS_REV / 60)
// ----- Driver pins -----
// Driver DIR pin
#define DRIVER_DIR 2
// Driver STEP pin
#define DRIVER_STEP 3
// Driver ENABLE pin. Comment to disable functionality
//#define DRIVER_EN 4
// M0. Only for microstepping-enabled drivers
#define MODE0 7
// M1. Only for microstepping-enabled drivers
#define MODE1 8
// M2. Only for microstepping-enabled drivers
#define MODE2 11

// ----- Polar finder light -----
#define ENABLE_POLAR_LIGHT false
#define POT_ANALOG_PIN 0
#define POLAR_LIGHT_LED 10

// ----- Customizable pins -----
// List of pins that can be modified by the serial port
#define CUSTOMIZABLE_PINS {}
