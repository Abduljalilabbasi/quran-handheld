// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <driver/i2c.h>
#include <epdiy.h>
#include "firasans_20.h"
#include "WaveState.h"
#include "Config.h"

static WaveState requested;
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
static EpdiyHighlevelState displayState;
static Preferences prefs;
static uint8_t *sourceImage, *fb;
static uint32_t painted = UINT32_MAX, lastSave = 0;
static uint16_t savedScreen = 0;
static bool savedPortrait = false, sdReady = false, touchReady = false;
static uint16_t touchWidth = 540, touchHeight = 960;
static uint8_t touchAddress = 0x5D;
static char errorText[100] = {};

static WaveState snapshot() {
  portENTER_CRITICAL(&stateMux); WaveState s = requested;
  portEXIT_CRITICAL(&stateMux); return s;
}
static void submit(WaveEvent e) {
  portENTER_CRITICAL(&stateMux); requested.apply(e, millis());
  portEXIT_CRITICAL(&stateMux);
}
// Use the IDF I2C driver installed by epdiy; do not install Wire on the same bus.
// Each transaction is serialized by the driver's bus mutex, without holding a
// lock across the physical refresh. Touch can therefore run during rendering.
static bool regRead(uint8_t address, uint16_t reg, uint8_t *out, size_t n, bool wide) {
  uint8_t bytes[2] = {uint8_t(reg >> 8), uint8_t(reg)};
  return i2c_master_write_read_device(I2C_NUM_0, address,
    wide ? bytes : bytes + 1, wide ? 2 : 1, out, n, pdMS_TO_TICKS(30)) == ESP_OK;
}
static bool touchAck() {
  uint8_t bytes[] = {0x81, 0x4E, 0};
  return i2c_master_write_to_device(I2C_NUM_0, touchAddress, bytes, 3, pdMS_TO_TICKS(30)) == ESP_OK;
}
struct Key {
  bool raw=false, stable=false, armed=false;
  uint32_t changed=0, down=0;
  void poll(bool value, bool menuKey) {
    uint32_t now=millis();
    if(value!=raw){raw=value;changed=now;}
    if(raw==stable || uint32_t(now-changed)<25)return;
    stable=raw;
    if(stable){down=now;armed=true;}
    else if(armed){armed=false;submit(menuKey ? (uint32_t(now-down)>=HOLD_MS ? WaveEvent::Hold : WaveEvent::Click) : WaveEvent::Power);}
  }
};
static void inputTask(void*) {
  Key menuKey, screenKey;
  // Ignore keys already held at startup until released.
  bool bootReleased=false, screenReleased=false, touching=false;
  bool homeDown=false, lightOn=false;
  uint32_t touchDown=0;
  int zone=1;
  for(;;) {
    bool boot=digitalRead(0)==LOW;
    if(!boot)bootReleased=true;
    if(bootReleased)menuKey.poll(boot,true);
    uint8_t keys;
    if(regRead(0x20,1,&keys,1,false)) {
      bool pressed=(keys & 4)==0; // PCA9535 IO12 user key
      if(!pressed)screenReleased=true;
      if(screenReleased)screenKey.poll(pressed,false);
    }
    if(touchReady) {
      uint8_t status;
      if(regRead(touchAddress,0x814E,&status,1,true) && (status&0x80)) {
        uint8_t count=status&15, point[8];
        bool valid=count==0 || (count<=5 && regRead(touchAddress,0x814F,point,8,true));
        if(valid && touchAck()) {
          // GT911's separate capacitive Home key is status bit 4 (LilyGO
          // SensorLib GT911_GET_HAVE_KEY). Latch until a release report so
          // holding the circle does not repeatedly toggle the frontlight.
          bool home=(status & 0x10)!=0;
          if(home && !homeDown) {
            lightOn=!lightOn;
            analogWrite(11,lightOn?230:0); // same high level as vendor UI
            Serial.printf("Frontlight %s\n",lightOn?"ON":"OFF");
          }
          homeDown=home;
          if(home){touching=false;vTaskDelay(pdMS_TO_TICKS(10));continue;}
          if(count && !touching) {
            int x=point[1]|(point[2]<<8), y=point[3]|(point[4]<<8);
            x=constrain(x,0,int(touchWidth)-1); y=constrain(y,0,int(touchHeight)-1);
            if(TOUCH_MIRROR_X)x=touchWidth-1-x;
            if(TOUCH_MIRROR_Y)y=touchHeight-1-y;
            // Normalize raw coordinates to native landscape physical pixels.
            int px,py;
            if(touchWidth<touchHeight){px=959-y*960/touchHeight;py=x*540/touchWidth;}
            else {px=x*960/touchWidth;py=y*540/touchHeight;}
            WaveState s=snapshot();
            int lx=s.portrait?py:px, w=s.portrait?540:960;
            if(FLIP_180)lx=w-1-lx;
            zone=lx<w/3?0:lx>=2*w/3?2:1;
            touching=true;touchDown=millis();
          } else if(!count && touching) {
            touching=false;
            if(uint32_t(millis()-touchDown)>=25) {
              if(zone==1)submit(uint32_t(millis()-touchDown)>=HOLD_MS?WaveEvent::Hold:WaveEvent::Click);
              else submit(zone==0?WaveEvent::Next:WaveEvent::Previous);
            }
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
static void text(const char *s,int x,int y) {
  EpdFontProperties props=epd_font_properties_default();
  epd_write_string(&FiraSans_20,s,&x,&y,fb,&props);
}
static bool loadImage(const WaveState &s) {
  if(!sdReady)sdReady=SD.begin(12,SPI,4000000);
  if(!sdReady){strcpy(errorText,"SD not found: FAT32 card required");return false;}
  char path[48];
  snprintf(path,sizeof(path),s.portrait?"/quran_portrait/%05u.bin":"/quran/%05u.bin",s.portrait?WaveState::page(s.screen):s.screen);
  File f=SD.open(path,FILE_READ);
  if(!f || f.size()!=48000 || f.read(sourceImage,48000)!=48000) {
    snprintf(errorText,sizeof(errorText),"Missing/wrong image: %s",path);return false;
  }
  f.close();errorText[0]=0;return true;
}
static bool render(const WaveState &s) {
  epd_set_rotation(s.portrait ? (FLIP_180?EPD_ROT_INVERTED_PORTRAIT:EPD_ROT_PORTRAIT)
    : (FLIP_180?EPD_ROT_INVERTED_LANDSCAPE:EPD_ROT_LANDSCAPE));
  memset(fb,255,960*540/2);
  bool loaded=false;
  if(s.menu) {
    char line[80];
    text("SELECT QURAN PAGE",30,65);
    snprintf(line,sizeof(line),"%u  %u  %u",s.digits[0],s.digits[1],s.digits[2]);text(line,30,130);
    snprintf(line,sizeof(line),"Field: %s",s.activeDigit==0?"hundreds":s.activeDigit==1?"tens":s.activeDigit==2?"units":"orientation");text(line,30,190);
    text(s.selectedPortrait?"PORTRAIT - complete page":"LANDSCAPE - segments",30,250);
    text("Left: +   Right: -",30,310);
    text("Tap centre: next field",30,370);
    text("Hold centre: apply",30,430);
  } else if(!s.blank) {
    loaded=loadImage(s);
    if(loaded) {
      int sw=s.portrait?480:800, sh=s.portrait?800:480;
      // Crop ONLY empty outer margins in landscape; retain every ink pixel.
      // Portrait deliberately preserves the complete source page geometry.
      int left=0,top=0,cw=sw,ch=sh;
      if(!s.portrait) {
        int right=-1,bottom=-1;left=sw;top=sh;
        for(int y=0;y<sh;y++)for(int x=0;x<sw;x++) {
          if(sourceImage[y*(sw/8)+x/8] & (0x80>>(x&7))) {
            left=min(left,x);right=max(right,x);top=min(top,y);bottom=max(bottom,y);
          }
        }
        if(right>=left){cw=right-left+1;ch=bottom-top+1;}
        else {left=top=0;cw=sw;ch=sh;}
      }
      int dw=s.portrait?540:960, dh=s.portrait?900:504;
      if(!s.portrait) {
        if(int64_t(cw)*dh>int64_t(ch)*dw)dh=ch*dw/cw;
        else dw=cw*dh/ch;
      }
      int ox=s.portrait?0:(960-dw)/2, oy=s.portrait?40:36+(504-dh)/2;
      for(int y=0;y<dh;y++) {
        int sy=top+y*ch/dh;
        for(int x=0;x<dw;x++) {
          int sx=left+x*cw/dw;
          if(sourceImage[sy*(sw/8)+sx/8] & (0x80>>(sx&7)))epd_draw_pixel(ox+x,oy+y,0,fb);
        }
      }
      char label[32]; uint16_t page=WaveState::page(s.screen);
      snprintf(label,sizeof(label),s.portrait?"Page %u":"%u/%u",s.portrait?page:s.screen-WaveState::first(page)+1,page);
      text(label,4,29);
      uint8_t soc[2];
      if(regRead(0x55,0x2C,soc,2,false)) {
        unsigned percent=soc[0]|unsigned(soc[1])<<8;
        if(percent<=100){snprintf(label,sizeof(label),"%u%%",percent);text(label,s.portrait?430:850,29);}
      }
    } else {text("QURAN READER",20,65);text(errorText,20,130);text("Copy quran and quran_portrait to SD root",20,200);}
  }
  // DU supports black/white transitions, including text antialiasing converted
  // here to solid black/white. Never send intermediate grayscale in DU mode.
  for(size_t i=0;i<960*540/2;i++)fb[i]=(fb[i]&0xF0)>=0x80?((fb[i]&15)>=8?255:240):((fb[i]&15)>=8?15:0);
  epd_poweron();
  EpdDrawError err=epd_hl_update_screen(&displayState,MODE_DU,20);
  epd_poweroff();
  if(err!=EPD_DRAW_SUCCESS){Serial.printf("Refresh error %u\n",unsigned(err));return false;}
  // Keep every byte synchronized, including the last byte of each row.
  memcpy(displayState.back_fb,fb,960*540/2);
  Serial.printf("Draw screen=%u page=%u portrait=%d menu=%d blank=%d image=%d %s\n",
    s.screen,WaveState::page(s.screen),s.portrait,s.menu,s.blank,loaded,errorText);
  if(loaded && (savedScreen!=s.screen || savedPortrait!=s.portrait)) {
    savedScreen=s.screen;savedPortrait=s.portrait;lastSave=millis();
  }
  return true;
}
void readerSetup() {
  Serial.begin(115200);
  pinMode(0,INPUT_PULLUP);
  pinMode(46,OUTPUT);digitalWrite(46,HIGH); // deselect LoRa on shared SPI bus
  pinMode(11,OUTPUT);digitalWrite(11,LOW); // frontlight off; no brightness gesture
  if(!psramFound()){Serial.println("Enable OPI PSRAM");for(;;)delay(1000);}
  sourceImage=(uint8_t*)ps_malloc(48000);
  if(!sourceImage){Serial.println("Image allocation failed");for(;;)delay(1000);}
  epd_init(&epd_board_v7,&ED047TC1,EPD_LUT_64K);
  epd_set_vcom(PANEL_VCOM_MV);
  displayState=epd_hl_init(EPD_BUILTIN_WAVEFORM);fb=epd_hl_get_framebuffer(&displayState);
  // One startup clean establishes a known physical state. No idle reinitialization.
  epd_poweron();epd_clear();epd_poweroff();
  SPI.begin(14,21,13,12);sdReady=SD.begin(12,SPI,4000000);
  prefs.begin("quran-t5pro",false);
  requested.screen=prefs.getUShort("screen",1);
  if(requested.screen<1 || requested.screen>1810)requested.screen=1;
  requested.portrait=prefs.getBool("portrait",false);
  // GT911 address 0x5D: INT low during reset, then return INT to input.
  pinMode(9,OUTPUT);digitalWrite(9,LOW);pinMode(3,OUTPUT);digitalWrite(3,LOW);
  delay(15);digitalWrite(9,HIGH);delay(60);pinMode(3,INPUT);
  uint8_t dimensions[4];
  touchReady=regRead(touchAddress,0x8146,dimensions,4,true);
  if(touchReady){touchWidth=dimensions[0]|dimensions[1]<<8;touchHeight=dimensions[2]|dimensions[3]<<8;
    if(!touchWidth || !touchHeight)touchReady=false;}
  Serial.printf("GT911 %s, range %u x %u\n",touchReady?"ready":"not found",touchWidth,touchHeight);
  if(xTaskCreatePinnedToCore(inputTask,"reader-input",4096,nullptr,2,nullptr,0)!=pdPASS){Serial.println("Input task failed");for(;;)delay(1000);}
  Serial.println("n next / p previous / m menu-apply / c field / b blank / f clean");
}
void readerLoop() {
  if(Serial.available())switch(Serial.read()) {
    case 'n':submit(WaveEvent::Next);break;case 'p':submit(WaveEvent::Previous);break;
    case 'm':submit(WaveEvent::Hold);break;case 'c':submit(WaveEvent::Click);break;
    case 'b':submit(WaveEvent::Power);break;
    case 'f':epd_poweron();epd_clear();epd_poweroff();memset(displayState.back_fb,255,960*540/2);painted=UINT32_MAX;break;
  }
  WaveState s=snapshot();
  if(s.revision!=painted && uint32_t(millis()-s.lastInput)>=INPUT_SETTLE_MS) {
    if(render(s))painted=s.revision;else delay(250);
  }
  if(lastSave && uint32_t(millis()-lastSave)>2000 && snapshot().revision==painted){prefs.putUShort("screen",savedScreen);prefs.putBool("portrait",savedPortrait);lastSave=0;}
  delay(5);
}
