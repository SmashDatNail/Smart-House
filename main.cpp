#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// ---------- WiFi credentials (fill these in) ----------
const char* WIFI_SSID = "Q=mcθ";
const char* WIFI_PASS = "NailIrfan4406";
// ---------- Pin assignments (DOIT ESP32 DevKit V1) ----------
// ---------- Pin assignments (DOIT ESP32 DevKit V1) ----------
constexpr uint8_t PIN_DHT       = 4;
constexpr uint8_t PIN_IR        = 18;  // IR obstacle sensor (digital out)
constexpr uint8_t PIN_RAIN      = 34;  // Water sensor S (analog out, ADC1)
constexpr uint8_t PIN_BUTTON    = 23;  // Toggle button -> GND (internal pull-up)
constexpr uint8_t PIN_LED_GREEN = 25;
constexpr uint8_t PIN_LED_RED   = 26;
constexpr uint8_t PIN_BUZZER    = 27;
 
// ---------- Sensor behaviour (flip if your module is wired the other way) ----------
constexpr uint8_t IR_ACTIVE     = LOW;   // most FC-51 obstacle modules pull LOW on detect
 
// ---------- Tunables ----------
constexpr float    TEMP_LIMIT_C = 35.0f; // "too high" threshold
constexpr int      RAIN_THRESHOLD = 800                ; // water sensor S reading (0-4095) above this = wet
constexpr uint32_t DHT_INTERVAL = 2000;  // DHT11 can't be read faster than ~1s
constexpr uint32_t DEBOUNCE_MS  = 50;
 
#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);
WebServer server(80);
 
// ---------- State ----------
bool     systemOn    = false;
float    lastTempC   = 0.0f;
float    lastHumidity = 0.0f;
uint32_t lastDhtRead = 0;
 
bool sIR = false, sRain = false, sTempHigh = false, sAlarm = false;
 
int      lastButtonReading = HIGH;
int      stableButton      = HIGH;
uint32_t lastButtonChange  = 0;
 
// ---------- Sensor helpers ----------
bool irTriggered()    { return digitalRead(PIN_IR)    == IR_ACTIVE; }
bool rainDetected()   { return analogRead(PIN_RAIN) >= RAIN_THRESHOLD; }
bool tempTooHigh()    { return lastTempC >= TEMP_LIMIT_C; }
 
void refreshTemperature() {
  if (millis() - lastDhtRead < DHT_INTERVAL) return;
  lastDhtRead = millis();
 
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) lastTempC = t;
  if (!isnan(h)) lastHumidity = h;
}
 
// ---------- Output helper ----------
void setOutputs(bool red, bool green, bool buzzer) {
  digitalWrite(PIN_LED_RED,   red    ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, green  ? HIGH : LOW);
  digitalWrite(PIN_BUZZER,    buzzer ? HIGH : LOW);
}
 
// ---------- Button: toggle systemOn on each clean press ----------
void handleButton() {
  int reading = digitalRead(PIN_BUTTON);
 
  if (reading != lastButtonReading) {
    lastButtonChange  = millis();
    lastButtonReading = reading;
  }
 
  if (millis() - lastButtonChange < DEBOUNCE_MS) return;
  if (reading == stableButton) return;
 
  stableButton = reading;
  if (stableButton != LOW) return;  // only act on press (button to GND)
 
  systemOn = !systemOn;
  Serial.print("System ");
  Serial.println(systemOn ? "ON" : "OFF");
}
 
