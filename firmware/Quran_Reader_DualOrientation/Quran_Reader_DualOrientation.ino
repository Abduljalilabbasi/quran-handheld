// SPDX-License-Identifier: GPL-3.0-or-later
#include <SPI.h>
#include <SD_MMC.h>
#include <GxEPD2_BW.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// ============================================================
// Waveshare ESP32-S3-ePaper-3.97 Quran Reader
//
// NORMAL READING
//   LEFT         = next Quran screen
//   RIGHT        = previous Quran screen
//   CENTER click = no action while reading
//   CENTER hold  = page number and orientation menu
//   POWER click  = completely blank / restore Quran page
//
// PAGE / ORIENTATION MENU
// LEFT / RIGHT: adjust digit or orientation
// CENTER click: cycle digits, then orientation
// CENTER hold: apply and read
// POWER click: close menu and blank
//
// LAST POSITION
//   Current Quran screen is stored in ESP32 NVS and survives
//   complete battery disconnection.
//
// Bitmap polarity: NORMAL / NOT INVERTED
// ============================================================


// ---------------- E-paper pins ----------------

constexpr int EPD_BUSY = 3;
constexpr int EPD_DC   = 9;
constexpr int EPD_CS   = 10;
constexpr int EPD_SCLK = 11;
constexpr int EPD_MOSI = 12;
constexpr int EPD_RST  = 46;


// ---------------- Confirmed controls ----------------
// Arabic page direction requested:
// LEFT = forward, RIGHT = backward.

constexpr int BTN_LEFT  = 6;
constexpr int BTN_PRESS = 5;
constexpr int BTN_RIGHT = 4;
constexpr int BTN_POWER = 1;  // PWR_OUT: HIGH while the side power key is pressed.
constexpr int BTN_BOOT  = 0;   // ESP32-S3 BOOT button
constexpr uint32_t BOOT_GAUGE_RESET_HOLD_MS = 5000;

// ESP32-S3 automatic deep sleep.
// The e-paper image remains visible while the ESP32 sleeps.
constexpr uint32_t AUTO_SLEEP_MS = 3UL * 60UL * 1000UL;  // 3 minutes
uint32_t lastActivityMs = 0;


// ---------------- Onboard microSD ----------------

constexpr int SD_CLK = 16;
constexpr int SD_CMD = 17;
constexpr int SD_D0  = 15;


// ---------------- AXP2101 battery gauge ----------------
// Waveshare board I2C bus: SDA=41, SCL=42
// AXP2101 PMIC address: 0x34
constexpr int I2C_SDA = 41;
constexpr int I2C_SCL = 42;
constexpr uint8_t AXP2101_ADDR = 0x34;
constexpr uint8_t AXP2101_STATUS1 = 0x00;
constexpr uint8_t AXP2101_STATUS2 = 0x01;
constexpr uint8_t AXP2101_BAT_VOLTAGE_H = 0x34;
constexpr uint8_t AXP2101_BAT_VOLTAGE_L = 0x35;
constexpr uint8_t AXP2101_BAT_PERCENT = 0xA4;


// ---------------- Display ----------------

constexpr uint16_t SCREEN_W = 800;
constexpr uint16_t SCREEN_H = 480;
constexpr size_t SCREEN_BYTES = (SCREEN_W * SCREEN_H) / 8;

constexpr unsigned long LONG_PRESS_MS = 900;

using EpdDriver = GxEPD2_397_GDEM0397T81;

