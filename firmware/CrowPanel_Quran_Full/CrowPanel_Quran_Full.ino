#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include "ReaderState.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <GxEPD2_BW.h>
#include <gdey/GxEPD2_579_GDEY0579T93.h>

constexpr uint8_t BTN_NEXT=6, BTN_PREVIOUS=4, BTN_CENTER=5;
constexpr uint8_t BTN_SCREEN=1, BTN_AUX=2, SCREEN_POWER=7;
constexpr uint8_t SD_POWER=42; // CrowPanel TF-card supply enable, active HIGH
constexpr uint8_t ROTATION=2; // landscape, 180 degrees
constexpr uint32_t DEBOUNCE_MS=20, HOLD_MS=900;
constexpr uint16_t PAGE_COUNT=604;
constexpr uint8_t PARTS_PER_PAGE=5;
constexpr size_t FRAME_BYTES=792*272/8;
using Panel=GxEPD2_579_GDEY0579T93;
GxEPD2_BW<Panel,Panel::HEIGHT> display(Panel(45,46,47,48));
SPIClass sdSPI(HSPI);
Preferences prefs;
struct Button {
  uint8_t pin;
  bool raw=HIGH, stable=HIGH, armed=false;
  uint32_t changed=0, down=0;
  void begin() {
    pinMode(pin,INPUT_PULLUP);
    raw=stable=digitalRead(pin);
    changed=millis();
  }
  // Release events: boot-held keys ignored; each debounced release counts.
  uint8_t poll() {
    const uint32_t now=millis();
    const bool level=digitalRead(pin);
    if(level!=raw) {raw=level;changed=now;}
    if(raw==stable || uint32_t(now-changed)<DEBOUNCE_MS) return 0;
    stable=raw;
    if(stable==LOW) {down=now;armed=true;return 0;}
    if(!armed) return 0;
    armed=false;
    return uint32_t(now-down)>=HOLD_MS ? 2 : 1;
  }
};
Button buttons[]={{BTN_NEXT},{BTN_PREVIOUS},{BTN_CENTER},{BTN_SCREEN},{BTN_AUX}};
// Source artwork buffers: MSB first, 1=black. Header rows 0..23 stay white.
uint8_t artwork[FRAME_BYTES], pendingArtwork[FRAME_BYTES];
uint16_t page=1;
uint8_t part=1, digits[3]={0,0,1}, activeDigit=2;
bool blanked=false, menu=false, sdReady=false, haveArtwork=false;
bool positionDirty=false, prefsReady=false;
uint32_t positionChangedAt=0;
char errorText[96]="";

void savePosition() {
  if(positionDirty && prefsReady) {
    prefs.putUShort("position",(page-1)*PARTS_PER_PAGE+(part-1));
    positionDirty=false;
  }
}
void markPosition() {positionDirty=true;positionChangedAt=millis();}
uint16_t selectedPage() {return digits[0]*100+digits[1]*10+digits[2];}
bool mountSD() {
  if(sdReady) return true;
  SD.end();
  sdReady=SD.begin(10,sdSPI,10000000);
  if(!sdReady || SD.cardType()==CARD_NONE) {
    sdReady=false;
    snprintf(errorText,sizeof(errorText),"SD not found: check card, then hold centre to retry.");
    return false;
  }
  return true;
}
bool loadPage(uint16_t targetPage,uint8_t targetPart) {
  if(targetPage<1 || targetPage>PAGE_COUNT || targetPart<1 || targetPart>PARTS_PER_PAGE) return false;
  if(!mountSD()) return false;
  char path[40]; snprintf(path,sizeof(path),"/quran579/%03u_%u.bin",targetPage,targetPart);
  File file=SD.open(path,FILE_READ);
  if(!file || file.size()!=FRAME_BYTES) {
    if(file) file.close();
    snprintf(errorText,sizeof(errorText),"Missing/invalid %s - copy SD_CARD contents.",path);
    sdReady=false;
    return false;
  }
  const size_t got=file.read(pendingArtwork,FRAME_BYTES);
  file.close();
  if(got!=FRAME_BYTES) {
    snprintf(errorText,sizeof(errorText),"SD read failed: %s. Try again.",path);
    sdReady=false;
    return false;
  }
  // Commit only after a complete read; failed reads keep the previous page.
  memcpy(artwork,pendingArtwork,FRAME_BYTES);
  page=targetPage;part=targetPart;haveArtwork=true;errorText[0]=0;
  return true;
}
void drawIndicator() {
  // Same upper-left location and part/page order as the original reader.
  // Reserved 24px header means this never overwrites the Quran artwork.
  display.setTextSize(2);display.setTextColor(GxEPD_BLACK);
  display.setCursor(4,4);display.printf("%u/%u",part,page);
}
void drawMenu() {
  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(3);display.setCursor(306,12);display.print("GO TO PAGE");
  display.setTextSize(8);
  for(uint8_t i=0;i<3;i++) {
    int x=270+i*90;
    display.setCursor(x,55);display.print(digits[i]);
    if(i==activeDigit) display.fillRect(x-3,122,57,4,GxEPD_BLACK);
  }
  display.setTextSize(2);display.setCursor(284,142);
  uint16_t selected=selectedPage();
  if(selected>=1 && selected<=PAGE_COUNT) display.printf("PAGE %u / 604",selected);
  else display.print("VALID: 001 - 604");
  display.setCursor(20,178);display.print("JOYSTICK: CHANGE DIGIT    CLICK: NEXT DIGIT");
  display.setCursor(20,204);display.print("HOLD CENTRE, THEN RELEASE: APPLY & READ");
}
void render(bool full=false) {
  display.fillScreen(GxEPD_WHITE);
  if(!blanked) {
    if(menu) drawMenu();
    else if(haveArtwork) {
      display.drawBitmap(0,0,artwork,792,272,GxEPD_BLACK);
      drawIndicator();
    }
    if(errorText[0]) {
      // On an SD error, show a dedicated error screen rather than cover verses.
      if(!menu) {
        display.fillScreen(GxEPD_WHITE);
        display.setTextSize(3);display.setTextColor(GxEPD_BLACK);
        display.setCursor(20,35);display.print("CHECK SD CARD");
        display.setTextSize(2);display.setCursor(20,90);
        display.printf("Current position: page %u, part %u",page,part);
        display.setCursor(20,130);display.print("Insert card; hold centre to select/retry a page.");
      }
      display.setTextSize(1);display.setTextColor(GxEPD_BLACK);
      display.setCursor(20,246);display.print(errorText);
    }
  }
  uint32_t started=millis();
  // Full only at startup / Serial f. Retain controller RAM between turns.
  display.display(!full);
  Serial.printf("%s %lu ms | page %u part %u%s%s\n",full?"FULL":"PARTIAL",
    (unsigned long)(millis()-started),page,part,blanked?" blank":"",menu?" menu":"");
  if(errorText[0]) Serial.println(errorText);

}
// The input task owns the Button objects after setup. The main task alone
// owns display, SD, buffers and Preferences. Copy requested state under a
// short critical section; never hold the lock across I/O or an e-paper wait.
ReaderState requested;
portMUX_TYPE inputMux=portMUX_INITIALIZER_UNLOCKED;
constexpr uint32_t INPUT_SETTLE_MS=180;
uint32_t paintedRevision=0, paintedCleanGeneration=0;
bool firstFrame=true;

