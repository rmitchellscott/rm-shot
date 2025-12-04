# rm-shot
[![rm2](https://img.shields.io/badge/rM2-supported-green)](https://remarkable.com/store/remarkable-2)
[![rmpp](https://img.shields.io/badge/rMPP-supported-green)](https://remarkable.com/store/overview/remarkable-paper-pro)
[![rmppm](https://img.shields.io/badge/rMPPM-supported-green)](https://remarkable.com/products/remarkable-paper/pro-move)
<img src="assets/rm-shot.svg" alt="rm-shot Icon" width="125" align="right">
<p align="justify">
A xovi extension that provides screenshot functionality for reMarkable tablets.
</p>

> [!NOTE]
> Device support is based on currently released versions of the dependencies below.
> - reMarkable 1 is not supported by framebuffer-spy at this time.

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

The parameter to `takeScreenshot` follows the format: `"path,delay_ms"` or just `"path"`

- **path**: Directory where screenshots will be saved
- **delay_ms** (optional): Milliseconds to wait before capturing (useful for closing UI elements)

Examples:
- `"/home/root"` - Immediate capture, saves to /home/root/
- `"/home/root/screenshots,100"` - Wait 100ms, saves to /home/root/screenshots/
- `"/mnt/usb,250"` - Wait 250ms, saves to /mnt/usb/

The directory will be created automatically if it doesn't exist.

### QML Usage

```qml
import net.asivery.XoviMessageBroker 1.0

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

MIT
