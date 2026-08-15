/**
 * @file TinyPrinter.h
 * @brief Industrial-Grade ESP32 BLE Thermal Printer Library for X5H & POS Printers.
 * @author Ramiz Mammadli
 * @version 1.6.0
 * @date 2026-08-15
 * 
 * Features:
 * - MSB-First Thermal Bit Mapping (Fixes 8-pixel horizontal byte-inversion bug for 100% instant QR & Barcode scanning).
 * - Multi-Level Thermal Printhead Density & Darkness Controls (DENSITY_LIGHT, DENSITY_NORMAL, DENSITY_DARK, DENSITY_ULTRA_DARK).
 * - Complete QR Payload Generator Suite (WiFi Instant Connect, Phone Call, Web URL, VCard Contact, Raw Text).
 * - ISO/IEC 18004 Standard QR Code Engine with GF256 Reed-Solomon ECC & 4-module Quiet Zone.
 * - Industrial 1D Barcode Suite (Code39, EAN13, EAN8, UPC-A, UPC-E, Code128, ITF, Codabar) with HRI text toggle & size presets.
 * - 10 Frame templates with size presets & custom dimension/thickness parameters.
 * - Global scaling, thickness modifier, inter-element spacer controls.
 */

#ifndef TINY_PRINTER_H
#define TINY_PRINTER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <functional>
#include "TinyFonts.h"

/// Printer physical width in dot pixels (58mm thermal roll at 203 DPI)
#define X5H_PRINTER_WIDTH_PX 384

/// Printer physical width in bytes (384 pixels / 8 bits per byte = 48 bytes per line)
#define X5H_PRINTER_WIDTH_BYTES 48

/**
 * @brief Text horizontal alignment options.
 */
enum PrintAlign {
  ALIGN_LEFT = 0,    ///< Align to left margin
  ALIGN_CENTER = 1,  ///< Center horizontally across paper
  ALIGN_RIGHT = 2    ///< Align to right margin
};

/**
 * @brief Print density & darkness levels for thermal paper activation.
 */
enum PrintDensity {
  DENSITY_LIGHT = 0,       ///< Low heating energy (saves battery)
  DENSITY_NORMAL = 1,      ///< Standard contrast for general receipt printing
  DENSITY_DARK = 2,        ///< High contrast for bold receipts
  DENSITY_ULTRA_DARK = 3   ///< Maximum heating energy & pulse width for deep pitch-black thermal prints
};

/**
 * @brief Industrial 1D Barcode symbology selections.
 */
enum BarcodeType {
  BARCODE_CODE39 = 0,   ///< Code 39 (Alphanumeric with start/stop *)
  BARCODE_EAN13 = 1,    ///< EAN-13 (13-digit European Article Number with auto checksum & guard bars)
  BARCODE_EAN8 = 2,     ///< EAN-8 (8-digit compact retail barcode)
  BARCODE_UPC_A = 3,    ///< UPC-A (12-digit North American retail barcode)
  BARCODE_UPC_E = 4,    ///< UPC-E (Zero-suppressed compact 8-digit UPC)
  BARCODE_CODE128 = 5,  ///< Code 128 (High-density ASCII Code B & C)
  BARCODE_ITF = 6,      ///< Interleaved 2 of 5 (Numeric paired barcode)
  BARCODE_CODABAR = 7   ///< Codabar / NW-7 (Medical & library barcode)
};

/**
 * @brief Barcode dimension size presets.
 */
enum BarcodeSize {
  BARCODE_SIZE_SMALL = 0,   ///< Small barcode (Height: 30px, Bar Width: 2px)
  BARCODE_SIZE_MEDIUM = 1,  ///< Medium barcode (Height: 50px, Bar Width: 3px)
  BARCODE_SIZE_LARGE = 2,   ///< Large barcode (Height: 80px, Bar Width: 4px - High Readability)
  BARCODE_SIZE_CUSTOM = 3   ///< User-defined custom height and bar width
};

/**
 * @brief QR Code size presets for maximum scanner readability.
 */
enum QRSize {
  QR_SMALL = 0,   ///< Small QR Code (~120px wide, module scale 4x4)
  QR_MEDIUM = 1,  ///< Medium QR Code (~200px wide, module scale 7x7)
  QR_LARGE = 2,   ///< Large QR Code (~280px wide, module scale 10x10 - High Scan Readability)
  QR_CUSTOM = 3   ///< User-defined custom module pixel scale
};

