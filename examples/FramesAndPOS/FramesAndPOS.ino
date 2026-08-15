/**
 * @file FramesAndPOS.ino
 * @brief Demonstrates Frame Templates with Sizing/Thickness, Custom User Frames, POS Table Columns, and QR Codes.
 * @author Ramiz Mammadli
 */

#include <NimBLEDevice.h>
#include <TinyPrinter.h>

TinyPrinter printer;

// Custom Frame Drawing Callback (Custom Double Line with Corner Accents)
void renderMyCustomFrame(uint8_t* canvas, int canvasW, int canvasH, int innerX, int innerY, int innerW, int innerH) {
  int xEnd = innerX + innerW - 1;
  int yEnd = innerY + innerH - 1;

  for (int x = innerX; x <= xEnd; x++) {
    int byteIdxTop = innerY * X5H_PRINTER_WIDTH_BYTES + (x / 8);
    int byteIdxBottom = yEnd * X5H_PRINTER_WIDTH_BYTES + (x / 8);
    canvas[byteIdxTop] |= (1 << (x % 8));
    canvas[byteIdxBottom] |= (1 << (x % 8));
  }

  for (int y = innerY; y <= yEnd; y++) {
    int byteIdxLeft = y * X5H_PRINTER_WIDTH_BYTES + (innerX / 8);
    int byteIdxRight = y * X5H_PRINTER_WIDTH_BYTES + (xEnd / 8);
    canvas[byteIdxLeft] |= (1 << (innerX % 8));
    canvas[byteIdxRight] |= (1 << (xEnd % 8));
  }
}

CustomFrame myFrameDef = {
  "CornerAccents",
  8, // 8px inner padding
  renderMyCustomFrame
};

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== TinyPrinter Pro - Frames, POS & QR Code Sizing Demo ===");

  NimBLEAddress printerAddr("00:00:00:05:95:40", BLE_ADDR_PUBLIC);
  if (!printer.begin(printerAddr)) {
    Serial.println("[ERROR] Connection failed!");
    return;
  }

  printer.setEnergy(0xFFFF);
  printer.setQuality(0x33);
  printer.setSpacer(10); // 10px vertical spacer gap

  // 1. Frame Templates with Size Presets & Stroke Weight
  printer.printFramedText("STORE RECEIPT", FRAME_ROUNDED_RECTANGLE, FRAME_SIZE_LARGE, 2, 1, ALIGN_CENTER);

  // 2. Custom Explicit Width (280px), Height (45px) and Border Thickness (2px)
  printer.printFramedText("SPECIAL VIP DISCOUNT", FRAME_DOUBLE_LINE, 280, 45, 2, 1, ALIGN_CENTER);

  // 3. User Callback Frame
  printer.printCustomFramedText("CUSTOM CALLBACK", &myFrameDef, 1, ALIGN_CENTER);

  // 4. POS Multi-Column Table
  TableColumn receiptCols[] = {
    { "Espresso Coffee", 50, ALIGN_LEFT },
    { "x 2",            20, ALIGN_CENTER },
    { "$7.00",          30, ALIGN_RIGHT }
  };
  printer.printTableRow(receiptCols, 3);

  // 5. Promo Coupon Voucher Badge
  printer.printCoupon("SUMMER SALE", "PROMO2026", "Valid thru: 31 DEC 2026");

  // 6. High-Readability QR Code Engine (ISO/IEC 18004 Standard + Quiet Zone)
  printer.printQRCode("https://github.com/ramo828/TinyPrinter", QR_LARGE, ALIGN_CENTER);

  // 7. Verified EAN-13 Barcode with Auto-Checksum & Guard Bars
  printer.printBarcode("8690000123456", BARCODE_EAN13, 45, 2, ALIGN_CENTER);

  printer.printCutLine();
  printer.feed(80);
  Serial.println("Frames & POS receipt printing completed.");
}

void loop() {
  delay(10);
}
