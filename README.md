# rm-shot

A xovi extension that provides screenshot functionality for reMarkable tablets.

## Features

- Automatic device detection (RM2, Paper Pro, Paper Pro Move)
- Timestamped screenshot filenames (`screenshot_YYYY-MM-DD_HH-mm-ss.png`)
- Configurable delay for UI animations to complete

## Dependencies

- **xovi** - Extension framework
- **framebuffer-spy** - Required to access the framebuffer address
- **xovi-message-broker** - Required for QML/C communication

## Installation

1. Ensure dependencies are installed
2. Copy the correct `.so` file for your architecture to `/home/root/xovi/extensions.d/`
3. Restart xovi

## QML Usage

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

## Parameter Format

The parameter to `takeScreenshot` follows the format: `"path,delay_ms"` or just `"path"`

- **path**: Directory where screenshots will be saved
- **delay_ms** (optional): Milliseconds to wait before capturing (useful for closing UI elements)

Examples:
- `"/home/root"` - Immediate capture, saves to /home/root/
- `"/home/root/screenshots,100"` - Wait 100ms, saves to /home/root/screenshots/
- `"/mnt/usb,250"` - Wait 250ms, saves to /mnt/usb/

The directory will be created automatically if it doesn't exist.

## Building

```bash
make
```

This builds both architectures:
- `rm-shot-aarch64.so` - For reMarkable Paper Pro / Paper Pro Move
- `rm-shot-armv7.so` - For reMarkable 2

## Integration Example

See `screenshotButton.qmd` for a complete example of integrating rm-shot into the reMarkable Quick Settings menu.

## License

MIT
