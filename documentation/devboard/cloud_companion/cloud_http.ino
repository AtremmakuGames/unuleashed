// cloud_http.ino
//
// Companion firmware for the Flipper Zero ESP32 "Wi-Fi Devboard".
// Bridges the Flipper's "Cloud" app (applications/main/cloud) to the public
// file host filebin.net over Wi-Fi, using a small line-based protocol on the
// Devboard's USB/GPIO serial port (115200 8N1 - the same port used by
// "expansion" apps such as UART bridges).
//
// Flash this with the Arduino IDE (or arduino-cli) using the "ESP32 Wi-Fi
// Devboard" / generic ESP32 Dev Module board profile.
// Required libraries: WiFi, HTTPClient, WiFiClientSecure (all bundled with
// the ESP32 Arduino core).
//
// Protocol (see applications/main/cloud/cloud_transport.h for the
// authoritative description):
//   WIFI_CONNECT <ssid> <password>   -> WIFI_OK | WIFI_FAIL
//   LIST <bin>                       -> FILE <name> <size> (repeated), END | ERR <reason>
//   GET <bin> <name>                 -> SIZE <n> <n raw bytes> END | ERR <reason>
//   PUT <bin> <name> <size>          -> READY, then reads <size> raw bytes -> OK | ERR <reason>
//
// "bin" is a filebin.net "bin" id: a freeform name that acts as the shared
// public folder/database. Anyone who knows the bin name can list, upload to,
// and download from it - that's what makes it a simple public database.
// filebin.net deletes bins that have been inactive for a while; treat this
// as a demo/lab backend, not permanent storage.

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define SERIAL_BAUD    115200
#define FILEBIN_HOST   "filebin.net"
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define HTTP_TIMEOUT_MS 15000
#define IO_CHUNK_SIZE   512

static String readLine() {
    String line = Serial.readStringUntil('\n');
    line.trim();
    return line;
}

static void cmdWifiConnect(const String& args) {
    int sp = args.indexOf(' ');
    String ssid = sp < 0 ? args : args.substring(0, sp);
    String pass = sp < 0 ? "" : args.substring(sp + 1);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while(WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(200);
    }

    Serial.println(WiFi.status() == WL_CONNECTED ? "WIFI_OK" : "WIFI_FAIL");
}

// Minimal, dependency-free scan for filebin.net's
// {"files":[{"filename":"a.txt","size":123,...}, ...]} response. Avoids
// pulling in ArduinoJson for two fields.
static void emitFilesFromJson(const String& json) {
    int pos = 0;
    while(true) {
        int nameKey = json.indexOf("\"filename\"", pos);
        if(nameKey < 0) break;
        int nameColon = json.indexOf(':', nameKey);
        int nameStart = json.indexOf('"', nameColon + 1);
        int nameEnd = json.indexOf('"', nameStart + 1);
        if(nameStart < 0 || nameEnd < 0) break;
        String name = json.substring(nameStart + 1, nameEnd);

        int sizeKey = json.indexOf("\"size\"", nameEnd);
        long size = 0;
        if(sizeKey >= 0) {
            int sizeColon = json.indexOf(':', sizeKey);
            int sizeEnd = sizeColon + 1;
            while(sizeEnd < (int)json.length() &&
                  (isDigit(json[sizeEnd]) || json[sizeEnd] == ' ')) {
                sizeEnd++;
            }
            size = json.substring(sizeColon + 1, sizeEnd).toInt();
        }

        Serial.printf("FILE %s %ld\n", name.c_str(), size);
        pos = nameEnd + 1;
    }
}

static void cmdList(const String& bin) {
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("ERR not connected to Wi-Fi");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // no cert pinning - acceptable for a lab/demo backend
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    String url = String("https://") + FILEBIN_HOST + "/" + bin;
    if(!http.begin(client, url)) {
        Serial.println("ERR could not start request");
        return;
    }
    http.addHeader("Accept", "application/json");

    int code = http.GET();
    if(code == 200) {
        emitFilesFromJson(http.getString());
        Serial.println("END");
    } else if(code == 404) {
        // Empty/nonexistent bin: treat as an empty database rather than an error.
        Serial.println("END");
    } else {
        Serial.printf("ERR HTTP %d\n", code);
    }
    http.end();
}

