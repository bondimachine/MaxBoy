#include <Arduino.h>
#include "DMD_SPWM_Driver_RP.h"
#include <AnimatedGIF.h>
#include <LittleFS.h>


//Number of panels in x and y axis
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1

#define ENABLE_DUAL_BUFFER false

#define DMD_PIN_A 6
#define DMD_PIN_B 7
#define DMD_PIN_C 8
#define DMD_PIN_D 9
#define DMD_PIN_E 10
// put all mux pins at list
uint8_t mux_list[] = { DMD_PIN_A , DMD_PIN_B , DMD_PIN_C , DMD_PIN_D , DMD_PIN_E };

// pin OE must be one of PB0 PB1 PA6 PA7
#define DMD_PIN_nOE 13
#define DMD_PIN_SCLK 12

// Pins for R0, G0, B0, R1, G1, B1 channels and for clock.
// By default the library uses RGB color order.2
// If you need to change this - reorder the R0, G0, B0, R1, G1, B1 pins.
// All this pins also must be consecutive in ascending order
uint8_t custom_rgbpins[] = { 11, 0,1,2,3,4,5 }; // CLK, R0, G0, B0, R1, G1, B1


#define RGB128x128plainS64 33,128,128,64,0		// 128x128 1/64
DMD_RGB_FM6373<RGB128x128plainS64,COLOR_4BITS> dmd(mux_list, DMD_PIN_nOE, DMD_PIN_SCLK, custom_rgbpins, DISPLAYS_ACROSS, DISPLAYS_DOWN, ENABLE_DUAL_BUFFER);


// AnimatedGIF Object
AnimatedGIF gif;
File gifFile;

// File Callbacks
void * GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = LittleFS.open(fname, "r");
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  File *f = (File *)pHandle;
  if (f != NULL)
     f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = (File *)pFile->fHandle;
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((f->size() - f->position()) < iLen)
       iBytesRead = f->size() - f->position() - 1; // <-- ugly work-around
    if (iBytesRead <= 0) return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos += iBytesRead;
    return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) { 
  File *f = (File *)pFile->fHandle;
  pFile->iPos = iPosition;
  f->seek(iPosition);
  return iPosition;
}

void GIFDrawDMD(GIFDRAW *pDraw) {
    if (pDraw->y == -1) return; // Header/Footer

    // Process line
    uint8_t *s = pDraw->pPixels;
    uint16_t *d, *usPalette;
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth + pDraw->iX > 128)
       iWidth = 128 - pDraw->iX;
    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    if (y >= 64 || pDraw->iX >= 128 || iWidth < 1)
       return; 
    
        
    for (x=0; x<iWidth; x++) {
        uint8_t pixel = *s++;
        if (pixel != pDraw->ucTransparent || !pDraw->ucHasTransparency) {
            dmd.writePixel(pDraw->iX + x, y, usPalette[pixel]);
        }
    }
}

void play(const char *szFilename) {
    if (gif.open(szFilename, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDrawDMD)) {
        Serial.printf("GIF Opened. Canvas: %dx%d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
        int lastResult = 0;
        while ((lastResult = gif.playFrame(true, NULL)) > 0) {
            // Playing
            Serial.println("frame");
            dmd.swapBuffers(true);
        }
        if (lastResult == 0) {
            Serial.println("GIF Playback Completed");
        } else {
            Serial.print("GIF Playback Error ");
            Serial.println(gif.getLastError());
        }
        gif.close();
        Serial.println("GIF Finished. Restarting...");
    } else {
        Serial.println("Error opening GIF");
        delay(5000);
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("Starting...");
	dmd.init(); 
    Serial.println("DMD init");
    dmd.configure_multiplexer(DMD_MUX_TYPE_SHIFTREG);
    Serial.println("MUX");

    // Initialize GIF
    gif.begin(GIF_PALETTE_RGB565_LE);
    
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }
    Serial.println("LittleFS mounted");

}

void test_screen_coverage() {
    for (int y = 0; y < 64; y++) {
        Serial.println(y);
        for (int x = 0; x < 128; x++) {
            int prev_x = x != 0 ? x - 1 : 127;
            int prev_y = x == 0 ? (y != 0 ? y - 1 : 63) : y;
            dmd.drawPixel(prev_x, prev_y, dmd.Color888(0, 0, 0));
            dmd.drawPixel(x, y, dmd.Color888(255, 0, 0));
            dmd.swapBuffers(true);
        }
    }
}
void loop() {

    play("/link.gif");
    // test_screen_coverage();
}