GxEPD2_BW<EpdDriver, EpdDriver::HEIGHT> display(
  EpdDriver(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);


// ---------------- Persistent storage ----------------

Preferences prefs;


// ---------------- Reader state ----------------

uint8_t* frameBuffer = nullptr;

uint16_t totalScreens = 0;
uint16_t currentScreen = 1;

RTC_DATA_ATTR bool screenBlanked = false;
bool portraitMode = false;
void applyOrientation()
{
  display.setRotation(portraitMode ? 3 : 2);
}

// Fast refresh counter; full refresh occurs only when explicitly requested.
uint8_t fastRefreshCount = 0;


// ============================================================
// Standard 604-page Madinah Mushaf navigation
//
// First half = first Hizb of each Juz
// Second half = second Hizb of each Juz
// ============================================================

const uint16_t JUZ_START_PAGE[30] =
{
    1,  22,  42,  62,  82,
  102, 121, 142, 162, 182,
  202, 222, 242, 262, 282,
  302, 322, 342, 362, 382,
  402, 422, 442, 462, 482,
  502, 522, 542, 562, 582
};

const uint16_t HALF2_START_PAGE[30] =
{
   11,  32,  52,  72,  92,
  112, 132, 152, 172, 192,
  212, 232, 252, 272, 292,
  312, 332, 352, 372, 392,
  412, 432, 452, 472, 492,
  512, 532, 552, 572, 592
};


// ============================================================
// Input helpers
// ============================================================

void waitForRelease(int pin)
{
  delay(30);

  while (digitalRead(pin) == LOW)
  {
    delay(10);
  }

  delay(50);
}


constexpr int CENTER_NONE  = 0;
constexpr int CENTER_SHORT = 1;
constexpr int CENTER_LONG  = 2;


int readCenterPress()
{
  if (digitalRead(BTN_PRESS) != LOW)
  {
    return CENTER_NONE;
  }

  unsigned long started = millis();
  bool isLong = false;

  while (digitalRead(BTN_PRESS) == LOW)
  {
    if ((millis() - started) >= LONG_PRESS_MS)
    {
      isLong = true;
    }

    delay(10);
  }

  delay(60);

  return isLong ? CENTER_LONG : CENTER_SHORT;
}


constexpr int MENU_LEFT   = 10;
constexpr int MENU_RIGHT  = 11;
constexpr int MENU_SELECT = 12;
constexpr int MENU_BACK   = 13;
constexpr int MENU_POWER  = 14;


// Short power-key presses are acted on after release. Long holds are left
// to the PMIC hardware; no power-management registers are changed.
bool readPowerShortPress()
{
  if (digitalRead(BTN_POWER) != HIGH) return false;
  uint32_t started = millis();
  delay(30);
  if (digitalRead(BTN_POWER) != HIGH) return false;
  bool longPress = false;
  while (digitalRead(BTN_POWER) == HIGH)
  {
    if ((uint32_t)(millis() - started) >= LONG_PRESS_MS) longPress = true;
    delay(10);
  }
  delay(40);
  return !longPress;
}

void toggleScreenFromPower()
{
  markActivity();
  if (screenBlanked) showCurrent();
  else blankScreen();
  markActivity();
}

int waitForMenuEvent()
{
  while (true)
  {
    if (readPowerShortPress()) return MENU_POWER;
    if (digitalRead(BTN_LEFT) == LOW)
    {
      waitForRelease(BTN_LEFT);
      return MENU_LEFT;
    }

    if (digitalRead(BTN_RIGHT) == LOW)
    {
      waitForRelease(BTN_RIGHT);
      return MENU_RIGHT;
    }

    int cp = readCenterPress();

    if (cp == CENTER_SHORT)
    {
      return MENU_SELECT;
    }

    if (cp == CENTER_LONG)
    {
      return MENU_BACK;
    }

    delay(10);
  }
}


// ============================================================
// Persistent reading position
// ============================================================

void saveCurrentPosition()
{
  prefs.putUShort("screen", currentScreen);
  prefs.putBool("portrait", portraitMode);

  Serial.print("Saved screen: ");
  Serial.println(currentScreen);
}


void restoreSavedPosition()
{
  uint16_t saved = prefs.getUShort("screen", 1);

  if (saved < 1)
  {
    saved = 1;
  }

  if (saved > totalScreens)
  {
    saved = totalScreens;
  }

  currentScreen = saved;

  Serial.print("Restored saved screen: ");
  Serial.println(currentScreen);
}


// ============================================================
// Fast full-screen update helper
// ============================================================

void beginFastFullScreen()
{
  display.setPartialWindow(0, 0, display.width(), display.height());
}

// ============================================================
// AXP2101 battery gauge
// ============================================================

bool axpReadReg(uint8_t reg, uint8_t &value)
{
  Wire.beginTransmission(AXP2101_ADDR);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }

  if (Wire.requestFrom((int)AXP2101_ADDR, 1) != 1)
  {
    return false;
  }

  value = Wire.read();
  return true;
}


