/**
 * @file TinyPrinter.cpp
 * @brief Industrial-Grade Implementation of X5H / Tiny BLE Thermal Printer Library.
 * @author Ramiz Mammadli
 * @version 1.6.0
 * @date 2026-08-15
 */

#include "TinyPrinter.h"
#include <vector>
#include <cstring>

// ============================================================
// DEFAULT BLE SERVICE & CHARACTERISTIC UUID DEFINITIONS
// ============================================================

static const char* X5H_SERVICE_AE30 = "0000ae30-0000-1000-8000-00805f9b34fb";
static const char* X5H_CHAR_AE01 = "0000ae01-0000-1000-8000-00805f9b34fb";
static const char* X5H_CHAR_AE03 = "0000ae03-0000-1000-8000-00805f9b34fb";
static const char* X5H_CHAR_AE10 = "0000ae10-0000-1000-8000-00805f9b34fb";
static const char* X5H_CHAR_AE02 = "0000ae02-0000-1000-8000-00805f9b34fb";
static const char* X5H_DEFAULT_NAME = "X5h-0000";

static X5hPrinter* g_instance = nullptr;

// ============================================================
// GF(256) GALOIS FIELD & REED-SOLOMON ERROR CORRECTION ENGINE
// ============================================================

static uint8_t gfExp[512];
static uint8_t gfLog[256];
static bool gfInitialized = false;

static void initGF256() {
  if (gfInitialized) return;
  int x = 1;
  for (int i = 0; i < 255; i++) {
    gfExp[i] = x;
    gfLog[x] = i;
    x <<= 1;
    if (x & 0x100) x ^= 0x11D;
  }
  for (int i = 255; i < 512; i++) {
    gfExp[i] = gfExp[i - 255];
  }
  gfInitialized = true;
}

static uint8_t gfMul(uint8_t a, uint8_t b) {
  if (a == 0 || b == 0) return 0;
  return gfExp[gfLog[a] + gfLog[b]];
}

void X5hPrinter::encodeReedSolomonECC(const uint8_t* dataBytes, size_t dataLen, uint8_t* eccBytes, size_t eccLen) {
  initGF256();

  std::vector<uint8_t> genPoly(eccLen + 1, 0);
  genPoly[0] = 1;

  for (size_t i = 0; i < eccLen; i++) {
    uint8_t root = gfExp[i];
    for (int j = i + 1; j >= 1; j--) {
      genPoly[j] = genPoly[j] ^ gfMul(genPoly[j - 1], root);
    }
  }

  std::vector<uint8_t> res(dataLen + eccLen, 0);
  for (size_t i = 0; i < dataLen; i++) {
    res[i] = dataBytes[i];
  }

  for (size_t i = 0; i < dataLen; i++) {
    uint8_t coef = res[i];
    if (coef != 0) {
      for (size_t j = 0; j <= eccLen; j++) {
        res[i + j] ^= gfMul(genPoly[j], coef);
      }
    }
  }

  for (size_t i = 0; i < eccLen; i++) {
    eccBytes[i] = res[dataLen + i];
  }
}

// ============================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================

X5hPrinter::X5hPrinter() {
  g_instance = this;
  targetName = X5H_DEFAULT_NAME;
  serviceUUIDStr = X5H_SERVICE_AE30;
  writeUUIDStr = X5H_CHAR_AE01;
  notifyUUIDStr = X5H_CHAR_AE02;
}

X5hPrinter::~X5hPrinter() {
  end();
  if (g_instance == this) {
    g_instance = nullptr;
  }
}

// ============================================================
// PRINT DENSITY & DARKNESS CONTROLS
// ============================================================

void X5hPrinter::setDensity(PrintDensity density) {
  if (density == DENSITY_LIGHT) {
    energy = 0x4000; quality = 0x25; lineDelay = 15;
  } else if (density == DENSITY_NORMAL) {
    energy = 0x8000; quality = 0x35; lineDelay = 20;
  } else if (density == DENSITY_DARK) {
    energy = 0xC000; quality = 0x45; lineDelay = 25;
  } else if (density == DENSITY_ULTRA_DARK) {
    energy = 0xFFFF; quality = 0x55; lineDelay = 30; // Pitch black uniform thermal activation
  }
  if (devMode) Serial.printf("[TINY_PRINTER DEV_MODE] Density set to %d (Energy: 0x%04X, Quality: 0x%02X)\n", (int)density, energy, quality);
}

void X5hPrinter::setDarkness(uint8_t level) {
  level = constrain(level, 0, 100);
  energy = map(level, 0, 100, 0x2000, 0xFFFF);
  quality = map(level, 0, 100, 0x15, 0x55);
  lineDelay = map(level, 0, 100, 15, 30);
  if (devMode) Serial.printf("[TINY_PRINTER DEV_MODE] Darkness set to %d%% (Energy: 0x%04X)\n", level, energy);
}

// ============================================================
// GLOBAL SCALING, THICKNESS & SPACER CONTROLS
// ============================================================

void X5hPrinter::setGlobalScale(uint8_t multiplier) {
  globalScale = (multiplier > 0) ? multiplier : 1;
  if (devMode) Serial.printf("[TINY_PRINTER DEV_MODE] Global scale set to %dx\n", globalScale);
}

void X5hPrinter::setGlobalThickness(uint8_t strokeWidth) {
  globalThickness = (strokeWidth > 0) ? strokeWidth : 1;
  if (devMode) Serial.printf("[TINY_PRINTER DEV_MODE] Global thickness set to %dpx\n", globalThickness);
}

void X5hPrinter::setSpacer(uint16_t verticalPixels) {
  globalSpacer = verticalPixels;
  if (devMode) Serial.printf("[TINY_PRINTER DEV_MODE] Global vertical spacer set to %dpx\n", globalSpacer);
}

void X5hPrinter::printSpacer(uint16_t verticalPixels) {
  uint16_t gap = verticalPixels ? verticalPixels : globalSpacer;
  if (gap > 0 && isConnected()) feed(gap);
}

// ============================================================
// DEVELOPER MODE & CUSTOM BLE UUIDS
// ============================================================

void X5hPrinter::setDevMode(bool enable) {
  devMode = enable;
  if (devMode) {
    Serial.println("[TINY_PRINTER DEV_MODE] Verbose debug logging enabled.");
  }
}

void X5hPrinter::setCustomBLEUUIDs(const char* serviceUUID, const char* writeUUID, const char* notifyUUID) {
  if (serviceUUID && strlen(serviceUUID) > 0) serviceUUIDStr = serviceUUID;
  if (writeUUID && strlen(writeUUID) > 0) writeUUIDStr = writeUUID;
  if (notifyUUID && strlen(notifyUUID) > 0) notifyUUIDStr = notifyUUID;

  if (devMode) {
    Serial.printf("[TINY_PRINTER DEV_MODE] Custom BLE UUIDs configured:\n  Service: %s\n  Write  : %s\n  Notify : %s\n",
                  serviceUUIDStr.c_str(), writeUUIDStr.c_str(), notifyUUIDStr.c_str());
  }
}

// ============================================================
// CHECKSUM & PROTOCOL PACKET TRANSMISSION
// ============================================================

uint8_t X5hPrinter::crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  if (!data) return 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x80) crc = ((crc << 1) ^ 0x07) & 0xFF;
      else crc = (crc << 1) & 0xFF;
    }
  }
  return crc;
}

void X5hPrinter::sendPacket(uint8_t cmd, const uint8_t* payload, size_t len) {
  if (!isConnected()) return;
  if (len > 240) return;

  uint8_t packet[256];
  packet[0] = 0x51;               // Protocol magic header 1
  packet[1] = 0x78;               // Protocol magic header 2
  packet[2] = cmd;                // Command identifier byte
  packet[3] = 0x00;               // Reserved flag
  packet[4] = len & 0xFF;         // Payload length low byte
  packet[5] = (len >> 8) & 0xFF;  // Payload length high byte

  if (payload && len) memcpy(&packet[6], payload, len);

  packet[6 + len] = crc8(payload, len);  // Checksum over payload
  packet[7 + len] = 0xFF;                // Packet terminator

  if (devMode) {
    Serial.printf("[DEV_MODE PKT] CMD: 0x%02X, LEN: %d | Hex: ", cmd, (int)len);
    for (size_t k = 0; k < 8 + len; k++) Serial.printf("%02X ", packet[k]);
    Serial.println();
  }

  writeData(packet, 8 + len);
  delay(lineDelay);
}

