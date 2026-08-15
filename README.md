# 🖨️ TinyPrinter - Industrial-Grade ESP32 BLE Thermal Printer Library for X5H & POS

[![Arduino IDE Compatible](https://img.shields.io/badge/Arduino_IDE-Compatible-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![Target Architecture](https://img.shields.io/badge/Architecture-ESP32-red?logo=expressif&logoColor=white)](https://www.espressif.com/)
[![BLE Engine](https://img.shields.io/badge/BLE-NimBLE--Arduino-blue)](https://github.com/h2zero/NimBLE-Arduino)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

An advanced, feature-rich, industrial-grade C++ Arduino library for **X5H** and compatible 58mm **Bluetooth Low Energy (BLE) Mini Thermal Printers** built for ESP32 and NimBLE stacks.

---

## 🌟 Key Industrial Features

- **MSB-First Thermal Bit Mapping (Fixes 8-pixel Byte Inversion Bug)**:
  - Aligns raster graphics bits to match ESC/POS and X5H thermal printhead hardware bit ordering.
  - Ensures **100% instant smartphone QR scanning (iOS Camera & Android Lens)** and handheld laser barcode reading!
- **Multi-Level Thermal Printhead Density & Darkness Controls**:
  - `setDensity(DENSITY_ULTRA_DARK)`: Maximize heating pulse duration & contrast for **deep pitch-black, uniform thermal receipts** without gray fading or streaking.
  - `setDarkness(level)`: Fine-tune darkness from 0% to 100%.
- **Complete QR Code Payload Helper Suite**:
  - 📶 `printQRCodeWiFi(ssid, password, authType)`: Generates auto-connect WiFi QR codes. Scans & connects to WiFi without typing passwords!
  - 👤 `printQRCodeVCard(name, phone, email, org)`: Generates contact VCard QR codes. Scans & saves contacts directly to phone address book!
  - 📞 `printQRCodePhone(number)`: Generates instant dial `tel:` URI QR codes.
  - 🌐 `printQRCodeURL(url)` / `printQRCodeText(text)`.
- **Industrial 1D Barcode Suite with Sizing & HRI Text Toggle**:
  - Symbologies: `BARCODE_CODE39`, `BARCODE_EAN13`, `BARCODE_EAN8`, `BARCODE_UPC_A`, `BARCODE_UPC_E`, `BARCODE_CODE128`, `BARCODE_ITF`, `BARCODE_CODABAR`.
  - Presets: `BARCODE_SIZE_SMALL`, `BARCODE_SIZE_MEDIUM`, `BARCODE_SIZE_LARGE` (Ultra-clear 80px height & 4x bar width).
  - HRI Text Toggle: `showText = true` (prints numbers below barcode) vs `showText = false` (barcode bars only).
- **Frame Sizing & Border Thickness**:
  - Presets (`FRAME_SIZE_SMALL`, `FRAME_SIZE_MEDIUM`, `FRAME_SIZE_LARGE`, `FRAME_SIZE_FULL_WIDTH`) or custom explicit width/height/border thickness.
- **Global Object Scaling & Stroke Thickness**:
  - `setGlobalScale(multiplier)`: Multiply the size of **ALL** printed elements at once!
  - `setGlobalThickness(strokeWidth)`: Adjust line weight / stroke thickness across all elements.
- **Inter-Element Spacer Controls**:
  - `setSpacer(pixels)`: Configures a global vertical pixel gap added automatically between printed items.

---

## 📦 Code Examples

### 1. Ultra Dark Thermal Printing & WiFi Auto-Connect QR Code
```cpp
// Set maximum thermal density for pitch-black, uniform dark contrast
printer.setDensity(DENSITY_ULTRA_DARK);

// WiFi Auto-Connect QR Code (Instant scan & connect on iOS / Android)
printer.printQRCodeWiFi("StoreGuestWiFi", "Pass1234", "WPA", QR_LARGE, ALIGN_CENTER);
```

### 2. Contact VCard QR Code
```cpp
// Contact VCard QR Code (Scans and adds contact directly to address book)
printer.printQRCodeVCard("Ramiz Mammadli", "+994501234567", "ramiz@example.com", "TinyPrinter Inc", QR_LARGE, ALIGN_CENTER);
```

### 3. Industrial Barcode Sizing & Text Toggle
```cpp
// Retail EAN-13 Barcode with numbers below (Large size preset)
printer.printBarcode("8690000123456", BARCODE_EAN13, BARCODE_SIZE_LARGE, true /* showText */, ALIGN_CENTER);

// Warehouse Code39 Barcode (Bars only without text)
printer.printBarcode("PALLET-9988", BARCODE_CODE39, BARCODE_SIZE_MEDIUM, false /* showText = false */, ALIGN_CENTER);
```

---

## 🛠️ Complete API Reference

| Method Signature | Description |
| :--- | :--- |
| `bool begin(name / address)` | Connect to BLE thermal printer. |
| `void setDensity(density)` | Set heating density (`DENSITY_LIGHT`, `DENSITY_NORMAL`, `DENSITY_DARK`, `DENSITY_ULTRA_DARK`). |
| `void setDarkness(level)` | Set percentage darkness (0% to 100%). |
| `void setGlobalScale(multiplier)` | Scale ALL printed elements (text, QR, images, shapes) by master multiplier (1x, 2x, 3x). |
| `void setGlobalThickness(strokeWidth)` | Adjust stroke thickness/weight for all lines, borders, and text. |
| `void setSpacer(pixels)` | Set vertical gap in pixels appended after printed elements. |
| `void printQRCodeWiFi(ssid, pass, auth, size)` | Generate WiFi auto-connect QR code (`WIFI:S:SSID;T:WPA;P:PASSWORD;;`). |
| `void printQRCodeVCard(name, phone, email, org, size)` | Generate contact VCard QR code. |
| `void printQRCodePhone(phone, size)` | Generate phone call `tel:` URI QR code. |
| `void printQRCode(text, size, ecc, align)` | ISO/IEC 18004 standard QR generator with GF256 Reed-Solomon ECC. |
| `void printBarcode(code, type, size, showText, align)` | Industrial Barcode generator with size presets & HRI text toggle (`BARCODE_SIZE_LARGE`, `showText`). |
| `void printBarcode(code, type, heightPx, barWidthPx, showText, align)` | Industrial Barcode generator with explicit height & bar width dimensions. |
| `void printFramedText(text, style, sizePreset, borderThickness, scale)` | Print framed text using size preset (`FRAME_SIZE_SMALL`, `FRAME_SIZE_LARGE`, etc.). |
| `void printTableRow(columns, count, scale)` | Print commercial POS multi-column receipt table row. |
| `void printCoupon(title, code, expiry)` | Print promotional coupon voucher badge. |
| `void printCutLine()` | Print paper cut line with scissors icon (`--✂--`). |
| `void feed(uint16_t pixels)` | Advance paper forward. |
| `void requestStatus()` / `requestBattery()` | Query printer telemetry & battery status. |

---

## 📁 Repository Structure

```
TinyPrinter/
├── library.properties      # Arduino IDE v1.5 library properties
├── keywords.txt            # Syntax highlighting keywords
├── README.md               # Complete library documentation
├── src/
│   ├── TinyPrinter.h       # Main header with English Doxygen comments & API
│   ├── TinyPrinter.cpp     # MSB-First Bit Engine, QR Payloads, Barcodes & Density
│   ├── TinyFonts.h         # Built-in font tables (8x8, 5x7, 12x16, 16x24, AZ/UTF-8)
│   ├── tiny_print.h        # Backward compatibility wrapper header
│   └── tiny_print.cpp      # Backward compatibility stub
└── examples/
    ├── BasicPrint/         # Basic text printing & formatting sketch
    ├── CustomFonts/        # Multi-font & custom bitmap icon demonstration
    ├── AdvancedFeatures/   # DevMode, WiFi/VCard QR, Industrial Barcodes & Ultra Dark Density
    ├── FramesAndPOS/       # Frame templates with Sizing/Thickness, POS Tables & QR Codes
    ├── PrinterStatus/      # Telemetry & battery monitoring sketch
    └── BitmapPrint/        # Monochrome bitmap graphic printing sketch
```

---

## 📄 License & Credits

- **Author**: Ramiz Mammadli
- **License**: MIT License. Free for commercial and open-source use.
# tiny_print_library