void submitEvent(ReaderEvent event) {
  const uint32_t now=millis();
  portENTER_CRITICAL(&inputMux);
  requested.apply(event,now);
  portEXIT_CRITICAL(&inputMux);
}
ReaderState snapshotState() {
  portENTER_CRITICAL(&inputMux);
  ReaderState copy=requested;
  portEXIT_CRITICAL(&inputMux);
  return copy;
}
void inputTask(void*) {
  for(;;) {
    uint8_t event=buttons[3].poll();
    if(event==1) submitEvent(ReaderEvent::Screen);
    event=buttons[2].poll();
    if(event) submitEvent(event==2?ReaderEvent::Hold:ReaderEvent::Click);
    if(buttons[0].poll()) submitEvent(ReaderEvent::Next);
    if(buttons[1].poll()) submitEvent(ReaderEvent::Previous);
    buttons[4].poll(); // reserved auxiliary button
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
void paintRequested(const ReaderState &state,bool full) {
  menu=state.menu;blanked=state.blanked;
  memcpy(digits,state.digits,sizeof(digits));activeDigit=state.activeDigit;
  if(!menu && !blanked) {
    const uint16_t targetPage=state.index/5+1;
    const uint8_t targetPart=state.index%5+1;
    if(!haveArtwork || page!=targetPage || part!=targetPart || errorText[0]) {
      const uint16_t oldIndex=(page-1)*5+(part-1);
      if(loadPage(targetPage,targetPart) && (state.index!=oldIndex || !haveArtwork)) markPosition();
    }
  } else if(menu) errorText[0]=0;
  render(full);
  // Record ONLY the snapshot actually drawn, not any newer input state.
  paintedRevision=state.revision;paintedCleanGeneration=state.cleanGeneration;
  if(blanked) savePosition();
}
void setup() {
  Serial.begin(115200);
  for(auto &button:buttons) button.begin();
  pinMode(SCREEN_POWER,OUTPUT);digitalWrite(SCREEN_POWER,HIGH);
  pinMode(SD_POWER,OUTPUT);digitalWrite(SD_POWER,HIGH);
  pinMode(10,OUTPUT);digitalWrite(10,HIGH);delay(100);
  SPI.begin(12,-1,11,45);
  sdSPI.begin(39,13,40,10);
  display.init(115200,true,10,false);
  display.setRotation(ROTATION);display.setTextWrap(false);
  prefsReady=prefs.begin("quran579",false);
  uint16_t saved=prefsReady?prefs.getUShort("position",0):0;
  if(saved>=ReaderState::SCREEN_COUNT) saved=0;
  requested.index=saved;
  page=saved/5+1;part=saved%5+1;
  // Core 0 samples inputs even while the Arduino loop is busy refreshing.
  if(xTaskCreatePinnedToCore(inputTask,"quran-input",3072,nullptr,2,nullptr,0)!=pdPASS) {
    snprintf(errorText,sizeof(errorText),"Input task could not start. Restart the reader.");
    render(true);
    for(;;) delay(1000);
  }
}
void loop() {
  if(Serial.available()) switch(Serial.read()) {
    case 'n':submitEvent(ReaderEvent::Next);break;
    case 'p':submitEvent(ReaderEvent::Previous);break;
    case 'm':submitEvent(ReaderEvent::Hold);break;
    case 'c':submitEvent(ReaderEvent::Click);break;
    case 'b':submitEvent(ReaderEvent::Screen);break;
    case 'f':submitEvent(ReaderEvent::Clean);break;
  }
  ReaderState state=snapshotState();
  const bool dirty=firstFrame || state.revision!=paintedRevision;
  if(dirty && (firstFrame || uint32_t(millis()-state.lastInputMs)>=INPUT_SETTLE_MS)) {
    // This call can block. Input continues independently and changes requested.
    paintRequested(state,firstFrame || state.cleanGeneration!=paintedCleanGeneration);
    firstFrame=false;
  }
  // Save a successfully loaded position only after the navigation burst settles.
  state=snapshotState();
  if(positionDirty && state.revision==paintedRevision && millis()-positionChangedAt>=2000) savePosition();
  delay(5);
}
