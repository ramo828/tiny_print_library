/**
 * @file CustomFonts.ino
 * @brief Demonstrates built-in multi-size fonts and user-defined custom bitmap fonts.
 * @author Ramiz Mammadli
 */

#include <NimBLEDevice.h>
#include <TinyPrinter.h>

TinyPrinter printer;

// ============================================================
// CUSTOM BITMAP GLYPH DEFINITIONS
// ============================================================

// 1. Custom Heart Icon Bitmaps (8x8)
static const uint8_t heartBitmap[8] = {
  0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00
};

// 2. Custom Star Icon Bitmaps (8x8)
static const uint8_t starBitmap[8] = {
  0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x42, 0x00
};

// Define Custom Glyph Array
static const CustomGlyph myGlyphs[] = {
  { 'H', 8, 8, heartBitmap }, // Maps 'H' to Heart icon
  { 'S', 8, 8, starBitmap }   // Maps 'S' to Star icon
};

// Complete Custom Font Definition
static const CustomFont myCustomIconFont = {
  "IconFont", // Font name
  8,          // Default width
  8,          // Default height
  2,          // Glyph count
  myGlyphs    // Glyphs array
};

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== TinyPrinter - Custom & Multi-Font Demo ===");

  NimBLEAddress printerAddr("00:00:00:05:95:40", BLE_ADDR_PUBLIC);
  if (!printer.begin(printerAddr)) {
    Serial.println("[ERROR] Failed to connect to printer!");
    return;
  }

  printer.setEnergy(0xFFFF);
  printer.setQuality(0x33);

  // 1. Built-in 8x8 Standard Font (Latin + Azerbaijani)
  printer.setFont(FONT_8X8);
  printer.printText("1. Font 8x8 (Azerbaijani: Ə, ğ, ı, ö, ü, ç, ş)", 1);

  // 2. Built-in 5x7 Compact Micro Font
  printer.setFont(FONT_5X7);
  printer.printText("2. FONT 5X7 MICRO RECEIPT TEXT - VERY DENSE & COMPACT", 1);

  // 3. Built-in 12x16 Medium Font
  printer.setFont(FONT_12X16);
  printer.printText("3. Font 12x16 Header", 1);

  // 4. Built-in 16x24 Large Title Font
  printer.setFont(FONT_16X24);
  printer.printText("4. 16x24 Title", 1);

  // 5. User-Defined Custom Icon Font
  printer.setCustomFont(&myCustomIconFont);
  printer.printText("Custom Icons: H S H S", 2, ALIGN_CENTER);

  // Reset to default 8x8 font
  printer.setFont(FONT_8X8);
  printer.feed(80);

  Serial.println("Custom font demo completed.");
}

void loop() {
  delay(10);
}
