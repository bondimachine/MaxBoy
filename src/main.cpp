#include <Arduino.h>
#include "DMD_SPWM_Driver_RP.h"
#include <AnimatedGIF.h>
#include <LittleFS.h>


#define USB_INPUT 1
#define SHOW_FPS 1
// #define WAIT_SERIAL 1

#ifdef USB_INPUT
  #define Serial Serial1
#endif

//Number of panels in x and y axis
#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1

#define ENABLE_DUAL_BUFFER false

#define DMD_PIN_A 6
#define DMD_PIN_B 7
#define DMD_PIN_C 8
#define DMD_PIN_D 9 // ignored as we are using an ABC shift register panel
#define DMD_PIN_E 10 // ignored
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
#define DMD_BASE_CLASS DMD_RGB_FM6373<RGB128x128plainS64,COLOR_4BITS>
#include "fastdmddraw.h"
FastDMD dmd(mux_list, DMD_PIN_nOE, DMD_PIN_SCLK, custom_rgbpins, DISPLAYS_ACROSS, DISPLAYS_DOWN, ENABLE_DUAL_BUFFER);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

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
    if (y >= 128 || pDraw->iX >= 128 || iWidth < 1)
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

void test_screen_coverage() {
    for (int y = 0; y < 128; y++) {
        Serial.println(y);
        for (int x = 0; x < 128; x++) {
            int prev_x = x != 0 ? x - 1 : 127;
            int prev_y = x == 0 ? (y != 0 ? y - 1 : 127) : y;
            dmd.drawPixel(prev_x, prev_y, dmd.Color888(0, 0, 0));
            dmd.drawPixel(x, y, dmd.Color888(255, 0, 0));
            dmd.swapBuffers(true);
        }
    }
}

File romFile;
#include "gb.h"

void load_rom(const char *path) {
    romFile = LittleFS.open(path, "r");
    if (!romFile) {
        while (1) {
            Serial.println("Failed to open ROM file");
        }
    }
    romFile.read(rom_bank0, sizeof(rom_bank0));
}

#define ROM_COUNT 4
const char* roms[] = { "/tetris.gb", "/drmario.gb", "/mario.gb", "/zelda.gb"  };
uint8_t current_rom = 0;
bool next_rom = false;

#ifdef USB_INPUT

#include <Adafruit_TinyUSB.h>
Adafruit_USBH_Host USBHost;
bool callback = false;

#if not(CFG_TUD_HID)
#error "No HID support"
#endif


#endif

void setup() {

    overclock();

    Serial.println("Overclocked");


  #ifdef USB_INPUT
  Serial.setTX(16);
  Serial.setRX(17);
#endif
    Serial.begin(115200);
#ifdef WAIT_SERIAL
    while (!Serial) {
      delay(10);   // wait for native usb
    }
#endif    
    Serial.println("Starting...");

    // Initialize GIF
    gif.begin(GIF_PALETTE_RGB565_LE);
    
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }
    Serial.println("LittleFS mounted");

    load_rom(roms[0]);
    setup_gb();
    palette = 1;
    gb.direct.frame_skip = 1;
    gb.direct.interlace = 1;

#ifdef USB_INPUT

    tuh_hid_mount_cb(0, 0, NULL, 0);
    while (!callback) {
      Serial1.println("Callbacks are not ours");
    }

    if(!USBHost.begin(0)) {
        Serial.println("USB Host failed to start");
    }

    Serial.println("USB intialized");
#endif
}

void test_palette() {
      for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
          dmd.writePixelFast(x, y, palettes[palette][0][2]);
        }
        for (int x = 64; x < 128; x++) {
          dmd.writePixelFast(x, y, palettes[palette][0][3]);
        }
    }

    for (int y = 64; y < 128; y++) {
        for (int x = 0; x < 64; x++) {
          dmd.writePixelFast(x, y, palettes[palette][0][0]);
        } 
        for (int x = 64; x < 128; x++) {
          dmd.writePixelFast(x, y, palettes[palette][0][1]);
        }
    }
    dmd.swapBuffers(true);
}

volatile bool frame_ready = false;
void loop() {

    #ifdef SHOW_FPS
      static uint8_t frame_count = 0;
      static uint32_t last_frame_time = millis();
    #endif
    // play("/link128x128.gif");
    // test_screen_coverage();
    loop_gb();
    frame_ready = true;

    #ifdef SHOW_FPS    
    frame_count++;
    uint32_t now = millis();
    if (now - last_frame_time >= 1000) {
        Serial.printf("FPS: %d\n", frame_count);
        frame_count = 0;
        last_frame_time = now;
    }
    #endif
  	if (next_rom) {
        current_rom = (current_rom + 1) % ROM_COUNT;
        palette = current_rom+1;
        load_rom(roms[current_rom]);
        Serial.println(roms[current_rom]);
        setup_gb();
	  	  next_rom = false;
	  }

#ifdef USB_INPUT
    USBHost.task();
#endif
}


