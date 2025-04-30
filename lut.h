#ifndef LUT_H
#define LUT_H

#include <vector>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "Arduino.h"
#include "FS.h"
#include "SD_MMC.h"
#include "esp_camera.h"
#include "esp_jpg_decode.h"
#include "img_converters.h"

// Structure for LUT color values
struct Vec3f
{
    float r, g, b;
};

// Function declarations
std::vector<Vec3f> read3DLUTfromSD(const char *filePath, int &lutSize);
void applyLUTFilter(uint8_t *image_data, int width, int height, const std::vector<Vec3f> &lut, int lutSize);
bool applyLUTToBuffer(uint8_t *rgb_buffer, int width, int height, const char *lutPath);
void checkRawRGBValues(uint8_t *rgb_buffer, int width, int height);

#endif // LUT_H