void X5hPrinter::sendPacket(uint8_t cmd, uint8_t b1) {
  uint8_t p[1] = { b1 };
  sendPacket(cmd, p, 1);
}

void X5hPrinter::sendPacket(uint8_t cmd, uint16_t value) {
  uint8_t p[2] = { (uint8_t)(value & 0xFF), (uint8_t)(value >> 8) };
  sendPacket(cmd, p, 2);
}

// ============================================================
// LOW-LEVEL BLE COMMUNICATION
// ============================================================

bool X5hPrinter::writeData(const uint8_t* data, size_t length) {
  if (!connected || !pClient || !pClient->isConnected() || !pWriteChar) {
    connected = false;
    return false;
  }
  if (!data || length == 0) return false;

  bool result = pWriteChar->writeValue(data, length, false);
  if (!result && devMode) {
    Serial.println("[TINY_PRINTER DEV_MODE] Write to characteristic failed!");
  }
  return result;
}

// ============================================================
// FONT & STYLING MANAGEMENT
// ============================================================

void X5hPrinter::setFont(FontType font) { activeFont = font; }
void X5hPrinter::setCustomFont(const CustomFont* font) {
  pCustomFont = font;
  if (font) activeFont = FONT_CUSTOM;
}
void X5hPrinter::setAlign(PrintAlign align) { activeAlign = align; }
void X5hPrinter::setBold(bool bold) { isBold = bold; }
void X5hPrinter::setInverse(bool inverse) { isInverse = inverse; }
void X5hPrinter::setUnderline(bool underline) { isUnderline = underline; }
void X5hPrinter::setStrikethrough(bool strikethrough) { isStrikethrough = strikethrough; }

bool X5hPrinter::getCustomGlyphBitmap(uint32_t codePoint, uint8_t* outGlyph, uint8_t& outWidth, uint8_t& outHeight) {
  if (!pCustomFont || !pCustomFont->glyphs || pCustomFont->glyphCount == 0) return false;

  for (size_t i = 0; i < pCustomFont->glyphCount; i++) {
    if (pCustomFont->glyphs[i].codePoint == codePoint) {
      outWidth = pCustomFont->glyphs[i].width;
      outHeight = pCustomFont->glyphs[i].height;
      size_t byteCount = (outWidth * outHeight + 7) / 8;
      if (pCustomFont->glyphs[i].bitmap) {
        memcpy(outGlyph, pCustomFont->glyphs[i].bitmap, byteCount);
        return true;
      }
    }
  }
  return false;
}

// ============================================================
// FONT RASTERIZER ENGINE (MSB-FIRST BIT ORDER FIX: 0x80 >> (x%8))
// ============================================================

void X5hPrinter::drawTextToCanvas(uint8_t* canvas, int heightRows, const char* text, int startX, int startY, int scale) {
  if (!text || !canvas) return;

  int effectiveScale = scale * globalScale;
  int strokeThickness = (isBold ? 2 : 1) * globalThickness;

  const uint8_t* p = (const uint8_t*)text;
  int currentX = startX;

  int baseW = 8, baseH = 8;
  if (activeFont == FONT_5X7) { baseW = 6; baseH = 7; }
  else if (activeFont == FONT_12X16) { baseW = 12; baseH = 16; }
  else if (activeFont == FONT_16X24) { baseW = 16; baseH = 24; }
  else if (activeFont == FONT_CUSTOM && pCustomFont) {
    baseW = pCustomFont->defaultWidth;
    baseH = pCustomFont->defaultHeight;
  }

  int calculatedWidth = 0;
  const uint8_t* pLen = (const uint8_t*)text;
  while (*pLen) {
    if ((*pLen & 0x80) == 0) { calculatedWidth += baseW * effectiveScale; pLen++; }
    else if ((*pLen & 0xE0) == 0xC0 && pLen[1]) { calculatedWidth += baseW * effectiveScale; pLen += 2; }
    else pLen++;
  }

  if (activeAlign == ALIGN_CENTER && startX == 4) {
    currentX = (X5H_PRINTER_WIDTH_PX - calculatedWidth) / 2;
    if (currentX < 0) currentX = 0;
  } else if (activeAlign == ALIGN_RIGHT && startX == 4) {
    currentX = X5H_PRINTER_WIDTH_PX - calculatedWidth - 4;
    if (currentX < 0) currentX = 0;
  }

  while (*p) {
    uint8_t glyph[64];
    memset(glyph, 0, sizeof(glyph));
    uint32_t code = 0;
    int bytes = 1;
    int curW = baseW, curH = baseH;

    if ((*p & 0x80) == 0) { code = *p; bytes = 1; }
    else if ((*p & 0xE0) == 0xC0 && p[1]) { code = ((*p & 0x1F) << 6) | (p[1] & 0x3F); bytes = 2; }
    else { p++; continue; }

    bool glyphFound = false;
    if (activeFont == FONT_CUSTOM) {
      uint8_t cw = 0, ch = 0;
      if (getCustomGlyphBitmap(code, glyph, cw, ch)) {
        curW = cw; curH = ch; glyphFound = true;
      }
    }

    if (!glyphFound) {
      if (activeFont == FONT_5X7) {
        uint8_t g57[7]; x5hGetFontGlyph5x7((char)code, g57);
        curW = 6; curH = 7; for (int r = 0; r < 7; r++) glyph[r] = g57[r];
      } else if (activeFont == FONT_12X16) {
        uint8_t g1216[24]; x5hGetFontGlyph12x16((char)code, g1216);
        curW = 12; curH = 16; for (int r = 0; r < 24; r++) glyph[r] = g1216[r];
      } else if (activeFont == FONT_16X24) {
        uint8_t g1624[48]; x5hGetFontGlyph16x24((char)code, g1624);
        curW = 16; curH = 24; for (int r = 0; r < 48; r++) glyph[r] = g1624[r];
      } else {
        uint8_t g88[8];
        if (bytes == 1) x5hGetFontGlyph8x8((char)code, g88);
        else x5hGetFontGlyph8x8Az((uint16_t)code, g88);
        curW = 8; curH = 8; for (int r = 0; r < 8; r++) glyph[r] = g88[r];
      }
    }

    int bytesPerRow = (curW + 7) / 8;
    for (int row = 0; row < curH; row++) {
      for (int col = 0; col < curW; col++) {
        bool bitOn = false;
        if (bytesPerRow == 1) bitOn = (glyph[row] & (0x80 >> col)) != 0;
        else {
          int bIdx = row * bytesPerRow + (col / 8);
          int bBit = 7 - (col % 8);
          bitOn = (glyph[bIdx] & (1 << bBit)) != 0;
        }

        if (bitOn) {
          for (int sy = 0; sy < effectiveScale; sy++) {
            for (int sx = 0; sx < effectiveScale; sx++) {
              int px = currentX + col * effectiveScale + sx;
              int py = startY + row * effectiveScale + sy;

              for (int t = 0; t < strokeThickness; t++) {
                int drawX = px + t;
                int drawY = py;
                if (drawX >= 0 && drawX < X5H_PRINTER_WIDTH_PX && drawY >= 0 && drawY < heightRows) {
                  setCanvasPixel(canvas, heightRows, drawX, drawY);
                }
              }
            }
          }
        }
      }
    }

    if (isUnderline) {
      int lineY = startY + curH * effectiveScale;
      if (lineY < heightRows) {
        for (int col = 0; col < curW * effectiveScale; col++) {
          setCanvasPixel(canvas, heightRows, currentX + col, lineY);
        }
      }
    }

    if (isStrikethrough) {
      int strikeY = startY + (curH * effectiveScale) / 2;
      if (strikeY < heightRows) {
        for (int col = 0; col < curW * effectiveScale; col++) {
          setCanvasPixel(canvas, heightRows, currentX + col, strikeY);
        }
      }
    }

    currentX += curW * effectiveScale;
    p += bytes;
  }

  if (isInverse) {
    int totalBytes = heightRows * X5H_PRINTER_WIDTH_BYTES;
    for (int i = 0; i < totalBytes; i++) canvas[i] = ~canvas[i];
  }
}

