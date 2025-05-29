#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <SD.h>
#include <SPI.h>

#define SD_NOT_INSERTED 0
#define SD_WRONG_OR_ABSENT_SECRET 1
#define SD_VALID_SECRET 2

const int chipSelect = 10;
const char* filename = "secret42.bin";
const char* valid_message = "Strawberry";

char decrypted[16];
VL53L0X sensor;

bool alarm = false;
const int transistorPin = 7; // D7


bool checkSDCardInserted() {
  return SD.begin(chipSelect);
}

void generateRandomPad(uint8_t* pad, size_t len) {
  for (size_t i = 0; i < len; i++) {
    pad[i] = random(0, 256);  // Basic entropy
  }
}

void xorData(const uint8_t* a, const uint8_t* b, char* result, size_t len) {
  for (size_t i = 0; i < len; i++) {
    result[i] = a[i] ^ b[i];
  }
}

void encrypt_and_save() {
  uint8_t pad[16] = {0};
  char cipher[16] = {0};

  generateRandomPad(pad, 16);
  xorData(pad, (const uint8_t*)valid_message, cipher, 16);

  File file = SD.open(filename, FILE_WRITE);
  if (file) {
    file.write(pad, 16);
    file.write(cipher, 16);
    file.close();
    // Serial.println("OTP encryption and storage done.");
  } else {
    // Serial.println("Failed to write OTP data.");
  }
}

int decrypt_and_check() {
  if (!checkSDCardInserted()) return SD_NOT_INSERTED;

  File file = SD.open(filename, FILE_READ);
  if (!file || file.size() != 32) return SD_WRONG_OR_ABSENT_SECRET;

  uint8_t pad[16] = {0};
  uint8_t cipher[16] = {0};

  file.read(pad, 16);
  file.read(cipher, 16);
  file.close();

  xorData(pad, cipher, decrypted, 16);
  decrypted[15] = '\0';

  if (strcmp(decrypted, valid_message) == 0) {
    return SD_VALID_SECRET;
  } else {
    return SD_WRONG_OR_ABSENT_SECRET;
  }
}

void setup() {
  // Serial.begin(9600);
  Wire.begin();
  pinMode(transistorPin, OUTPUT);

  sensor.setTimeout(500);
  if (!sensor.init()) {
    // Serial.println("Failed to detect and initialize VL53L0X!");
    while (1);
  }


  if (checkSDCardInserted()) {
    // Serial.println("SD card detected.");
    if (!SD.exists(filename)) {
      // Serial.println("Creating OTP secret file...");
      encrypt_and_save();
    }
  } else {
    // Serial.println("No SD card at startup.");
  }
  randomSeed(42); // Seed
}


int readDistance() {
  int distance = sensor.readRangeSingleMillimeters();

  // Serial.print("Distance: ");
  // Serial.print(distance);
  // Serial.println(" mm");

  if (sensor.timeoutOccurred()) {
    // Serial.println("Sensor timeout!");
    return -1;
  }

  return distance;
}

void loop() {
  if (alarm || readDistance() > 60) {
    switch (decrypt_and_check()) {
    case SD_NOT_INSERTED:
      // Serial.println("SD not inserted.");
      alarm = true;
      break;
    case SD_WRONG_OR_ABSENT_SECRET:
      // Serial.println("Wrong or missing secret.");
      alarm = true;
      break;
    case SD_VALID_SECRET:
      // Serial.println("Valid secret verified!");
      alarm = false;
      break;
    }
  }
  
  if(alarm) {
    digitalWrite(transistorPin, HIGH);
    // Serial.println("ALARM ON");
  } else {
    digitalWrite(transistorPin, LOW);
  }
  delay(500);
}