static void cmdGet(const String& args) {
    int sp = args.indexOf(' ');
    if(sp < 0) {
        Serial.println("ERR bad GET arguments");
        return;
    }
    String bin = args.substring(0, sp);
    String name = args.substring(sp + 1);

    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("ERR not connected to Wi-Fi");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    String url = String("https://") + FILEBIN_HOST + "/" + bin + "/" + name;
    if(!http.begin(client, url)) {
        Serial.println("ERR could not start request");
        return;
    }

    int code = http.GET();
    if(code != 200) {
        Serial.printf("ERR HTTP %d\n", code);
        http.end();
        return;
    }

    int size = http.getSize();
    Serial.printf("SIZE %d\n", size);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[IO_CHUNK_SIZE];
    int remaining = size;
    while(remaining > 0) {
        size_t avail = stream->available();
        if(avail == 0) {
            if(!http.connected()) break;
            delay(1);
            continue;
        }
        size_t chunk = min((size_t)remaining, min(avail, sizeof(buffer)));
        size_t got = stream->readBytes(buffer, chunk);
        Serial.write(buffer, got);
        remaining -= got;
    }
    Serial.println("END");
    http.end();
}

static void cmdPut(const String& args) {
    int sp1 = args.indexOf(' ');
    int sp2 = args.indexOf(' ', sp1 + 1);
    if(sp1 < 0 || sp2 < 0) {
        Serial.println("ERR bad PUT arguments");
        return;
    }
    String bin = args.substring(0, sp1);
    String name = args.substring(sp1 + 1, sp2);
    long size = args.substring(sp2 + 1).toInt();

    Serial.println("READY");

    // Buffer the upload in PSRAM/heap before sending: filebin.net needs a
    // Content-Length up front and the ESP32 HTTPClient does not support
    // chunked request bodies from an arbitrary stream.
    uint8_t* data = (uint8_t*)malloc(size > 0 ? size : 1);
    if(!data) {
        // Drain what we can so the Flipper's TX doesn't stall waiting for us.
        Serial.println("ERR out of memory");
        return;
    }

    long received = 0;
    uint32_t lastByteAt = millis();
    while(received < size) {
        if(Serial.available()) {
            int got = Serial.readBytes(data + received, min((long)IO_CHUNK_SIZE, size - received));
            received += got;
            lastByteAt = millis();
        } else if(millis() - lastByteAt > HTTP_TIMEOUT_MS) {
            free(data);
            Serial.println("ERR timed out receiving file");
            return;
        }
    }

    if(WiFi.status() != WL_CONNECTED) {
        free(data);
        Serial.println("ERR not connected to Wi-Fi");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    String url = String("https://") + FILEBIN_HOST + "/" + bin + "/" + name;
    if(!http.begin(client, url)) {
        free(data);
        Serial.println("ERR could not start request");
        return;
    }
    http.addHeader("Content-Type", "application/octet-stream");

    int code = http.POST(data, size);
    free(data);
    http.end();

    Serial.println((code >= 200 && code < 300) ? "OK" : ("ERR HTTP " + String(code)));
}

void setup() {
    Serial.begin(SERIAL_BAUD);
}

void loop() {
    if(!Serial.available()) return;

    String line = readLine();
    if(line.length() == 0) return;

    if(line.startsWith("WIFI_CONNECT ")) {
        cmdWifiConnect(line.substring(13));
    } else if(line.startsWith("LIST ")) {
        cmdList(line.substring(5));
    } else if(line.startsWith("GET ")) {
        cmdGet(line.substring(4));
    } else if(line.startsWith("PUT ")) {
        cmdPut(line.substring(4));
    } else {
        Serial.println("ERR unknown command");
    }
}
