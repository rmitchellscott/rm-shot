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

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} CropRect;

typedef enum {
    ROT_0 = 0,
    ROT_90,
    ROT_180,
    ROT_270
} Rotation;

typedef struct {
    Rotation rotation;
    bool cropEnabled;
    CropRect cropRect;
} ScreenshotOptions;


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

unsigned char *transformRGB(
    const unsigned char *source,
    int sourceWidth,
    int sourceHeight,
    int *imageWidth,
    int *imageHeight,
    const ScreenshotOptions *opt)
{
    int rotatedWidth = sourceWidth;
    int rotatedHeigth = sourceHeight;

    if (opt->rotation == ROT_90 || opt->rotation == ROT_270) {
        rotatedWidth = sourceHeight;
        rotatedHeigth = sourceWidth;
    }

    int left = 0, top = 0, right = rotatedWidth, bottom = rotatedHeigth;

    if (opt && opt->cropEnabled) {
        left = opt->cropRect.left;
        top = opt->cropRect.top;
        right = opt->cropRect.right;
        bottom = opt->cropRect.bottom;

        if (left < 0) left = 0;
        if (top < 0) top = 0;
        if (right > rotatedWidth) right = rotatedWidth;
        if (bottom > rotatedHeigth) bottom = rotatedHeigth;

        if (right <= left || bottom <= top)
            return NULL;
    }

    int finalWidth = right - left;
    int finalHeight = bottom - top;

    unsigned char *finalRgb = malloc(finalWidth * finalHeight * 3);
    if (!finalRgb)
        return NULL;

    for (int y = 0; y < finalHeight; y++) {
        for (int x = 0; x < finalWidth; x++) {

            int rx = x + left;
            int ry = y + top;

            int sx, sy;

            switch (opt->rotation) {

                case ROT_0:
                    sx = rx;
                    sy = ry;
                    break;

                case ROT_90:
                    sx = ry;
                    sy = sourceHeight - 1 - rx;
                    break;

                case ROT_180:
                    sx = sourceWidth - 1 - rx;
                    sy = sourceHeight - 1 - ry;
                    break;

                case ROT_270:
                    sx = sourceWidth - 1 - ry;
                    sy = rx;
                    break;

                default:
                    free(finalRgb);
                    return NULL;
            }

            const unsigned char *sourcePixel =
                source + (sy * sourceWidth + sx) * 3;

            unsigned char *finalPixel =
                finalRgb + (y * finalWidth + x) * 3;

            finalPixel[0] = sourcePixel[0];
            finalPixel[1] = sourcePixel[1];
            finalPixel[2] = sourcePixel[2];
        }
    }

    *imageWidth = finalWidth;
    *imageHeight = finalHeight;

    return finalRgb;
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


char* takeScreenshot(const char* basePath, const ScreenshotOptions* opt)
{
    mkdirp(basePath);

    DeviceInfo device = detectDevice();

    void* fbAddr = device.framebufferAddress;
    if (!fbAddr) {
        debug_log("[rm-shot]: Cannot capture - framebuffer address not available\n");
        return NULL;
    }

    if ((void*)framebuffer_spy$refreshFramebuffer) {
        ((void (*)()) framebuffer_spy$refreshFramebuffer)();
    }

    unsigned char* fbData = readFramebuffer(fbAddr, device);
    if (!fbData) {
        debug_log("[rm-shot]: Failed to read framebuffer\n");
        return NULL;
    }

    int imageWidth = device.displayWidth;
    int imageHeight = device.height;

    unsigned char* rgb = NULL;
    if (device.isRGBA) {
        rgb = convertBGRAtoRGB(fbData, device.width, imageHeight, imageWidth);
    } else {
        rgb = convertRGB565toRGB888(fbData, device.width, imageHeight, imageWidth);
    }
    free(fbData);

    if (!rgb) {
        return NULL;
    }
    int transformedWidth, transformedHeight;
    
    //do the transform
    unsigned char *transformedRgb = transformRGB(rgb, imageWidth,imageHeight, &transformedWidth, &transformedHeight, opt);
    free(rgb);
    rgb = transformedRgb;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);

    char filename[512];
    snprintf(filename, sizeof(filename), "%s/screenshot_%s.png", basePath, timestamp);

    int result = stbi_write_png(filename, transformedWidth, transformedHeight, 3, rgb, transformedWidth * 3);
    free(rgb);

    if (result) {
        debug_log("[rm-shot]: Screenshot saved successfully to: %s\n", filename);
        return strdup(filename);
    } else {
        debug_log("[rm-shot]: Failed to save screenshot to: %s\n", filename);
        return NULL;
    }
}

// Xovi constructor - called when extension loads
void _xovi_construct() {
    debug_log("[rm-shot]: Extension loaded\n");
}

typedef struct {
    char* path;
    int delay_ms;
    ScreenshotOptions *opt;
} ScreenshotThreadArgs;

void* screenshotThread(void* arg) {
    ScreenshotThreadArgs* args = (ScreenshotThreadArgs*)arg;

    if (args->delay_ms > 0) {
        usleep(args->delay_ms * 1000);
    }
    char* filepath;
    filepath = takeScreenshot(args->path, args->opt);


    if (filepath) {
        if ((void*)xovi_message_broker$broadcast)
            ((char *(*)(const char *, const char *))xovi_message_broker$broadcast)("screenshotComplete", filepath);
        free(filepath);
    } else {
        if ((void*)xovi_message_broker$broadcast)
            ((char *(*)(const char *, const char *))xovi_message_broker$broadcast)("screenshotFailed", "");
    }

    free(args->opt);
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
    
    ScreenshotOptions* opt = malloc(sizeof(ScreenshotOptions));
    memset(opt, 0, sizeof(ScreenshotOptions));

    opt->rotation = ROT_0;
    opt->cropEnabled = false;
    
    char* tmp = strdup(input);
    if (!tmp) {
        free(opt);
        return strdup("failed");
    }

    char* saveptr = NULL;
    char* token = strtok_r(tmp, ",", &saveptr);

    if (token) {
        path = strdup(token);
    } else {
        path = strdup("/home/root");
    }

    token = strtok_r(NULL, ",", &saveptr);
    if (token) {
        delay_ms = atoi(token);
    }

    token = strtok_r(NULL, ",", &saveptr);
    if (token) {
        opt->rotation = (Rotation)atoi(token);
    }

    int left, top, right, bottom;
    token = strtok_r(NULL, ",", &saveptr);
    if (!token) {
        opt->cropEnabled = false;
    } else {
        left = atoi(token);
    
        token = strtok_r(NULL, ",", &saveptr);
        if (!token) {
            opt->cropEnabled = false;
        } else {
            top = atoi(token);
    
            token = strtok_r(NULL, ",", &saveptr);
            if (!token) {
                opt->cropEnabled = false;
            } else {
                right = atoi(token);
    
                token = strtok_r(NULL, ",", &saveptr);
                if (!token) {
                    opt->cropEnabled = false;
                } else {
                    bottom = atoi(token);
    
                    opt->cropRect.left = left;
                    opt->cropRect.top = top;
                    opt->cropRect.right = right;
                    opt->cropRect.bottom = bottom;
    
                    opt->cropEnabled = true;
                }
            }
        }
    }

    free(tmp);
        
    ScreenshotThreadArgs* args = malloc(sizeof(ScreenshotThreadArgs));
    args->path = path;
    args->delay_ms = delay_ms;
    args->opt = opt;

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
