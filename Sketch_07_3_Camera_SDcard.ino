#include "esp_camera.h"
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"
#include "ws2812.h"
#include "LUT.h"
#include "sd_read_write.h"
#include "ble_control.h"

#define BUTTON_PIN 0
#define FILTER_TOGGLE_PIN 21  // Button to cycle through filter modes
#define BUZZER_PIN 20         // Buzzer for sound feedback

// Enum to keep track of filter mode
enum FilterMode {
  COLOR_MODE = 0,
  GRAYSCALE_MODE = 1,
  SEPIA_MODE = 2,
  NEGATIVE_MODE = 3,
  WARM_MODE = 4,
  COOL_MODE = 5,
  LUT_MODE = 6,
  FILTER_MODE_COUNT = 7
};

FilterMode currentFilterMode = COLOR_MODE;       // Default to color mode
framesize_t currentResolution = FRAMESIZE_UXGA;  // Default high resolution

// Flag for remote photo capture
volatile bool remotePhotoRequest = false;

// Flag for remote filter change
volatile bool remoteFilterChangeRequest = false;

// BLE command callback function
void handleBLECommand(const std::string &command) {

  if (command == "Connected to PixelBox!") {
    char filterMessage[50];  // Increased size to accommodate longer filter names

    // Get the name of the current filter
    const char *filterName;
    switch (currentFilterMode) {
      case COLOR_MODE:
        filterName = "Normal";
        break;
      case GRAYSCALE_MODE:
        filterName = "Grayscale";
        break;
      case SEPIA_MODE:
        filterName = "Sepia";
        break;
      case NEGATIVE_MODE:
        filterName = "Negative";
        break;
      case WARM_MODE:
        filterName = "Warm";
        break;
      case COOL_MODE:
        filterName = "Cool";
        break;
      case LUT_MODE:
        filterName = "LUT";
        break;
      default:
        filterName = "unknown";
    }

    sprintf(filterMessage, "filter:%s", filterName);
    notifyBLEClients(filterMessage);
  } else if (command == "takephoto") {
    remotePhotoRequest = true;
  } else if (command.substr(0, 6) == "filter") {
    // Extract filter number if provided (e.g., "filter:2")
    if (command.length() > 7 && command[6] == ':') {
      int filterNum = command[7] - '0';
      if (filterNum >= 0 && filterNum < FILTER_MODE_COUNT) {
        currentFilterMode = static_cast<FilterMode>(filterNum);
        applyFilterEffect(currentFilterMode);
      }
    } else {
      remoteFilterChangeRequest = true;  // Just toggle to next filter
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FILTER_TOGGLE_PIN, INPUT_PULLUP);  // Set up the filter toggle button
  pinMode(BUZZER_PIN, OUTPUT);               // Set buzzer pin as output
  digitalWrite(BUZZER_PIN, LOW);             // Ensure buzzer is off at startup

  ws2812Init();
  sdmmcInit();
  // removeDir(SD_MMC, "/camera");
  createDir(SD_MMC, "/camera");
  // listDir(SD_MMC, "/camera", 0);

  if (cameraSetup() == 1) {
    ws2812SetColor(2);
    Serial.println("Camera initialized successfully");
  } else {
    ws2812SetColor(1);
    Serial.println("Camera initialization failed");
    return;
  }

  // Initialise BLE with device name "ESP32Camera"
  if (initialiseBLE("PixelBox")) {
    Serial.println("BLE initialised successfully!");
  } else {
    Serial.println("BLE initialisation failed!");
  }

  Serial.println("Ready to take photos. Press the button to capture.");
  Serial.println("Press filter button to toggle between filter modes.");
  Serial.println("Connect via BLE to control camera remotely.");
}

void loop() {
  // Check BLE status (handle connections/disconnections)
  checkBLEStatus();

  // Check for filter toggle button press
  if (digitalRead(FILTER_TOGGLE_PIN) == LOW || remoteFilterChangeRequest) {
    remoteFilterChangeRequest = false;  // Reset the flag

    if (digitalRead(FILTER_TOGGLE_PIN) == LOW) {
      delay(20);  // Debounce
      if (digitalRead(FILTER_TOGGLE_PIN) == LOW) {
        while (digitalRead(FILTER_TOGGLE_PIN) == LOW)
          ;  // Wait for button release
      }
    }

    // Cycle to the next filter mode
    currentFilterMode = static_cast<FilterMode>((currentFilterMode + 1) % FILTER_MODE_COUNT);

    // Apply the selected filter
    applyFilterEffect(currentFilterMode);

    // Visual feedback with LED and serial output
    switch (currentFilterMode) {
      case COLOR_MODE:
        ws2812SetColor(2);  // Green for color mode
        Serial.println("Filter: Color mode (normal)");
        break;
      case GRAYSCALE_MODE:
        ws2812SetColor(4);  // Blue for grayscale mode
        Serial.println("Filter: Grayscale mode");
        break;
      case SEPIA_MODE:
        ws2812SetColor(5);  // Purple for sepia mode
        Serial.println("Filter: Sepia tone mode");
        break;
      case NEGATIVE_MODE:
        ws2812SetColor(1);  // Red for negative mode
        Serial.println("Filter: Negative mode");
        break;
      case WARM_MODE:
        ws2812SetColor(6);  // Orange for warm mode
        Serial.println("Filter: Warm tone mode");
        break;
      case COOL_MODE:
        ws2812SetColor(7);  // Cyan for cool mode
        Serial.println("Filter: Cool tone mode");
        break;
      case LUT_MODE:
        Serial.println("Filter: LUT mode");
        break;
    }

    // Notify connected BLE clients about the filter change
    char message_filter_change[50];
    sprintf(message_filter_change, "filter:%d", currentFilterMode);
    notifyBLEClients(message_filter_change);
  }

  // Check for photo capture button press or remote request
  if (digitalRead(BUTTON_PIN) == LOW || remotePhotoRequest) {
    remotePhotoRequest = false;  // Reset the flag

    captureAndSavePhoto();

    // Return to the current filter mode color
    switch (currentFilterMode) {
      case COLOR_MODE:
        ws2812SetColor(2);  // Green
        break;
      case GRAYSCALE_MODE:
        ws2812SetColor(4);  // Blue
        break;
      case SEPIA_MODE:
        ws2812SetColor(5);  // Purple
        break;
      case NEGATIVE_MODE:
        ws2812SetColor(1);  // Red
        break;
      case WARM_MODE:
        ws2812SetColor(6);  // Orange
        break;
      case COOL_MODE:
        ws2812SetColor(7);  // Cyan
        break;
      case LUT_MODE:
        break;  // Don't do any color change LUT will handle it.
    }
  }
}

// Apply filter effect using camera hardware settings
void applyFilterEffect(FilterMode mode) {
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    // Reset to default settings first
    s->set_saturation(s, 0);
    s->set_contrast(s, 0);
    s->set_brightness(s, 0);
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);

    // Apply specific filter effect
    switch (mode) {
      case COLOR_MODE:
        // Normal color - already reset above
        s->set_saturation(s, 0);
        s->set_brightness(s, 1);
        break;

      case GRAYSCALE_MODE:
        // Hardware grayscale effect
        s->set_special_effect(s, 2);  // 2 = grayscale
        break;

      case SEPIA_MODE:
        // Sepia-like effect using combination of settings
        s->set_saturation(s, -2);
        s->set_contrast(s, 1);
        s->set_brightness(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        break;

      case NEGATIVE_MODE:
        // Hardware negative effect
        s->set_special_effect(s, 1);  // 1 = negative
        break;

      case WARM_MODE:
        // Warm filter (reddish/yellowish)
        s->set_saturation(s, 1);  // Increase saturation
        s->set_contrast(s, 1);    // Slight contrast boost
        s->set_brightness(s, 1);  // Slightly brighter
        s->set_awb_gain(s, 0);    // Disable auto white balance gain
        s->set_wb_mode(s, 6);     // Use a warmer white balance mode
        break;

      case COOL_MODE:
        // Cool filter (bluish)
        s->set_saturation(s, 1);  // Increase saturation
        s->set_contrast(s, 0);    // Normal contrast
        s->set_brightness(s, 0);  // Normal brightness
        s->set_awb_gain(s, 0);    // Disable auto white balance gain
        s->set_wb_mode(s, 4);     // Use a cooler white balance mode
        break;
    }
  }
}

