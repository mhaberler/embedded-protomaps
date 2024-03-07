#include "FS.h"
#include <SdFat.h>
#include <M5Unified.h>

const char *boardType(void) {
  const char *name = "unknwown";

#ifdef M5UNIFIED
  switch (M5.getBoard()) {
  case m5::board_t::board_M5StackCoreS3:
    name = "StackS3";
    break;
  case m5::board_t::board_M5AtomS3Lite:
    name = "ATOMS3Lite";
    break;
  case m5::board_t::board_M5AtomS3:
    name = "ATOMS3";
    break;
  case m5::board_t::board_M5StampC3:
    name = "StampC3";
    break;
  case m5::board_t::board_M5StampC3U:
    name = "StampC3U";
    break;
  case m5::board_t::board_M5Stack:
    name = "Stack";
    break;
  case m5::board_t::board_M5StackCore2:
    name = "StackCore2";
    break;
  case m5::board_t::board_M5StickC:
    name = "StickC";
    break;
  case m5::board_t::board_M5StickCPlus:
    name = "StickCPlus";
    break;
  case m5::board_t::board_M5StackCoreInk:
    name = "CoreInk";
    break;
  case m5::board_t::board_M5Paper:
    name = "Paper";
    break;
  case m5::board_t::board_M5Tough:
    name = "Tough";
    break;
  case m5::board_t::board_M5Station:
    name = "Station";
    break;
  case m5::board_t::board_M5Atom:
    name = "ATOM";
    break;
  case m5::board_t::board_M5AtomPsram:
    name = "ATOM PSRAM";
    break;
  case m5::board_t::board_M5AtomU:
    name = "ATOM U";
    break;
  case m5::board_t::board_M5TimerCam:
    name = "TimerCamera";
    break;
  case m5::board_t::board_M5StampPico:
    name = "StampPico";
    break;
  default:
    name = "Who am I ?";
    break;
  }
#endif
  return name;
}
