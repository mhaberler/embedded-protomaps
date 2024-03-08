#include <SdFat.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include <string>

#include "logging.hpp"

using namespace std;

#ifdef ARDUINO_M5STACK_CORES3
    #define TFCARD_CS_PIN 4
    #define SPI_CLOCK 40000000L
#endif

#ifdef ARDUINO_M5STACK_Core2
    #define TFCARD_CS_PIN 4
    #define SPI_CLOCK 25000000L
#endif

#ifdef ARDUINO_M5STACK_Paper
    #define TFCARD_CS_PIN 4
    #define SPI_CLOCK 25000000L
#endif

SdFs sd;

#define display M5.Display

void describeCard(SdFs &sdfs) {
    LOG_INFO("SD card mounted successfully");
    LOG_INFO("card size: %.0f GB", sd.card()->sectorCount() * 512E-9);
    switch (sdfs.card()->type()) {
        case SD_CARD_TYPE_SD1:
            LOG_INFO("Card type: SD1");
            break;
        case SD_CARD_TYPE_SD2:
            LOG_INFO("Card type: SD2");
            break;
        case SD_CARD_TYPE_SDHC:
            LOG_INFO("Card type: SDHC/SDXC");
            break;
        default:
            LOG_INFO("Card type unknown");
            break;
    }
    switch (sdfs.fatType()) {
        case FAT_TYPE_EXFAT:
            LOG_INFO("FS type: exFat");
            break;
        case FAT_TYPE_FAT32:
            LOG_INFO("FS type: FAT32");
            break;
        case FAT_TYPE_FAT16:
            LOG_INFO("FS type: FAT16");
            break;
        case FAT_TYPE_FAT12:
            LOG_INFO("FS type: FAT12");
            break;
    }
    cid_t cid;
    if (!sdfs.card()->readCID(&cid)) {
        LOG_ERROR("readCID failed");
        return;
    }
    LOG_INFO("Manufacturer ID: 0x%x", int(cid.mid));
    LOG_INFO("OEM ID: 0x%x 0x%x", cid.oid[0], cid.oid[1]);
    LOG_INFO("Product: '%s'", string(cid.pnm, sizeof(cid.pnm)).c_str());
    LOG_INFO("Revision: %d.%d", cid.prvN(), cid.prvM());
    LOG_INFO("Serial number: %lu", cid.psn());
    LOG_INFO("Manufacturing date: %d/%d", cid.mdtMonth(), cid.mdtYear());
}

bool init_sd_card(void) {
    int retry = 3;
    bool mounted{};
    while(retry-- && !(mounted = sd.begin((unsigned)TFCARD_CS_PIN, SD_SCK_MHZ(25))) ) {
        delay(100);
    }
    if(!mounted) {
        M5_LOGE("Failed to mount 0x%x", sd.sdErrorCode());
        display.startWrite();
        display.clear(TFT_RED);
        display.endWrite();
        return false;
    }
    display.startWrite();
    display.clear(TFT_GREEN);
    display.endWrite();
    describeCard(sd);
    return true;
}