/**
 * @brief QR Code Error Correction Levels (ISO/IEC 18004 standard).
 */
enum QRErrorCorrection {
  QR_ECC_LOW = 0,       ///< Level L (7% error recovery)
  QR_ECC_MEDIUM = 1,    ///< Level M (15% error recovery - Recommended)
  QR_ECC_QUARTILE = 2,  ///< Level Q (25% error recovery)
  QR_ECC_HIGH = 3       ///< Level H (30% error recovery)
};

/**
 * @brief Pre-defined frame and border template styles.
 */
enum FrameStyle {
  FRAME_RECTANGLE = 0,         ///< Solid single-line box
  FRAME_ROUNDED_RECTANGLE = 1, ///< Box with rounded corners
  FRAME_DOUBLE_LINE = 2,       ///< Double-line border box
  FRAME_DASHED_RECTANGLE = 3,  ///< Dashed border box
  FRAME_CIRCLE = 4,            ///< Elliptical / Circle frame
  FRAME_TRIANGLE = 5,          ///< Triangular header frame
  FRAME_DIAMOND = 6,           ///< Diamond border frame
  FRAME_COUPON_TICKET = 7,     ///< Coupon ticket with side cut-out notches
  FRAME_STAR_BURST = 8,        ///< Starburst decorative border box
  FRAME_CUSTOM = 9             ///< User-defined custom frame callback
};

/**
 * @brief Frame dimension size presets.
 */
enum FrameSizePreset {
  FRAME_SIZE_AUTO = 0,       ///< Auto-fit width & height tightly around text
  FRAME_SIZE_SMALL = 1,      ///< Small box (~160px wide)
  FRAME_SIZE_MEDIUM = 2,     ///< Medium box (~250px wide)
  FRAME_SIZE_LARGE = 3,      ///< Large box (~330px wide)
  FRAME_SIZE_FULL_WIDTH = 4  ///< Full paper width (384px wide)
};

/**
 * @brief Structure for custom user frame rendering.
 */
struct CustomFrame {
  const char* name;          ///< Custom frame name
  uint8_t borderPadding;     ///< Padding between text and border in pixels
  /// Callback function to draw custom frame border graphics onto canvas
  std::function<void(uint8_t* canvas, int canvasW, int canvasH, int innerX, int innerY, int innerW, int innerH)> renderFrame;
};

/**
 * @brief Column definition for POS multi-column table rows.
 */
struct TableColumn {
  const char* text;       ///< Column cell text
  uint8_t widthPercent;   ///< Column width percentage (sum of row columns = 100)
  PrintAlign align;       ///< Alignment inside column cell
};

/**
 * @brief Status metrics structure received from printer.
 */
struct X5hStatus {
  bool connected = false;   ///< True if BLE connection is active
  bool coverOpen = false;   ///< True if printer paper cover/door is open
  bool paperOut = false;    ///< True if paper roll is out / exhausted
  bool overheat = false;    ///< True if thermal printhead protection is active
  bool lowBattery = false;  ///< True if battery voltage is critically low
  bool bufferFull = false;  ///< True if print buffer is temporarily full
  uint8_t battery = 0;      ///< Raw battery level index (0 to 15)
  uint8_t lastCmd = 0;      ///< Last executed protocol command code

  /**
   * @brief Convert raw battery level (0-15) into percentage (0-100%).
   * @return Battery level percentage (0 to 100%).
   */
  uint8_t getBatteryPercent() const {
    int pct = map(battery, 0, 15, 0, 100);
    return (uint8_t)constrain(pct, 0, 100);
  }
};

/**
 * @brief Callback function for asynchronous printer status notifications.
 */
using StatusCallback = std::function<void(const X5hStatus&)>;

/**
 * @class X5hPrinter
 * @brief Main class representing an X5H BLE Thermal Printer interface.
 */
class X5hPrinter {
public:
  X5hPrinter();
  ~X5hPrinter();

  // ============================================================
  // 1. CONNECTION MANAGEMENT & DEVELOPER MODE
  // ============================================================

  bool begin(const char* namePrefix = nullptr);
  bool begin(NimBLEAddress address);
  bool autoConnect(const char* targetName = nullptr, const char* targetMac = nullptr);
  void end();
  bool isConnected();

  void setDevMode(bool enable);
  bool getDevMode() const { return devMode; }
  void setCustomBLEUUIDs(const char* serviceUUID, const char* writeUUID, const char* notifyUUID);

