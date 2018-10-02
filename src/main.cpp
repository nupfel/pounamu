#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <SPI.h>
#include <FastLED.h>

// required to disable brownout detection
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BRIGHTNESS  50

#define DEFAULT_PATTERN 0
#define OFF_PATTERN     1
#define RAINBOW_PATTERN 0
#define STATIC_PATTERN  2

#define NO_OVERLAY      0
#define LIGHTNING_OVERLAY   1

// setup functions
void LED_setup();
void WIFI_setup();
void HTTPServer_setup();

// pattern functions
void rainbow();
void blackout();
void static_colour();

// overlay functions
void no_overlay() {}
void lightning();

// HTTP endpoint functions
void index_page();
void on();
void off();
void changePattern();
void patternRainbow();
void patternStatic();
void changeOverlay();
void overlayOff();
void overlayLightning();
void changeFPS();
void changeSpeed();
void changeBrightness();
void changeLightningChance();

// helper functions
void show();
void colorBars();

const char* host = "pounamu";
const char* ssid = "ElKaTo";
const char* password = "ziggyisthecutest";

WebServer server(80);

typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { rainbow, blackout, static_colour };
SimplePatternList gOverlays = { no_overlay, lightning };
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gCurrentOverlayNumber = 0; // Index number of which overlay is current
uint8_t gHue = 0;   // rotating "base color" used by many of the patterns

// global brightness, fps, speed, hue and the currentLed
uint8_t brightness = BRIGHTNESS;     // 0 - 255
unsigned int fps = 100;              // 1 - 255
unsigned int speed = 1;              // 1 - 255
unsigned int chanceOfLightning = 2;

CRGB led;

const char* web_ui =
 "<html>"
    "<head><meta name='viewport' content='width=device-width,initial-scale=1'></head>"
    "<body style='width:100%;height:100%;overflow:hidden;'>"
        "<svg style='width:100%;height:100%;' viewbox='0 0 500 500' id='svg'>"
            "<path style='fill:#80e5ff;fill-opacity:1;fill-rule:nonzero;stroke:none'"
                  "d='M 250 0 C 111.92882 3.7895613e-14 0 111.92882 0 250 C -1.249508e-14 341.05067 48.689713 420.72528 121.4375 464.4375 L 154.625 409.40625 C 100.50052 376.95218 64.28125 317.69934 64.28125 250 C 64.28125 147.43284 147.43284 64.28125 250 64.28125 C 352.56716 64.28125 435.71875 147.43284 435.71875 250 C 435.71875 317.53896 399.66155 376.65256 345.75 409.15625 L 378.71875 464.34375 C 451.37991 420.61135 500 340.98541 500 250 C 500 111.92882 388.07118 -1.8947806e-14 250 0 z '"
                  "id='ring'/>"
            "<rect style='fill:#ffffff;fill-opacity:1;fill-rule:nonzero;stroke:none'"
                  "id='needle'"
                  "width='16'"
                  "height='80'"
                  "x='242'/>"
            "<text xml:space='preserve'"
                  "style='font-size:122.59261322px;font-style:normal;font-variant:normal;font-weight:normal;font-stretch:normal;text-align:center;line-height:125%;letter-spacing:0px;word-spacing:0px;text-anchor:middle;fill:#000000;fill-opacity:1;stroke:none;font-family:Helvetica;-inkscape-font-specification:Helvetica'"
                  "x='250.01915'"
                  "y='845.31812'"
                  "id='text'>"
                  "<tspan id='label' x='250.01915' y='292.95594'>0</tspan>"
            "</text>"
            "<path style='fill:#d5f6ff;fill-opacity:1;fill-rule:nonzero;stroke:none'"
                  "id='up'"
                  "d='m 294.75099,133.39225 -90.93056,0 45.46528,-78.748173 z'"
                  "transform='matrix(0.61903879,0,0,0.61903879,95.682477,91.16682)' />"
            "<path transform='matrix(0.61903879,0,0,-0.61903879,95.682477,408.80767)'"
                  "d='m 294.75099,133.39225 -90.93056,0 45.46528,-78.748173 z'"
                  "id='dn'"
                  "style='fill:#d5f6ff;fill-opacity:1;fill-rule:nonzero;stroke:none' />"
        "</svg>"
        "<script>"
            "/* Convert touch to mouse event for mobile devices */"
            "function touchHandler(event) {"
                "var touches = event.changedTouches, first = touches[0], type = '';"
                "switch(event.type) {"
                    "case 'touchstart': type='mousedown'; break;"
                    "case 'touchmove':  type='mousemove'; break;"
                    "case 'touchend':   type='mouseup'; break;"
                    "default: return;"
                "}"
                "var simulatedEvent = document.createEvent('MouseEvent');"
                "simulatedEvent.initMouseEvent(type, true, true, window, 1, first.screenX, first.screenY, first.clientX, first.clientY, false, false, false, false, 0/*left*/, null);"
                "first.target.dispatchEvent(simulatedEvent);"
                "event.preventDefault();"
            "}"

            "document.addEventListener('touchstart', touchHandler, true);"
            "document.addEventListener('touchmove', touchHandler, true);"
            "document.addEventListener('touchend', touchHandler, true);"
            "document.addEventListener('touchcancel', touchHandler, true);"

            "/* rotate needle to correct position */"
            "var pos = 0;"
            "function setPos(p) {"
                "if (p < 0) p = 0;"
                "if (p > 255) p = 255;"
                "pos = p;"
                "document.getElementById('label').textContent = pos;"
                "var a = (pos - 127) * 1.1;"
                "document.getElementById('needle').setAttribute('transform','rotate('+a+' 250 250)');"
            "}"
            "setPos(pos);"

            "/* handle events */"
            "var dragging = false;"
            "function dragStart() {"
                "dragging = true;"
                "document.getElementById('ring').style.fill = '#ff0000';"
            "}"
            "document.addEventListener('mousemove', function(e) {"
                "if (dragging) {"
                    "e.preventDefault();"
                    "var svg = document.getElementById('svg');"
                    "var ang = Math.atan2(e.clientX - (svg.clientWidth/2), (svg.clientHeight/2) - e.clientY) * 180 / Math.PI;"
                    "setPos(Math.round((ang/1.1)+127));"
                "}"
            "});"
            "document.addEventListener('mouseup', function(e) {"
                "dragging = false;"
                "document.getElementById('ring').style.fill = '#80e5ff';"
                "document.getElementById('up').style.fill = '#d5f6ff';"
                "document.getElementById('dn').style.fill = '#d5f6ff';"
                "var req=new XMLHttpRequest();"
                "req.open('GET','/pattern/colour?value='+pos, true);"
                "req.send();"
            "});"
            "document.getElementById('ring').onmousedown = dragStart;"
            "document.getElementById('needle').onmousedown = dragStart;"
            "document.getElementById('up').onmousedown = function(e) { e.preventDefault(); this.style.fill = '#ff0000'; };"
            "document.getElementById('dn').onmousedown = function(e) { e.preventDefault(); this.style.fill = '#00ff00'; };"
            "document.getElementById('up').onmouseup = function(e) { setPos(pos+10); };"
            "document.getElementById('dn').onmouseup = function(e) { setPos(pos-10); };"
        "</script>"
    "</body>"
 "</html>";

