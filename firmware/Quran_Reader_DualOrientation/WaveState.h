#pragma once
#include <stdint.h>
enum class WaveEvent : uint8_t { Next, Previous, Click, Hold, Power, Gauge };
struct WaveState {
  uint16_t screen=1, count=1810;
  bool portrait=false, blank=false, menu=false, selectedPortrait=false;
  uint8_t digits[3]={0,0,1}, activeDigit=2;
  uint32_t revision=0,lastInput=0,lastActivity=0,gaugeGeneration=0;
  bool anyHeld=false;
  static uint16_t first(uint16_t page) {return page<=1?1:page==2?3:3*page-4;}
  static uint16_t page(uint16_t screen) {return screen<=2?1:screen<=4?2:(screen-5)/3+3;}
  uint16_t selectedPage() const {return digits[0]*100+digits[1]*10+digits[2];}
  bool apply(WaveEvent e,uint32_t now) {
    lastActivity=now;
    switch(e) {
      case WaveEvent::Next:
      case WaveEvent::Previous: {
        if(blank)return false;
        int d=e==WaveEvent::Next?1:-1;
        if(menu) {
          if(activeDigit==3)selectedPortrait=!selectedPortrait;
          else digits[activeDigit]=(digits[activeDigit]+(d>0?1:9))%10;
        } else {
          int target;
          if(portrait) {
            int p=int(page(screen))+d;
            if(p<1 || p>604)return false;
            target=first(p);
          } else target=int(screen)+d;
          if(target<1 || target>count)return false;
          screen=uint16_t(target);
        }
        break;
      }
      case WaveEvent::Click:
        if(!menu)return false;
        activeDigit=activeDigit==0?3:activeDigit==3?2:activeDigit-1;
        break;
      case WaveEvent::Hold:
        if(menu) {
          uint16_t p=selectedPage();
          if(p<1 || p>604 || first(p)>count)return false;
          screen=first(p);portrait=selectedPortrait;menu=false;blank=false;
        } else {
          uint16_t p=page(screen);
          digits[0]=p/100;digits[1]=p/10%10;digits[2]=p%10;
          activeDigit=2;selectedPortrait=portrait;menu=true;blank=false;
        }
        break;
      case WaveEvent::Power:
        if(menu){menu=false;blank=true;}else blank=!blank;
        break;
      case WaveEvent::Gauge:
        if(!blank || menu)return false;
        ++gaugeGeneration;
        break;
    }
    ++revision;lastInput=now;return true;
  }
};
