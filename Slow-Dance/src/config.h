#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration
// ============================================
const char* WIFI_SSID = "DUSHYANT-NEW";
const char* WIFI_PASSWORD = "ahuja987";

// ============================================
// GPIO Pin Configuration
// ============================================
const int LED_PIN = 27;
const int MAGNET_PIN = 14;
const int MAGNET2_PIN = 12;
const int BUTTON_PIN = 26;

// ============================================
// Default PWM Settings
// ============================================
#define DEFAULT_LED_FREQ 80.5
#define DEFAULT_MAGNET_FREQ 79.8
#define DEFAULT_LED_DUTY 50.0
#define DEFAULT_MAGNET_DUTY 50.0

// ============================================
// Button Configuration
// ============================================
const unsigned long DEBOUNCE_DELAY = 300;  // milliseconds

// ============================================
// Debug Configuration
// ============================================
#define DEBUG  // Comment out to disable debug output

#ifndef DEBUG_PRINT
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif
#endif

// ============================================
// Web Interface HTML
// ============================================
const char HTML_PAGE[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Slow Motion Controller</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 600px;
      margin: 20px auto;
      padding: 20px;
      background: #1a1a2e;
      color: #eee;
    }
    h1 { text-align: center; color: #00d4ff; }
    .section {
      background: #16213e;
      padding: 20px;
      margin: 15px 0;
      border-radius: 8px;
      border: 1px solid #00d4ff;
    }
    .section h2 { margin-top: 0; color: #00d4ff; font-size: 1.2em; }
    label {
      display: block;
      margin: 10px 0 5px;
      font-weight: bold;
    }
    input[type='number'] {
      width: 100%;
      padding: 10px;
      background: #0f3460;
      border: 1px solid #00d4ff;
      border-radius: 5px;
      color: #eee;
      font-size: 16px;
      box-sizing: border-box;
    }
    .value-display {
      display: inline-block;
      float: right;
      color: #00d4ff;
      font-weight: bold;
    }
    button {
      width: 100%;
      padding: 15px;
      margin: 10px 0;
      font-size: 16px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-weight: bold;
      transition: all 0.3s;
    }
    .btn-save { background: #00d4ff; color: #000; }
    .btn-reset { background: #e74c3c; color: #fff; }
    .btn-ota { background: #f39c12; color: #000; }
    button:hover { opacity: 0.8; transform: scale(1.02); }
    .status {
      text-align: center;
      padding: 10px;
      margin: 20px 0;
      background: #0f3460;
      border-radius: 5px;
      font-weight: bold;
    }
    .info { font-size: 0.9em; color: #aaa; margin-top: 5px; }
  </style>
</head>
<body>
  <h1>Slow Motion Controller</h1>
  
  <div class='status' id='status'>Loading...</div>
  
  <div class='section'>
    <h2>LED Strip</h2>
    <label>Frequency (Hz): <span class='value-display' id='ledFreqVal'>0</span></label>
    <input type='number' id='ledFreq' min='50' max='150' step='0.1' value='80.5'>
    <div class='info'>Recommended: 70-90 Hz</div>
    
    <label>Brightness (%): <span class='value-display' id='ledDutyVal'>0</span></label>
    <input type='number' id='ledDuty' min='1' max='100' step='1' value='50'>
    <div class='info'>Recommended: 30-70%</div>
  </div>
  
  <div class='section'>
    <h2>Electromagnets</h2>
    <label>Frequency (Hz): <span class='value-display' id='magFreqVal'>0</span></label>
    <input type='number' id='magFreq' min='50' max='150' step='0.1' value='79.8'>
    <div class='info'>Recommended: 70-90 Hz</div>
    
    <label>Duty Cycle (%): <span class='value-display' id='magDutyVal'>0</span></label>
    <input type='number' id='magDuty' min='10' max='90' step='1' value='50'>
    <div class='info'>Recommended: 40-60%</div>
  </div>
  
  <button class='btn-save' onclick='saveSettings()'>Save Settings</button>
  <button class='btn-reset' onclick='resetDefaults()'>Reset to Defaults</button>
  <button class='btn-ota' onclick='location.href="/update"'>OTA Update</button>

  <script>
    let autoRefresh = true;
    let userIsEditing = false;
    
    function updateDisplays() {
      document.getElementById('ledFreqVal').textContent = document.getElementById('ledFreq').value;
      document.getElementById('ledDutyVal').textContent = document.getElementById('ledDuty').value;
      document.getElementById('magFreqVal').textContent = document.getElementById('magFreq').value;
      document.getElementById('magDutyVal').textContent = document.getElementById('magDuty').value;
    }
    
    ['ledFreq', 'ledDuty', 'magFreq', 'magDuty'].forEach(id => {
      const elem = document.getElementById(id);
      elem.oninput = () => {
        updateDisplays();
        userIsEditing = true;
        setTimeout(() => { userIsEditing = false; }, 5000);
      };
    });
    
    function loadSettings() {
      if (userIsEditing) return;
      
      fetch('/api/settings')
        .then(r => r.json())
        .then(data => {
          document.getElementById('ledFreq').value = data.ledFreq;
          document.getElementById('ledDuty').value = data.ledDuty;
          document.getElementById('magFreq').value = data.magFreq;
          document.getElementById('magDuty').value = data.magDuty;
          updateDisplays();
          document.getElementById('status').textContent = 'Status: ' + (data.enabled ? 'ENABLED' : 'DISABLED');
        });
    }
    
    function saveSettings() {
      const data = {
        ledFreq: parseFloat(document.getElementById('ledFreq').value),
        ledDuty: parseFloat(document.getElementById('ledDuty').value),
        magFreq: parseFloat(document.getElementById('magFreq').value),
        magDuty: parseFloat(document.getElementById('magDuty').value)
      };
      
      fetch('/api/settings', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.text())
      .then(msg => {
        userIsEditing = false;
        loadSettings();
        document.getElementById('status').textContent = 'Settings Saved!';
        setTimeout(() => { loadSettings(); }, 1000);
      });
    }
    
    function resetDefaults() {
      if (confirm('Reset to default settings?')) {
        fetch('/api/reset', {method: 'POST'})
          .then(r => r.text())
          .then(msg => {
            userIsEditing = false;
            loadSettings();
            document.getElementById('status').textContent = 'Reset to Defaults!';
            setTimeout(() => { loadSettings(); }, 1000);
          });
      }
    }
    
    loadSettings();
    setInterval(loadSettings, 3000);
  </script>
</body>
</html>
)";

#endif // CONFIG_H