bool axpWriteReg(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(AXP2101_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}


bool resetFuelGauge()
{
  uint8_t reg18 = 0;
  uint8_t reg17 = 0;

  // Enable fuel gauge: REG 0x18 bit 3.
  if (!axpReadReg(0x18, reg18))
  {
    return false;
  }

  reg18 |= 0x08;

  if (!axpWriteReg(0x18, reg18))
  {
    return false;
  }

  delay(100);

  // Reset fuel gauge: REG 0x17 bit 3.
  if (!axpReadReg(0x17, reg17))
  {
    return false;
  }

  reg17 |= 0x08;

  if (!axpWriteReg(0x17, reg17))
  {
    return false;
  }

  delay(1500);
  return true;
}


int readBatteryPercent()
{
  uint8_t status1 = 0;
  uint8_t percent = 0;

  if (!axpReadReg(AXP2101_STATUS1, status1))
  {
    return -1;
  }

  // AXP2101 STATUS1 bit 3 = battery present.
  if ((status1 & (1 << 3)) == 0)
  {
    return -2; // no battery detected
  }

  if (!axpReadReg(AXP2101_BAT_PERCENT, percent))
  {
    return -1;
  }

  if (percent > 100)
  {
    return -1;
  }

  return (int)percent;
}



int readBatteryMillivolts()
{
  uint8_t hi = 0;
  uint8_t lo = 0;

  if (!axpReadReg(AXP2101_BAT_VOLTAGE_H, hi)) return -1;
  if (!axpReadReg(AXP2101_BAT_VOLTAGE_L, lo)) return -1;

  // AXP2101 battery voltage ADC: 14-bit big-endian, 1 mV/LSB.
  int mv = ((int)(hi & 0x3F) << 8) | lo;
  if (mv < 2000 || mv > 5000) return -1;
  return mv;
}

bool readUsbPresent()
{
  uint8_t status1 = 0;
  if (!axpReadReg(AXP2101_STATUS1, status1)) return false;
  return (status1 & (1 << 5)) != 0; // VBUS good
}

bool readCharging()
{
  uint8_t status2 = 0;
  if (!axpReadReg(AXP2101_STATUS2, status2)) return false;
  return ((status2 >> 5) & 0x03) == 0x01;
}




void drawBatteryIndicator()
{
  int pct = readBatteryPercent();

  // Battery indicator lives in the upper-right corner.
  // It is redrawn AFTER the underlying page/menu so it stays visible.
  constexpr int boxW = 44;
  constexpr int boxH = 22;
  constexpr int tipW = 4;
  const int x = display.width() - 118;
  constexpr int y = 5;

  // White background patch.
  display.fillRect(x - 4, 0, 122, 34, GxEPD_WHITE);

  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(2);

  if (pct >= 0)
  {
    char txt[8];
    snprintf(txt, sizeof(txt), "%d%%", pct);
    display.setCursor(x, 8);
    display.print(txt);

    int bx = display.width() - boxW - tipW - 6;
    int by = y;

    // Battery outline.
    display.drawRect(bx, by, boxW, boxH, GxEPD_BLACK);
    display.fillRect(bx + boxW, by + 6, tipW, 10, GxEPD_BLACK);

    // Fill level inside the battery.
    int innerW = boxW - 6;
    int fillW = (innerW * pct) / 100;

    if (fillW > 0)
    {
      display.fillRect(
        bx + 3,
        by + 3,
        fillW,
        boxH - 6,
        GxEPD_BLACK
      );
    }
  }
  else if (pct == -2)
  {
    display.setCursor(display.width() - 74, 8);
    display.print("USB");
  }
  else
  {
    display.setCursor(display.width() - 74, 8);
    display.print("--%");
  }
}


// ============================================================
// Basic e-paper helpers
// ============================================================

void showStatus(const char* line1, const char* line2 = "")
{
  beginFastFullScreen();

  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.setTextSize(portraitMode ? 3 : 4);
    display.setCursor(20, 185);
    display.print(line1);

    display.setTextSize(2);
    display.setCursor(45, 260);
    display.print(line2);

    drawBatteryIndicator();
  }
  while (display.nextPage());
}

