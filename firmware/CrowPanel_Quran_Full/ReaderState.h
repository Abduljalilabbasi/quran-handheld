#pragma once
#include <stdint.h>

// Only this small state is shared. No display, SD or Preferences calls here.
enum class ReaderEvent : uint8_t { Next, Previous, Click, Hold, Screen, Clean };
struct ReaderState {
  static constexpr uint16_t SCREEN_COUNT=604*5;
  uint16_t index=0;
  uint8_t digits[3]={0,0,1}, activeDigit=2;
  bool menu=false, blanked=false;
  uint32_t revision=0, lastInputMs=0, cleanGeneration=0;

  uint16_t selectedPage() const {return digits[0]*100+digits[1]*10+digits[2];}
  bool apply(ReaderEvent event,uint32_t now) {
    switch(event) {
      case ReaderEvent::Next:
      case ReaderEvent::Previous: {
        if(blanked) return false;
        const int delta=event==ReaderEvent::Next?1:-1;
        if(menu) digits[activeDigit]=(digits[activeDigit]+(delta>0?1:9))%10;
        else {
          int next=int(index)+delta;
          if(next<0 || next>=SCREEN_COUNT) return false;
          index=uint16_t(next);
        }
        break;
      }
      case ReaderEvent::Click:
        if(!menu) return false;
        activeDigit=activeDigit==0?2:activeDigit-1;
        break;
      case ReaderEvent::Hold:
        if(menu) {
          const uint16_t page=selectedPage();
          if(page<1 || page>604) return false;
          index=(page-1)*5; menu=false;blanked=false;
        } else {
          uint16_t page=index/5+1;
          digits[0]=page/100;digits[1]=(page/10)%10;digits[2]=page%10;
          activeDigit=2;menu=true;blanked=false;
        }
        break;
      case ReaderEvent::Screen:
        if(menu) {menu=false;blanked=true;}
        else blanked=!blanked;
        break;
      case ReaderEvent::Clean:
        ++cleanGeneration;
        break;
    }
    ++revision;lastInputMs=now;return true;
  }
};
