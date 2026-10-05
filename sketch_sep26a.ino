#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ESP32Servo.h>

// --- Wi-Fi Credentials ---
const char* ssid = "Baba Yaga";
const char* password = "*#BabaYaga#*";

// --- Pin Definitions (ESP32 DevKit) ---
#define DHTPIN 4          // GPIO 4 (D4) - DHT11 Data Pin
#define DHTTYPE DHT11
#define MQ2_PIN 34        // GPIO 34 (D34) - MQ-2 Analog Out (AO)
#define SERVO_PIN 18      // GPIO 18 (D18) - Servo Signal Pin
#define LED_PIN 2         // GPIO 2 (D2) - Warning LED Pin

// --- Thresholds ---
const float TEMP_THRESHOLD = 25.0;  // Temperature limit in Celsius
const int GAS_THRESHOLD = 1500;     // MQ-2 threshold (ESP32 ADC range: 0 - 4095)

DHT dht(DHTPIN, DHTTYPE);
Servo myServo;
WebServer server(80);

// Sensor variables
float temperature = 0.0;
float humidity = 0.0;
int gasLevel = 0;

// Timers and Servo Sweep Control
unsigned long lastSensorRead = 0;
unsigned long lastServoMove = 0;
int servoAngle = 0;
int servoDirection = 1;             // 1 = moving right, -1 = moving left
const int SERVO_SWEEP_SPEED = 15;   // Milliseconds per 1 degree movement (lower = faster fan sweep)
const int SERVO_MIN_ANGLE = 0;      // Fan oscillation min angle
const int SERVO_MAX_ANGLE = 140;    // Fan oscillation max angle

