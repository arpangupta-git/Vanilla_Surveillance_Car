#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>

const char* ssid     = "SpyCar_Network";
const char* password = "password123"; // Must be at least 8 chars

const bool INVERT_RIGHT_MOTOR = false; 
const bool INVERT_LEFT_MOTOR = false;  

// ==========================================
//   PIN DEFINITIONS
// ==========================================
// Shared Enable Pin for Speed Control
#define EN_PIN 13 

// Right Motor Direction
#define IN1 12
#define IN2 15
// Left Motor Direction
#define IN3 14
#define IN4 2

// Flashlight
#define LIGHT_PIN 4

// PWM Channels
#define PWM_CH_SPEED 2
#define PWM_CH_LIGHT 3

// Camera Pins (AI Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsCarInput("/CarInput");

unsigned long lastFrameTime = 0;
int currentSpeed = 150; 

// Thread-safe flags for camera resolution
volatile bool isChangingResolution = false;
volatile int requestedQuality = -1;

// Variables for features
unsigned long lastCmdTime = 0;
volatile int requestedNightVision = -1;
bool sneakMode = false;

// ==========================================
//   MODERN WEB UI
// ==========================================
const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Surveillance HUD</title>
    <style>
        :root {
            --primary: #00f7ff;
            --accent: #ff0055;
            --bg: #0b0d15;
            --panel: rgba(16, 20, 30, 0.9);
        }
        body {
            margin: 0; padding: 15px; background-color: var(--bg); color: white;
            font-family: 'Courier New', Courier, monospace;
            display: flex; flex-direction: column; align-items: center;
            min-height: 100vh; touch-action: pan-y;
        }
        h2 { color: var(--primary); text-shadow: 0 0 10px var(--primary); margin: 0 0 10px 0; letter-spacing: 2px; text-align: center; font-size: 1.2rem; transition: color 0.3s;}
        
        .video-container {
            position: relative; width: 100%; max-width: 400px;
            border: 2px solid var(--primary); border-radius: 10px;
            overflow: hidden; box-shadow: 0 0 20px rgba(0, 247, 255, 0.2);
            background: #000; min-height: 240px; margin-bottom: 15px;
            display: flex; justify-content: center; align-items: center;
        }
        #cameraImage { width: 100%; height: auto; display: block; }
        .cam-text { position: absolute; color: #444; font-weight: bold; }

        .quality-controls {
            display: flex; gap: 10px; margin-bottom: 15px; width: 100%; max-width: 400px;
        }
        .q-btn {
            flex: 1; background: rgba(0, 247, 255, 0.1); border: 1px solid var(--primary);
            color: var(--primary); padding: 8px; border-radius: 5px; cursor: pointer;
            font-weight: bold; transition: 0.2s;
        }
        .q-btn:active, .q-btn.active { background: var(--primary); color: #000; }

        .btn-capture { 
            width: 100%; max-width: 400px; padding: 12px; background: rgba(0, 247, 255, 0.1); 
            border: 2px solid var(--primary); color: var(--primary); font-weight: bold; 
            border-radius: 8px; cursor: pointer; margin-bottom: 20px; 
            font-family: 'Courier New', Courier, monospace; font-size: 1rem; transition: 0.2s; 
            box-shadow: 0 0 10px rgba(0, 247, 255, 0.2); 
        }
        .btn-capture:active { background: var(--primary); color: #000; box-shadow: 0 0 20px var(--primary); }

        .action-controls {
            display: flex; gap: 10px; margin-bottom: 20px; width: 100%; max-width: 400px;
        }
        .action-btn {
            flex: 1; background: rgba(16, 20, 30, 0.9); border: 1px solid var(--accent);
            color: var(--accent); padding: 10px; border-radius: 5px; cursor: pointer;
            font-weight: bold; transition: 0.3s;
        }
        .action-btn.active { background: var(--accent); color: white; box-shadow: 0 0 15px var(--accent); }

        .joystick-area {
            position: relative; width: 180px; height: 180px; margin-bottom: 25px;
            background: radial-gradient(circle, rgba(255,255,255,0.05) 0%, rgba(0,0,0,0) 70%);
            border-radius: 50%; border: 1px dashed var(--primary);
            display: flex; justify-content: center; align-items: center;
            touch-action: none;
        }
        .joystick-knob {
            width: 60px; height: 60px; background: rgba(0, 247, 255, 0.2);
            border: 2px solid var(--primary); border-radius: 50%;
            box-shadow: 0 0 15px var(--primary); position: absolute; pointer-events: none;
        }

        @keyframes pulseAlert { 0% { box-shadow: 0 0 0 0 rgba(255,0,85, 0.7); } 70% { box-shadow: 0 0 0 10px rgba(255,0,85, 0); } 100% { box-shadow: 0 0 0 0 rgba(255,0,85, 0); } }
        #thermalWarning { 
            display: none; background: var(--accent); color: white; padding: 10px; 
            border-radius: 5px; margin-bottom: 15px; text-align: center; font-weight: bold; 
            width: 100%; max-width: 350px; box-sizing: border-box; border: 1px solid white; 
            animation: pulseAlert 1.5s infinite; 
        }

        .controls-panel {
            width: 100%; max-width: 350px; background: var(--panel);
            padding: 15px; border-radius: 15px; border-left: 3px solid var(--accent);
            display: flex; flex-direction: column; gap: 15px;
            box-sizing: border-box;
        }
        .label-row { display: flex; justify-content: space-between; color: #aaa; font-size: 0.9rem; }
        input[type=range] { -webkit-appearance: none; width: 100%; background: transparent; }
        input[type=range]::-webkit-slider-thumb {
            -webkit-appearance: none; height: 20px; width: 20px; border-radius: 50%;
            background: var(--accent); cursor: pointer; margin-top: -8px; box-shadow: 0 0 10px var(--accent);
        }
        input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 4px; background: #444; border-radius: 2px; }
    </style>
</head>
<body>

    <h2 id="statusTitle">SURVEILLANCE UNIT [CONNECTING...]</h2>

    <div class="video-container">
        <span class="cam-text" id="camPlaceholder">AWAITING VIDEO FEED...</span>
        <img id="cameraImage" src="">
    </div>

    <div class="quality-controls">
        <button class="q-btn active" id="btn-q1" onclick="setQuality(1)">LOW</button>
        <button class="q-btn" id="btn-q2" onclick="setQuality(2)">MED</button>
        <button class="q-btn" id="btn-q3" onclick="setQuality(3)">HIGH</button>
    </div>

    <button class="btn-capture" onclick="capturePhoto()">&#128248; CAPTURE PHOTO</button>

    <div class="action-controls">
        <button class="action-btn" id="btn-nv" onclick="toggleNV()">&#127769; NIGHT VISION</button>
        <button class="action-btn" id="btn-sneak" onclick="toggleSneak()">&#128034; SNEAK MODE</button>
    </div>

    <div class="joystick-area" id="joystick">
        <div class="joystick-knob" id="knob"></div>
    </div>

    <div id="thermalWarning">⚠️ HIGH TEMP WARNING<br><span style="font-size: 0.8em; font-weight: normal;">Flashlight on high for >30s. Reduce brightness!</span></div>

    <div class="controls-panel">
        <div>
            <div class="label-row"><span>SPEED</span><span id="speed-val">60%</span></div>
            <input type="range" min="0" max="255" value="150" id="Speed" oninput="updateSetting('Speed', this.value, 'speed-val')">
        </div>
        <div>
            <div class="label-row"><span>FLASHLIGHT</span><span id="light-val">0%</span></div>
            <input type="range" min="0" max="255" value="0" id="Light" oninput="updateSetting('Light', this.value, 'light-val')">
        </div>
    </div>

    <script>
        var wsCamera;
        var wsCar;
        const statusTitle = document.getElementById('statusTitle');

        function connectCar() {
            wsCar = new WebSocket("ws://" + window.location.hostname + "/CarInput");
            wsCar.onopen = () => { 
                statusTitle.innerText = "SURVEILLANCE UNIT [ONLINE]"; 
                statusTitle.style.color = "#00ff00"; 
            };
            wsCar.onclose = () => { 
                statusTitle.innerText = "SURVEILLANCE UNIT [OFFLINE]"; 
                statusTitle.style.color = "#ff0000"; 
                setTimeout(connectCar, 2000); 
            };
            wsCar.onerror = (e) => { wsCar.close(); };
        }

        function connectCam() {
            wsCamera = new WebSocket("ws://" + window.location.hostname + "/Camera");
            wsCamera.binaryType = 'blob';
            wsCamera.onmessage = function(event) {
                document.getElementById('camPlaceholder').style.display = 'none';
                var img = document.getElementById("cameraImage");
                if (img.src) URL.revokeObjectURL(img.src);
                img.src = URL.createObjectURL(event.data);
            };
            wsCamera.onclose = () => { setTimeout(connectCam, 2000); };
            wsCamera.onerror = (e) => { wsCamera.close(); };
        }

        // Initialize WebSockets
        connectCar();
        connectCam();

        function sendCmd(key, value) {
            if(wsCar && wsCar.readyState === WebSocket.OPEN) {
                wsCar.send(key + "," + value);
            }
        }

        let flashTimer = null;
        const thermalWarning = document.getElementById('thermalWarning');

        function updateSetting(key, val, textId) {
            document.getElementById(textId).innerText = Math.round((val/255)*100) + "%";
            sendCmd(key, val);

            if (key === 'Light') {
                if (val > 128) { 
                    if (!flashTimer) flashTimer = setTimeout(() => { thermalWarning.style.display = 'block'; }, 30000); 
                } else {
                    clearTimeout(flashTimer); flashTimer = null; thermalWarning.style.display = 'none';
                }
            }
        }

        function setQuality(level) {
            document.querySelectorAll('.q-btn').forEach(b => b.classList.remove('active'));
            document.getElementById('btn-q' + level).classList.add('active');
            sendCmd("Quality", level);
            document.getElementById('camPlaceholder').style.display = 'block';
            document.getElementById('camPlaceholder').innerText = 'CHANGING RESOLUTION...';
        }

        function capturePhoto() {
            var img = document.getElementById("cameraImage");
            if (!img.src || img.src === "") return; 
            var a = document.createElement("a");
            a.href = img.src;
            var now = new Date();
            a.download = "Surveillance_" + now.getHours() + now.getMinutes() + now.getSeconds() + ".jpg";
            document.body.appendChild(a); a.click(); document.body.removeChild(a);
            
            var btn = document.querySelector('.btn-capture');
            var orig = btn.innerText;
            btn.innerText = "\u2705 SAVED!"; btn.style.background = "var(--primary)"; btn.style.color = "#000";
            setTimeout(() => { btn.innerText = orig; btn.style.background = ""; btn.style.color = ""; }, 1000);
        }

        let nvActive = false;
        function toggleNV() {
            nvActive = !nvActive;
            document.getElementById('btn-nv').classList.toggle('active', nvActive);
            sendCmd("NightVision", nvActive ? 1 : 0);
        }

        let sneakActive = false;
        function toggleSneak() {
            sneakActive = !sneakActive;
            document.getElementById('btn-sneak').classList.toggle('active', sneakActive);
            sendCmd("Sneak", sneakActive ? 1 : 0);
        }

        // Joystick Logic
        const joystick = document.getElementById('joystick');
        const knob = document.getElementById('knob');
        let isDragging = false;
        let lastCmd = "0";
        let heartbeatTimer; // Keeps car alive while holding joystick still

        const startDrag = (e) => { 
            isDragging = true; 
            handleMove(e); 
            heartbeatTimer = setInterval(() => { 
                if (isDragging && lastCmd !== "0") sendMove(lastCmd); 
            }, 150);
        };
        
        const endDrag = () => { 
            isDragging = false; 
            clearInterval(heartbeatTimer);
            knob.style.transform = `translate(0px, 0px)`; 
            sendMove("0"); 
            lastCmd = "0"; 
        };

        const handleMove = (e) => {
            if (!isDragging) return;
            if (e.cancelable) e.preventDefault();

            const rect = joystick.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const clientY = e.touches ? e.touches[0].clientY : e.clientY;

            const dx = clientX - rect.left - (rect.width / 2);
            const dy = clientY - rect.top - (rect.height / 2);
            const distance = Math.min(Math.sqrt(dx*dx + dy*dy), 60); 
            const angle = Math.atan2(dy, dx);

            knob.style.transform = `translate(${Math.cos(angle)*distance}px, ${Math.sin(angle)*distance}px)`;

            let cmd = "0";
            if (Math.abs(dx) > Math.abs(dy)) {
                if (dx > 20) cmd = "4"; // Right
                else if (dx < -20) cmd = "3"; // Left
            } else {
                if (dy > 20) cmd = "2"; // Down
                else if (dy < -20) cmd = "1"; // Up
            }

            if (cmd !== lastCmd) { sendMove(cmd); lastCmd = cmd; }
        };

        function sendMove(cmd) { sendCmd("Move", cmd); }

        joystick.addEventListener('mousedown', startDrag);
        joystick.addEventListener('touchstart', startDrag, {passive: false});
        window.addEventListener('mouseup', endDrag);
        window.addEventListener('touchend', endDrag);
        window.addEventListener('mousemove', handleMove);
        window.addEventListener('touchmove', handleMove, {passive: false});
    </script>
</body>
</html>
)HTMLHOMEPAGE";

// ==========================================
//   MOTOR CONTROL LOGIC
// ==========================================

void setMotorState(bool leftFwd, bool leftRev, bool rightFwd, bool rightRev) {
    // Apply inversion flags if needed
    if (INVERT_LEFT_MOTOR) { bool temp = leftFwd; leftFwd = leftRev; leftRev = temp; }
    if (INVERT_RIGHT_MOTOR) { bool temp = rightFwd; rightFwd = rightRev; rightRev = temp; }

    digitalWrite(IN3, leftFwd ? HIGH : LOW);
    digitalWrite(IN4, leftRev ? HIGH : LOW);

    digitalWrite(IN1, rightFwd ? HIGH : LOW);
    digitalWrite(IN2, rightRev ? HIGH : LOW);
}

void moveCar(int dir) {
    int applySpeed = sneakMode ? (currentSpeed / 2) : currentSpeed;
    
    // If not stopping, turn on PWM to the shared Enable Pin
    if (dir != 0) {
        ledcWrite(PWM_CH_SPEED, applySpeed);
    }

    switch (dir) {
        case 1: // UP (Forward)
            setMotorState(true, false, true, false);
            break;
        case 2: // DOWN (Reverse)
            setMotorState(false, true, false, true);
            break;
        case 3: // LEFT (Tank Turn)
            setMotorState(false, true, true, false);
            break;
        case 4: // RIGHT (Tank Turn)
            setMotorState(true, false, false, true);
            break;
        case 0: // STOP
        default:
            setMotorState(false, false, false, false);
            ledcWrite(PWM_CH_SPEED, 0); // Disable PWM to save power & reduce heat
            break;
    }
}

// ==========================================
//   WEBSOCKET & CAMERA SETUP
// ==========================================

void onCarInputEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            std::string msg((char*)data, len);
            int commaIdx = msg.find(',');
            if(commaIdx != -1) {
                String key = String(msg.substr(0, commaIdx).c_str());
                int value = atoi(msg.substr(commaIdx + 1).c_str());

                if (key == "Move") {
                    moveCar(value);
                    lastCmdTime = millis(); // Pet the watchdog timer
                }
                else if (key == "Speed") currentSpeed = value;
                else if (key == "Light") ledcWrite(PWM_CH_LIGHT, value);
                else if (key == "Sneak") sneakMode = (value == 1);
                else if (key == "NightVision") requestedNightVision = value;
                else if (key == "Quality") {
                    requestedQuality = value; 
                }
            }
        }
    } else if (type == WS_EVT_DISCONNECT) {
        moveCar(0); // Stop car if disconnected
    }
}

void setupCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM; config.pin_sscb_sda = SIOD_GPIO_NUM; config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
    
    // OV3660 OPTIMIZATION: Set to 18MHz (Sweet spot for stability and heat reduction)
    config.xclk_freq_hz = 18000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    // Dynamically assign buffers based on PSRAM availability
    if(psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 2; // Double buffering enables zero-freeze operations
    } else {
        config.frame_size = FRAMESIZE_QVGA; 
        config.jpeg_quality = 15; 
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }
}

void setup() {
    Serial.begin(115200);

    // Setup Direction Pins
    pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

    // Setup PWM Pins (Core 2.x standard)
    ledcSetup(PWM_CH_SPEED, 1000, 8); ledcAttachPin(EN_PIN, PWM_CH_SPEED);
    ledcSetup(PWM_CH_LIGHT, 5000, 8); ledcAttachPin(LIGHT_PIN, PWM_CH_LIGHT);

    setupCamera();

    WiFi.softAP(ssid, password);
    Serial.print("Connect to AP and go to: http://");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", htmlHomePage);
    });
    
    wsCarInput.onEvent(onCarInputEvent);

    server.addHandler(&wsCamera);
    server.addHandler(&wsCarInput);
    server.begin();
    
    lastCmdTime = millis(); // Initialize watchdog timer
}