  // ============================================================
  // 2. PRINT DENSITY & DARKNESS CONTROLS
  // ============================================================

  /**
   * @brief Set printhead heating density preset (DENSITY_LIGHT, DENSITY_NORMAL, DENSITY_DARK, DENSITY_ULTRA_DARK).
   * @param density Density level for thermal paper activation.
   */
  void setDensity(PrintDensity density);

  /**
   * @brief Set explicit percentage print darkness (0% to 100%).
   * @param level Darkness percentage (0 = lightest, 100 = pitch black).
   */
  void setDarkness(uint8_t level);

  // ============================================================
  // 3. GLOBAL SCALING, THICKNESS & SPACER CONTROLS
  // ============================================================

  void setGlobalScale(uint8_t multiplier);
  uint8_t getGlobalScale() const { return globalScale; }

  void setGlobalThickness(uint8_t strokeWidth);
  uint8_t getGlobalThickness() const { return globalThickness; }

  void setSpacer(uint16_t verticalPixels);
  uint16_t getSpacer() const { return globalSpacer; }

  void printSpacer(uint16_t verticalPixels = 0);

  // ============================================================
  // 4. FONT & STYLING MANAGEMENT
  // ============================================================

  void setFont(FontType font);
  void setCustomFont(const CustomFont* font);
  FontType getFont() const { return activeFont; }

  void setAlign(PrintAlign align);
  void setBold(bool bold);
  void setInverse(bool inverse);
  void setUnderline(bool underline);
  void setStrikethrough(bool strikethrough);

  // ============================================================
  // 5. PRINT CONFIGURATION SETTINGS
  // ============================================================

  void setEnergy(uint16_t value);
  void setQuality(uint8_t value);
  void setDelay(uint16_t ms);
  void setPrintMode(uint8_t mode);

  // ============================================================
  // 6. FRAMES & BORDER TEMPLATES
  // ============================================================

  void printFramedText(const char* text, FrameStyle style = FRAME_RECTANGLE, FrameSizePreset size = FRAME_SIZE_AUTO, uint8_t borderThickness = 1, uint8_t textScale = 1, PrintAlign align = ALIGN_CENTER);
  void printFramedText(const char* text, FrameStyle style, uint16_t customWidthPx, uint16_t customHeightPx, uint8_t borderThickness = 1, uint8_t textScale = 1, PrintAlign align = ALIGN_CENTER);
  void printCustomFramedText(const char* text, const CustomFrame* customFrame, uint8_t scale = 1, PrintAlign align = ALIGN_CENTER);

  // ============================================================
  // 7. COMMERCIAL POS RECEIPT FEATURES
  // ============================================================

  void printTableRow(const TableColumn* columns, size_t count, uint8_t scale = 1);
  void printCoupon(const char* title, const char* code, const char* expiry);
  void printCutLine();

  // ============================================================
  // 8. ADVANCED TEXT & POSITIONING OPERATIONS
  // ============================================================

  void printText(const char* text, uint8_t scale = 1, PrintAlign align = ALIGN_LEFT);
  void printText(const char* text, uint8_t scale, bool center);
  void printText(const String& text, uint8_t scale = 1, PrintAlign align = ALIGN_LEFT);

  void printTextXY(const char* text, int x, int y, uint8_t scale = 1);
  void printTextBounds(const char* text, int x, int y, int width, int height, uint8_t scale = 1, PrintAlign align = ALIGN_LEFT);

  // ============================================================
  // 9. MATRIX & PIXEL GRID PRINTING
  // ============================================================

  void printMatrix(const uint8_t* matrix, int rows, int cols, uint8_t pixelScale = 4, PrintAlign align = ALIGN_CENTER);
  void printPixelGrid(const uint8_t* grid, int rows, int cols, uint8_t pixelScale = 4, PrintAlign align = ALIGN_CENTER) {
    printMatrix(grid, rows, cols, pixelScale, align);
  }

  // ============================================================
  // 10. ISO/IEC 18004 STANDARD QR CODE ENGINE & PAYLOAD HELPERS
  // ============================================================