void blankScreen()
{
  // Fast white update without a deliberate full-refresh cycle.
  beginFastFullScreen();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
  screenBlanked = true;
  Serial.println("Screen completely blank.");
}

// ============================================================
// BOOT button battery-gauge reset
//
// Available ONLY while the Quran page is blank.
// Hold BOOT (GPIO0) for 5 seconds.
// ============================================================

bool bootHeldForGaugeReset()
{
  if (digitalRead(BTN_BOOT) != LOW)
  {
    return false;
  }

  uint32_t started = millis();

  while (digitalRead(BTN_BOOT) == LOW)
  {
    if ((uint32_t)(millis() - started) >= BOOT_GAUGE_RESET_HOLD_MS)
    {
      while (digitalRead(BTN_BOOT) == LOW)
      {
        delay(10);
      }

      delay(80);
      return true;
    }

    delay(10);
  }

  return false;
}


void handleFuelGaugeResetFromBoot()
{
  markActivity();

  beginFastFullScreen();
  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(3);
    display.setCursor(12, 190);
    display.print("RESETTING GAUGE");
    drawBatteryIndicator();
  }
  while (display.nextPage());

  bool ok = resetFuelGauge();

  beginFastFullScreen();
  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(4);
    display.setCursor(20, 180);
    display.print(ok ? "GAUGE RESET" : "RESET FAILED");

    display.setTextSize(2);
    display.setCursor(20, 255);
    display.print("Returning to battery screen...");
  }
  while (display.nextPage());

  delay(1000);

  // Return to the completely blank screen.
  blankScreen();
}


// ============================================================
// SD Quran file helpers
// ============================================================

String screenPath(uint16_t index)
{
  char buf[32];

  snprintf(
    buf,
    sizeof(buf),
    portraitMode ? "/quran_portrait/%05u.bin" : "/quran/%05u.bin",
    portraitMode ? pageFromScreen(index) : index
  );

  return String(buf);
}


bool loadCount()
{
  File f = SD_MMC.open("/quran/count.txt", FILE_READ);

  if (!f)
  {
    Serial.println("ERROR: missing /quran/count.txt");
    return false;
  }

  String s = f.readStringUntil('\n');
  f.close();

  s.trim();

  totalScreens = (uint16_t)s.toInt();

  Serial.print("Total screens: ");
  Serial.println(totalScreens);

  return totalScreens > 0;
}


bool loadScreenBytes(uint16_t index)
{
  if (index < 1 || index > totalScreens)
  {
    Serial.println("ERROR: screen index out of range");
    return false;
  }

  String path = screenPath(index);

  Serial.print("Opening: ");
  Serial.println(path);

  File f = SD_MMC.open(path, FILE_READ);

  if (!f)
  {
    Serial.println("ERROR: could not open screen file");
    return false;
  }

  size_t got = f.read(frameBuffer, SCREEN_BYTES);
  f.close();

  if (got != SCREEN_BYTES)
  {
    Serial.print("ERROR: wrong byte count: ");
    Serial.println((unsigned long)got);
    return false;
  }

  // No inversion.

  return true;
}



// ============================================================
// Small part/page indicator
//
// Normal pages use three e-paper screens:
//   1/301, 2/301, 3/301
//
// Pages 1 and 2 use two screens.
// ============================================================

void getPageAndPart(uint16_t screen, uint16_t &page, uint8_t &part)
{
  if (screen <= 2)
  {
    page = 1;
    part = (uint8_t)screen;
    return;
  }

  if (screen <= 4)
  {
    page = 2;
    part = (uint8_t)(screen - 2);
    return;
  }

  page = (uint16_t)(((screen - 5) / 3) + 3);
  part = (uint8_t)(((screen - 5) % 3) + 1);
}