// ============================================================
// FRAMES & BORDER RASTERIZER TEMPLATES
// ============================================================

void X5hPrinter::drawFrameToCanvas(uint8_t* canvas, int canvasW, int canvasH, int boxX, int boxY, int boxW, int boxH, FrameStyle style, uint8_t borderThickness) {
  if (!canvas || boxW <= 0 || boxH <= 0) return;

  int thickness = borderThickness * globalThickness;

  auto setPixel = [&](int px, int py) {
    for (int tx = 0; tx < thickness; tx++) {
      for (int ty = 0; ty < thickness; ty++) {
        setCanvasPixel(canvas, canvasH, px + tx, py + ty);
      }
    }
  };

  int xEnd = boxX + boxW - 1;
  int yEnd = boxY + boxH - 1;

  switch (style) {
    case FRAME_RECTANGLE:
      for (int x = boxX; x <= xEnd; x++) { setPixel(x, boxY); setPixel(x, yEnd); }
      for (int y = boxY; y <= yEnd; y++) { setPixel(boxX, y); setPixel(xEnd, y); }
      break;

    case FRAME_ROUNDED_RECTANGLE:
      for (int x = boxX + 4; x <= xEnd - 4; x++) { setPixel(x, boxY); setPixel(x, yEnd); }
      for (int y = boxY + 4; y <= yEnd - 4; y++) { setPixel(boxX, y); setPixel(xEnd, y); }
      setPixel(boxX + 2, boxY + 1); setPixel(boxX + 1, boxY + 2);
      setPixel(xEnd - 2, boxY + 1); setPixel(xEnd - 1, boxY + 2);
      setPixel(boxX + 2, yEnd - 1); setPixel(boxX + 1, yEnd - 2);
      setPixel(xEnd - 2, yEnd - 1); setPixel(xEnd - 1, yEnd - 2);
      break;

    case FRAME_DOUBLE_LINE:
      for (int x = boxX; x <= xEnd; x++) { setPixel(x, boxY); setPixel(x, yEnd); }
      for (int y = boxY; y <= yEnd; y++) { setPixel(boxX, y); setPixel(xEnd, y); }
      for (int x = boxX + 2; x <= xEnd - 2; x++) { setPixel(x, boxY + 2); setPixel(x, yEnd - 2); }
      for (int y = boxY + 2; y <= yEnd - 2; y++) { setPixel(boxX + 2, y); setPixel(xEnd + 2, y); }
      break;

    case FRAME_DASHED_RECTANGLE:
      for (int x = boxX; x <= xEnd; x++) {
        if ((x / 4) % 2 == 0) { setPixel(x, boxY); setPixel(x, yEnd); }
      }
      for (int y = boxY; y <= yEnd; y++) {
        if ((y / 4) % 2 == 0) { setPixel(boxX, y); setPixel(xEnd, y); }
      }
      break;

    case FRAME_CIRCLE: {
      int cx = boxX + boxW / 2;
      int cy = boxY + boxH / 2;
      int rx = boxW / 2;
      int ry = boxH / 2;
      for (int a = 0; a < 360; a += 2) {
        float rad = a * 3.14159f / 180.0f;
        int px = cx + (int)(rx * cos(rad));
        int py = cy + (int)(ry * sin(rad));
        setPixel(px, py);
      }
      break;
    }

    case FRAME_TRIANGLE: {
      int apexX = boxX + boxW / 2;
      for (int i = 0; i <= boxW / 2; i++) {
        setPixel(apexX - i, boxY + i / 2);
        setPixel(apexX + i, boxY + i / 2);
      }
      for (int x = boxX; x <= xEnd; x++) setPixel(x, yEnd);
      for (int y = boxY + boxW / 4; y <= yEnd; y++) { setPixel(boxX, y); setPixel(xEnd, y); }
      break;
    }

    case FRAME_DIAMOND: {
      int cx = boxX + boxW / 2;
      int cy = boxY + boxH / 2;
      for (int i = 0; i <= boxW / 2; i++) {
        int yOffset = (i * boxH) / boxW;
        setPixel(cx - i, cy - yOffset); setPixel(cx + i, cy - yOffset);
        setPixel(cx - i, cy + yOffset); setPixel(cx + i, cy + yOffset);
      }
      break;
    }

    case FRAME_COUPON_TICKET:
      for (int x = boxX; x <= xEnd; x++) { setPixel(x, boxY); setPixel(x, yEnd); }
      for (int y = boxY; y <= yEnd; y++) { setPixel(boxX, y); setPixel(xEnd, y); }
      for (int r = -6; r <= 6; r++) {
        int notchW = (int)sqrt(36 - r * r);
        for (int w = 0; w < notchW; w++) {
          setPixel(boxX + w, boxY + boxH / 2 + r);
          setPixel(xEnd - w, boxY + boxH / 2 + r);
        }
      }
      break;

    case FRAME_STAR_BURST: {
      for (int x = boxX + 6; x <= xEnd - 6; x++) { setPixel(x, boxY); setPixel(x, yEnd); }
      for (int y = boxY + 6; y <= yEnd - 6; y++) { setPixel(boxX, y); setPixel(xEnd, y); }
      auto drawStar = [&](int sx, int sy) {
        for (int i = -3; i <= 3; i++) { setPixel(sx + i, sy); setPixel(sx, sy + i); }
      };
      drawStar(boxX + 3, boxY + 3); drawStar(xEnd - 3, boxY + 3);
      drawStar(boxX + 3, yEnd - 3); drawStar(xEnd - 3, yEnd - 3);
      break;
    }

    default:
      break;
  }
}

void X5hPrinter::printFramedText(const char* text, FrameStyle style, FrameSizePreset size, uint8_t borderThickness, uint8_t textScale, PrintAlign align) {
  if (!text || !isConnected()) return;

  int effectiveScale = textScale * globalScale;

  int baseFontW = 8, baseFontH = 8;
  if (activeFont == FONT_5X7) { baseFontW = 6; baseFontH = 7; }
  else if (activeFont == FONT_12X16) { baseFontW = 12; baseFontH = 16; }
  else if (activeFont == FONT_16X24) { baseFontW = 16; baseFontH = 24; }

  int textPixelW = strlen(text) * baseFontW * effectiveScale;
  int boxW = textPixelW + 24;

  if (size == FRAME_SIZE_SMALL) boxW = 160;
  else if (size == FRAME_SIZE_MEDIUM) boxW = 250;
  else if (size == FRAME_SIZE_LARGE) boxW = 330;
  else if (size == FRAME_SIZE_FULL_WIDTH) boxW = 380;

  if (boxW > X5H_PRINTER_WIDTH_PX) boxW = X5H_PRINTER_WIDTH_PX - 8;
  int boxH = baseFontH * effectiveScale + 20;

  printFramedText(text, style, (uint16_t)boxW, (uint16_t)boxH, borderThickness, textScale, align);
}

