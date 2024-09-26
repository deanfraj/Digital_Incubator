#include <Wire.h>
#include <Grove_I2C_Motor_Driver.h>
#include <rgb_lcd.h>
#include <AHT20.h>
#include <DS1307.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32RotaryEncoder.h>

#define RELAY 18 //A0
#define STEPPER_MOTOR_I2C_ADDRESS 0x0f
#define PIN           16 //A2
#define NUMPIXELS     1
#define SELECTEDPIXEL 0
#define BUTTON 14
#define BUZZER 9

const uint8_t DI_ENCODER_A   = 39;
const uint8_t DI_ENCODER_B   = 38;

RotaryEncoder rotaryEncoder(DI_ENCODER_A, DI_ENCODER_B);

float setpoint = 37.7;
float hysteresis = 0.1;
bool stepperMotorOn = true;
float humidity, temp;
int ret;
int screenPage = 1;
volatile unsigned long buttonTime = 0;
volatile unsigned long lastButtonTime = 0;
int dayCounter = 0;
bool countOff = false;
bool alarmOff = false;

DS1307 rtc;
AHT20 aht;
rgb_lcd lcd;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  rotaryEncoder.setEncoderType(EncoderType::HAS_PULLUP );
  rotaryEncoder.setBoundaries(1, 2, false);
  rotaryEncoder.onTurned(&knobCallback);
  rotaryEncoder.begin();
  Wire.begin();
  Motor.begin(STEPPER_MOTOR_I2C_ADDRESS);
  pinMode(RELAY, OUTPUT);
  pinMode(BUTTON, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, RISING);

  lcd.begin(16, 2);

  pixels.begin();

  aht.begin();

  rtc.begin();
  rtc.fillByYMD(2024, 9, 3); // 19.July.2024
  rtc.fillByHMS(17, 59, 30); // 21:10:30
  rtc.setTime();//write time to the RTC chip

  pixels.setBrightness(255);
  pixels.setPixelColor(SELECTEDPIXEL, pixels.Color(255, 0, 0));
  pixels.show();

  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Digital");
  lcd.setCursor(3, 1);
  lcd.print("Incubator :)");

  tone(BUZZER, 500, 500);
  delay(1000);
}

void loop() {
  printData();
  countingDays();
  if (dayCounter == 21) {
    turnOffEverythingAndSoundTheAlarm();
  } else {
    regulateTemperature();
    activateStepperMotor();
  }
}

void printData() {
  ret = aht.getSensor(&humidity, &temp);

  if (screenPage == 1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temp, 1);
    lcd.print((char)223);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Humi: ");
    lcd.print((int)(humidity * 100));
    lcd.print("%");
    delay(500);

  } else if (screenPage == 2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp setpoint: ");
    lcd.setCursor(0, 1);
    lcd.print(setpoint, 1);
    lcd.print((char)223);
    lcd.print("C");
  }
}

void regulateTemperature() {
  if (temp > (setpoint + hysteresis)) {
    digitalWrite(RELAY, LOW);
    pixels.setPixelColor(SELECTEDPIXEL, pixels.Color(0, 0, 255));
    pixels.show();
  } else if (temp < (setpoint - hysteresis)) {
    digitalWrite(RELAY, HIGH);
    pixels.setPixelColor(SELECTEDPIXEL, pixels.Color(255, 255, 0));
    pixels.show();
  }
}

void activateStepperMotor() {
  rtc.getTime();
  if ((rtc.hour == 2) || (rtc.hour == 10) || (rtc.hour == 18) && stepperMotorOn) {
    Motor.StepperRun(256, 0, 0);
    stepperMotorOn = false;
  }
  if ((rtc.hour == 1) || (rtc.hour == 9) || (rtc.hour == 17) && !stepperMotorOn) {
    stepperMotorOn = true;
  }
}

void countingDays() {
  rtc.getTime();
  if ((rtc.hour == 18) && countOff) {
    dayCounter++;
    countOff = false;
  }
  if ((rtc.hour == 17) && !countOff) {
    countOff = true;
  }
}

void turnOffEverythingAndSoundTheAlarm() {
  digitalWrite(RELAY, LOW);
  Motor.StepperRun(0, 0, 0);
  if (!alarmOff) {
    tone(BUZZER, 1000, 500);
    tone(BUZZER, 2000, 500);
    tone(BUZZER, 1000, 500);
    tone(BUZZER, 2000, 500);
  }
}

void buttonPressed() {
  buttonTime = millis();
  if (buttonTime - lastButtonTime > 200) {
    screenPage++;
    if (screenPage == 3) {
      screenPage = 1;
    }
    if ((dayCounter == 21) && !alarmOff) {
      alarmOff = true;
    }
    lastButtonTime = buttonTime;
  }
}

void knobCallback(long value) {
  if (screenPage == 2) {
    if (value == 2) {
      setpoint += 0.1;
    } else if (value == 1) {
      setpoint -= 0.1;
    }
  }
  if (setpoint > 45) setpoint = 45;
  if (setpoint < 30) setpoint = 30;
}