void setup1() {
	  dmd.init(); 
    Serial.println("DMD initialized");
}

void loop1() {
    if (frame_ready) {
      frame_ready = false;
      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        uint8_t *line = &buffer[y * SCREEN_WIDTH];
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          uint8_t pixel = line[x];
          uint8_t* colors = &palettes[palette][(pixel & LCD_PALETTE_ALL) >> 4]
                              [pixel & 3][0];
          dmd.writePixelFast(x, y, colors);
        }
      }
      dmd.swapBuffers(true);
    }
    if (Serial.available()) {
      int inByte = Serial.read();
      switch(inByte) {
        case 'r':
          reset_usb_boot(0, 0);
          break;
        case 'l':
          line_skip = line_skip == 0 ? line_skip : line_skip - 1;
          Serial.println(line_skip);
          break;
        case 'L':
          line_skip++;
          Serial.println(line_skip);
          break;
        case 'c':
          col_skip = col_skip == 0 ? col_skip : col_skip - 1;
          Serial.println(col_skip);
          break;
        case 'C':
          col_skip++;
          Serial.println(col_skip);
          break;
        case 'p':
          palette = palette == 0 ? 0 : palette - 1;
          Serial.println(palette);
          break;
        case 'P':
          palette = (palette + 1) % PALETTE_COUNT;
          Serial.println(palette);
          break;
        case 'x':
          shift_x = shift_x == 0 ? shift_x : shift_x - 1;
          Serial.println(shift_x);
          break;
        case 'X':
          shift_x++;
          Serial.println(shift_x);
          break;
        case 'y':
          shift_y = shift_y == 0 ? shift_y : shift_y - 1;
          Serial.println(shift_y);
          break;
        case 'Y':
          shift_y++;
          Serial.println(shift_y);
          break;
      }
    }

}

#ifdef USB_INPUT

void tuh_mount_cb(uint8_t daddr) {
  Serial.printf("Device attached, address = %d\r\n", daddr);
}

void tuh_umount_cb(uint8_t daddr) {
  Serial.printf("Device removed, address = %d\r\n", daddr);
}

#define MAX_REPORT 4


