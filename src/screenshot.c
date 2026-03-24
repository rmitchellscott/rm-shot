#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifndef VERSION
#define VERSION "unknown"
#endif

const char *rm_shot_version = "rm-shot version " VERSION;

#include "xovi.h"

#define FBSPY_TYPE_RGB565 1
#define FBSPY_TYPE_RGBA 2

struct FramebufferConfig {
    void *framebufferAddress;
    int width, height, type, bpl;
    _Bool requiresReload;
};

static void debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}

typedef struct {
    void* framebufferAddress;
    int width;
    int height;
    int displayWidth;
    int bytesPerPixel;
    int isRGBA;
    const char* name;
} DeviceInfo;

static DeviceInfo detectDevice(void)
{
    struct FramebufferConfig fbConfig = ((struct FramebufferConfig (*)()) framebuffer_spy$getFramebufferConfig)();

    int displayWidth = fbConfig.width;
    const char* name = "Unknown";

    FILE* f = fopen("/sys/devices/soc0/machine", "r");
    if (f) {
        char machine[64] = {0};
        if (fgets(machine, sizeof(machine), f)) {
            for (char* p = machine; *p; p++) {
                if (*p >= 'A' && *p <= 'Z') *p += 32;
            }
        }
        fclose(f);

        if (strstr(machine, "chiappa")) {
            displayWidth = 954;
            name = "Paper Pro Move";
        } else if (strstr(machine, "ferrari")) {
            displayWidth = 1620;
            name = "Paper Pro";
        } else if (strstr(machine, "remarkable 1.0") || strstr(machine, "remarkable prototype 1")) {
            displayWidth = fbConfig.width - 4;
            name = "RM1";
        } else {
            name = "RM2";
        }
    }

    DeviceInfo dev;
    dev.framebufferAddress = fbConfig.framebufferAddress;
    dev.isRGBA = (fbConfig.type == FBSPY_TYPE_RGBA);
    dev.bytesPerPixel = dev.isRGBA ? 4 : 2;
    dev.width = dev.isRGBA ? (fbConfig.bpl >> 2) : (fbConfig.bpl >> 1);
    dev.height = fbConfig.height;
    dev.displayWidth = displayWidth;
    dev.name = name;
    return dev;
}

static unsigned char* readFramebuffer(void* address, DeviceInfo device)
{
    size_t fbSize = device.width * device.height * device.bytesPerPixel;
    unsigned char* buffer = malloc(fbSize);
    if (!buffer) return NULL;

    memcpy(buffer, address, fbSize);
    return buffer;
}

// Convert RGB565 to RGB888
static unsigned char* convertRGB565toRGB888(unsigned char* rgb565, int width, int height, int displayWidth)
{
    size_t pixelCount = displayWidth * height;
    unsigned char* rgb888 = malloc(pixelCount * 3);
    if (!rgb888) return NULL;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < displayWidth; x++) {
            size_t srcIdx = y * width + x;
            size_t dstIdx = y * displayWidth + x;
            unsigned short pixel = ((unsigned short*)rgb565)[srcIdx];

            // RGB565: RRRRR GGGGGG BBBBB
            unsigned char r = (pixel >> 11) & 0x1F;
            unsigned char g = (pixel >> 5) & 0x3F;
            unsigned char b = pixel & 0x1F;

            // Scale to 8-bit
            rgb888[dstIdx * 3 + 0] = (r * 255) / 31;
            rgb888[dstIdx * 3 + 1] = (g * 255) / 63;
            rgb888[dstIdx * 3 + 2] = (b * 255) / 31;
        }
    }

    return rgb888;
}

// Convert BGRA to RGB (swap R and B, drop A)
static unsigned char* convertBGRAtoRGB(unsigned char* bgra, int width, int height, int displayWidth)
{
    size_t pixelCount = displayWidth * height;
    unsigned char* rgb = malloc(pixelCount * 3);
    if (!rgb) return NULL;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < displayWidth; x++) {
            size_t srcIdx = (y * width + x) * 4;
            size_t dstIdx = (y * displayWidth + x) * 3;
            rgb[dstIdx + 0] = bgra[srcIdx + 2]; // R = B
            rgb[dstIdx + 1] = bgra[srcIdx + 1]; // G = G
            rgb[dstIdx + 2] = bgra[srcIdx + 0]; // B = R
        }
    }

    return rgb;
}

static int mkdirp(const char* path)
{
    char tmp[512];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                debug_log("[rm-shot]: Failed to create directory: %s\n", tmp);
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        debug_log("[rm-shot]: Failed to create directory: %s\n", tmp);
        return -1;
    }

    return 0;
}

int takeScreenshot(const char* basePath)
{
    mkdirp(basePath);

    DeviceInfo device = detectDevice();

    void* fbAddr = device.framebufferAddress;
    if (!fbAddr) {
        debug_log("[rm-shot]: Cannot capture - framebuffer address not available\n");
        return 0;
    }

    if ((void*)framebuffer_spy$refreshFramebuffer) {
        ((void (*)()) framebuffer_spy$refreshFramebuffer)();
    }

    unsigned char* fbData = readFramebuffer(fbAddr, device);
    if (!fbData) {
        debug_log("[rm-shot]: Failed to read framebuffer\n");
        return 0;
    }

    unsigned char* rgb = NULL;
    if (device.isRGBA) {
        rgb = convertBGRAtoRGB(fbData, device.width, device.height, device.displayWidth);
    } else {
        rgb = convertRGB565toRGB888(fbData, device.width, device.height, device.displayWidth);
    }
    free(fbData);

    if (!rgb) {
        return 0;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);

    char filename[512];
    snprintf(filename, sizeof(filename), "%s/screenshot_%s.png", basePath, timestamp);

    int result = stbi_write_png(filename, device.displayWidth, device.height, 3, rgb, device.displayWidth * 3);
    free(rgb);

    if (result) {
        debug_log("[rm-shot]: Screenshot saved successfully to: %s\n", filename);
        return 1;
    } else {
        debug_log("[rm-shot]: Failed to save screenshot to: %s\n", filename);
        return 0;
    }
}

// Xovi constructor - called when extension loads
void _xovi_construct() {
    debug_log("[rm-shot]: Extension loaded\n");
}

typedef struct {
    char* path;
    int delay_ms;
} ScreenshotThreadArgs;

void* screenshotThread(void* arg) {
    ScreenshotThreadArgs* args = (ScreenshotThreadArgs*)arg;

    if (args->delay_ms > 0) {
        usleep(args->delay_ms * 1000);
    }

    takeScreenshot(args->path);

    free(args->path);
    free(args);
    return NULL;
}

// Message broker handler - called from QML via xovi-message-broker
char* screenshotHandler(const char* param)
{
    const char* input = (param && param[0]) ? param : "/home/root,0";
    char* path = NULL;
    int delay_ms = 0;

    const char* comma = strchr(input, ',');
    if (comma) {
        size_t pathLen = comma - input;
        path = malloc(pathLen + 1);
        memcpy(path, input, pathLen);
        path[pathLen] = '\0';
        delay_ms = atoi(comma + 1);
    } else {
        path = strdup(input);
        delay_ms = 0;
    }

    ScreenshotThreadArgs* args = malloc(sizeof(ScreenshotThreadArgs));
    args->path = path;
    args->delay_ms = delay_ms;

    pthread_t thread;
    if (pthread_create(&thread, NULL, screenshotThread, args) == 0) {
        pthread_detach(thread);
        return strdup("success");
    } else {
        debug_log("[rm-shot]: Failed to create screenshot thread\n");
        free(path);
        free(args);
        return strdup("failed");
    }
}