// Function to capture and save a photo with the current filter
void captureAndSavePhoto() {
  // Add countdown timer of 5 seconds
  Serial.println("Starting 5 second timer...");
  char timerMessage[40];

  for (int i = 5; i > 0; i--) {
    Serial.printf("%d seconds remaining...\n", i);
    sprintf(timerMessage, "%d seconds remaining...", i);
    notifyBLEClients(timerMessage);  // Send notification to connected BLE clients about timer
    delay(1000);                     // Wait 1 second
  }

  Serial.println("Taking photo now!");
  notifyBLEClients("Taking photo now!");

  // Longer beep when taking the photo
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);  // Longer "shutter" sound
  digitalWrite(BUZZER_PIN, LOW);

  camera_fb_t *fb = NULL;

  // Capture the image with the current filter settings
  fb = esp_camera_fb_get();

  if (fb != NULL) {
    digitalWrite(BUZZER_PIN, HIGH);  // Start processing sound - continuous beeping while processing

    int photo_index = readFileNum(SD_MMC, "/camera");

    if (photo_index != -1) {
      String path = "/camera/" + String(photo_index);

      // Add filter type to filename
      switch (currentFilterMode) {
        case COLOR_MODE:
          path += "_normal.jpg";
          break;
        case GRAYSCALE_MODE:
          path += "_gray.jpg";
          break;
        case SEPIA_MODE:
          path += "_sepia.jpg";
          break;
        case NEGATIVE_MODE:
          path += "_negative.jpg";
          break;
        case WARM_MODE:
          path += "_warm.jpg";
          break;
        case COOL_MODE:
          path += "_cool.jpg";
          break;
        case LUT_MODE:
          path += "_lut.jpg";

          // --- Start of LUT handling ---
          // 1. Decode JPEG to RGB
          uint8_t *rgb_buffer = (uint8_t *)malloc(fb->width * fb->height * 3);
          if (!rgb_buffer) {
            Serial.println("Failed to allocate RGB buffer");
            esp_camera_fb_return(fb);
            return;
          }

          Serial.printf("RGB buffer allocated: %d bytes\n", fb->width * fb->height * 3);
          Serial.printf("Current free heap: %d bytes\n", ESP.getFreeHeap());

          if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb_buffer)) {
            Serial.println("Failed to decode JPEG");
            free(rgb_buffer);
            esp_camera_fb_return(fb);
            return;
          }

          // 2. Apply LUT
          if (!applyLUTToBuffer(rgb_buffer, fb->width, fb->height, "/LUTS/33_points.cube")) {
            Serial.println("Failed to apply LUT");
            free(rgb_buffer);
            esp_camera_fb_return(fb);
            return;
          }

          // 3. Encode back to JPEG with higher quality setting
          uint8_t *jpg_buffer = NULL;
          size_t jpg_len = 0;

          Serial.println("Converting processed RGB data to JPEG...");

          // Improved JPEG quality (95 instead of 80) for better output
          const int jpegQuality = 95;
          bool jpg_success = fmt2jpg(rgb_buffer, fb->width * fb->height * 3,
                                     fb->width, fb->height, PIXFORMAT_RGB888,
                                     jpegQuality, &jpg_buffer, &jpg_len);

          // Free RGB buffer as soon as we're done with it
          free(rgb_buffer);
          rgb_buffer = NULL;

          if (!jpg_success || jpg_buffer == NULL) {
            Serial.println("Failed to encode JPEG");
            if (jpg_buffer)
              free(jpg_buffer);
            esp_camera_fb_return(fb);
            return;
          }

          Serial.printf("JPEG encoded successfully: %d bytes (quality: %d)\n", jpg_len, jpegQuality);

          // Save the LUT-processed image directly to SD card
          writejpg(SD_MMC, path.c_str(), jpg_buffer, jpg_len);
          Serial.println("LUT-processed photo saved: " + path);

          // Clean up
          free(jpg_buffer);
          esp_camera_fb_return(fb);

          // Notify connected BLE clients about the new photo
          String formattedMessage = "Photo saved: " + String(path.c_str());
          notifyBLEClients(formattedMessage.c_str());

          // After processing is complete
          digitalWrite(BUZZER_PIN, LOW);
          delay(200);
          digitalWrite(BUZZER_PIN, HIGH);  // Success beep
          delay(200);
          digitalWrite(BUZZER_PIN, LOW);
          delay(100);
          digitalWrite(BUZZER_PIN, HIGH);
          delay(200);
          digitalWrite(BUZZER_PIN, LOW);

          return;  // Exit early, we've already saved the image

          break;
      }

      // For non-LUT filters, save the image with the applied filter (done by hardware)
      writejpg(SD_MMC, path.c_str(), fb->buf, fb->len);
      Serial.println("Photo saved: " + path);

      // Notify connected BLE clients about the new photo
      String formattedMessage = "Photo saved: " + String(path.c_str());
      notifyBLEClients(formattedMessage.c_str());

      // Turn off processing sound and make success sound
      digitalWrite(BUZZER_PIN, LOW);
      delay(200);
      digitalWrite(BUZZER_PIN, HIGH);  // Success beep pattern
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);
    }

    esp_camera_fb_return(fb);
  } else {
    Serial.println("Camera capture failed.");

    // Error sound - three short beeps
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
  }
}

// Function to adjust camera resolution
void setResolution(framesize_t resolution) {
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_framesize(s, resolution);
    currentResolution = resolution;
    Serial.printf("Resolution set to %d\n", resolution);
  }
}

int cameraSetup(void) {
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
  config.xclk_freq_hz = 10000000;

  // Initial resolution - will be adjusted later based on mode
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // If PSRAM IC present, use higher quality settings
  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Print PSRAM size
    Serial.printf("PSRAM found! Size: %d bytes\n", ESP.getFreePsram());
  } else {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;

    Serial.println("No PSRAM found, using reduced resolution");
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return 0;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 1);       // flip it back
  s->set_brightness(s, 1);  // up the brightness just a bit
  s->set_saturation(s, 0);  // lower the saturation
  s->set_brightness(s, 1);  // up the brightness just a bit
  s->set_saturation(s, 0);  // lower the saturation

  Serial.println("Camera configuration complete!");
  return 1;
}