/*
 * Login page
 */

const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form) {"
        "if (form.userid.value == 'admin' && form.pwd.value == 'admin') {"
            "window.open('/serverIndex')"
        "}"
        "else {"
            "alert('Error Password or Username')/*displays error message*/"
        "}"
    "}"
"</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
"</form>"
"<div id='prg'>progress: 0%</div>"

"<script>"
    "$('form').submit(function(e) {"
        "e.preventDefault();"
        "var form = $('#upload_form')[0];"
        "var data = new FormData(form);"
        "$.ajax({"
            "url: '/update',"
            "type: 'POST',"
            "data: data,"
            "contentType: false,"
            "processData: false,"
            "xhr: function() {"
                "var xhr = new window.XMLHttpRequest();"
                "xhr.upload.addEventListener('progress', function(evt) {"
                    "if (evt.lengthComputable) {"
                        "var per = evt.loaded / evt.total;"
                        "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
                    "}"
                "}, false);"
                "return xhr;"
            "},"
            "success:function(d, s) {"
                "console.log('success!')"
            "},"
            "error: function (a, b, c) {}"
        "});"
    "});"
 "</script>";

/*
 * setup function
 */
void setup(void) {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  LED_setup();
  // WIFI_setup();
  // HTTPServer_setup();

  random16_set_seed(0);

  Serial.begin(115200);
  Serial.println("bootup");

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.on("/colour", index_page);
  server.on("/on", on);
  server.on("/off", off);
  server.on("/pattern", changePattern);
  server.on("/pattern/rainbow", patternRainbow);
  server.on("/pattern/colour", patternStatic);
  server.on("/overlay", changeOverlay);
  server.on("/overlay/off", overlayOff);
  server.on("/overlay/lightning", overlayLightning);
  server.on("/fps", changeFPS);
  server.on("/speed", changeSpeed);
  server.on("/brightness", changeBrightness);
  server.on("/lightning/chance", changeLightningChance);
  server.begin();
#ifdef DEBUG
  Serial.println("HTTP server started");
#endif

  server.begin();
}

void loop(void) {
  server.handleClient();

  // Call the current pattern and overlay function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();
  gOverlays[gCurrentOverlayNumber]();

  show();

  // insert a delay to keep the framerate modest
  // FastLED.delay(1000/fps);

  // do some periodic updates
  // slowly cycle the "base color" through the rainbow
  EVERY_N_MILLISECONDS(10) { gHue += speed; }
}

/*
    SETUP functions
*/
void LED_setup() {
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    // SPI.setFrequency(10000000); //We are using a relatively HIGH driveing Frequency. You may need to lower this depending on your hardware set up
    // SPI.setClockDivider(SPI_CLOCK_DIV128);

#ifdef DEBUG
    Serial.println("SPI started");
#endif

    // Flash the "hello" color sequence: R, G, B, black.
    colorBars();
}

/*
    pattern functions
*/
void rainbow() {
    // Use FastLED automatic HSV->RGB conversion
    led = CHSV(gHue, 255, brightness);
}

void blackout() {
    led = CRGB::Black;
}

void static_colour() {
    speed = 0;
    led = CHSV(gHue, 255, brightness);
}

/*
    overlay functions
*/
void lightning() {
    if (random8() < chanceOfLightning) {
        CRGB oldled = led;
        led += CRGB::White;

        // additional chance for a second lightning right after the first
        if (random8() < 128) {
            show();
            FastLED.delay(1000/fps);
            led = oldled;
            show();
            FastLED.delay(1000/fps);
        }
    }
}

/*
    HTTP endpoint functions
*/
void index_page() {
    server.send(200, "text/html", web_ui);
}

void on() {
    gCurrentPatternNumber = DEFAULT_PATTERN;
    gCurrentOverlayNumber = NO_OVERLAY;
    speed = 1;
    fps = 100;
    gHue = 0;
    brightness = BRIGHTNESS;
    server.send(200, "text/html", "ON");
}

void off() {
    gCurrentPatternNumber = OFF_PATTERN;
    gCurrentOverlayNumber = NO_OVERLAY;
    server.send(200, "text/html", "OFF");
}

void changeBrightness() {
    // grab brightness from GET /brightness&value=<0-255>
    brightness = server.arg("value").toInt();

    if (brightness >= 0 || brightness <= 255) {
        // FastLED.setBrightness(brightness);
        server.send(200, "text/html", "brightness: " + String(brightness) + "");
    }
}

void changeFPS() {
    // grab fps from GET /fps&value=<1-255>
    fps = server.arg("value").toInt();
    if (fps == 0) { fps = 1; }
    server.send(200, "text/html", "fps: " + String(fps) + "");
}

void changeSpeed() {
    // grab speed from GET /speed&value=<1-255>
    speed = server.arg("value").toInt();
    // if (speed == 0) { speed = 1; }
    server.send(200, "text/html", "speed: " + String(speed) + "");
}

void changeLightningChance() {
    // grab value from GET /lightning/chance&value=<1-255>
    chanceOfLightning = server.arg("value").toInt();
    server.send(200, "text/html", "chanceOfLightning: " + String(chanceOfLightning) + "");
}

void changePattern() {
    gCurrentPatternNumber = server.arg("value").toInt();
    server.send(200, "text/html", "pattern: " + String(gCurrentPatternNumber) + "");
}

void patternOff() {
    gCurrentPatternNumber = OFF_PATTERN;
    server.send(200, "text/html", "pattern: OFF");
}

void patternRainbow() {
    gCurrentPatternNumber = RAINBOW_PATTERN;
    speed = 1;
    server.send(200, "text/html", "pattern: rainbow");
}

void patternStatic() {
    // grab value from GET /pattern/colour&value=<0-255>
    gCurrentPatternNumber = STATIC_PATTERN;
    gHue = server.arg("value").toInt();
    speed = 0;
    server.send(200, "text/html", "pattern: static colour");
}

void changeOverlay() {
    gCurrentOverlayNumber = server.arg("value").toInt();
    server.send(200, "text/html", "overlay: " + String(gCurrentOverlayNumber) + "");
}

void overlayOff() {
    gCurrentOverlayNumber = NO_OVERLAY;
    server.send(200, "text/html", "overlay: OFF");
}

void overlayLightning() {
    gCurrentOverlayNumber = LIGHTNING_OVERLAY;
    server.send(200, "text/html", "overlay: lightning");
}

/*
    helper functions
*/
void show() {
    // start frame: 4 bytes all 0
    // led frame: 4 bytes
    // end frame: 4 bytes all 255
    SPI.write32(0);
    SPI.writeBytes(&brightness, 1);
    SPI.writeBytes(&led.b, 1);
    SPI.writeBytes(&led.g, 1);
    SPI.writeBytes(&led.r, 1);
    SPI.write32(255);
}

// colorBars: flashes Red, then Green, then Blue, then Black.
// Helpful for diagnosing if you've mis-wired which is which.
void colorBars() {
    led = CRGB::Red; show(); delay(500);
    led = CRGB::Green; show(); delay(500);
    led = CRGB::Blue; show(); delay(500);
    show(); delay(500);
}
