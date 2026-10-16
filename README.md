# Kibo ✨

*A little digital creature with a memory, a rhythm, and a personality of her own.*

Kibo is Sanket’s tiny desk buddy: expressive eyes on a 128×64 OLED, a curious touch sensor, and a whole lot of character living inside an ESP32-C3. She blinks, gets sleepy, reacts to your taps, and remembers a few things between reboots. No menu maze. Just Kibo, checking in from her little window.

> Small screen. Big feelings. One very opinionated GPIO pin.

## What she gets up to

- Wanders through a collection of animated moods and little idle gestures.
- Reacts to taps, quick tap bursts, and a good long pet.
- Dozes when left alone, then wakes up when you check in.
- Remembers bits of her personality across restarts.
- Drops the occasional tiny pixel-art surprise and shifts her idle moods with her energy and temperament.
- Keeps a tiny arcade tucked into her page carousel.
- Hosts a tiny Wi-Fi setup page and draws its QR code right on the OLED.
- Shows the time, current weather, and a short forecast when connected.

## The bits and wires

| Part | Connection |
| --- | --- |
| ESP32-C3 Super Mini (HW-466AB) | Kibo’s tiny brain |
| SSD1306 OLED, 128×64, I²C address `0x3C` | SDA → GPIO 0, SCL → GPIO 1. Firmware also tries SDA 8 / SCL 9 if needed. |
| TTP223 capacitive touch module | Signal → GPIO 4; active high |

The firmware’s GPIO definitions are left as-is. Board variants can label pins differently, so check your exact board before wiring. Use power levels supported by your OLED and touch breakouts, and keep ESP32-C3 logic levels in mind.

## Bring Kibo to life

1. Install the Arduino IDE and the ESP32 Arduino core. Pick a board profile compatible with the ESP32-C3 Super Mini.
2. Install **Adafruit GFX Library**, **Adafruit SSD1306**, and **Arduino_JSON**. The ESP32 core provides `WiFi`, `WebServer`, `Preferences`, `HTTPClient`, and `Wire`.
3. Keep `Kibo.ino`, `qrcode.c`, and `qrcode.h` together in a sketch folder named `Kibo`. Open `Kibo.ino`, choose the board and port, then build and upload.
4. On first setup, join the **Kibo-Setup** Wi-Fi network (default password: `12345678`) and visit [http://192.168.4.1/](http://192.168.4.1/). Add your Wi-Fi details there. Weather is optional and needs an OpenWeather API key.
5. Change the setup access-point password before sharing the device. Kibo stores Wi-Fi credentials and settings in ESP32 preferences.

### Weather in more than one place

Add up to four cities in Kibo’s setup page. Use a semicolon and a two-letter country code for each city, like this:

```text
London|GB; Tokyo|JP; New York|US
```

You can also enter comma-separated city names and use one shared country code in the country field. On either weather screen, press and hold the touch sensor to move to the next saved city.

### Tiny arcade

Tap through the pages after the forecast to find Kibo’s arcade. A single tap starts the highlighted game; double tap switches between **Flappy Kibo** and **Blink Reflex**. While playing, a single tap is the control: flap through the pipes, or tap as soon as Blink Reflex says **GO!** Long press to leave a game; Kibo reacts to your run when you return. The games use the touch sensor already on the build—no extra buttons needed.

On the face, single tap moves to the next page, double tap gets a reaction, and triple tap sends Kibo to sleep. A long press on the time page opens the world clock. Page changes get a quick little wipe, and Kibo’s idle face can stretch, yawn, glance around, wink, laugh, side-eye, and more.

## In the folder

- `Kibo.ino` — the firmware, setup portal, animations, and Kibo’s behavior.
- `qrcode.c` and `qrcode.h` — the QR encoder used by the setup screen.
- `Kibo.stl` — the printable model, ready for your slicer of choice.

## License & credits

This project is licensed under the [MIT License](file:///Users/sanketbharadwaj/Developer/Kibo/LICENSE) — see the [LICENSE](file:///Users/sanketbharadwaj/Developer/Kibo/LICENSE) file for details.

The QR encoder in `qrcode.c` and `qrcode.h` carries its own MIT license and Richard Moore’s copyright notice. Its header also credits Nayuki as inspiration.
