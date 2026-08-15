/**
 * @file AdvancedFeatures.ino
 * @brief Demonstrates Density/Darkness, WiFi & VCard QR Codes, Industrial Barcodes, and Scaling.
 * @author Ramiz Mammadli
 */

#include <NimBLEDevice.h>
#include <TinyPrinter.h>

TinyPrinter printer;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== TinyPrinter Pro - Industrial Thermal Printer Features ===");

  // 1. Enable Developer Mode for verbose BLE packet hex logging
  printer.setDevMode(true);

  NimBLEAddress printerAddr("00:00:00:05:95:40", BLE_ADDR_PUBLIC);
  if (!printer.begin(printerAddr)) {
    Serial.println("[ERROR] Connection failed!");
    return;
  }

  // 2. Set Thermal Print Density to ULTRA DARK for deep pitch-black, uniform contrast
  printer.setDensity(DENSITY_ULTRA_DARK);

  // 3. Configure Global Object Spacers & Stroke Thickness
  printer.setSpacer(12);          // 12px vertical spacing after each printed element
  printer.setGlobalThickness(2);  // Heavy 2px stroke weight for text, shapes, and borders

  // 4. Header Text
  printer.printText("INDUSTRIAL POS PRO", 2, ALIGN_CENTER);
  printer.printLine(2);

  // 5. High-Readability WiFi Auto-Connect QR Code (Scans & connects to WiFi without typing password!)
  printer.printText("SCAN TO CONNECT WIFI:", 1, ALIGN_CENTER);
  printer.printQRCodeWiFi("MyStoreGuestWiFi", "SecurePassword2026", "WPA", QR_LARGE, ALIGN_CENTER);

  // 6. Contact VCard QR Code (Scans and adds contact directly to phone address book!)
  printer.printText("ADD CONTACT VCARD:", 1, ALIGN_CENTER);
  printer.printQRCodeVCard("Ramiz Mammadli", "+994501234567", "ramiz@example.com", "TinyPrinter Corp", QR_LARGE, ALIGN_CENTER);

  // 7. Phone Call QR Code
  printer.printQRCodePhone("+994501234567", QR_MEDIUM, ALIGN_CENTER);

  // 8. Industrial EAN-13 Barcode (Large Size preset with HRI text digits below barcode)
  printer.printText("RETAIL BARCODE (EAN-13):", 1, ALIGN_CENTER);
  printer.printBarcode("8690000123456", BARCODE_EAN13, BARCODE_SIZE_LARGE, true /* showText */, ALIGN_CENTER);

  // 9. Industrial Code39 Barcode (Bars only without text)
  printer.printText("WAREHOUSE BARCODE (BARS ONLY):", 1, ALIGN_CENTER);
  printer.printBarcode("PALLET-9988", BARCODE_CODE39, BARCODE_SIZE_MEDIUM, false /* showText = false */, ALIGN_CENTER);

  printer.feed(80);
  Serial.println("Industrial features demo completed.");
}

void loop() {
  delay(10);
}