  void printQRCode(const char* text, QRSize size = QR_MEDIUM, QRErrorCorrection ecc = QR_ECC_MEDIUM, PrintAlign align = ALIGN_CENTER);
  void printQRCode(const char* text, QRSize size, PrintAlign align);
  void printQRCode(const char* text, uint8_t customModuleScale, QRErrorCorrection ecc = QR_ECC_MEDIUM, PrintAlign align = ALIGN_CENTER);
  void printQRCode(const char* text, uint8_t customModuleScale, PrintAlign align);
  void printQRCode(const String& text, QRSize size = QR_MEDIUM, QRErrorCorrection ecc = QR_ECC_MEDIUM, PrintAlign align = ALIGN_CENTER);
  void printQRCode(const String& text, QRSize size, PrintAlign align);

  /**
   * @brief Helper to generate & print Web URL link QR Code.
   */
  void printQRCodeURL(const char* url, QRSize size = QR_MEDIUM, PrintAlign align = ALIGN_CENTER);

  /**
   * @brief Helper to generate & print Phone Call `tel:` URI QR Code.
   */
  void printQRCodePhone(const char* phoneNumber, QRSize size = QR_MEDIUM, PrintAlign align = ALIGN_CENTER);

  /**
   * @brief Helper to generate & print WiFi Auto-Connect QR Code (`WIFI:S:SSID;T:WPA;P:PASSWORD;;`).
   * Smartphones scan and connect to the WiFi network automatically!
   * @param ssid WiFi Network SSID name.
   * @param password WiFi Password.
   * @param authType Encryption type ("WPA", "WEP", or "nopass").
   * @param size QR Size preset.
   * @param align Horizontal alignment.
   */
  void printQRCodeWiFi(const char* ssid, const char* password, const char* authType = "WPA", QRSize size = QR_MEDIUM, PrintAlign align = ALIGN_CENTER);

  /**
   * @brief Helper to generate & print Contact VCard QR Code.
   * Smartphones scan and add the contact directly to address book!
   */
  void printQRCodeVCard(const char* name, const char* phone, const char* email, const char* org = nullptr, QRSize size = QR_MEDIUM, PrintAlign align = ALIGN_CENTER);

  /**
   * @brief Helper to generate & print Raw Text QR Code.
   */
  void printQRCodeText(const char* text, QRSize size = QR_MEDIUM, PrintAlign align = ALIGN_CENTER) {
    printQRCode(text, size, align);
  }

  void printCustomQRCode(const uint8_t* qrBitmap, uint16_t sizePx, QRSize size = QR_MEDIUM, PrintAlign align = ALIGN_CENTER);
  void printCustomQRCode(const uint8_t* qrBitmap, uint16_t sizePx, uint8_t scale, PrintAlign align = ALIGN_CENTER);

  // ============================================================
  // 11. INDUSTRIAL 1D BARCODE SUITE WITH HRI TEXT TOGGLE & SIZING
  // ============================================================

  /**
   * @brief Industrial 1D Barcode generator with size presets and optional Human Readable Interpretation (HRI) text below barcode.
   * @param code Barcode string payload.
   * @param type Barcode type (BARCODE_CODE39, BARCODE_EAN13, BARCODE_EAN8, BARCODE_UPC_A, BARCODE_UPC_E, BARCODE_CODE128, BARCODE_ITF, BARCODE_CODABAR).
   * @param size Barcode size preset (BARCODE_SIZE_SMALL, BARCODE_SIZE_MEDIUM, BARCODE_SIZE_LARGE - High Readability).
   * @param showText True to print numeric/text digits below the barcode, false for barcode bars only.
   * @param align Horizontal alignment across thermal paper.
   */
  void printBarcode(const char* code, BarcodeType type = BARCODE_CODE39, BarcodeSize size = BARCODE_SIZE_MEDIUM, bool showText = true, PrintAlign align = ALIGN_CENTER);

  /**
   * @brief Industrial 1D Barcode generator with explicit custom height and bar module width dimensions.
   * @param code Barcode string payload.
   * @param type Barcode type.
   * @param heightPx Height of barcode bars in dot lines (e.g. 40px, 60px, 80px).
   * @param barWidthPx Width multiplier per bar module (e.g. 2px, 3px, 4px).
   * @param showText True to print numeric/text digits below the barcode, false for barcode bars only.
   * @param align Horizontal alignment.
   */
  void printBarcode(const char* code, BarcodeType type, uint8_t heightPx, uint8_t barWidthPx, bool showText = true, PrintAlign align = ALIGN_CENTER);

  void printBarcode(const char* code, uint8_t heightPx, uint8_t scale = 2, PrintAlign align = ALIGN_CENTER) {
    printBarcode(code, BARCODE_CODE39, heightPx, scale, true, align);
  }

