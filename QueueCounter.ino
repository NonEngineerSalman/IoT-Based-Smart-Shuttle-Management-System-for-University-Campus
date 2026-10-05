#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin Definitions
const int IR1_PIN = 2;        // Queue Sensor 1 (Entry side)
const int IR2_PIN = 3;        // Queue Sensor 2 (Exit side)
const int DRIVER_IR_PIN = 4;  // Driver Detection Sensor
const int LED_PIN = 6;        // Queue Full Indicator LED

// Sensor Detection Active Logic (LOW = Obstacle Detected)
const int SENSOR_ACTIVE = LOW;

// Queue Management
int queueCount = 0;
const int MAX_QUEUE = 3;

// Sequence Tracking State Machine
enum QueueState { IDLE, IN_ENTRY, IN_EXIT };
QueueState currentState = IDLE;

bool hitSecondSensor = false;
unsigned long stateStartTime = 0;
const unsigned long TIMEOUT_MS = 2500; // Reset state if sequence stalls > 2.5s

// Display State
bool oledIsOn = false;

void setup() {
  pinMode(IR1_PIN, INPUT);
  pinMode(IR2_PIN, INPUT);
  pinMode(DRIVER_IR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW); // Start with LED OFF

  // Initialize OLED (Default I2C address 0x3C)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;); // Halt execution if display hardware fails
  }

  display.clearDisplay();
  display.display();
}

void loop() {
  // Read current sensor states
  bool ir1 = (digitalRead(IR1_PIN) == SENSOR_ACTIVE);
  bool ir2 = (digitalRead(IR2_PIN) == SENSOR_ACTIVE);
  bool driverDetected = (digitalRead(DRIVER_IR_PIN) == SENSOR_ACTIVE);

  unsigned long currentMillis = millis();

  // 1. Queue Counter Logic (2-IR Sequential Logic)
  switch (currentState) {
    case IDLE:
      if (ir1 && !ir2) {
        currentState = IN_ENTRY;
        hitSecondSensor = false;
        stateStartTime = currentMillis;
      } 
      else if (ir2 && !ir1) {
        currentState = IN_EXIT;
        hitSecondSensor = false;
        stateStartTime = currentMillis;
      }
      break;

    case IN_ENTRY:
      if (ir2) hitSecondSensor = true;

      // Count entry once both sensors return to clear
      if (!ir1 && !ir2) {
        if (hitSecondSensor) {
          if (queueCount < MAX_QUEUE) {
            queueCount++;
          }
        }
        currentState = IDLE;
      }

      if (currentMillis - stateStartTime > TIMEOUT_MS) {
        currentState = IDLE;
      }
      break;

    case IN_EXIT:
      if (ir1) hitSecondSensor = true;

      // Count exit once both sensors return to clear
      if (!ir1 && !ir2) {
        if (hitSecondSensor) {
          if (queueCount > 0) {
            queueCount--;
          }
        }
        currentState = IDLE;
      }

      if (currentMillis - stateStartTime > TIMEOUT_MS) {
        currentState = IDLE;
      }
      break;
  }

  // 2. LED Control Logic: Lit ONLY when Queue is FULL (3/3)
  if (queueCount >= MAX_QUEUE) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  // 3. Driver Presence & OLED Display Control
  if (driverDetected) {
    renderDisplay();
    oledIsOn = true;
  } else {
    if (oledIsOn) {
      display.clearDisplay();
      display.display();
      oledIsOn = false;
    }
  }

  delay(20); // Stability debounce
}

void renderDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("DRIVER DISPLAY");

  // Queue Count
  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print("Queue: ");
  display.print(queueCount);
  display.print("/");
  display.print(MAX_QUEUE);

  // Status Text
  display.setCursor(0, 42);
  if (queueCount >= MAX_QUEUE) {
    display.println("Start Bus");
  } else {
    display.println("SPACE OK");
  }

  display.display();
}