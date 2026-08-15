/**
 * @file PrinterStatus.ino
 * @brief Demonstrates BLE status callbacks, battery monitoring, and sensor reporting.
 * @author Ramiz Mammadli
 */

#include <NimBLEDevice.h>
#include <TinyPrinter.h>

TinyPrinter printer;

// Callback function triggered whenever the printer sends an AE02 status notification
void onPrinterStatus(const X5hStatus& st) {
  Serial.println("\n========== PRINTER STATUS NOTIFICATION ==========");
  Serial.printf("Battery Level   : %d%% (Raw Level: %d/15)\n", st.getBatteryPercent(), st.battery);
  Serial.printf("Paper Sensor    : %s\n", st.paperOut ? "OUT OF PAPER !" : "Loaded OK");
  Serial.printf("Cover Door      : %s\n", st.coverOpen ? "COVER OPEN !" : "Closed");
  Serial.printf("Thermal Overheat: %s\n", st.overheat ? "OVERHEATING !" : "Normal");
  Serial.printf("Buffer Status   : %s\n", st.bufferFull ? "FULL" : "Ready");
  Serial.printf("Last Command    : 0x%02X\n", st.lastCmd);
  Serial.println("=================================================\n");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== TinyPrinter - Printer Status & Telemetry ===");

  // Register asynchronous status callback before establishing connection
  printer.onStatus(onPrinterStatus);

  NimBLEAddress printerAddr("00:00:00:05:95:40", BLE_ADDR_PUBLIC);
  if (!printer.begin(printerAddr)) {
    Serial.println("[ERROR] Failed to connect to printer!");
    return;
  }

  Serial.println("[OK] Connected! Requesting telemetry...");
  delay(500);

  // Request status parameters
  printer.requestStatus();
  delay(1000);

  // Request battery ADC reading
  printer.requestBattery();
  delay(1000);
}

void loop() {
  delay(1000);
}
