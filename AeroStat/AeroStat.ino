/*
 * AeroStat — Temperature-Controlled Smart Fan
 *
 * Hardware:
 *   - ESP32 microcontroller
 *   - LM35 temperature sensor  → LM358 op-amp (voltage follower) → GPIO 34 (ADC)
 *   - DHT11 sensor             → GPIO 4  (temperature + humidity)
 *   - P-channel MOSFET gate    → GPIO 26 (DAC)  → 12 V DC fan
 *
 * Pin summary:
 *   GPIO  4  – DHT11 data
 *   GPIO 26  – DAC output → MOSFET gate (low ≈ fan ON, high ≈ fan OFF)
 *   GPIO 34  – ADC input  ← LM358 buffered LM35 signal
 *
 * Dependencies (install via Arduino Library Manager):
 *   DHT sensor library by Adafruit  ≥ 1.4.4
 *   Adafruit Unified Sensor          ≥ 1.1.9
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ---------- PIN DEFINITIONS ----------
#define DHTPIN    4    // DHT11 data pin
#define DHTTYPE   DHT11
#define LM35_PIN  34   // ADC input – LM35 via LM358 buffer
#define FAN_PIN   26   // DAC output to P-channel MOSFET gate

// ---------- WIFI CREDENTIALS ----------
const char* ssid     = "YOUR_WIFI_NAME";      // <-- CHANGE THIS
const char* password = "YOUR_WIFI_PASSWORD";  // <-- CHANGE THIS

// ---------- OBJECTS ----------
WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);

// ---------- STATE ----------
float threshold  = 40.0;   // °C – fan turns ON above this
bool  autoMode   = true;
bool  fanState   = false;

// Hysteresis: fan turns ON at (threshold + HYST), OFF at (threshold - HYST).
// Prevents rapid cycling when temperature hovers near the threshold.
static const float HYST = 0.5;

// ---------- SENSOR HELPERS ----------

// Read LM35 temperature (°C) via LM358-buffered ADC on GPIO 34.
// LM35 output: 10 mV/°C.  ESP32 ADC reference: 3300 mV, 12-bit (0-4095).
float readLM35() {
  // Average 10 samples to reduce ADC noise
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(LM35_PIN);
    delay(2);
  }
  float millivolts = (sum / 10.0) * (3300.0 / 4095.0);
  return millivolts / 10.0;   // 10 mV per °C
}

// ---------- HTML PAGE ----------
String getPage(float lm35Temp, float dhtTemp, float humidity) {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AeroStat</title>
<link href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=DM+Sans:wght@300;400;600&display=swap" rel="stylesheet">
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  :root {
    --bg:         #0d0f14;
    --surface:    #161a23;
    --surface2:   #1e2330;
    --border:     rgba(255,255,255,0.07);
    --text:       #eaeef8;
    --muted:      #6b7280;
    --accent:     #38bdf8;
    --accent-dim: rgba(56,189,248,0.12);
    --on:         #34d399;
    --on-dim:     rgba(52,211,153,0.12);
    --off:        #f87171;
    --off-dim:    rgba(248,113,113,0.12);
    --warn:       #fbbf24;
  }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'DM Sans', sans-serif;
    min-height: 100vh;
    padding: 0 0 40px;
  }

  /* Header */
  .header {
    background: var(--surface);
    border-bottom: 1px solid var(--border);
    padding: 18px 24px;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .header-title {
    font-family: 'Space Mono', monospace;
    font-size: 15px;
    letter-spacing: 0.08em;
    color: var(--accent);
  }
  .header-right {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .live-dot {
    width: 7px; height: 7px;
    border-radius: 50%;
    background: var(--on);
    animation: pulse 2s infinite;
  }
  @keyframes pulse {
    0%,100% { opacity: 1; transform: scale(1); }
    50%      { opacity: 0.4; transform: scale(0.8); }
  }
  .live-label {
    font-size: 11px;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    letter-spacing: 0.05em;
  }

  /* Content */
  .content { max-width: 480px; margin: 0 auto; padding: 24px 16px 0; }

  /* Big temp display */
  .temp-hero {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 20px;
    padding: 32px 24px;
    text-align: center;
    margin-bottom: 14px;
    position: relative;
    overflow: hidden;
  }
  .temp-hero::before {
    content: '';
    position: absolute;
    top: -40px; right: -40px;
    width: 160px; height: 160px;
    border-radius: 50%;
    background: var(--accent-dim);
    filter: blur(40px);
    pointer-events: none;
  }
  .temp-label {
    font-size: 11px;
    letter-spacing: 0.12em;
    color: var(--muted);
    text-transform: uppercase;
    font-family: 'Space Mono', monospace;
    margin-bottom: 8px;
  }
  .temp-value {
    font-family: 'Space Mono', monospace;
    font-size: 72px;
    font-weight: 700;
    line-height: 1;
    color: var(--text);
    letter-spacing: -2px;
  }
  .temp-unit {
    font-size: 28px;
    color: var(--muted);
    vertical-align: super;
    margin-left: 2px;
  }
  .temp-sub {
    margin-top: 14px;
    font-size: 13px;
    color: var(--muted);
  }
  .temp-sub span { color: var(--accent); font-weight: 600; }

  /* Stats row */
  .stats-row {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    gap: 10px;
    margin-bottom: 14px;
  }
  .stat-card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 16px 14px;
  }
  .stat-card-label {
    font-size: 11px;
    letter-spacing: 0.1em;
    text-transform: uppercase;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    margin-bottom: 8px;
  }
  .stat-card-value {
    font-family: 'Space Mono', monospace;
    font-size: 22px;
    font-weight: 700;
    line-height: 1;
  }
  .stat-card-value.fan-on   { color: var(--on); }
  .stat-card-value.fan-off  { color: var(--off); }
  .stat-card-value.humidity { color: var(--accent); }
  .stat-card-value.dht-temp { color: var(--warn); }

  /* Section card */
  .card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 20px;
    margin-bottom: 12px;
  }
  .card-title {
    font-size: 11px;
    letter-spacing: 0.1em;
    text-transform: uppercase;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    margin-bottom: 16px;
  }

  /* Mode toggle */
  .mode-row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .mode-btn {
    display: block;
    padding: 12px;
    border-radius: 10px;
    border: 1px solid var(--border);
    background: var(--surface2);
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    font-size: 12px;
    letter-spacing: 0.06em;
    text-align: center;
    text-decoration: none;
    cursor: pointer;
    transition: all 0.15s;
  }
  .mode-btn:hover { border-color: var(--accent); color: var(--accent); }
  .mode-btn.active-auto {
    background: var(--accent-dim);
    border-color: var(--accent);
    color: var(--accent);
  }
  .mode-btn.active-manual {
    background: rgba(251,191,36,0.1);
    border-color: var(--warn);
    color: var(--warn);
  }

  /* Slider */
  .threshold-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 12px;
  }
  .threshold-num {
    font-family: 'Space Mono', monospace;
    font-size: 22px;
    font-weight: 700;
    color: var(--warn);
  }
  input[type=range] {
    -webkit-appearance: none;
    width: 100%;
    height: 4px;
    border-radius: 2px;
    background: var(--surface2);
    outline: none;
    border: 1px solid var(--border);
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 20px; height: 20px;
    border-radius: 50%;
    background: var(--warn);
    cursor: pointer;
    border: 2px solid var(--bg);
    box-shadow: 0 0 0 3px rgba(251,191,36,0.2);
  }

  /* Manual controls */
  .ctrl-row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .ctrl-btn {
    display: block;
    padding: 14px;
    border-radius: 12px;
    border: 1px solid var(--border);
    font-family: 'Space Mono', monospace;
    font-size: 13px;
    letter-spacing: 0.05em;
    text-align: center;
    text-decoration: none;
    cursor: pointer;
    transition: all 0.15s;
    font-weight: 700;
  }
  .ctrl-btn.fan-on-btn {
    background: var(--on-dim);
    border-color: var(--on);
    color: var(--on);
  }
  .ctrl-btn.fan-off-btn {
    background: var(--off-dim);
    border-color: var(--off);
    color: var(--off);
  }
  .ctrl-btn:hover  { filter: brightness(1.2); transform: translateY(-1px); }
  .ctrl-btn:active { transform: translateY(0); }

  /* Refresh timer bar */
  .refresh-bar-wrap {
    margin-top: 24px;
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .refresh-label { font-size: 11px; color: var(--muted); font-family: 'Space Mono', monospace; }
  .refresh-bar-track {
    flex: 1;
    height: 3px;
    background: var(--surface2);
    border-radius: 2px;
    overflow: hidden;
  }
  .refresh-bar-fill {
    height: 100%;
    background: var(--accent);
    border-radius: 2px;
    width: 100%;
    animation: countdown 10s linear forwards;
  }
  @keyframes countdown { from { width: 100%; } to { width: 0%; } }
</style>
</head>
<body>

<div class="header">
  <span class="header-title">// AERO_STAT</span>
  <div class="header-right">
    <div class="live-dot"></div>
    <span class="live-label" id="clock">--:--:--</span>
  </div>
</div>

<div class="content">

  <!-- Temperature Hero (LM35 primary sensor) -->
  <div class="temp-hero">
    <div class="temp-label">temperature &nbsp;·&nbsp; LM35</div>
    <div class="temp-value">)rawliteral" + String(lm35Temp, 1) + R"rawliteral(<span class="temp-unit">°C</span></div>
    <div class="temp-sub">
      Threshold &nbsp;<span id="thresh-display">)rawliteral" + String(threshold, 1) + R"rawliteral(°C</span>
    </div>
  </div>

  <!-- Stats Row -->
  <div class="stats-row">
    <div class="stat-card">
      <div class="stat-card-label">fan status</div>
      <div class="stat-card-value )rawliteral" + String(fanState ? "fan-on" : "fan-off") + R"rawliteral(">)rawliteral" + String(fanState ? "ON" : "OFF") + R"rawliteral(</div>
    </div>
    <div class="stat-card">
      <div class="stat-card-label">humidity</div>
      <div class="stat-card-value humidity">)rawliteral" + String(humidity, 0) + R"rawliteral(%</div>
    </div>
    <div class="stat-card">
      <div class="stat-card-label">DHT11 temp</div>
      <div class="stat-card-value dht-temp">)rawliteral" + String(dhtTemp, 1) + R"rawliteral(°C</div>
    </div>
  </div>

  <!-- Mode -->
  <div class="card">
    <div class="card-title">mode</div>
    <div class="mode-row">
      <a href="/auto"   class="mode-btn )rawliteral" + String(autoMode  ? "active-auto"   : "") + R"rawliteral(">AUTO</a>
      <a href="/manual" class="mode-btn )rawliteral" + String(!autoMode ? "active-manual" : "") + R"rawliteral(">MANUAL</a>
    </div>
  </div>

  <!-- Threshold -->
  <div class="card">
    <div class="card-title">threshold</div>
    <div class="threshold-row">
      <span>Set trigger point</span>
      <span class="threshold-num" id="thresh-val">)rawliteral" + String((int)threshold) + R"rawliteral(°C</span>
    </div>
    <input type="range" min="20" max="50" value=")rawliteral" + String((int)threshold) + R"rawliteral("
      id="thresh-slider"
      oninput="document.getElementById('thresh-val').textContent=this.value+'°C';document.getElementById('thresh-display').textContent=this.value+'.0°C';"
      onchange="location.href='/set?value='+this.value">
  </div>

  <!-- Manual Control -->
  <div class="card">
    <div class="card-title">manual control</div>
    <div class="ctrl-row">
      <a href="/on"  class="ctrl-btn fan-on-btn">FAN ON</a>
      <a href="/off" class="ctrl-btn fan-off-btn">FAN OFF</a>
    </div>
  </div>

  <!-- Refresh bar -->
  <div class="refresh-bar-wrap">
    <span class="refresh-label">refresh in</span>
    <div class="refresh-bar-track">
      <div class="refresh-bar-fill"></div>
    </div>
    <span class="refresh-label" id="rsec">10s</span>
  </div>

</div>

<script>
  function tick() {
    const now = new Date();
    document.getElementById('clock').textContent = now.toLocaleTimeString();
  }
  tick(); setInterval(tick, 1000);

  let secs = 10;
  const rsec = document.getElementById('rsec');
  const timer = setInterval(() => {
    secs--;
    rsec.textContent = secs + 's';
    if (secs <= 0) { clearInterval(timer); location.reload(); }
  }, 1000);
</script>
</body>
</html>
)rawliteral";
  return page;
}