// ---------- Web server ----------
const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart House</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,sans-serif;background:#15171a;color:#e8e6e0;display:flex;justify-content:center}
.wrap{width:100%;max-width:420px;padding:20px}
h1{font-size:20px;font-weight:600;margin:0 0 16px}
.banner{border-radius:14px;padding:24px;text-align:center;font-size:22px;font-weight:600;margin-bottom:16px}
.ok{background:#173d1f;color:#9fe0a6}
.alarm{background:#48171a;color:#f3a0a0}
.off{background:#26282c;color:#9b9892}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:16px}
.card{background:#1d2024;border-radius:12px;padding:14px}
.card .label{font-size:12px;color:#9b9892}
.card .val{font-size:24px;font-weight:600;margin-top:4px}
.chips{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:18px}
.chip{font-size:13px;padding:7px 12px;border-radius:999px;background:#1d2024;color:#9b9892}
.chip.hot{background:#48171a;color:#f3a0a0}
button{width:100%;padding:14px;border:0;border-radius:12px;font-size:16px;font-weight:600;cursor:pointer;background:#2f6fd6;color:#fff}
</style></head><body><div class="wrap">
<h1>Smart House</h1>
<div id="banner" class="banner off">connecting</div>
<div class="grid">
<div class="card"><div class="label">Temperature</div><div class="val"><span id="temp">--</span> &deg;C</div></div>
<div class="card"><div class="label">Humidity</div><div class="val"><span id="hum">--</span> %</div></div>
</div>
<div class="chips" id="chips"></div>
<button id="btn" onclick="toggle()">Toggle system</button>
</div>
<script>
function chip(name,on){return '<span class="chip'+(on?' hot':'')+'">'+name+(on?' !':'')+'</span>'}
function render(s){
  var b=document.getElementById('banner');
  if(!s.on){b.className='banner off';b.textContent='System off'}
  else if(s.alarm){
    var r=[];
    if(s.rain)r.push('Rain');
    if(s.ir)r.push('Obstacle');
    if(s.tempHigh)r.push('High temp');
    b.className='banner alarm';b.textContent=r.join(' + ')+' detected';
  }
  else{b.className='banner ok';b.textContent='All clear'}
  document.getElementById('temp').textContent=s.temp;
  document.getElementById('hum').textContent=s.hum;
  document.getElementById('chips').innerHTML=
    chip('IR',s.ir)+chip('Rain',s.rain)+chip('Heat',s.tempHigh);
  document.getElementById('btn').textContent=s.on?'Turn system off':'Turn system on';
}
async function refresh(){try{var r=await fetch('/status');render(await r.json())}catch(e){}}
async function toggle(){try{var r=await fetch('/toggle');render(await r.json())}catch(e){}}
refresh();setInterval(refresh,1000);
</script></body></html>
)HTML";
 
String statusJson() {
  String j = "{";
  j += "\"on\":";       j += systemOn  ? "true" : "false"; j += ",";
  j += "\"alarm\":";    j += sAlarm    ? "true" : "false"; j += ",";
  j += "\"ir\":";       j += sIR       ? "true" : "false"; j += ",";
  j += "\"rain\":";     j += sRain     ? "true" : "false"; j += ",";
  j += "\"tempHigh\":"; j += sTempHigh ? "true" : "false"; j += ",";
  j += "\"temp\":";     j += String(lastTempC, 1);  j += ",";
  j += "\"hum\":";      j += String(lastHumidity, 0);
  j += "}";
  return j;
}
 
void handleRoot()   { server.send_P(200, "text/html", PAGE); }
void handleStatus() { server.send(200, "application/json", statusJson()); }
void handleToggle() {
  systemOn = !systemOn;
  server.send(200, "application/json", statusJson());
}
 
void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
 
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
 
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected. Open http://");
    Serial.println(WiFi.localIP());
    return;
  }
  Serial.println("\nWiFi failed - running offline (physical button still works).");
}
 
void setup() {
  Serial.begin(115200);
 
  pinMode(PIN_IR,     INPUT);
  pinMode(PIN_RAIN,   INPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
 
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED,   OUTPUT);
  pinMode(PIN_BUZZER,    OUTPUT);
 
  dht.begin();
  setOutputs(false, false, false);
 
  connectWifi();
  server.on("/",       handleRoot);
  server.on("/status", handleStatus);
  server.on("/toggle", handleToggle);
  server.begin();
}
 
void loop() {
  handleButton();
  server.handleClient();
  refreshTemperature();   // always refreshed so the dashboard shows live temp/humidity
 
  // System off -> everything dark and silent.
  if (!systemOn) {
    sIR = sRain = sTempHigh = sAlarm = false;
    setOutputs(false, false, false);
    return;
  }
 
  sIR       = irTriggered();
  sRain     = rainDetected();
  sTempHigh = tempTooHigh();
  sAlarm    = sIR || sRain || sTempHigh;
 
  if (sAlarm) {
    setOutputs(true, false, true);   // red LED + buzzer
    return;
  }
 
  setOutputs(false, true, false);    // all clear -> green LED
}
