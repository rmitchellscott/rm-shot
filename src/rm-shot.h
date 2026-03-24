#ifndef RM_SHOT_H
#define RM_SHOT_H

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Captures a screenshot and saves it as a PNG file.
 *
 * This function captures the current framebuffer contents and saves them to a PNG file
 * with a timestamped filename in the format: screenshot_YYYY-MM-DD_HH-MM-SS.png
 *
 * @param basePath Directory where the screenshot will be saved. The directory will be
 *                 created if it doesn't exist.
 * @param delay_ms Milliseconds to wait before capturing the screenshot. Pass 0 for
 *                 immediate capture.
 * @param output_path Optional buffer to receive the full path of the saved screenshot.
 *                    Pass NULL if you don't need the path. If provided, buffer must be
 *                    at least PATH_MAX bytes.
 *
 * @return 1 on success, 0 on failure
 *
 * Thread-safe: This function uses internal locking and can be called from multiple threads.
 *
 * Requirements:
 *   - The framebuffer-spy extension must be loaded
 *
 * Example usage:
 *
 *   // Simple case - just capture, don't need the filename
 *   if (capture("/home/root/screenshots", 0, NULL)) {
 *       printf("Screenshot captured successfully\n");
 *   }
 *
 *   // Capture with delay and get the filename
 *   char path[PATH_MAX];
 *   if (capture("/home/root/screenshots", 500, path)) {
 *       printf("Screenshot saved to: %s\n", path);
 *   }
 *
 * Usage from other xovi extensions:
 *
 * In your .xovi file:
 *   depends-on rm-shot:0.3.0
 *   import rm-shot$capture
 *
 * In your C code:
 *   #include "xovi.h"
 *
 *   void take_screenshot() {
 *       char path[PATH_MAX];
 *       int result = rm_shot$capture("/home/root", 0, path);
 *       if (result) {
 *           fprintf(stderr, "Screenshot: %s\n", path);
 *       }
 *   }
 */
int capture(const char* basePath, int delay_ms, char* output_path);

#ifdef __cplusplus
}
#endif

#endif // RM_SHOT_H