// ---------- ROUTES ----------
void handleRoot() {
  float lm35Temp = readLM35();
  float dhtTemp  = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (isnan(dhtTemp))  dhtTemp  = 0.0;
  if (isnan(humidity)) humidity = 0.0;
  server.send(200, "text/html", getPage(lm35Temp, dhtTemp, humidity));
}

void handleSet() {
  if (server.hasArg("value")) {
    float val = server.arg("value").toFloat();
    if (val >= 20.0 && val <= 50.0) {
      threshold = val;
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAuto() {
  autoMode = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleManual() {
  autoMode = false;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOn() {
  autoMode  = false;
  fanState  = true;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleOff() {
  autoMode  = false;
  fanState  = false;
  server.sendHeader("Location", "/");
  server.send(303);
}

// ---------- FAN CONTROL ----------
// P-channel MOSFET: low gate voltage → MOSFET ON → Fan ON
//                   high gate voltage → MOSFET OFF → Fan OFF
void applyFanState() {
  if (fanState) {
    dacWrite(FAN_PIN, 5);    // ~0 V  → P-ch MOSFET ON  → Fan ON
  } else {
    dacWrite(FAN_PIN, 200);  // ~2.6 V → P-ch MOSFET OFF → Fan OFF
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  // Configure ADC attenuation only for LM35 pin (full 0-3.3 V range)
  analogSetPinAttenuation(LM35_PIN, ADC_11db);

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/",       handleRoot);
  server.on("/set",    handleSet);
  server.on("/auto",   handleAuto);
  server.on("/manual", handleManual);
  server.on("/on",     handleOn);
  server.on("/off",    handleOff);
  server.begin();

  // Ensure fan starts in a known OFF state
  applyFanState();
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();

  float lm35Temp = readLM35();

  if (!isnan(lm35Temp) && autoMode) {
    if (!fanState && lm35Temp > threshold + HYST) {
      fanState = true;   // turn ON when clearly above threshold
    } else if (fanState && lm35Temp < threshold - HYST) {
      fanState = false;  // turn OFF when clearly below threshold
    }
  }

  applyFanState();

  Serial.printf("LM35: %.1f°C  |  Fan: %s  |  Mode: %s\n",
                lm35Temp,
                fanState ? "ON" : "OFF",
                autoMode ? "AUTO" : "MANUAL");

  delay(1000);
}