void loop() {
    wsCamera.cleanupClients();
    wsCarInput.cleanupClients();

    // Provide CPU time to the WiFi background tasks. 
    // Without this, the ESP32 tries to process frames too fast and starves the WiFi, causing 10s of lag!
    delay(2);

    // WATCHDOG: Stop car if no command received for 500ms
    static bool isStopped = true;
    if (millis() - lastCmdTime > 500) {
        if (!isStopped) {
            moveCar(0); 
            isStopped = true;
        }
    } else {
        isStopped = false;
    }

    // Thread-safe Night Vision changer
    if (requestedNightVision != -1) {
        sensor_t * s = esp_camera_sensor_get();
        if (s != NULL) {
            if (requestedNightVision == 1) {
                s->set_special_effect(s, 2); // 2 = Grayscale
                s->set_brightness(s, 2);     // Boost brightness
                s->set_contrast(s, 2);       // Boost contrast
            } else {
                s->set_special_effect(s, 0); // 0 = No Effect
                s->set_brightness(s, 0);     // Normal brightness
                s->set_contrast(s, 0);       // Normal contrast
            }
        }
        requestedNightVision = -1; // Reset flag
    }

    // Thread-safe resolution changer processed securely in the main loop
    if (requestedQuality != -1) {
        isChangingResolution = true;
        delay(100); 
        sensor_t * s = esp_camera_sensor_get();
        if (s != NULL) {
            if (requestedQuality == 1) { s->set_framesize(s, FRAMESIZE_QVGA); s->set_quality(s, 12); }
            else if (requestedQuality == 2) { s->set_framesize(s, FRAMESIZE_VGA);  s->set_quality(s, 12); }
            else if (requestedQuality == 3) { s->set_framesize(s, FRAMESIZE_SVGA); s->set_quality(s, 12); }
        }
        delay(50);
        isChangingResolution = false;
        requestedQuality = -1; // Reset flag
    }

    // SMART FRAME LIMITER: ~16 FPS Max
    if (wsCamera.count() > 0 && millis() - lastFrameTime > 60 && !isChangingResolution) {
        
        bool isNetworkBusy = false;
        for(auto client : wsCamera.getClients()) {
            if(client->queueIsFull()) { 
                isNetworkBusy = true; 
                break; 
            }
        }

        // If network is completely clear, process the next frame
        if (!isNetworkBusy) {
            camera_fb_t * fb = esp_camera_fb_get();
            if (fb) {
                wsCamera.binaryAll(fb->buf, fb->len);
                esp_camera_fb_return(fb);
            }
            lastFrameTime = millis();
        }
    }
}