void drawPageIndicator()
{
  uint16_t page;
  uint8_t part;

  getPageAndPart(currentScreen, page, part);

  char label[16];
  snprintf(label, sizeof(label), "%u/%u", part, page);

  // Approximately 4x the original tiny label.
  display.setTextSize(4);
  display.setTextColor(GxEPD_BLACK);

  int16_t textW = (int16_t)strlen(label) * 24;

  // Upper-left white patch so the indicator is clear.
  display.fillRect(0, 0, textW + 8, 38, GxEPD_WHITE);

  display.setCursor(4, 4);
  display.print(label);
}


void drawBuffer(bool forceFullRefresh = false)
{
  if (forceFullRefresh)
  {
    display.setFullWindow();
    fastRefreshCount = 0;
  }
  else
  {
    beginFastFullScreen();
    fastRefreshCount++;
  }

  display.firstPage();

  do
  {
    display.fillScreen(GxEPD_WHITE);

    display.drawBitmap(
      0,
      0,
      frameBuffer,
      display.width(),
      display.height(),
      GxEPD_BLACK
    );

    // Do not cover any of the complete portrait Quran page.
    if (!portraitMode)
    {
      drawPageIndicator();
      drawBatteryIndicator();
    }
  }
  while (display.nextPage());
}

bool showCurrent()
{
  if (!loadScreenBytes(currentScreen))
  {
    String path = screenPath(currentScreen);

    showStatus(
      "FILE ERROR",
      path.c_str()
    );

    return false;
  }

  drawBuffer();

  screenBlanked = false;

  Serial.print("Showing screen ");
  Serial.print(currentScreen);
  Serial.print("/");
  Serial.println(totalScreens);

  return true;
}


void goToScreen(uint16_t targetScreen)
{
  if (targetScreen < 1)
  {
    targetScreen = 1;
  }

  if (targetScreen > totalScreens)
  {
    targetScreen = totalScreens;
  }

  uint16_t previous = currentScreen;

  currentScreen = targetScreen;

  if (showCurrent())
  {
    // NVS write happens only after a valid screen is displayed.
    saveCurrentPosition();
  }
  else
  {
    currentScreen = previous;
  }
}


// ============================================================
// Mushaf page <-> generated screen mapping
//
// Browser SVG full build:
//   page 1 = 2 screens
//   page 2 = 2 screens
//   page 3..604 = 3 screens each
// ============================================================

uint16_t firstScreenOfPage(uint16_t mushafPage)
{
  if (mushafPage <= 1)
  {
    return 1;
  }

  if (mushafPage == 2)
  {
    return 3;
  }

  return (uint16_t)(3 * mushafPage - 4);
}


uint16_t pageFromScreen(uint16_t screen)
{
  if (screen <= 2)
  {
    return 1;
  }

  if (screen <= 4)
  {
    return 2;
  }

  return (uint16_t)(((screen - 5) / 3) + 3);
}


uint8_t juzFromPage(uint16_t page)
{
  for (int i = 29; i >= 0; --i)
  {
    if (page >= JUZ_START_PAGE[i])
    {
      return (uint8_t)(i + 1);
    }
  }

  return 1;
}


// ============================================================
// Direct page-selection menu
//
// Three digits select Mushaf page 001..604.
//
// CENTER short press:
//   move active digit one place left:
//   units -> tens -> hundreds -> orientation -> units
//
// LEFT:
//   increase active digit 0..9, then wrap to 0
//
// RIGHT:
//   decrease active digit 9..0, then wrap to 9
//
// CENTER long press:
//   jump directly to the selected Mushaf page
// ============================================================

uint16_t digitsToPage(const uint8_t digits[3])
{
  return (uint16_t)(digits[0] * 100 + digits[1] * 10 + digits[2]);
}


void pageToDigits(uint16_t page, uint8_t digits[3])
{
  if (page > 999)
  {
    page = 999;
  }

  digits[0] = (uint8_t)((page / 100) % 10);
  digits[1] = (uint8_t)((page / 10) % 10);
  digits[2] = (uint8_t)(page % 10);
}