// Each HID instance can has multiple reports
static struct {
  uint8_t report_count;
  tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static void process_generic_report(uint8_t dev_addr, uint8_t instance,
                                   uint8_t const *report, uint16_t len);

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len) {
  if (dev_addr == 0) {
    callback = true;
    return;
  }
  Serial.printf("HID device address = %d, instance = %d is mounted\r\n",
                 dev_addr, instance);

  // Interface protocol (hid_interface_protocol_enum_t)
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  // By default, host stack will use boot protocol on supported interface.
  // Therefore for this simple example, we only need to parse generic report
  // descriptor (with built-in parser)
  if (itf_protocol == HID_ITF_PROTOCOL_NONE) {
    hid_info[instance].report_count = tuh_hid_parse_report_descriptor(
        hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
    Serial.printf("HID has %u reports \r\n", hid_info[instance].report_count);
  }

  // request to receive report
  // tuh_hid_report_received_cb() will be invoked when report is available
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    Serial.printf("Error: cannot request to receive report\r\n");
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  Serial.printf("HID device address = %d, instance = %d is unmounted\r\n",
                 dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const *report, uint16_t len) {

    process_generic_report(dev_addr, instance, report, len);

  // continue to request to receive report
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    Serial.printf("Error: cannot request to receive report\r\n");
  }
}

static void process_gamepad_report(hid_gamepad_report_t const *report) {
    Serial.printf("Gamepad: x=%d, y=%d, z=%d, rz=%d, rx=%d, ry=%d, hat=%02X, buttons=%08X\r\n",
                    report->x, report->y, report->z, report->rz,
                    report->rx, report->ry, report->hat, report->buttons);
    gb.direct.joypad_bits.a = (report->buttons & 0x01) != 0;
    gb.direct.joypad_bits.b = (report->buttons & 0x02) != 0;
    gb.direct.joypad_bits.select = (report->buttons & 0x04) != 0;
    gb.direct.joypad_bits.start = (report->buttons & 0x08) != 0;
    gb.direct.joypad_bits.left = report->x < 0;
    gb.direct.joypad_bits.right = report->x > 0;
    gb.direct.joypad_bits.up = report->y > 0;
    gb.direct.joypad_bits.down = report->y < 0;
}

typedef struct TU_ATTR_PACKED {
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint16_t ignore;
    uint16_t buttons;
} joystick_report;
  
static void process_joystick_report(joystick_report const *report) {
    uint16_t buttons = (report->buttons >> 4);    
    static uint16_t prev_buttons = 0;
    static uint8_t prev_x = 0;
    static uint8_t prev_y = 0;
    if (buttons == prev_buttons && report->x == prev_x && report->y == prev_y) {
      return;
    }
    // Serial.printf("Joystick: x=%d, y=%d, buttons=%04X\r\n", report->x, report->y, buttons);
    prev_buttons = buttons;
    prev_x = report->x;
    prev_y = report->y;
    gb.direct.joypad_bits.a = !((buttons & 0x02) != 0);
    gb.direct.joypad_bits.b = !((buttons & 0x04) != 0);
    gb.direct.joypad_bits.select = !((buttons & 0x100) != 0);
    gb.direct.joypad_bits.start = !((buttons & 0x200) != 0);
    gb.direct.joypad_bits.left = !(report->x < 127);
    gb.direct.joypad_bits.right = !(report->x > 127);
    gb.direct.joypad_bits.up = !(report->y < 127);
    gb.direct.joypad_bits.down = !(report->y > 127);
    if ((buttons & 0x10) != 0 && (buttons & 0x20) == 0) {
      if (!gb.direct.joypad_bits.right) {
        shift_x = shift_x == 0 ? shift_x : shift_x - 1;
        Serial.println(shift_x);
      }
      if (!gb.direct.joypad_bits.left) {
        shift_x = shift_x == LCD_WIDTH - SCREEN_WIDTH - 1 ? shift_x : shift_x + 1;
        Serial.println(shift_x);
      }
      if (!gb.direct.joypad_bits.down) {
        shift_y = shift_y == 0 ? shift_y : shift_y - 1;
        Serial.println(shift_y);
      }
      if (!gb.direct.joypad_bits.up) {
        shift_y = shift_y == LCD_HEIGHT - SCREEN_HEIGHT - 1 ? shift_y : shift_y + 1;
        Serial.println(shift_y);
      }
    } else if ((buttons & 0x10) == 0 && (buttons & 0x20) != 0) {
      if (!gb.direct.joypad_bits.up) {
        line_skip = line_skip <= 8 ? 0 : line_skip - 1;
        Serial.println(line_skip);
      }
      if (!gb.direct.joypad_bits.down) {
        if (line_skip == 0) {
          line_skip = 8;
        } else {
          line_skip++;
        }
        line_skip++;
        Serial.println(line_skip);
      }
      if (!gb.direct.joypad_bits.left) {
        col_skip = col_skip <= 5 ? 0 : col_skip - 1;
        Serial.println(col_skip);
      }
      if (!gb.direct.joypad_bits.right) {
        if (col_skip == 0) {
          col_skip = 5;
        } else {
          col_skip++;
        }
        Serial.println(col_skip);
      }
    } else if ((buttons & 0x10) != 0 && (buttons & 0x20) != 0) {
      if (!gb.direct.joypad_bits.start) {
        next_rom = true;
      } else {
        palette = (palette + 1) % PALETTE_COUNT;
        Serial.println(palette);
      }
    }
}
//--------------------------------------------------------------------+
// Generic Report
//--------------------------------------------------------------------+
static void process_generic_report(uint8_t dev_addr, uint8_t instance,
                                   uint8_t const *report, uint16_t len) {
  (void)dev_addr;
  (void)len;

  uint8_t const rpt_count = hid_info[instance].report_count;
  tuh_hid_report_info_t *rpt_info_arr = hid_info[instance].report_info;
  tuh_hid_report_info_t *rpt_info = NULL;

  if (rpt_count == 1 && rpt_info_arr[0].report_id == 0) {
    // Simple report without report ID as 1st byte
    rpt_info = &rpt_info_arr[0];
  } else {
    // Composite report, 1st byte is report ID, data starts from 2nd byte
    uint8_t const rpt_id = report[0];

    // Find report id in the array
    for (uint8_t i = 0; i < rpt_count; i++) {
      if (rpt_id == rpt_info_arr[i].report_id) {
        rpt_info = &rpt_info_arr[i];
        break;
      }
    }

    report++;
    len--;
  }

  if (!rpt_info) {
    Serial.printf("Couldn't find report info !\r\n");
    return;
  }

  if (rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP) {
    switch (rpt_info->usage) {
    case HID_USAGE_DESKTOP_JOYSTICK:
      process_joystick_report((joystick_report const *)report);
      break;
    case HID_USAGE_DESKTOP_GAMEPAD:
      process_gamepad_report((hid_gamepad_report_t const *)report);
      break;    

    default:
      Serial.printf("report[%u] ", rpt_info->report_id);
      Serial.printf("Usage Page: 0x%02X, Usage: 0x%02X, Report Length: %u\r\n",
                    rpt_info->usage_page, rpt_info->usage, len);
      for (uint8_t i = 0; i < len; i++) {
        Serial.printf("%02X ", report[i]);
      }
      Serial.printf("\r\n");
      break;
    }
  }
}



#endif