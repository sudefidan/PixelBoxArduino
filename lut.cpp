#include "LUT.h"

// Function to read 3D LUT from SD card
std::vector<Vec3f> read3DLUTfromSD(const char *filePath, int &lutSize) {
  std::vector<Vec3f> lut;
  File file = SD_MMC.open(filePath);
  if (!file) {
    Serial.println("Failed to open LUT file");
    return lut;
  }


  int index = 0;
  const size_t bufferSize = 1024;
  char buffer[bufferSize];
  size_t bytesRead;

  while ((bytesRead = file.readBytes(buffer, bufferSize)) > 0) {
    size_t start = 0;
    for (size_t i = 0; i < bytesRead; ++i) {
      if (buffer[i] == '\n' || i == bytesRead - 1) {
        buffer[i] = '\0';  // Null-terminate the line
        char *line = buffer + start;
        start = i + 1;

        // Trim leading and trailing whitespace
        while (isspace(*line))
          ++line;
        char *end = line + strlen(line) - 1;
        while (end > line && isspace(*end))
          --end;
        *(end + 1) = '\0';

        // Skip comments and empty lines
        if (line[0] == '#' || strlen(line) == 0)
          continue;

        if (strncmp(line, "LUT_3D_SIZE", 11) == 0) {
          sscanf(line + 11, "%d", &lutSize);
          int maxLutSize = 33;  // Limit size for memory reasons
          if (lutSize > maxLutSize) {
            lutSize = maxLutSize;
          }
          lut.resize(lutSize * lutSize * lutSize);
          Serial.printf("LUT size: %d\n", lutSize);
          continue;
        }

        if (index < lutSize * lutSize * lutSize) {
          float r, g, b;
          sscanf(line, "%f %f %f", &r, &g, &b);
          lut[index++] = { r, g, b };
        }
      }
    }
  }

  file.close();
  Serial.printf("Loaded %d LUT entries\n", lut.size());
  return lut;
}


