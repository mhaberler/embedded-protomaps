#include <SdFat.h>
#include <string>
#include <SPI.h>

#include "logging.hpp"

using namespace std;

#ifdef CORES3
    #define TFCARD_CS_PIN 4
    #define SPI_CLOCK 40000000L
#endif

#ifdef CORE2
    #define TFCARD_CS_PIN 4
    #define SPI_CLOCK 25000000L
#endif

#ifdef EPAPER
    #define TFCARD_CS_PIN 4
    #define SPI_CLOCK 25000000L
#endif


#define SD_FAT_TYPE 2  //exFat 

SdFat sdfat;
SdBaseFile file;

// M5Core2 + M5CoreS3 share SPI between SD card and the LCD display
#define SD_CONFIG SdSpiConfig(TFCARD_CS_PIN, SHARED_SPI, SPI_CLOCK)

void describeCard(SdFat &sd) {
    LOG_INFO("SD card mounted successfully");
    LOG_INFO("card size: %.0f GB", sd.card()->sectorCount() * 512E-9);
    switch (sd.card()->type()) {
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
    switch (sd.fatType()) {
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
    if (!sd.card()->readCID(&cid)) {
        LOG_ERROR("readCID failed");
        return;
    }
    LOG_INFO("Manufacturer ID: 0x%x", int(cid.mid));
    LOG_INFO("OEM ID: 0x%x 0x%x", cid.oid[0], cid.oid[1]);
    LOG_INFO("Product: '%s'", string(cid.pnm, sizeof(cid.pnm)).c_str());
    LOG_INFO("Revision: %d.%d", cid.prvN(), cid.prvM());
    LOG_INFO("Serial number: %lu", cid.psn());
    LOG_INFO("Manufacturing date: %d/%d", cid.mdtMonth(), cid.mdtYear());
    // LOG_INFO("SdFat library version: %s", SD_FAT_VERSION_STR);
}


bool init_sd_card(void) {
    if (!sdfat.begin(SD_CONFIG)) {
        LOG_ERROR("sdfat.begin() failed");
        return false;
    }
    describeCard(sdfat);
    // sdfat.ls(LS_DATE|LS_SIZE|LS_R);
    return true;
}