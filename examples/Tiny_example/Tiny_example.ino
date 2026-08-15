/**
 * @file Tiny_example.ino
 * @brief Complete demonstration sketch for TinyPrinter library.
 * @author Ramiz Mammadli
 */

#include <NimBLEDevice.h>
#include <TinyPrinter.h>

TinyPrinter printer;

// Callback function triggered upon receiving status notifications
void onPrinterStatus(const X5hStatus& st) {
  Serial.println();
  Serial.println("========== PRINTER TELEMETRY ==========");
  Serial.printf("Battery Level  : %d%% (Level: %d/15)\n", st.getBatteryPercent(), st.battery);
  Serial.printf("Cover          : %s\n", st.coverOpen ? "OPEN !" : "Closed");
  Serial.printf("Paper          : %s\n", st.paperOut ? "OUT !" : "Loaded");
  Serial.printf("Overheat       : %s\n", st.overheat ? "YES !" : "No");
  Serial.printf("Buffer Full    : %s\n", st.bufferFull ? "YES" : "No");
  Serial.printf("Last Command   : 0x%02X\n", st.lastCmd);
  Serial.println("=======================================");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== X5H PRINTER - MULTI-FONT & BATTERY TEST ===");

  // Define callback before connection is established
  printer.onStatus(onPrinterStatus);

  NimBLEAddress addr("00:00:00:05:95:40", BLE_ADDR_PUBLIC);

  if (!printer.begin(addr)) {
    Serial.println("[ERROR] Connection failed! Device is off or out of range.");
    return;
  }

  Serial.println("[OK] Connected!");
  delay(500);

  // Configuration for maximum print darkness
  printer.setEnergy(0xFFFF);
  printer.setQuality(0x33);
  printer.setPrintMode(0x00);
  printer.setDelay(12);

  // 1. Query telemetry
  Serial.println("[REQUEST] Querying status & battery...");
  printer.requestStatus();
  delay(500);
  printer.requestBattery();
  delay(1000);

  // 2. Multi-Font Demonstration
  Serial.println("[PRINT] Printing multi-font demonstration...");

  printer.setFont(FONT_16X24);
  printer.printText("TITLE PRINT", 1, ALIGN_CENTER);

  printer.setFont(FONT_12X16);
  printer.printText("Sub-Header 12x16", 1, ALIGN_CENTER);

  printer.setFont(FONT_8X8);
  printer.setBold(true);
  printer.printText("Bold 8x8 Text (AZ: Ə, ğ, ı, ö, ü, ç, ş)", 1, ALIGN_LEFT);
  printer.setBold(false);

  printer.setFont(FONT_5X7);
  printer.printText("MICRO RECEIPT TEXT 5X7 COMPACT MODE", 1, ALIGN_LEFT);

  // 3. Inverse style
  printer.setFont(FONT_8X8);
  printer.setInverse(true);
  printer.printText(" INVERSE MODE TEST ", 1, ALIGN_CENTER);
  printer.setInverse(false);

  printer.feed(80); // Advance paper
  Serial.println("Test completed successfully.");
}

void loop() {
  delay(10);
}
