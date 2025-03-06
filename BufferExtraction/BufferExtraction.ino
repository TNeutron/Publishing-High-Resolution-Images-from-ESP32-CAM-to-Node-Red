#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <MQTTPubSubClient.h>
#include "camera_pins.h"


// Enter your WiFi credentials
const char* ssid = "";
const char* password = "";

char mqtt_topic[50];
unsigned int t1 = 0;
unsigned int t2 = 0;
unsigned int latency = 0;
int qos = 2;

WiFiClient client;
MQTTPubSubClient mqtt;


void MQTTconnect() {

    Serial.print("connecting to host...");
    while (!client.connect("YOUR-BROKER-SERVER-HERE", 1883)) { // <--- Put your own credentials
        Serial.print(".");
        delay(100);
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected");
        
        }
    }

    Serial.println(" connected!");
    Serial.print("connecting to mqtt broker...");
    mqtt.disconnect();

    while (!mqtt.connect("59844897kjnib65knj", "", "")) { // <---- Put your own credentials
        Serial.print(".");
        delay(1000);
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected");
   
        }
        if (client.connected() != 1) {
            Serial.println("WiFiClient disconnected");
            esp_restart();
        }
    }
    Serial.println(" connected!");
}

void publishImageWithMarkers() {
    camera_fb_t *fb = esp_camera_fb_get();  // Capture image
    if (!fb) {
        Serial.println("Camera capture failed! Restarting ESP...");
        ESP.restart();
        return;
    }

    Serial.printf("Captured Image, size: %d bytes\n", fb->len);

    int chunkSize = 1024*8;  // 8KB per chunk
    int totalSize = fb->len;
    int NoI = totalSize/chunkSize;
    char mqtt_topic[50] = "ESP32/Cam/ImagePart";

    // Send "start" marker
    mqtt.publish(mqtt_topic, "START", false, 1);

    Serial.println("START Marker Sent");
    // Print Buffer Start Address 
    Serial.print("Starting Frame buffer address - ");
    Serial.println((uintptr_t)(fb->buf), HEX);  

    // Print Buffer Length
    Serial.print("Frame length - ");
    Serial.println(fb->len);
    
    // Print Buffer End Address
    Serial.print("End Address is - ");
    Serial.println((uintptr_t)(fb->buf + fb->len), HEX);
    
    // Start transmitting image chunks  
    for(int i = 0; i<=NoI; i++){
      snprintf(mqtt_topic, sizeof(mqtt_topic), mqtt_topic);
      Serial.print("Transmitting from Address - ");
      Serial.print((uintptr_t)(fb->buf + (i * chunkSize)), HEX);  // Print address in HEX
      Serial.print(" to ");
      Serial.println((uintptr_t)(fb->buf + (i * chunkSize) + chunkSize), HEX);

      bool success = mqtt.publish(mqtt_topic, fb->buf + (i*chunkSize), chunkSize, false, 1);
      if (!success) {
          Serial.println("Failed to send image chunk!");
          break;
      }

      Serial.printf("Sent chunk %d/%d (%d bytes)\n", i, totalSize / chunkSize, chunkSize);
      delay(1);  // Small delay to prevent buffer overflow
    }

      Serial.print("Transmitting from Address - ");
      Serial.print((uintptr_t)(fb->buf + (NoI * chunkSize)), HEX);  // Print address in HEX
      Serial.print(" to ");
      Serial.println((uintptr_t)(fb->buf + (NoI * chunkSize) + (totalSize-(NoI*chunkSize)) ), HEX);

      bool success = mqtt.publish(mqtt_topic, fb->buf + ((NoI*chunkSize)), (totalSize-(NoI*chunkSize)), false, 1);
      Serial.println("Sent last bytes");


    // Send "end" marker
    mqtt.publish(mqtt_topic, "END", false, 1);
    Serial.println("Sent END marker");

    esp_camera_fb_return(fb);  // Free memory
}

void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;

  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; 
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 4;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV2640_PID) {
    Serial.println("Detected Camera Lens: OV2640");
  }


  // Connect WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Connect MQTT
  mqtt.begin(client);
  MQTTconnect();

    // Subscribe callback which is called when every packet has come
  mqtt.subscribe([](const String& topic, const String& payload, const size_t size) {
      Serial.println("mqtt received: " + topic + " - " + payload);
  });

    // Subscribe Topic and Callback Example
    mqtt.subscribe("ESP32/Cam/Result", [](const String& payload, const size_t size) {

        // Print the incoming Result 
        Serial.print("ESP32/Cam/Result :: ");
        Serial.println(payload);


    });

}

void loop() {
  mqtt.update();  

  // Function to Publish Image
  publishImageWithMarkers();  

}