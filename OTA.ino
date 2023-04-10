#include <WiFi.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Update.h>
#include <Crypto.h>
#include <mbedtls/sha256.h>

const char* ssid = "Palla iP13";
const char* password = "qwertyui";
const char* firmware_url = "https://raw.githubusercontent.com/aggarpa/iot/main/SimpleTime_2.bin";
const char* firmware_filename = "/SimpleTime_2.bin";

String followRedirect(const char* url) {
  HTTPClient redirectClient;
  redirectClient.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  redirectClient.begin(url);
  int httpCode = redirectClient.GET();
  if (httpCode == 302 || httpCode == 301) {
    String location = redirectClient.header("Location");
    Serial.printf("Location: %s\n", location.c_str());
    return location;
  } else {
    Serial.printf("Url: %s\n", url);
    return String(url);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to Wi-Fi
  Serial.println();
  Serial.printf("Connecting to Wi-Fi network %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("Connected to Wi-Fi network %s\n", ssid);

  // Download firmware file
  Serial.println();
  Serial.printf("Downloading firmware from %s\n", firmware_url);
  String final_firmware_url = followRedirect(firmware_url);
  HTTPClient client;
  client.begin(final_firmware_url);
  int httpCode = client.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Failed to download firmware: %d\n", httpCode);
    return;
  }
  Stream& stream = client.getStream();
  size_t firmware_size = client.getSize();

  Serial.println();
  Serial.println("Starting firmware update...");

  Serial.printf("Flash chip size: %u bytes\n", ESP.getFlashChipSize());

  // Begin OTA update
  if (Update.begin(firmware_size, U_FLASH)) {
    Serial.println("Update started...");
  } else {
    Serial.printf("Failed to begin OTA update: %s\n", Update.errorString());
    return;
  }

  // Update firmware
  size_t bytes_written = 0;
  uint8_t buffer[2048];

  while (bytes_written < firmware_size) {
    int bytes_read = stream.readBytes(buffer, sizeof(buffer));
    if (bytes_read < 0) {
      Serial.println("Failed to read firmware file!");
      return;
    }

    // Write the firmware to the OTA partition
    size_t written = Update.write(buffer, bytes_read);
    if (written != bytes_read) {
      Serial.printf("Failed to write firmware to OTA partition! %s\n", Update.errorString());
      return;
    }

    bytes_written += bytes_read;
  }

  if (Update.end()) {
    Serial.println("Firmware update complete!");
  } else {
    Serial.printf("Failed to end OTA update: %s\n", Update.errorString());
    return;
  }

  if (Update.isFinished()) {
    Serial.println("Restarting...");
    ESP.restart();
  } else {
    Serial.println("Update not finished. Something went wrong!");
  }
}


String calculateFileSHA256(fs::File& file) {
  static const size_t bufferSize = 512;
  uint8_t buffer[bufferSize];

  mbedtls_sha256_context sha256;
  mbedtls_sha256_init(&sha256);
  mbedtls_sha256_starts_ret(&sha256, 0);  // 0 for SHA-256, 1 for SHA-224

  while (file.available()) {
    size_t bytesRead = file.read(buffer, bufferSize);
    mbedtls_sha256_update_ret(&sha256, buffer, bytesRead);
  }

  uint8_t hash[32];
  mbedtls_sha256_finish_ret(&sha256, hash);
  mbedtls_sha256_free(&sha256);

  String hex = "";
  for (int i = 0; i < 32; i++) {
    char hexStr[3];
    sprintf(hexStr, "%02x", hash[i]);
    hex += String(hexStr);
  }

  return hex;
}

void loop() {
  // your code here
}