  // ============================================================
  // 12. SEPARATOR LINES & GRAPHICS
  // ============================================================

  void printLine(uint16_t thickness = 2, uint16_t widthPx = 384, PrintAlign align = ALIGN_CENTER);
  void printDashedLine(uint16_t thickness = 2, uint16_t dashLen = 8, uint16_t gapLen = 4);

  void printBitmap(const uint8_t* data, uint16_t widthBytes, uint16_t height);
  void printImage(const uint8_t* data, uint16_t widthPx, uint16_t height);

  void feed(uint16_t pixels = 50);
  void retract(uint16_t pixels = 50);
  void clear();
  void printSample();

  // ============================================================
  // 13. STATUS & TELEMETRY
  // ============================================================

  void enableNotifications(bool enable = true);
  void requestStatus();
  void requestBattery();
  X5hStatus getStatus() const { return status; }
  void onStatus(StatusCallback cb);

  // ============================================================
  // 14. LOW-LEVEL COMMUNICATIONS
  // ============================================================

  bool writeData(const uint8_t* data, size_t length);
  void sendPacket(uint8_t cmd, const uint8_t* payload = nullptr, size_t len = 0);
  void sendPacket(uint8_t cmd, uint8_t b1);
  void sendPacket(uint8_t cmd, uint16_t value);

private:
  NimBLEClient* pClient = nullptr;
  NimBLERemoteService* pService = nullptr;
  NimBLERemoteCharacteristic* pWriteChar = nullptr;
  NimBLERemoteCharacteristic* pNotifyChar = nullptr;

  String serviceUUIDStr;
  String writeUUIDStr;
  String notifyUUIDStr;

  bool connected = false;
  bool bleInitialized = false;
  bool devMode = false;
  String targetName;

  // Print darkness and density settings
  uint16_t energy = 0xFFFF;    ///< Deep black heating energy
  uint8_t quality = 0x45;     ///< High contrast strobe pulse timing
  uint16_t lineDelay = 25;    ///< Optimal line cooling delay (ms)
  uint8_t printMode = 0x00;

  // Global Scaling, Thickness & Spacer State
  uint8_t globalScale = 1;
  uint8_t globalThickness = 1;
  uint16_t globalSpacer = 8;

  // Font & Styling state
  FontType activeFont = FONT_8X8;
  const CustomFont* pCustomFont = nullptr;
  PrintAlign activeAlign = ALIGN_LEFT;
  bool isBold = false;
  bool isInverse = false;
  bool isUnderline = false;
  bool isStrikethrough = false;

  X5hStatus status;
  StatusCallback statusCb = nullptr;

  void initBLE();
  NimBLEAdvertisedDevice* findPrinter();
  bool connectToDevice(NimBLEAdvertisedDevice* device);
  bool connectToAddress(const NimBLEAddress& address);
  NimBLERemoteCharacteristic* findWriteCharacteristic(NimBLERemoteService* service);
  void setupNotifications();

  static void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);
  void handleNotification(uint8_t* data, size_t len);
  uint8_t crc8(const uint8_t* data, size_t len);

  // Canvas bit helpers (Standard LSB-first bit order for X5H printhead: 1 << (x%8))
  inline void setCanvasPixel(uint8_t* canvas, int canvasH, int x, int y) {
    if (x >= 0 && x < X5H_PRINTER_WIDTH_PX && y >= 0 && y < canvasH) {
      canvas[y * X5H_PRINTER_WIDTH_BYTES + (x / 8)] |= (1 << (x % 8));
    }
  }

  void drawTextToCanvas(uint8_t* canvas, int heightRows, const char* text, int startX, int startY, int scale);
  void drawFrameToCanvas(uint8_t* canvas, int canvasW, int canvasH, int boxX, int boxY, int boxW, int boxH, FrameStyle style, uint8_t borderThickness = 1);
  bool getCustomGlyphBitmap(uint32_t codePoint, uint8_t* outGlyph, uint8_t& outWidth, uint8_t& outHeight);

  // ISO/IEC 18004 Standard QR Code Encoder Internal Helpers
  bool generateISO18004QRCode(const char* text, QRErrorCorrection eccLevel, std::vector<uint8_t>& outModules, int& outDimension);
  void encodeReedSolomonECC(const uint8_t* dataBytes, size_t dataLen, uint8_t* eccBytes, size_t eccLen);
};

using TinyPrinter = X5hPrinter;

#endif // TINY_PRINTER_H
