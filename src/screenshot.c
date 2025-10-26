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

// Import from framebuffer-spy (optional)
void* getFramebufferAddress() __attribute__((weak));

// Debug logging to stderr only
static void debug_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}

typedef struct {
    int width;
    int height;
    int bytesPerPixel;
    int isRGBA; // 1 for RGBA (Paper Pro), 0 for RGB565 (RM2)
    const char* name;
} DeviceInfo;

static DeviceInfo detectDevice(void)
{
    FILE* f = fopen("/sys/devices/soc0/machine", "r");
    if (!f) {
        DeviceInfo dev = {1404, 1872, 2, 0, "RM2"};
        return dev;
    }

    char machine[64] = {0};
    if (fgets(machine, sizeof(machine), f)) {
        // Convert to lowercase
        for (char* p = machine; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p += 32;
        }
    }
    fclose(f);

    DeviceInfo dev;
    if (strstr(machine, "chiappa")) {
        dev.width = 960;
        dev.height = 1696;
        dev.bytesPerPixel = 4;
        dev.isRGBA = 1;
        dev.name = "Paper Pro Move";
    } else if (strstr(machine, "ferrari")) {
        dev.width = 1632;
        dev.height = 2154;
        dev.bytesPerPixel = 4;
        dev.isRGBA = 1;
        dev.name = "Paper Pro";
    } else {
        dev.width = 1404;
        dev.height = 1872;
        dev.bytesPerPixel = 2;
        dev.isRGBA = 0;
        dev.name = "RM2";
    }
    return dev;
}

static void* getFramebufferAddr(void)
{
    // Get framebuffer address from environment variable set by framebuffer-spy
    const char* envAddr = getenv("FRAMEBUFFER_SPY_EXTENSION_FBADDR");
    if (envAddr) {
        void* parsedAddr = NULL;
        if (sscanf(envAddr, "%p", &parsedAddr) == 1) {
            return parsedAddr;
        }
    }

    return NULL;
}

static unsigned char* readFramebuffer(void* address, DeviceInfo device)
{
    char memPath[64];
    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", getpid());

    int fd = open(memPath, O_RDONLY);
    if (fd < 0) {
        debug_log("[rm-shot]: Failed to open %s\n", memPath);
        return NULL;
    }

    size_t fbSize = device.width * device.height * device.bytesPerPixel;
    unsigned char* buffer = malloc(fbSize);
    if (!buffer) {
        close(fd);
        return NULL;
    }

    if (lseek(fd, (off_t)address, SEEK_SET) == -1) {
        debug_log("[rm-shot]: Failed to seek to framebuffer address\n");
        close(fd);
        free(buffer);
        return NULL;
    }

    ssize_t bytesRead = read(fd, buffer, fbSize);
    close(fd);

    if (bytesRead != (ssize_t)fbSize) {
        debug_log("[rm-shot]: Failed to read framebuffer (read %zd expected %zu)\n", bytesRead, fbSize);
        free(buffer);
        return NULL;
    }

    return buffer;
}

// Convert RGB565 to RGB888
static unsigned char* convertRGB565toRGB888(unsigned char* rgb565, int width, int height)
{
    size_t pixelCount = width * height;
    unsigned char* rgb888 = malloc(pixelCount * 3);
    if (!rgb888) return NULL;

    for (size_t i = 0; i < pixelCount; i++) {
        unsigned short pixel = ((unsigned short*)rgb565)[i];

        // RGB565: RRRRR GGGGGG BBBBB
        unsigned char r = (pixel >> 11) & 0x1F;
        unsigned char g = (pixel >> 5) & 0x3F;
        unsigned char b = pixel & 0x1F;

        // Scale to 8-bit
        rgb888[i * 3 + 0] = (r * 255) / 31;
        rgb888[i * 3 + 1] = (g * 255) / 63;
        rgb888[i * 3 + 2] = (b * 255) / 31;
    }

    return rgb888;
}

// Convert BGRA to RGB (swap R and B, drop A)
static unsigned char* convertBGRAtoRGB(unsigned char* bgra, int width, int height)
{
    size_t pixelCount = width * height;
    unsigned char* rgb = malloc(pixelCount * 3);
    if (!rgb) return NULL;

    for (size_t i = 0; i < pixelCount; i++) {
        rgb[i * 3 + 0] = bgra[i * 4 + 2]; // R = B
        rgb[i * 3 + 1] = bgra[i * 4 + 1]; // G = G
        rgb[i * 3 + 2] = bgra[i * 4 + 0]; // B = R
    }

    return rgb;
}

// Create directory recursively (like mkdir -p)
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
    // Ensure directory exists
    mkdirp(basePath);

    DeviceInfo device = detectDevice();

    void* fbAddr = getFramebufferAddr();
    if (!fbAddr) {
        debug_log("[rm-shot]: Cannot capture - framebuffer address not available\n");
        return 0;
    }

    unsigned char* fbData = readFramebuffer(fbAddr, device);
    if (!fbData) {
        debug_log("[rm-shot]: Failed to read framebuffer\n");
        return 0;
    }

    // Convert to RGB888 format for PNG
    unsigned char* rgb = NULL;
    if (device.isRGBA) {
        rgb = convertBGRAtoRGB(fbData, device.width, device.height);
    } else {
        rgb = convertRGB565toRGB888(fbData, device.width, device.height);
    }
    free(fbData);

    if (!rgb) {
        return 0;
    }

    // Generate timestamp filename
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);

    char filename[512];
    snprintf(filename, sizeof(filename), "%s/screenshot_%s.png", basePath, timestamp);

    // Save PNG
    int result = stbi_write_png(filename, device.width, device.height, 3, rgb, device.width * 3);
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

// Thread argument structure
typedef struct {
    char* path;
    int delay_ms;
} ScreenshotThreadArgs;

// Background thread function
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
// Parameter format: "path" or "path,delay_ms"
// Examples: "/home/root" or "/home/root,100"
char* screenshotHandler(const char* param)
{
    // Parse parameter: "path,delay_ms" or just "path"
    const char* input = (param && param[0]) ? param : "/home/root,0";
    char* path = NULL;
    int delay_ms = 0;

    const char* comma = strchr(input, ',');
    if (comma) {
        // Format: "path,delay_ms"
        size_t pathLen = comma - input;
        path = malloc(pathLen + 1);
        memcpy(path, input, pathLen);
        path[pathLen] = '\0';
        delay_ms = atoi(comma + 1);
    } else {
        // Format: "path" (no delay)
        path = strdup(input);
        delay_ms = 0;
    }

    // Create thread arguments
    ScreenshotThreadArgs* args = malloc(sizeof(ScreenshotThreadArgs));
    args->path = path;
    args->delay_ms = delay_ms;

    // Launch background thread
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
