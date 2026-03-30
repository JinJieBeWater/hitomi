#include <Arduino.h>
#include <esp_ota_ops.h>

constexpr int kBootKeyPin = 0;
constexpr unsigned long kSerialWaitTimeoutMs = 2000;
constexpr unsigned long kButtonPollDelayMs = 50;

bool gLastButtonPressed = false;

void waitForSerial() {
  unsigned long startedAt = millis();

  while (!Serial && millis() - startedAt < kSerialWaitTimeoutMs) {
    delay(10);
  }
}

void printRuntimeCheck() {
  const esp_partition_t* partition = esp_ota_get_running_partition();

  Serial.println();
  Serial.println("=== SZPI ESP32-S3 runtime check ===");
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
  Serial.printf("SDK version: %s\n", ESP.getSdkVersion());
  Serial.printf("CPU frequency: %lu MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));

#if ARDUINO_USB_CDC_ON_BOOT
  Serial.println("USB CDC on boot: enabled");
#else
  Serial.println("USB CDC on boot: disabled");
#endif

#if defined(BOARD_HAS_PSRAM)
  Serial.println("BOARD_HAS_PSRAM: defined");
#else
  Serial.println("BOARD_HAS_PSRAM: not defined");
#endif

  Serial.printf("PSRAM detected: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("PSRAM size: %u MB\n", ESP.getPsramSize() / (1024 * 1024));
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());

  if (partition != nullptr) {
    Serial.printf("Running partition: %s (%u bytes)\n", partition->label, partition->size);
  }

  Serial.println("===============================");
}

void printButtonState(bool pressed) {
  Serial.println(pressed ? "BOOT pressed" : "BOOT released");
}

void setup() {
  pinMode(kBootKeyPin, INPUT_PULLUP);

  Serial.begin(115200);
  waitForSerial();
  Serial.println("SZPI ESP32-S3 first flash test");
  Serial.println("BOOT key is on GPIO0.");
  printRuntimeCheck();

  gLastButtonPressed = digitalRead(kBootKeyPin) == LOW;
  printButtonState(gLastButtonPressed);
}

void loop() {
  bool buttonPressed = digitalRead(kBootKeyPin) == LOW;

  if (buttonPressed != gLastButtonPressed) {
    gLastButtonPressed = buttonPressed;
    printButtonState(buttonPressed);
  }

  delay(kButtonPollDelayMs);
}