void drawPageSelectMenu(const uint8_t digits[3], uint8_t activeDigit, bool selectedPortrait)
{
  beginFastFullScreen();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    int w = display.width();
    display.setTextSize(3);
    display.setCursor((w - 180) / 2, 55);
    display.print("GO TO PAGE");
    display.setTextSize(8);
    for (uint8_t i = 0; i < 3; ++i)
    {
      int x = (w - 240) / 2 + i * 90;
      display.setCursor(x, 115);
      display.print(digits[i]);
      if (i == activeDigit) display.fillRect(x - 3, 187, 57, 5, GxEPD_BLACK);
    }
    uint16_t selectedPage = digitsToPage(digits);
    display.setTextSize(2);
    display.setCursor((w - 240) / 2, 220);
    if (selectedPage >= 1 && selectedPage <= 604)
    {
      display.print("PAGE "); display.print(selectedPage); display.print(" / 604");
    }
    else display.print("VALID: 001 - 604");
    display.setTextSize(3);
    display.setCursor((w - 234) / 2, 270);
    display.print(selectedPortrait ? "PORTRAIT" : "LANDSCAPE");
    if (activeDigit == 3) display.fillRect((w - 252) / 2, 300, 252, 4, GxEPD_BLACK);
    display.setTextSize(2);
    display.setCursor(20, 340); display.print("LEFT/RIGHT: CHANGE");
    display.setCursor(20, 375); display.print("CLICK: NEXT FIELD");
    display.setCursor(20, 410); display.print("HOLD: APPLY & READ");
    drawBatteryIndicator();
  } while (display.nextPage());
}


// ============================================================
// Return to reading state after cancelling menu
// ============================================================

void returnFromMenu(bool wasBlanked)
{
  if (wasBlanked)
  {
    blankScreen();
  }
  else
  {
    showCurrent();
  }
}


// ============================================================
// Direct page-selection menu
// ============================================================

void openPageSelectMenu()
{
  uint8_t digits[3];
  pageToDigits(pageFromScreen(currentScreen), digits);
  uint8_t activeDigit = 2;
  bool selectedPortrait = portraitMode;
  while (true)
  {
    drawPageSelectMenu(digits, activeDigit, selectedPortrait);
    int ev = waitForMenuEvent();
    markActivity();
    if (ev == MENU_POWER)
    {
      // Close the menu and blank the physical screen, regardless of the
      // reading state that was active before opening this menu.
      blankScreen();
      markActivity();
      return;
    }
    if (ev == MENU_LEFT || ev == MENU_RIGHT)
    {
      if (activeDigit == 3) selectedPortrait = !selectedPortrait;
      else digits[activeDigit] = (digits[activeDigit] + (ev == MENU_LEFT ? 1 : 9)) % 10;
    }
    else if (ev == MENU_SELECT)
    {
      // Units -> tens -> hundreds -> orientation -> units.
      activeDigit = activeDigit == 0 ? 3 : (activeDigit == 3 ? 2 : activeDigit - 1);
    }
    else if (ev == MENU_BACK)
    {
      uint16_t targetPage = digitsToPage(digits);
      if (targetPage < 1 || targetPage > 604) continue;
      bool previousMode = portraitMode;
      uint16_t previousScreen = currentScreen;
      portraitMode = selectedPortrait;
      currentScreen = firstScreenOfPage(targetPage);
      if (!loadScreenBytes(currentScreen))
      {
        portraitMode = previousMode;
        currentScreen = previousScreen;
        showStatus("MISSING PAGE", selectedPortrait ? "Copy quran_portrait to SD" : "Check quran folder on SD");
        delay(1800);
        continue;
      }
      applyOrientation();
      drawBuffer();
      screenBlanked = false;
      saveCurrentPosition();
      markActivity();
      return;
    }
  }
}


// ============================================================
// SD initialization
// ============================================================