// Function to apply LUT filter
void applyLUTFilter(uint8_t *image_data, int width, int height, const std::vector<Vec3f> &lut, int lutSize) {
  Serial.println("Applying LUT filter...");

  // Check available memory before starting process
  size_t freeHeap = ESP.getFreeHeap();
  Serial.printf("Free heap before processing: %d bytes\n", freeHeap);

  // Calculate size of image data
  size_t imageDataSize = width * height * 3;
  Serial.printf("Image dimensions: %d x %d (%d bytes)\n", width, height, imageDataSize);

  // Debug LUT data
  Serial.printf("LUT size: %d (total entries: %d)\n", lutSize, (int)lut.size());
  if (lut.size() > 0) {
    Serial.printf("First LUT entry: R=%.6f, G=%.6f, B=%.6f\n", lut[0].r, lut[0].g, lut[0].b);
    int mid = lut.size() / 2;
    Serial.printf("Middle LUT entry: R=%.6f, G=%.6f, B=%.6f\n", lut[mid].r, lut[mid].g, lut[mid].b);
    Serial.printf("Last LUT entry: R=%.6f, G=%.6f, B=%.6f\n", lut[lut.size()-1].r, lut[lut.size()-1].g, lut[lut.size()-1].b);
  }

  for (int y = 0; y < height; y++) {
    if (y % 50 == 0) {
      Serial.printf("Processing row %d of %d\n", y, height);

      // Release some memory and monitor heap periodically
      if (y > 0 && y % 200 == 0) {
        yield();
        Serial.printf("Free heap at row %d: %d bytes\n", y, ESP.getFreeHeap());
      }
    }

    for (int x = 0; x < width; x++) {
      int index = (y * width + x) * 3;

      // Validate memory access within bounds
      if (index + 2 >= imageDataSize) {
        Serial.printf("WARNING: Index out of bounds at (%d,%d), index=%d, max=%d\n",
                      x, y, index + 2, imageDataSize - 1);
        continue;
      }

      // Normalize RGB values to the LUT space (0 to lutSize-1)
      float r = image_data[index] / 255.0f * (lutSize - 1);
      float g = image_data[index + 1] / 255.0f * (lutSize - 1);
      float b = image_data[index + 2] / 255.0f * (lutSize - 1);

      // Get the coordinates of the 8 surrounding points in the cube
      int r0 = std::max(0, std::min(static_cast<int>(std::floor(r)), lutSize - 1));
      int g0 = std::max(0, std::min(static_cast<int>(std::floor(g)), lutSize - 1));
      int b0 = std::max(0, std::min(static_cast<int>(std::floor(b)), lutSize - 1));
      
      int r1 = std::max(0, std::min(r0 + 1, lutSize - 1));
      int g1 = std::max(0, std::min(g0 + 1, lutSize - 1));
      int b1 = std::max(0, std::min(b0 + 1, lutSize - 1));
      
      // Calculate the fractional components for interpolation
      float rDelta = r - r0;
      float gDelta = g - g0;
      float bDelta = b - b0;
      
      // Calculate the array indices for the 8 corner points.
      // index = r + g * lutSize + b * lutSize * lutSize
      int i000 = r0 + g0 * lutSize + b0 * lutSize * lutSize;
      int i001 = r0 + g0 * lutSize + b1 * lutSize * lutSize;
      int i010 = r0 + g1 * lutSize + b0 * lutSize * lutSize;
      int i011 = r0 + g1 * lutSize + b1 * lutSize * lutSize;
      int i100 = r1 + g0 * lutSize + b0 * lutSize * lutSize;
      int i101 = r1 + g0 * lutSize + b1 * lutSize * lutSize;
      int i110 = r1 + g1 * lutSize + b0 * lutSize * lutSize;
      int i111 = r1 + g1 * lutSize + b1 * lutSize * lutSize;
      
      // Bounds check
      int maxIndex = lutSize * lutSize * lutSize - 1;
      if (i000 < 0 || i000 > maxIndex || i111 < 0 || i111 > maxIndex) {
        continue;
      }
      
      // Get the 8 corner values
      const Vec3f& c000 = lut[i000];
      const Vec3f& c001 = lut[i001];
      const Vec3f& c010 = lut[i010];
      const Vec3f& c011 = lut[i011];
      const Vec3f& c100 = lut[i100];
      const Vec3f& c101 = lut[i101];
      const Vec3f& c110 = lut[i110];
      const Vec3f& c111 = lut[i111];

      // Perform trilinear interpolation for each channel
      // This calculates the weighted average of the 8 corners based on the distance
      float rScaled = 
        c000.r * (1 - rDelta) * (1 - gDelta) * (1 - bDelta) +
        c100.r * rDelta * (1 - gDelta) * (1 - bDelta) +
        c010.r * (1 - rDelta) * gDelta * (1 - bDelta) +
        c110.r * rDelta * gDelta * (1 - bDelta) +
        c001.r * (1 - rDelta) * (1 - gDelta) * bDelta +
        c101.r * rDelta * (1 - gDelta) * bDelta +
        c011.r * (1 - rDelta) * gDelta * bDelta +
        c111.r * rDelta * gDelta * bDelta;

      float gScaled = 
        c000.g * (1 - rDelta) * (1 - gDelta) * (1 - bDelta) +
        c100.g * rDelta * (1 - gDelta) * (1 - bDelta) +
        c010.g * (1 - rDelta) * gDelta * (1 - bDelta) +
        c110.g * rDelta * gDelta * (1 - bDelta) +
        c001.g * (1 - rDelta) * (1 - gDelta) * bDelta +
        c101.g * rDelta * (1 - gDelta) * bDelta +
        c011.g * (1 - rDelta) * gDelta * bDelta +
        c111.g * rDelta * gDelta * bDelta;

      float bScaled = 
        c000.b * (1 - rDelta) * (1 - gDelta) * (1 - bDelta) +
        c100.b * rDelta * (1 - gDelta) * (1 - bDelta) +
        c010.b * (1 - rDelta) * gDelta * (1 - bDelta) +
        c110.b * rDelta * gDelta * (1 - bDelta) +
        c001.b * (1 - rDelta) * (1 - gDelta) * bDelta +
        c101.b * rDelta * (1 - gDelta) * bDelta +
        c011.b * (1 - rDelta) * gDelta * bDelta +
        c111.b * rDelta * gDelta * bDelta;

      // For grayscale, ensure all channels are equal to avoid color artifacts
      // Calculate average value
      float avg = (rScaled + gScaled + bScaled) / 3.0f;

      // Clamp values between 0 and 1 to prevent overflow
      avg = std::max(0.0f, std::min(1.0f, avg));
      
      // Assign the same average value to all channels for true grayscale
      image_data[index] = static_cast<uint8_t>(avg * 255);
      image_data[index + 1] = static_cast<uint8_t>(avg * 255);
      image_data[index + 2] = static_cast<uint8_t>(avg * 255);
    }
  }

  // Final heap status
  Serial.printf("Free heap after processing: %d bytes\n", ESP.getFreeHeap());
  Serial.println("LUT filter applied.");
}


void checkRawRGBValues(uint8_t *rgb_buffer, int width, int height) {
  Serial.println("Checking Raw RGB Values of a Pixel ");
  int x_mid = width / 2;
  int y_mid = height / 2;
  int index_mid = (y_mid * width + x_mid) * 3;

  uint8_t r_mid = rgb_buffer[index_mid];
  uint8_t g_mid = rgb_buffer[index_mid + 1];
  uint8_t b_mid = rgb_buffer[index_mid + 2];

  Serial.printf("Center pixel RGB: R=%d, G=%d, B=%d\n", r_mid, g_mid, b_mid);
}

bool applyLUTToBuffer(uint8_t *rgb_buffer, int width, int height, const char *lutPath) {
  // Load LUT from SD card
  int lutSize;
  std::vector<Vec3f> lut = read3DLUTfromSD(lutPath, lutSize);
  if (lut.empty()) {
    Serial.println("Error: Failed to load LUT file");
    return false;
  }

  Serial.println("LUT file loaded successfully");

  // Check Raw RGB values
  //checkRawRGBValues(rgb_buffer, width, height);

  // Apply the LUT filter to the RGB buffer
  applyLUTFilter(rgb_buffer, width, height, lut, lutSize);

  return true;
}
