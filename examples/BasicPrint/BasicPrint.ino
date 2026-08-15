/**
 * @file BasicPrint.ino
 * @brief Basic text printing and styling example for TinyPrinter (X5H Thermal Printer).
 * @author Ramiz Mammadli
 */

#include <NimBLEDevice.h>
#include <TinyPrinter.h>

TinyPrinter printer;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== TinyPrinter - Basic Print Example ===");

  // Connect via MAC address or call printer.begin() to auto-scan by name
  NimBLEAddress printerAddr("00:00:00:05:95:40", BLE_ADDR_PUBLIC);

  if (!printer.begin(printerAddr)) {
    Serial.println("[ERROR] Failed to connect to printer!");
    return;
  }

  Serial.println("[OK] Connected to X5H Thermal Printer!");

  // Thermal energy & density configuration
  printer.setEnergy(0xFFFF); // Maximum heat density for dark output
  printer.setQuality(0x33);   // High contrast print profile
  printer.setDelay(15);       // Line delay (ms)

  // 1. Centered Header
  printer.setAlign(ALIGN_CENTER);
  printer.printText("MY STORE RECEIPT", 2);
  printer.printText("------------------------", 1);

  // 2. Left Aligned Items
  printer.setAlign(ALIGN_LEFT);
  printer.printText("Item 1: Espresso    $3.50", 1);
  printer.printText("Item 2: Croissant   $2.80", 1);

  // 3. Styled Text
  printer.setBold(true);
  printer.printText("TOTAL:             $6.30", 1);
  printer.setBold(false);

  // 4. Inverse Text
  printer.setInverse(true);
  printer.printText(" THANK YOU FOR VISITING ", 1, ALIGN_CENTER);
  printer.setInverse(false);

  // Advance paper
  printer.feed(80);

  Serial.println("Printing completed.");
}

void loop() {
  delay(10);
}