bool initSDCard()
{
  pinMode(SD_D0, INPUT_PULLUP);

  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0))
  {
    Serial.println("ERROR: SD_MMC.setPins failed");
    return false;
  }

  if (!SD_MMC.begin("/sdcard", true))
  {
    Serial.println("ERROR: SD mount failed");
    return false;
  }

  if (SD_MMC.cardType() == CARD_NONE)
  {
    Serial.println("ERROR: no SD card detected");
    return false;
  }

  Serial.print("SD size MB: ");
  Serial.println(
    (unsigned long long)(
      SD_MMC.cardSize() / (1024ULL * 1024ULL)
    )
  );

  return true;
}


// ============================================================
// ESP32-S3 deep sleep
// ============================================================

void markActivity()
{
  lastActivityMs = millis();
}


void enterEspDeepSleep()
{
  Serial.println("No activity for 3 minutes.");
  Serial.println("ESP32-S3 entering deep sleep.");
  Serial.println("E-paper image will remain visible.");

  // Save position once more before sleeping.
  saveCurrentPosition();

  // Wake on any of the three active-low buttons.
  uint64_t wakeMask =
      (1ULL << BTN_LEFT) |
      (1ULL << BTN_PRESS) |
      (1ULL << BTN_RIGHT);

  // GPIO 4, 5 and 6 are RTC-capable on ESP32-S3.
  // Keep their pull-ups active so an unpressed button remains HIGH.
  rtc_gpio_pullup_en((gpio_num_t)BTN_LEFT);
  rtc_gpio_pulldown_dis((gpio_num_t)BTN_LEFT);

  rtc_gpio_pullup_en((gpio_num_t)BTN_PRESS);
  rtc_gpio_pulldown_dis((gpio_num_t)BTN_PRESS);

  rtc_gpio_pullup_en((gpio_num_t)BTN_RIGHT);
  rtc_gpio_pulldown_dis((gpio_num_t)BTN_RIGHT);

  // Side power key is active HIGH, whereas the slider keys are active LOW.
  // EXT0 and EXT1 provide the two separate wake polarities on ESP32-S3.
  rtc_gpio_pullup_dis((gpio_num_t)BTN_POWER);
  rtc_gpio_pulldown_en((gpio_num_t)BTN_POWER);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_POWER, 1);

  esp_sleep_enable_ext1_wakeup(
    wakeMask,
    ESP_EXT1_WAKEUP_ANY_LOW
  );

  Serial.flush();
  delay(50);

  // Only the ESP32 is put into deep sleep in this version.
  // We deliberately do not power down the SD card or PMIC yet.
  esp_deep_sleep_start();
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);
  delay(700);

  esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  bool wokeFromPower = (wakeCause == ESP_SLEEP_WAKEUP_EXT0);
  bool wokeFromButton = wokeFromPower || (wakeCause == ESP_SLEEP_WAKEUP_EXT1);

  // Return all RTC wake pins to normal GPIO use.
  rtc_gpio_deinit((gpio_num_t)BTN_POWER);
  rtc_gpio_deinit((gpio_num_t)BTN_LEFT);
  rtc_gpio_deinit((gpio_num_t)BTN_PRESS);
  rtc_gpio_deinit((gpio_num_t)BTN_RIGHT);

  // Battery gauge / onboard sensor I2C bus.
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_PRESS, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_BOOT, INPUT_PULLUP);
  pinMode(BTN_POWER, INPUT); // External pull-up and inverting MOSFET on board.

  SPI.begin(
    EPD_SCLK,
    -1,
    EPD_MOSI,
    EPD_CS
  );

  // Retained display power/RAM is required for fast refresh after deep sleep.
  // A cold boot still performs the normal initial full refresh.
  display.init(115200, !wokeFromButton);
  prefs.begin("quran", false);
  portraitMode = prefs.getBool("portrait", false);
  applyOrientation();
  if (!wokeFromButton) screenBlanked = false;

  // On a deep-sleep wake, leave the existing e-paper image untouched.
  // This gives an almost instant visual wake with no unnecessary flash.
  if (!wokeFromButton)
  {
    showStatus(
      "QURAN READER",
      "Starting..."
    );
  }

  frameBuffer = (uint8_t*)malloc(SCREEN_BYTES);

  if (!frameBuffer)
  {
    showStatus("MEMORY ERROR");
    return;
  }

  if (!initSDCard())
  {
    showStatus(
      "SD ERROR",
      "Check card"
    );

    return;
  }

  if (!loadCount())
  {
    showStatus(
      "FILE ERROR",
      "Missing count.txt"
    );

    return;
  }

  if (!SD_MMC.exists("/quran/00001.bin"))
  {
    showStatus(
      "FILE ERROR",
      "Missing 00001.bin"
    );

    return;
  }

  // NVS was opened before selecting display orientation.

  restoreSavedPosition();

  if (!wokeFromButton)
  {
    if (loadScreenBytes(currentScreen))
    {
      drawBuffer(false);
      screenBlanked = false;
    }
    else
    {
      showStatus("MISSING PAGE", portraitMode ? "Copy quran_portrait to SD" : "Check quran folder on SD");
    }
  }
  else
  {
    // The e-paper retained the image displayed before sleeping.
    // The wake button only wakes the reader; the next press performs an action.
    Serial.println("Woke from deep sleep by button.");
    if (wokeFromPower)
    {
      // A quick tap may already be released by the time setup completes.
      if (digitalRead(BTN_POWER) == LOW || readPowerShortPress())
        toggleScreenFromPower();
    }
  }

  // Do not interpret the hard-power-on hold as a short screen toggle.
  if (!wokeFromButton)
  {
    while (digitalRead(BTN_POWER) == HIGH) delay(10);
    delay(40);
  }
  markActivity();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  if (readPowerShortPress())
  {
    toggleScreenFromPower();
    return;
  }
  // ----------------------------------------------------------
  // Automatic ESP32-only deep sleep after 3 minutes inactivity.
  // The current e-paper image remains on-screen.
  // ----------------------------------------------------------

  if ((uint32_t)(millis() - lastActivityMs) >= AUTO_SLEEP_MS)
  {
    enterEspDeepSleep();
  }

  // ----------------------------------------------------------
  // CENTER:
  // short = no action while reading
  // long  = page / orientation menu
  // ----------------------------------------------------------

  int cp = readCenterPress();

  if (cp == CENTER_LONG)
  {
    markActivity();
    openPageSelectMenu();

    delay(10);
    return;
  }

  if (cp == CENTER_SHORT)
  {
    // No screen action on the reading page; menu clicks are handled separately.
    markActivity();
    return;
  }


  // ----------------------------------------------------------
  // When blanked, ignore arrows.
  // Long center still works because it is checked above.
  // ----------------------------------------------------------

  if (screenBlanked)
  {
    // While blanked only: hold BOOT for 5 seconds to reset the battery gauge.
    if (bootHeldForGaugeReset())
    {
      handleFuelGaugeResetFromBoot();
      delay(10);
      return;
    }

    delay(10);
    return;
  }


  // ----------------------------------------------------------
  // LEFT = NEXT
  // ----------------------------------------------------------

  if (digitalRead(BTN_LEFT) == LOW)
  {
    markActivity();
    waitForRelease(BTN_LEFT);

    if (currentScreen < totalScreens)
    {
      if (portraitMode)
      {
        uint16_t page = pageFromScreen(currentScreen);
        if (page < 604) goToScreen(firstScreenOfPage(page + 1));
      }
      else goToScreen(currentScreen + 1);
    }
  }


  // ----------------------------------------------------------
  // RIGHT = PREVIOUS
  // ----------------------------------------------------------

  if (digitalRead(BTN_RIGHT) == LOW)
  {
    markActivity();
    waitForRelease(BTN_RIGHT);

    if (currentScreen > 1)
    {
      if (portraitMode)
      {
        uint16_t page = pageFromScreen(currentScreen);
        if (page > 1) goToScreen(firstScreenOfPage(page - 1));
      }
      else goToScreen(currentScreen - 1);
    }
  }


  delay(10);
}
