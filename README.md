# ESP32 Web Controller Sketch

Control your ESP32 and OLED display remotely from anywhere using [espcon.vercel.app](https://espcon.vercel.app)

## Features

- **WiFiManager Integration** - Easy WiFi setup via captive portal (no hardcoded credentials)
- **Multiple Display Modes:**
  - OLED Text Display with animations
  - Pixel Paint Canvas (64x64)
  - Weather Monitor
  - Image to Pixel Converter
- **Real-time Control** - Changes sync instantly from the web app
- **Secure API Key Authentication** - Each device uses a unique API key

## Compatible OLED Displays

This sketch supports **128x64 OLED displays** using the U8g2 library. The following controllers are compatible:

| Controller | Interface | Tested |
|------------|-----------|--------|
| **SH1106** | I2C | Yes |
| **SSD1306** | I2C | Yes |
| **SH1107** | I2C | Yes |
| **SSD1309** | I2C | Yes |

### Changing OLED Controller

If you're using a different OLED controller, modify line 18 in the sketch:

```cpp
// For SH1106 (default):
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// For SSD1306:
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// For SH1107:
U8G2_SH1107_128X64_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// For SSD1309:
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
```

## Wiring Diagram

### ESP32 to OLED Connection (I2C)

```
ESP32           OLED Display
─────           ────────────
3.3V    ───────► VCC
GND     ───────► GND
GPIO 21 ───────► SDA
GPIO 22 ───────► SCL
```

### Pin Reference Table

| ESP32 Pin | OLED Pin | Description |
|-----------|----------|-------------|
| 3.3V | VCC | Power supply (3.3V) |
| GND | GND | Ground |
| GPIO 21 | SDA | I2C Data |
| GPIO 22 | SCL | I2C Clock |

> **Note:** Some OLED modules have VCC and GND swapped. Always verify your module's pinout before connecting!

## Installation

### Prerequisites

1. [Arduino IDE](https://www.arduino.cc/en/software) or [Arduino CLI](https://arduino.github.io/arduino-cli/)
2. ESP32 Board Support Package
3. Required Libraries:
   - U8g2
   - WiFiManager (by tzapu)
   - ArduinoJson
   - HTTPClient (included with ESP32)

### Installing Libraries

**Arduino IDE:**
1. Go to `Sketch > Include Library > Manage Libraries`
2. Search and install:
   - `U8g2` by oliver
   - `WiFiManager` by tzapu
   - `ArduinoJson` by Benoit Blanchon

**Arduino CLI:**
```bash
arduino-cli lib install "U8g2"
arduino-cli lib install "WiFiManager"
arduino-cli lib install "ArduinoJson"
```

### Upload to ESP32

1. Clone this repository:
   ```bash
   git clone https://github.com/Bugshan122/ESP32WebConSketch.git
   ```

2. Open `ESPWebController.ino` in Arduino IDE

3. Select Board: `ESP32 Dev Module`

4. Select your COM port

5. Click Upload

**Or using Arduino CLI:**
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 ESPWebController
arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200 -p /dev/cu.usbserial-* ESPWebController
```

## First-Time Setup

1. **Power on the ESP32** - The OLED will show "ESP32 Setup"

2. **Connect to WiFi AP** - Look for `ESP32-Setup` network on your phone/computer

3. **Open Configuration Portal** - Navigate to `192.168.4.1` in your browser

4. **Enter Credentials:**
   - Select your WiFi network
   - Enter WiFi password
   - Enter your ESP API Key from [espcon.vercel.app](https://espcon.vercel.app)
   - Enter your OpenWeatherMap API Key (for weather feature)

5. **Save and Reboot** - ESP32 will connect to your WiFi and start syncing

### Getting Your API Keys

**ESP API Key:**
1. Visit [espcon.vercel.app](https://espcon.vercel.app)
2. Create an account or sign in
3. Go to Settings/Device section
4. Copy your unique API key

**OpenWeatherMap API Key (optional, for weather feature):**
1. Visit [openweathermap.org](https://openweathermap.org/api)
2. Create a free account
3. Go to API Keys section
4. Copy your API key

## Reset WiFi Settings

To clear saved WiFi credentials and reconfigure:

**Long-press the BOOT button for 3+ seconds**

The ESP32 will restart and create the setup access point again.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| OLED not displaying | Check wiring, verify VCC is 3.3V |
| Can't find ESP32-Setup AP | Press BOOT button for 3 sec to reset |
| "API Error" on display | Verify API key is correct |
| "WiFi Failed" | Check credentials, move closer to router |
| Display shows garbage | Wrong OLED controller selected in code |

## Project Structure

```
ESPWebController/
├── ESPWebController.ino    # Main sketch file
├── README.md               # This file
└── LICENSE                 # MIT License
```

## Web Application

Control your ESP32 from the web at **[espcon.vercel.app](https://espcon.vercel.app)**

Features:
- Real-time OLED text control
- Pixel paint canvas
- Weather display configuration
- Image to pixel converter
- Device management

## Contributing

Contributions are welcome! Feel free to:
- Report bugs
- Suggest features
- Submit pull requests

## License

This project is open source and available under the [MIT License](LICENSE).

## Links

- **Web App:** [espcon.vercel.app](https://espcon.vercel.app)
- **Issues:** [GitHub Issues](https://github.com/Bugshan122/ESP32WebConSketch/issues)

---

Made with love for the maker community
