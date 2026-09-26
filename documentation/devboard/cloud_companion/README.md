# Cloud companion firmware (ESP32 Wi-Fi Devboard)

This is the ESP32-side half of the firmware's **Cloud** app (main menu ->
Cloud). The Flipper itself has no Wi-Fi radio, so the Cloud app talks over
UART to an ESP32 "Wi-Fi Devboard" plugged into the Flipper's GPIO header,
and that ESP32 talks HTTPS to the internet.

## What it does

- Connects the Devboard to a Wi-Fi network on request from the Flipper.
- Lists, uploads, and downloads files against a "bin" (a named folder) on
  [filebin.net](https://filebin.net) - a free, public, no-signup file host.
  Anyone who knows the bin name can read/write it, which is what makes it
  usable as a shared public "database" of files without running your own
  server. Bins are not permanent: filebin.net removes bins that have been
  inactive for a while, so treat this as a demo/lab backend.

## Flashing

1. Install the Arduino IDE (2.x) and the "esp32 by Espressif Systems" board
   package (Boards Manager).
2. Connect the Wi-Fi Devboard to your computer via its USB-C port (not
   through the Flipper).
3. Board: "ESP32 Dev Module" (or your Devboard's specific entry, if the
   board package ships one). Flash size / partition scheme: defaults are
   fine.
4. Open `cloud_http.ino` and upload it.
5. Disconnect from your computer and mount the Devboard on the Flipper's
   GPIO header as usual.

## Protocol

The sketch and `applications/main/cloud/cloud_transport.c` implement the
same line-based, 115200 8N1 protocol:

```
-> WIFI_CONNECT <ssid> <password>
<- WIFI_OK | WIFI_FAIL

-> LIST <bin>
<- FILE <name> <size>      (zero or more lines)
<- END                     (or ERR <reason>)

-> GET <bin> <name>
<- SIZE <n>
<- <n raw bytes>
<- END                     (or ERR <reason> instead of SIZE)

-> PUT <bin> <name> <size>
<- READY
-> <size raw bytes>
<- OK | ERR <reason>
```

If you'd rather point this at your own backend instead of filebin.net,
implement the same four commands against your API and the Flipper app does
not need to change at all - only `FILEBIN_HOST` and the request shapes in
`cloud_http.ino` need to change.

## Known limitations

- Uploads are buffered fully in the ESP32's RAM before being sent (filebin.net
  needs a `Content-Length` up front), so very large files may fail to upload
  on boards without PSRAM.
- TLS certificates are not validated (`setInsecure()`), which is standard
  practice for small hobbyist ESP32 sketches but is worth knowing about.
- This sketch has not been flashed and tested on real hardware as part of
  this change; treat it as a documented starting point and verify it on your
  Devboard before relying on it.
