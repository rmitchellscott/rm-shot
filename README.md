# rm-shot
i[![rm1](https://img.shields.io/badge/rM1-supported-green)](https://remarkable.com/store/remarkable)
[![rm2](https://img.shields.io/badge/rM2-supported-green)](https://remarkable.com/store/remarkable-2)
[![rmpp](https://img.shields.io/badge/rMPP-supported-green)](https://remarkable.com/store/overview/remarkable-paper-pro)
[![rmppm](https://img.shields.io/badge/rMPPM-supported-green)](https://remarkable.com/products/remarkable-paper/pro-move)
[![vellum](https://img.shields.io/badge/vellum-rm--shot-purple)](https://vellum.delivery/#/package/rm-shot/)
<img src="assets/rm-shot.svg" alt="rm-shot Icon" width="125" align="right">
<p align="justify">
A xovi extension that provides screenshot functionality for reMarkable tablets.
</p>

<br clear="right">

## Just Need To Install This To Use Something Else?
### Dependencies

- [xovi](https://github.com/asivery/rm-xovi-extensions/blob/master/INSTALL.MD) - Extension framework
    - framebuffer-spy - Required to access the framebuffer address
    - xovi-message-broker - Required for QML/C communication

### Installation

1. Ensure dependencies are installed
2. Download the `.so` file for your architecture from the [latest release](https://github.com/rmitchellscott/rm-shot/releases/latest) and place it in `/home/root/xovi/extensions.d/` on your reMarkable tablet
    - **reMarkable 2**: `rm-shot-armv7.so`
    - **reMarkable Paper Pro**: `rm-shot-aarch64.so`
3. Restart xovi

### Checking Version

To verify which version of rm-shot is installed:

```bash
strings /home/root/xovi/extensions.d/rm-shot-*.so | grep "rm-shot version"
```

## Want To Build Something Using This?
### Features

- Automatic device detection
- Timestamped screenshot filenames (`screenshot_YYYY-MM-DD_HH-mm-ss.png`)
- Configurable screenshot destination
- Configurable delay for UI animations to complete

### Parameter Format

The parameter format is `"path[,delay_ms[,rotation[,left,top,right,bottom]]]"`.

- **path**: Directory for a timestamped file, or a `.png` filename to save exactly there. Missing directories are created.
- **delay_ms** (optional): Milliseconds to wait before capturing (useful for closing UI elements).
- **rotation** (optional): `0`-`3`, matching xochitl's orientation (`1` = 90°). `0` or omitted means no rotation.
- **left,top,right,bottom** (optional): Crop rectangle in the rotated image's coordinates. All four required.

Examples:
- `"/home/root"` - Immediate capture, saves to /home/root/
- `"/home/root/screenshots,100"` - Wait 100ms, saves to /home/root/screenshots/
- `"/home/root/shot.png"` - Saves to that exact file
- `"/home/root,0,1"` - Rotate for a device at 90°
- `"/home/root,0,0,10,10,200,300"` - Crop to (10,10)-(200,300)

### Shell Usage

```bash
echo '>etakeScreenshot:/home/root/screenshots,100' > /run/xovi-mb; cat /run/xovi-mb-out
```

### QML Usage

```qml
import net.asivery.XoviMessageBroker 2.0

Item {
    XoviMessageBroker {
        id: screenshotBroker
    }

    Button {
        onClicked: {
            // Basic usage (no delay, saves to /home/root)
            var result = screenshotBroker.sendSimpleSignal("takeScreenshot", "/home/root");

            // With delay (waits 100ms before capture)
            var result = screenshotBroker.sendSimpleSignal("takeScreenshot", "/home/root/screenshots,100");

            // Check result
            if (result === "success") {
                console.log("Screenshot thread started");
            } else {
                console.log("Screenshot failed to start");
            }
        }
    }
}
```

### Building

```bash
make
```

This builds both architectures:
- `rm-shot-aarch64.so` - For reMarkable Paper Pro
- `rm-shot-armv7.so` - For reMarkable 2

### License

Copyright (C) 2025 Mitchell Scott

Licensed under the GNU General Public License v3.0.