void X5hPrinter::printFramedText(const char* text, FrameStyle style, uint16_t customWidthPx, uint16_t customHeightPx, uint8_t borderThickness, uint8_t textScale, PrintAlign align) {
  if (!text || !isConnected()) return;

  int effectiveScale = textScale * globalScale;

  int baseFontW = 8, baseFontH = 8;
  if (activeFont == FONT_5X7) { baseFontW = 6; baseFontH = 7; }
  else if (activeFont == FONT_12X16) { baseFontW = 12; baseFontH = 16; }
  else if (activeFont == FONT_16X24) { baseFontW = 16; baseFontH = 24; }

  int textPixelW = strlen(text) * baseFontW * effectiveScale;

  int boxW = customWidthPx;
  if (boxW > X5H_PRINTER_WIDTH_PX) boxW = X5H_PRINTER_WIDTH_PX - 8;
  int boxH = customHeightPx;

  int startX = 4;
  if (align == ALIGN_CENTER) startX = (X5H_PRINTER_WIDTH_PX - boxW) / 2;
  else if (align == ALIGN_RIGHT) startX = X5H_PRINTER_WIDTH_PX - boxW - 4;
  if (startX < 0) startX = 0;

  int canvasHeight = boxH + 8;
  uint8_t* canvas = (uint8_t*)calloc(canvasHeight * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  drawFrameToCanvas(canvas, X5H_PRINTER_WIDTH_PX, canvasHeight, startX, 4, boxW, boxH, style, borderThickness);

  int textX = startX + (boxW - textPixelW) / 2;
  int textY = 4 + (boxH - baseFontH * effectiveScale) / 2;
  drawTextToCanvas(canvas, canvasHeight, text, textX, textY, textScale);

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (int r = 0; r < canvasHeight; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

void X5hPrinter::printCustomFramedText(const char* text, const CustomFrame* customFrame, uint8_t scale, PrintAlign align) {
  if (!text || !customFrame || !isConnected()) return;

  int effectiveScale = scale * globalScale;

  int baseFontW = 8, baseFontH = 8;
  int textLen = strlen(text);
  int textPixelW = textLen * baseFontW * effectiveScale;
  int padding = customFrame->borderPadding ? customFrame->borderPadding : 10;

  int boxW = textPixelW + padding * 2;
  int boxH = baseFontH * effectiveScale + padding * 2;

  int startX = (align == ALIGN_CENTER) ? (X5H_PRINTER_WIDTH_PX - boxW) / 2 : 4;
  int canvasHeight = boxH + 8;

  uint8_t* canvas = (uint8_t*)calloc(canvasHeight * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  if (customFrame->renderFrame) {
    customFrame->renderFrame(canvas, X5H_PRINTER_WIDTH_PX, canvasHeight, startX, 4, boxW, boxH);
  }

  int textX = startX + padding;
  int textY = 4 + padding;
  drawTextToCanvas(canvas, canvasHeight, text, textX, textY, scale);

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (int r = 0; r < canvasHeight; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

// ============================================================
// COMMERCIAL POS RECEIPT FEATURES
// ============================================================

void X5hPrinter::printTableRow(const TableColumn* columns, size_t count, uint8_t scale) {
  if (!columns || count == 0 || !isConnected()) return;

  int effectiveScale = scale * globalScale;

  int baseFontH = 8;
  if (activeFont == FONT_5X7) baseFontH = 7;
  else if (activeFont == FONT_12X16) baseFontH = 16;
  else if (activeFont == FONT_16X24) baseFontH = 24;

  int canvasHeight = baseFontH * effectiveScale + 8;
  uint8_t* canvas = (uint8_t*)calloc(canvasHeight * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  int currentX = 0;
  for (size_t i = 0; i < count; i++) {
    int colW = (X5H_PRINTER_WIDTH_PX * columns[i].widthPercent) / 100;
    PrintAlign prevAlign = activeAlign;
    activeAlign = columns[i].align;

    int cellX = currentX + 2;
    if (columns[i].align == ALIGN_RIGHT) cellX = currentX + colW - 4;
    else if (columns[i].align == ALIGN_CENTER) cellX = currentX + colW / 2;

    drawTextToCanvas(canvas, canvasHeight, columns[i].text, cellX, 4, scale);
    activeAlign = prevAlign;
    currentX += colW;
  }

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (int r = 0; r < canvasHeight; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

void X5hPrinter::printCoupon(const char* title, const char* code, const char* expiry) {
  printDashedLine(2, 6, 3);
  printFramedText(title, FRAME_COUPON_TICKET, FRAME_SIZE_MEDIUM, 1, 1, ALIGN_CENTER);
  printText("PROMO CODE:", 1, ALIGN_CENTER);
  setBold(true);
  printText(code, 2, ALIGN_CENTER);
  setBold(false);
  if (expiry) {
    printTextXY(expiry, 10, 4, 1);
  }
  printDashedLine(2, 6, 3);
}

void X5hPrinter::printCutLine() {
  printText("-- ✂ -------------------------------- ✂ --", 1, ALIGN_CENTER);
}

// ============================================================
// HIGH-LEVEL TEXT & POSITIONING PRINTING API
// ============================================================

void X5hPrinter::printText(const char* text, uint8_t scale, bool center) {
  printText(text, scale, center ? ALIGN_CENTER : activeAlign);
}

void X5hPrinter::printText(const char* text, uint8_t scale, PrintAlign align) {
  if (!text || !isConnected()) return;

  int effectiveScale = scale * globalScale;

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, printMode); delay(30);

  int baseFontH = 8;
  if (activeFont == FONT_5X7) baseFontH = 7;
  else if (activeFont == FONT_12X16) baseFontH = 16;
  else if (activeFont == FONT_16X24) baseFontH = 24;
  else if (activeFont == FONT_CUSTOM && pCustomFont) baseFontH = pCustomFont->defaultHeight;

  int canvasHeight = baseFontH * effectiveScale + 12;
  if (canvasHeight > 240) canvasHeight = 240;

  uint8_t* canvas = (uint8_t*)calloc(canvasHeight * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  PrintAlign prevAlign = activeAlign;
  activeAlign = align;

  int startX = (align == ALIGN_CENTER) ? 20 : (align == ALIGN_RIGHT ? 340 : 4);
  drawTextToCanvas(canvas, canvasHeight, text, startX, 4, scale);
  activeAlign = prevAlign;

  for (int r = 0; r < canvasHeight; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }

  free(canvas);
  delay(120);

  if (globalSpacer > 0) feed(globalSpacer);
}

void X5hPrinter::printText(const String& text, uint8_t scale, PrintAlign align) {
  printText(text.c_str(), scale, align);
}

void X5hPrinter::printTextXY(const char* text, int x, int y, uint8_t scale) {
  if (!text || !isConnected()) return;

  int effectiveScale = scale * globalScale;

  int baseFontH = 8;
  if (activeFont == FONT_5X7) baseFontH = 7;
  else if (activeFont == FONT_12X16) baseFontH = 16;
  else if (activeFont == FONT_16X24) baseFontH = 24;

  int canvasHeight = y + baseFontH * effectiveScale + 12;
  if (canvasHeight > 280) canvasHeight = 280;

  uint8_t* canvas = (uint8_t*)calloc(canvasHeight * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  drawTextToCanvas(canvas, canvasHeight, text, x, y, scale);

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, printMode); delay(30);

  for (int r = 0; r < canvasHeight; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

void X5hPrinter::printTextBounds(const char* text, int x, int y, int width, int height, uint8_t scale, PrintAlign align) {
  if (!text || !isConnected() || width <= 0 || height <= 0) return;

  int canvasHeight = y + height + 10;
  if (canvasHeight > 300) canvasHeight = 300;

  uint8_t* canvas = (uint8_t*)calloc(canvasHeight * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  PrintAlign prevAlign = activeAlign;
  activeAlign = align;

  drawTextToCanvas(canvas, canvasHeight, text, x, y, scale);
  activeAlign = prevAlign;

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, printMode); delay(30);

  for (int r = 0; r < canvasHeight; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

// ============================================================
// MATRIX & PIXEL GRID PRINTING
// ============================================================

void X5hPrinter::printMatrix(const uint8_t* matrix, int rows, int cols, uint8_t pixelScale, PrintAlign align) {
  if (!matrix || !isConnected() || rows <= 0 || cols <= 0) return;

  int effectivePixelScale = pixelScale * globalScale;
  int thickness = globalThickness;

  int totalW = cols * effectivePixelScale;
  int totalH = rows * effectivePixelScale;

  int startX = 4;
  if (align == ALIGN_CENTER) startX = (X5H_PRINTER_WIDTH_PX - totalW) / 2;
  else if (align == ALIGN_RIGHT) startX = X5H_PRINTER_WIDTH_PX - totalW - 4;
  if (startX < 0) startX = 0;

  uint8_t* canvas = (uint8_t*)calloc(totalH * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  for (int r = 0; r < rows; r++) {
    for (int c = 0; c < cols; c++) {
      uint8_t val = matrix[r * cols + c];
      if (val != 0) {
        for (int py = 0; py < effectivePixelScale; py++) {
          for (int px = 0; px < effectivePixelScale; px++) {
            int outX = startX + c * effectivePixelScale + px;
            int outY = r * effectivePixelScale + py;

            for (int tx = 0; tx < thickness; tx++) {
              setCanvasPixel(canvas, totalH, outX + tx, outY);
            }
          }
        }
      }
    }
  }

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (int r = 0; r < totalH; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

// ============================================================
// ISO/IEC 18004 STANDARD QR CODE GENERATOR & PAYLOAD HELPERS
// ============================================================
bool X5hPrinter::generateISO18004QRCode(const char* text, QRErrorCorrection eccLevel, std::vector<uint8_t>& outModules, int& outDimension) {
  if (!text) return false;

  const size_t dataLen = strlen(text);
  if (dataLen == 0 || dataLen > 80) return false;

  // ----------------------------------------------------------
  // 1. Version seçimi (Byte mode, Level M kapasiteleri)
  // V1: 14 byte, V2: 26 byte, V3: 42 byte, V4: 62 byte (yaklaşık)
  // ----------------------------------------------------------
  int version;
  size_t dataCodewords, eccCodewords;

  if (dataLen <= 14) {
    version = 1; dataCodewords = 16; eccCodewords = 10;   // total 26
  } else if (dataLen <= 26) {
    version = 2; dataCodewords = 28; eccCodewords = 16;   // total 44
  } else if (dataLen <= 42) {
    version = 3; dataCodewords = 44; eccCodewords = 26;   // total 70
  } else {
    version = 4; dataCodewords = 64; eccCodewords = 36;   // total 100
  }

  const int dimension = 17 + version * 4;   // 21, 25, 29, 33

  // ----------------------------------------------------------
  // 2. Bit buffer ile doğru Byte Mode encoding
  // ----------------------------------------------------------
  std::vector<uint8_t> dataBytes(dataCodewords, 0);
  int bitCount = 0;

  auto putBits = [&](uint32_t val, int nbits) {
    for (int i = nbits - 1; i >= 0; --i) {
      if (bitCount >= (int)dataCodewords * 8) return;
      if (val & (1u << i)) {
        dataBytes[bitCount >> 3] |= (uint8_t)(1 << (7 - (bitCount & 7)));
      }
      ++bitCount;
    }
  };

  putBits(0b0100, 4);                 // Mode = Byte
  putBits((uint32_t)dataLen, 8);      // Character count (V1-9)

  for (size_t i = 0; i < dataLen; ++i)
    putBits((uint8_t)text[i], 8);

  // Terminator (max 4 zero bits)
  int rem = (int)dataCodewords * 8 - bitCount;
  if (rem > 4) rem = 4;
  if (rem > 0) putBits(0, rem);

  // Byte hizalama
  while (bitCount & 7) putBits(0, 1);

  // Pad 0xEC / 0x11
  bool padEC = true;
  while ((bitCount >> 3) < (int)dataCodewords) {
    putBits(padEC ? 0xEC : 0x11, 8);
    padEC = !padEC;
  }

  // ----------------------------------------------------------
  // 3. Reed-Solomon
  // ----------------------------------------------------------
  std::vector<uint8_t> eccBytes(eccCodewords, 0);
  encodeReedSolomonECC(dataBytes.data(), dataCodewords, eccBytes.data(), eccCodewords);

  std::vector<uint8_t> codewords;
  codewords.reserve(dataCodewords + eccCodewords);
  codewords.insert(codewords.end(), dataBytes.begin(), dataBytes.end());
  codewords.insert(codewords.end(), eccBytes.begin(), eccBytes.end());

  // ----------------------------------------------------------
  // 4. Matrix + Quiet Zone
  // ----------------------------------------------------------
  const int quiet = 4;
  const int size  = dimension + quiet * 2;
  outModules.assign(size * size, 0);
  std::vector<uint8_t> reserved(size * size, 0);

  auto set = [&](int r, int c, bool black, bool res = true) {
    if (r < 0 || r >= dimension || c < 0 || c >= dimension) return;
    int gr = r + quiet, gc = c + quiet;
    outModules[gr * size + gc] = black ? 1 : 0;
    if (res) reserved[gr * size + gc] = 1;
  };

  // Finder + separator
  auto finder = [&](int tr, int tc) {
    for (int r = -1; r <= 7; ++r) {
      for (int c = -1; c <= 7; ++c) {
        int mr = tr + r, mc = tc + c;
        if (mr < 0 || mr >= dimension || mc < 0 || mc >= dimension) continue;
        bool black = false;
        if (r >= 0 && r <= 6 && c >= 0 && c <= 6) {
          black = (r == 0 || r == 6 || c == 0 || c == 6 ||
                   (r >= 2 && r <= 4 && c >= 2 && c <= 4));
        }
        set(mr, mc, black, true);
      }
    }
  };
  finder(0, 0);
  finder(0, dimension - 7);
  finder(dimension - 7, 0);

  // Alignment (V2+)
  if (version >= 2) {
    int ac = dimension - 7;
    for (int r = -2; r <= 2; ++r)
      for (int c = -2; c <= 2; ++c)
        set(ac + r, ac + c, (abs(r) == 2 || abs(c) == 2 || (r == 0 && c == 0)), true);
  }

  // Timing
  for (int i = 8; i < dimension - 8; ++i) {
    set(6, i, (i & 1) == 0, true);
    set(i, 6, (i & 1) == 0, true);
  }

  // Dark module
  set(4 * version + 9, 8, true, true);

  // Format info alanlarını rezerve et
  for (int i = 0; i < 9; ++i) {
    if (i != 6) { set(8, i, false, true); set(i, 8, false, true); }
  }
  for (int i = 0; i < 8; ++i) {
    set(8, dimension - 1 - i, false, true);
    set(dimension - 1 - i, 8, false, true);
  }

  // ----------------------------------------------------------
  // 5. Data yerleştirme (doğru zigzag)
  // ----------------------------------------------------------
  int bitIdx = 0;
  const int totalBits = (int)codewords.size() * 8;

  for (int col = dimension - 1; col > 0; col -= 2) {
    if (col == 6) col--;                         // timing kolonunu atla

    // Yön: çift çift kolonlarda yukarı, teklerde aşağı
    // col=dimension-1 (sağ kenar) → yukarı git
    bool upward = ((dimension - 1 - col) / 2) % 2 == 0;

    for (int i = 0; i < dimension; ++i) {
      int row = upward ? (dimension - 1 - i) : i;

      for (int dc = 0; dc < 2; ++dc) {
        int c = col - dc;
        int gr = row + quiet, gc = c + quiet;

        if (reserved[gr * size + gc]) continue;

        bool bit = false;
        if (bitIdx < totalBits) {
          int bi = bitIdx >> 3;
          int bp = 7 - (bitIdx & 7);
          bit = (codewords[bi] & (1 << bp)) != 0;
          ++bitIdx;
        }

        // Mask 0
        if (((row + c) & 1) == 0) bit = !bit;

        set(row, c, bit, false);
      }
    }
  }

  // ----------------------------------------------------------
  // 6. Format Information (Mask 0 + Level M = 0x5412)
  // Tek sefer, doğru pozisyonlar
  // ----------------------------------------------------------
  const uint16_t format = 0x5412;   // Level M (01) + Mask 0 (000) after BCH + mask

  // Sol-üst dikey + yatay
  for (int i = 0; i < 6; ++i) set(i, 8, (format >> i) & 1, true);
  set(7, 8, (format >> 6) & 1, true);
  set(8, 8, (format >> 7) & 1, true);
  set(8, 7, (format >> 8) & 1, true);
  for (int i = 9; i < 15; ++i) set(8, 14 - i, (format >> i) & 1, true);

  // Sağ-üst kopya (yatay)
  for (int i = 0; i < 8; ++i)
    set(8, dimension - 1 - i, (format >> i) & 1, true);

  // Sol-alt kopya (dikey)
  for (int i = 0; i < 7; ++i)
    set(dimension - 1 - i, 8, (format >> (i + 8)) & 1, true);

  outDimension = size;
  return true;
}

void X5hPrinter::printQRCode(const char* text, uint8_t customModuleScale, QRErrorCorrection ecc, PrintAlign align) {
  if (!text || !isConnected()) return;

  std::vector<uint8_t> modules;
  int dimension = 0;

  if (generateISO18004QRCode(text, ecc, modules, dimension)) {
    printMatrix(modules.data(), dimension, dimension, customModuleScale, align);
  }
}

void X5hPrinter::printQRCode(const char* text, QRSize size, QRErrorCorrection ecc, PrintAlign align) {
  uint8_t moduleScale = 7;
  if (size == QR_SMALL) moduleScale = 4;
  else if (size == QR_MEDIUM) moduleScale = 7;
  else if (size == QR_LARGE) moduleScale = 10;

  printQRCode(text, moduleScale, ecc, align);
}

void X5hPrinter::printQRCode(const char* text, QRSize size, PrintAlign align) {
  printQRCode(text, size, QR_ECC_MEDIUM, align);
}

void X5hPrinter::printQRCode(const char* text, uint8_t customModuleScale, PrintAlign align) {
  printQRCode(text, customModuleScale, QR_ECC_MEDIUM, align);
}

void X5hPrinter::printQRCode(const String& text, QRSize size, QRErrorCorrection ecc, PrintAlign align) {
  printQRCode(text.c_str(), size, ecc, align);
}

void X5hPrinter::printQRCode(const String& text, QRSize size, PrintAlign align) {
  printQRCode(text.c_str(), size, QR_ECC_MEDIUM, align);
}

// QR Payload Helpers
void X5hPrinter::printQRCodeURL(const char* url, QRSize size, PrintAlign align) {
  printQRCode(url, size, align);
}

void X5hPrinter::printQRCodePhone(const char* phoneNumber, QRSize size, PrintAlign align) {
  String uri = "tel:";
  uri += phoneNumber;
  printQRCode(uri.c_str(), size, align);
}

void X5hPrinter::printQRCodeWiFi(const char* ssid, const char* password, const char* authType, QRSize size, PrintAlign align) {
  String wifiUri = "WIFI:S:";
  wifiUri += ssid;
  wifiUri += ";T:";
  wifiUri += (authType ? authType : "WPA");
  wifiUri += ";P:";
  wifiUri += (password ? password : "");
  wifiUri += ";;";
  printQRCode(wifiUri.c_str(), size, align);
}

void X5hPrinter::printQRCodeVCard(const char* name, const char* phone, const char* email, const char* org, QRSize size, PrintAlign align) {
  String vcard = "BEGIN:VCARD\nVERSION:3.0\nN:";
  vcard += name ? name : "Contact";
  vcard += "\nFN:";
  vcard += name ? name : "Contact";
  if (phone) { vcard += "\nTEL:"; vcard += phone; }
  if (email) { vcard += "\nEMAIL:"; vcard += email; }
  if (org) { vcard += "\nORG:"; vcard += org; }
  vcard += "\nEND:VCARD";
  printQRCode(vcard.c_str(), size, align);
}

void X5hPrinter::printCustomQRCode(const uint8_t* qrBitmap, uint16_t sizePx, QRSize size, PrintAlign align) {
  uint8_t scale = 7;
  if (size == QR_SMALL) scale = 4;
  else if (size == QR_MEDIUM) scale = 7;
  else if (size == QR_LARGE) scale = 10;

  printCustomQRCode(qrBitmap, sizePx, scale, align);
}

void X5hPrinter::printCustomQRCode(const uint8_t* qrBitmap, uint16_t sizePx, uint8_t scale, PrintAlign align) {
  if (!qrBitmap || sizePx == 0 || !isConnected()) return;

  int effectiveScale = scale * globalScale;
  uint16_t bytesPerRow = (sizePx + 7) / 8;

  int totalW = sizePx * effectiveScale;
  int totalH = sizePx * effectiveScale;

  int startX = (align == ALIGN_CENTER) ? (X5H_PRINTER_WIDTH_PX - totalW) / 2 : 4;
  if (startX < 0) startX = 0;

  uint8_t* canvas = (uint8_t*)calloc(totalH * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  for (uint16_t r = 0; r < sizePx; r++) {
    for (uint16_t c = 0; c < sizePx; c++) {
      uint8_t byteVal = qrBitmap[r * bytesPerRow + (c / 8)];
      bool bitOn = (byteVal & (1 << (c % 8))) != 0;
      if (bitOn) {
        for (uint8_t sy = 0; sy < effectiveScale; sy++) {
          for (uint8_t sx = 0; sx < effectiveScale; sx++) {
            setCanvasPixel(canvas, totalH, startX + c * effectiveScale + sx, r * effectiveScale + sy);
          }
        }
      }
    }
  }

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (int r = 0; r < totalH; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

// ============================================================
// INDUSTRIAL 1D BARCODE SUITE WITH HRI TEXT TOGGLE & SIZING
// ============================================================

void X5hPrinter::printBarcode(const char* code, BarcodeType type, BarcodeSize size, bool showText, PrintAlign align) {
  uint8_t hPx = 50;
  uint8_t barW = 3;

  if (size == BARCODE_SIZE_SMALL) { hPx = 30; barW = 2; }
  else if (size == BARCODE_SIZE_MEDIUM) { hPx = 50; barW = 3; }
  else if (size == BARCODE_SIZE_LARGE) { hPx = 80; barW = 4; }

  printBarcode(code, type, hPx, barW, showText, align);
}

void X5hPrinter::printBarcode(const char* code, BarcodeType type, uint8_t heightPx, uint8_t barWidthPx, bool showText, PrintAlign align) {
  if (!code || !isConnected()) return;

  int effectiveBarWidth = barWidthPx * globalScale;
  int quietZoneModules = 10;

  static const uint16_t code39Patterns[] = {
    0x034, 0x121, 0x061, 0x160, 0x031, 0x130, 0x070, 0x025, 0x124, 0x064,
    0x109, 0x049, 0x148, 0x019, 0x118, 0x058, 0x00D, 0x10C, 0x04C, 0x01C,
    0x103, 0x043, 0x142, 0x013, 0x112, 0x052, 0x007, 0x106, 0x046, 0x016,
    0x181, 0x0C1, 0x1C0, 0x091, 0x190, 0x0D0, 0x085, 0x184, 0x0C4, 0x094
  };

  int codeLen = strlen(code);
  int totalModules = quietZoneModules * 2;

  if (type == BARCODE_CODE39) totalModules += (codeLen + 2) * 13;
  else if (type == BARCODE_EAN13) totalModules += 95;
  else if (type == BARCODE_EAN8) totalModules += 67;
  else totalModules += (codeLen + 2) * 11 + 13;

  int totalWidthPx = totalModules * effectiveBarWidth;
  int startX = (align == ALIGN_CENTER) ? (X5H_PRINTER_WIDTH_PX - totalWidthPx) / 2 : 4;
  if (startX < 0) startX = 0;

  uint8_t hriScale = 1;
  if (barWidthPx >= 4 || heightPx >= 70) hriScale = 2;
  else if (barWidthPx >= 3 || heightPx >= 50) hriScale = 2;

  int hriTextH = showText ? (hriScale * 8 + 6) : 0;
  int totalCanvasH = heightPx + hriTextH;

  uint8_t* canvas = (uint8_t*)calloc(totalCanvasH * X5H_PRINTER_WIDTH_BYTES, 1);
  if (!canvas) return;

  int currentX = startX + quietZoneModules * effectiveBarWidth;

  if ((type == BARCODE_EAN13 || type == BARCODE_UPC_A) && (codeLen == 12 || codeLen == 13)) {
    static const uint8_t eanL[10] = { 0x0D, 0x19, 0x13, 0x3D, 0x23, 0x31, 0x2F, 0x3B, 0x37, 0x0B };
    static const uint8_t eanG[10] = { 0x27, 0x33, 0x1B, 0x21, 0x1D, 0x39, 0x05, 0x03, 0x09, 0x17 };
    static const uint8_t eanR[10] = { 0x72, 0x66, 0x6C, 0x42, 0x5C, 0x4E, 0x50, 0x44, 0x48, 0x74 };
    static const uint8_t eanStructure[10] = { 0x00, 0x0B, 0x0D, 0x0E, 0x13, 0x19, 0x1C, 0x15, 0x16, 0x1A };

    uint8_t digits[13] = { 0 };
    for (int i = 0; i < 12 && i < codeLen; i++) digits[i] = code[i] - '0';

    int sum = 0;
    for (int i = 0; i < 12; i++) sum += digits[i] * ((i % 2 == 0) ? 1 : 3);
    digits[12] = (10 - (sum % 10)) % 10;

    auto drawModules = [&](uint8_t pattern, int bits) {
      for (int b = bits - 1; b >= 0; b--) {
        bool isBar = (pattern & (1 << b)) != 0;
        if (isBar) {
          for (int w = 0; w < effectiveBarWidth; w++) {
            for (int h = 0; h < heightPx; h++) {
              setCanvasPixel(canvas, totalCanvasH, currentX + w, h);
            }
          }
        }
        currentX += effectiveBarWidth;
      }
    };

    drawModules(0x05, 3); // Left Guard 101
    uint8_t firstDigit = digits[0];
    uint8_t parity = eanStructure[firstDigit];

    for (int i = 1; i <= 6; i++) {
      bool useG = (parity & (1 << (6 - i))) != 0;
      drawModules(useG ? eanG[digits[i]] : eanL[digits[i]], 7);
    }

    drawModules(0x0A, 5); // Center Guard 01010

    for (int i = 7; i <= 12; i++) {
      drawModules(eanR[digits[i]], 7);
    }

    drawModules(0x05, 3); // Right Guard 101
  } else {
    auto drawCode39Char = [&](uint16_t pattern) {
      for (int bit = 8; bit >= 0; bit--) {
        bool isBar = (bit % 2 == 0);
        bool isWide = (pattern & (1 << bit)) != 0;
        int width = (isWide ? 3 : 1) * effectiveBarWidth;
        if (isBar) {
          for (int w = 0; w < width; w++) {
            for (int h = 0; h < heightPx; h++) {
              setCanvasPixel(canvas, totalCanvasH, currentX + w, h);
            }
          }
        }
        currentX += width;
      }
      currentX += 1 * effectiveBarWidth;
    };

    drawCode39Char(0x094); // '*' delimiter

    for (int i = 0; i < codeLen; i++) {
      char c = toupper(code[i]);
      uint16_t pat = 0x034;
      if (c >= '0' && c <= '9') pat = code39Patterns[c - '0'];
      else if (c >= 'A' && c <= 'Z') pat = code39Patterns[c - 'A' + 10];
      else if (c == '-') pat = code39Patterns[36];
      else if (c == '.') pat = code39Patterns[37];
      else if (c == ' ') pat = code39Patterns[38];
      drawCode39Char(pat);
    }

    drawCode39Char(0x094);
  }

  if (showText) {
    int textPixelW = codeLen * 8 * hriScale;
    int textX = startX + (totalWidthPx - textPixelW) / 2;
    if (textX < 0) textX = 0;
    int textY = heightPx + 2;

    drawTextToCanvas(canvas, totalCanvasH, code, textX, textY, hriScale);
  }

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (int r = 0; r < totalCanvasH; r++) {
    sendPacket(0xA2, &canvas[r * X5H_PRINTER_WIDTH_BYTES], X5H_PRINTER_WIDTH_BYTES);
  }
  free(canvas);

  if (globalSpacer > 0) feed(globalSpacer);
}

// ============================================================
// SEPARATOR LINES & GRAPHICS
// ============================================================

void X5hPrinter::printLine(uint16_t thickness, uint16_t widthPx, PrintAlign align) {
  if (!isConnected() || thickness == 0) return;
  if (widthPx > X5H_PRINTER_WIDTH_PX) widthPx = X5H_PRINTER_WIDTH_PX;

  int effectiveThickness = thickness * globalThickness;

  int startX = 0;
  if (align == ALIGN_CENTER) startX = (X5H_PRINTER_WIDTH_PX - widthPx) / 2;
  else if (align == ALIGN_RIGHT) startX = X5H_PRINTER_WIDTH_PX - widthPx;

  uint8_t lineRow[X5H_PRINTER_WIDTH_BYTES] = { 0 };
  for (int px = startX; px < startX + widthPx; px++) {
    if (px >= 0 && px < X5H_PRINTER_WIDTH_PX) {
      lineRow[px / 8] |= (1 << (px % 8));
    }
  }

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (uint16_t t = 0; t < effectiveThickness; t++) {
    sendPacket(0xA2, lineRow, X5H_PRINTER_WIDTH_BYTES);
  }

  if (globalSpacer > 0) feed(globalSpacer);
}

void X5hPrinter::printDashedLine(uint16_t thickness, uint16_t dashLen, uint16_t gapLen) {
  if (!isConnected() || thickness == 0) return;

  int effectiveThickness = thickness * globalThickness;

  uint8_t lineRow[X5H_PRINTER_WIDTH_BYTES] = { 0 };
  uint16_t period = dashLen + gapLen;

  for (int px = 0; px < X5H_PRINTER_WIDTH_PX; px++) {
    if ((px % period) < dashLen) {
      lineRow[px / 8] |= (1 << (px % 8));
    }
  }

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (uint16_t t = 0; t < effectiveThickness; t++) {
    sendPacket(0xA2, lineRow, X5H_PRINTER_WIDTH_BYTES);
  }

  if (globalSpacer > 0) feed(globalSpacer);
}

// ============================================================
// BITMAP & GRAPHICS PRINTING
// ============================================================

void X5hPrinter::printBitmap(const uint8_t* data, uint16_t widthBytes, uint16_t height) {
  if (!data || !isConnected() || widthBytes == 0 || height == 0) return;

  sendPacket(0xA4, quality); delay(20);
  sendPacket(0xAF, energy); delay(30);
  sendPacket(0xBE, (uint8_t)0x00); delay(30);

  for (uint16_t r = 0; r < height; r++) {
    if (widthBytes == X5H_PRINTER_WIDTH_BYTES) {
      sendPacket(0xA2, &data[r * widthBytes], X5H_PRINTER_WIDTH_BYTES);
    } else {
      uint8_t row[X5H_PRINTER_WIDTH_BYTES] = { 0 };
      size_t copyLen = (widthBytes < X5H_PRINTER_WIDTH_BYTES) ? widthBytes : X5H_PRINTER_WIDTH_BYTES;
      memcpy(row, &data[r * widthBytes], copyLen);
      sendPacket(0xA2, row, X5H_PRINTER_WIDTH_BYTES);
    }
  }
  delay(100);

  if (globalSpacer > 0) feed(globalSpacer);
}

void X5hPrinter::printImage(const uint8_t* data, uint16_t widthPx, uint16_t height) {
  uint16_t wBytes = (widthPx + 7) / 8;
  printBitmap(data, wBytes, height);
}

// ============================================================
// PAPER MOVEMENT & QUEUE CONTROL
// ============================================================

void X5hPrinter::feed(uint16_t pixels) {
  if (!isConnected()) return;
  sendPacket(0xA1, pixels);
}

void X5hPrinter::retract(uint16_t pixels) {
  if (!isConnected()) return;
  sendPacket(0xA0, pixels);
}

void X5hPrinter::clear() { feed(50); }

void X5hPrinter::setEnergy(uint16_t v) { energy = v; }
void X5hPrinter::setQuality(uint8_t v) { quality = v; }
void X5hPrinter::setDelay(uint16_t ms) { lineDelay = ms; }
void X5hPrinter::setPrintMode(uint8_t m) { printMode = m; }

// ============================================================
// BLE NOTIFICATION ROUTER & EVENT DISPATCHING
// ============================================================

void X5hPrinter::notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (g_instance) {
    g_instance->handleNotification(pData, length);
  }
}

void X5hPrinter::handleNotification(uint8_t* data, size_t len) {
  if (len < 8) return;
  if (data[0] != 0x51 || data[1] != 0x78) return;

  uint8_t cmd = data[2];
  uint16_t plen = data[4] | (data[5] << 8);

  if (devMode) {
    Serial.printf("[TINY_PRINTER DEV_MODE NOTIFY] CMD: 0x%02X, Payload Len: %d\n", cmd, plen);
  }

  if (cmd == 0xA3) {
    uint8_t statusByte = data[5];
    status.coverOpen = (statusByte & 0x01) != 0;
    status.paperOut = (statusByte & 0x02) != 0;
    status.overheat = (statusByte & 0x04) != 0;
    status.bufferFull = false;
    status.battery = data[7];
    status.lastCmd = cmd;

    if (statusCb) statusCb(status);
  } else if (cmd == 0xAE) {
    status.bufferFull = (data[5] == 0x10 || data[6] == 0x10);
    status.lastCmd = cmd;

    if (statusCb) statusCb(status);
  } else if (cmd == 0xBA) {
    uint16_t rawAdc = 0;
    if (plen >= 2 && len >= 8) rawAdc = data[5] | (data[6] << 8);
    else if (len >= 6) rawAdc = data[5];

    if (rawAdc >= 4095) status.battery = 15;
    else if (rawAdc <= 3400) status.battery = 0;
    else status.battery = (uint8_t)(((rawAdc - 3400) * 15) / (4095 - 3400));

    status.lastCmd = cmd;
    if (statusCb) statusCb(status);
  }
}

// ============================================================
// BLE CONNECTION MANAGEMENT
// ============================================================

void X5hPrinter::enableNotifications(bool enable) {
  if (!pNotifyChar) return;

  if (enable) {
    bool success = pNotifyChar->subscribe(true, notifyCallback);
    if (success && devMode) {
      Serial.println("[TINY_PRINTER DEV_MODE] Subscribed to notifications.");
    }
  } else {
    pNotifyChar->unsubscribe();
  }
}

void X5hPrinter::requestStatus() { sendPacket(0xA3, (uint8_t)0x00); }

void X5hPrinter::requestBattery() {
  uint8_t pkt[] = { 0x51, 0x78, 0xBA, 0x00, 0x00, 0x00, 0x00, 0xFF };
  if (pWriteChar) pWriteChar->writeValue(pkt, sizeof(pkt), false);
}

void X5hPrinter::onStatus(StatusCallback cb) { statusCb = cb; }

void X5hPrinter::initBLE() {
  if (bleInitialized) return;
  if (devMode) Serial.println("[TINY_PRINTER DEV_MODE] Initializing NimBLE...");
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  bleInitialized = true;
}

bool X5hPrinter::isConnected() {
  if (!connected || !pClient || !pClient->isConnected() || !pWriteChar) {
    connected = false;
    return false;
  }
  return true;
}

void X5hPrinter::end() {
  enableNotifications(false);
  connected = false;
  pWriteChar = nullptr;
  pNotifyChar = nullptr;
  pService = nullptr;
  if (pClient) {
    if (pClient->isConnected()) {
      pClient->disconnect();
      delay(200);
    }
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }
  if (devMode) Serial.println("[TINY_PRINTER DEV_MODE] BLE Disconnected.");
}

NimBLERemoteCharacteristic* X5hPrinter::findWriteCharacteristic(NimBLERemoteService* service) {
  if (!service) return nullptr;

  NimBLERemoteCharacteristic* ch = service->getCharacteristic(NimBLEUUID(writeUUIDStr.c_str()));
  if (ch && (ch->canWriteNoResponse() || ch->canWrite())) return ch;

  ch = service->getCharacteristic(NimBLEUUID(X5H_CHAR_AE01));
  if (ch && (ch->canWriteNoResponse() || ch->canWrite())) return ch;

  ch = service->getCharacteristic(NimBLEUUID(X5H_CHAR_AE03));
  if (ch && (ch->canWriteNoResponse() || ch->canWrite())) return ch;

  ch = service->getCharacteristic(NimBLEUUID(X5H_CHAR_AE10));
  if (ch && ch->canWrite()) return ch;

  return nullptr;
}

bool X5hPrinter::connectToDevice(NimBLEAdvertisedDevice* device) {
  if (!device) return false;
  return connectToAddress(device->getAddress());
}

bool X5hPrinter::connectToAddress(const NimBLEAddress& address) {
  initBLE();

  if (pClient) {
    if (pClient->isConnected()) pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
  }

  pService = nullptr;
  pWriteChar = nullptr;
  pNotifyChar = nullptr;
  connected = false;

  pClient = NimBLEDevice::createClient();
  if (!pClient) return false;

  pClient->setConnectionParams(12, 24, 0, 100);
  pClient->setConnectTimeout(10 * 1000);

  if (devMode) Serial.printf("[TINY_PRINTER DEV_MODE] Connecting to MAC: %s\n", address.toString().c_str());
  if (!pClient->connect(address)) {
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
    return false;
  }

  connected = true;

  pService = pClient->getService(NimBLEUUID(serviceUUIDStr.c_str()));
  if (!pService) {
    end();
    return false;
  }

  pNotifyChar = pService->getCharacteristic(NimBLEUUID(notifyUUIDStr.c_str()));
  pWriteChar = findWriteCharacteristic(pService);

  if (!pWriteChar) {
    end();
    return false;
  }

  if (pNotifyChar && pNotifyChar->canNotify()) {
    enableNotifications(true);
  }

  if (devMode) Serial.println("[TINY_PRINTER DEV_MODE] Connected and ready.");
  return true;
}

NimBLEAdvertisedDevice* X5hPrinter::findPrinter() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (!scan) return nullptr;

  if (devMode) Serial.println("[TINY_PRINTER DEV_MODE] Scanning for BLE printer...");
  scan->clearResults();
  NimBLEScanResults results = scan->getResults(5, false);

  int count = results.getCount();
  for (int i = 0; i < count; i++) {
    const NimBLEAdvertisedDevice* device = results.getDevice(i);
    if (!device) continue;

    String name = device->haveName() ? String(device->getName().c_str()) : "";
    if (device->haveName() && name.equalsIgnoreCase(targetName)) {
      return const_cast<NimBLEAdvertisedDevice*>(device);
    }
    if (device->isAdvertisingService(NimBLEUUID(serviceUUIDStr.c_str()))) {
      return const_cast<NimBLEAdvertisedDevice*>(device);
    }
  }

  return nullptr;
}

bool X5hPrinter::begin(const char* namePrefix) {
  initBLE();
  if (namePrefix && strlen(namePrefix) > 0) targetName = namePrefix;
  else targetName = X5H_DEFAULT_NAME;

  auto* device = findPrinter();
  if (!device) return false;
  return connectToDevice(device);
}

bool X5hPrinter::begin(NimBLEAddress address) { return connectToAddress(address); }

bool X5hPrinter::autoConnect(const char* targetName_, const char* targetMac) {
  initBLE();
  if (targetMac && strlen(targetMac) > 0) {
    NimBLEAddress address(std::string(targetMac), BLE_ADDR_PUBLIC);
    if (connectToAddress(address)) return true;
  }

  if (targetName_ && strlen(targetName_) > 0) targetName = targetName_;
  else targetName = X5H_DEFAULT_NAME;

  auto* device = findPrinter();
  if (!device) return false;
  return connectToDevice(device);
}

void X5hPrinter::printSample() {
  setDensity(DENSITY_ULTRA_DARK);
  printText("TinyPrinter POS Pro", 2, ALIGN_CENTER);
  printLine(2);
  printFramedText("DISCOUNT COUPON", FRAME_COUPON_TICKET, FRAME_SIZE_MEDIUM, 1, 1, ALIGN_CENTER);
  printQRCodeWiFi("MyHomeWiFi", "SecretPass123", "WPA", QR_LARGE, ALIGN_CENTER);
  printCutLine();
  printBarcode("8690000123456", BARCODE_EAN13, BARCODE_SIZE_LARGE, true, ALIGN_CENTER);
  feed(80);
}