// --- Embedded Minimal Dark UI ---
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>System Dashboard</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background-color: #0b0f19; color: #f1f5f9; min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }
    .container { width: 100%; max-width: 850px; }
    .header { margin-bottom: 28px; }
    .header h1 { font-size: 1.5rem; font-weight: 600; color: #f8fafc; letter-spacing: -0.02em; }
    .header p { color: #64748b; font-size: 0.875rem; margin-top: 4px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 16px; }
    .card { background: #131b2e; border: 1px solid #1e293b; border-radius: 16px; padding: 24px; transition: transform 0.2s ease, border-color 0.2s ease; }
    .card:hover { border-color: #334155; transform: translateY(-2px); }
    .card-label { font-size: 0.75rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.08em; color: #64748b; margin-bottom: 12px; }
    .card-value { font-size: 2.5rem; font-weight: 700; color: #f8fafc; line-height: 1; }
    .unit { font-size: 1.125rem; font-weight: 400; color: #64748b; margin-left: 2px; }
    .badge { display: inline-flex; align-items: center; gap: 8px; margin-top: 16px; padding: 6px 12px; border-radius: 9999px; font-size: 0.75rem; font-weight: 600; }
    .badge-ok { background: rgba(16, 185, 129, 0.1); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.2); }
    .badge-active { background: rgba(59, 130, 246, 0.15); color: #60a5fa; border: 1px solid rgba(59, 130, 246, 0.3); }
    .badge-danger { background: rgba(239, 68, 68, 0.1); color: #ef4444; border: 1px solid rgba(239, 68, 68, 0.2); }
    .dot { width: 6px; height: 6px; border-radius: 50%; background-color: currentColor; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>Environment Dashboard</h1>
      <p>Real-time telemetry and automated controls</p>
    </div>
    <div class="grid">
      <div class="card">
        <div class="card-label">Temperature</div>
        <div class="card-value"><span id="temp">--</span><span class="unit">&deg;C</span></div>
        <div id="fan-badge" class="badge badge-ok"><span class="dot"></span><span id="fan-state">FAN IS OFF</span></div>
      </div>
      <div class="card">
        <div class="card-label">Humidity</div>
        <div class="card-value"><span id="hum">--</span><span class="unit">%</span></div>
        <div class="badge badge-ok" style="visibility: hidden;"><span class="dot"></span><span>OK</span></div>
      </div>
      <div class="card">
        <div class="card-label">Gas Concentration</div>
        <div class="card-value"><span id="gas">--</span><span class="unit">ADC</span></div>
        <div id="gas-badge" class="badge badge-ok"><span class="dot"></span><span id="gas-state">AIR SAFE</span></div>
      </div>
    </div>
  </div>

  <script>
    async function updateDashboard() {
      try {
        const res = await fetch('/data');
        const data = await res.json();
        
        document.getElementById('temp').innerText = data.temp.toFixed(1);
        document.getElementById('hum').innerText = data.hum.toFixed(1);
        document.getElementById('gas').innerText = data.gas;

        // Update Fan State Badge
        const fanBadge = document.getElementById('fan-badge');
        const fanState = document.getElementById('fan-state');
        if (data.fan) {
          fanBadge.className = 'badge badge-active';
          fanState.innerText = 'FAN IS ON';
        } else {
          fanBadge.className = 'badge badge-ok';
          fanState.innerText = 'FAN IS OFF';
        }

        // Update Gas State Badge
        const gasBadge = document.getElementById('gas-badge');
        const gasState = document.getElementById('gas-state');
        if (data.gasAlert) {
          gasBadge.className = 'badge badge-danger';
          gasState.innerText = 'GAS DANGER (LED ON)';
        } else {
          gasBadge.className = 'badge badge-ok';
          gasState.innerText = 'AIR SAFE (LED OFF)';
        }
      } catch (err) {
        console.error('Failed to fetch data:', err);
      }
    }

    // Refresh telemetry every 1.5 seconds asynchronously
    setInterval(updateDashboard, 1500);
    updateDashboard();
  </script>
</body>
</html>
)rawliteral";

// --- Web Server Handlers ---
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleData() {
  bool isFanOn = (temperature > TEMP_THRESHOLD);
  bool isGasAlert = (gasLevel > GAS_THRESHOLD);

  String json = "{";
  json += "\"temp\":" + String(temperature, 1) + ",";
  json += "\"hum\":" + String(humidity, 1) + ",";
  json += "\"gas\":" + String(gasLevel) + ",";
  json += "\"fan\":" + String(isFanOn ? "true" : "false") + ",";
  json += "\"gasAlert\":" + String(isGasAlert ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  // Pin setup
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Peripheral initialization
  dht.begin();
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // Wi-Fi Connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
  Serial.print("Dashboard URL: http://");
  Serial.println(WiFi.localIP());

  // Web routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();

  // 1. Read sensors every 2000ms
  if (currentMillis - lastSensorRead >= 2000) {
    lastSensorRead = currentMillis;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int g = analogRead(MQ2_PIN);

    if (!isnan(t)) temperature = t;
    if (!isnan(h)) humidity = h;
    gasLevel = g;

    // Gas Alert Logic (LED ON)
    if (gasLevel > GAS_THRESHOLD) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  }

  // 2. Non-blocking Servo Fan Oscillating Logic (> 25°C)
  if (temperature > TEMP_THRESHOLD) {
    if (currentMillis - lastServoMove >= SERVO_SWEEP_SPEED) {
      lastServoMove = currentMillis;

      servoAngle += servoDirection;
      if (servoAngle >= SERVO_MAX_ANGLE) {
        servoAngle = SERVO_MAX_ANGLE;
        servoDirection = -1; // Reverse sweep direction
      } else if (servoAngle <= SERVO_MIN_ANGLE) {
        servoAngle = SERVO_MIN_ANGLE;
        servoDirection = 1;  // Forward sweep direction
      }

      myServo.write(servoAngle);
    }
  } else {
    // Return servo to home position when temperature drops below threshold
    if (servoAngle != 0) {
      servoAngle = 0;
      myServo.write(0);
    }
  }
}