#include "hardware/vreg.h"

#define ENABLE_LCD	1

#ifndef ENABLE_SOUND
# define ENABLE_SOUND	0
#endif

/* Project headers */
#include "peanut_gb.h"
#include "gb_palette.h"


unsigned char rom_bank0[65536];
static uint8_t ram[32768];


/* Pixel data is stored in here. */
static uint8_t pixels_buffer[LCD_WIDTH];


/**
 * Returns a byte from the ROM file at the given address.
 */
uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr) {
	(void) gb;
	if(addr < sizeof(rom_bank0))
		return rom_bank0[addr];

    romFile.seek(addr, SeekSet);
	return (uint8_t) romFile.peek();
}

/**
 * Returns a byte from the cartridge RAM at the given address.
 */
uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr) {
	(void) gb;
	return ram[addr];
}

/**
 * Writes a given byte to the cartridge RAM at the given address.
 */
void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr,
		       const uint8_t val){
	ram[addr] = val;
}

/**
 * Ignore all errors.
 */
void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val) {
#if 1
	const char* gb_err_str[4] = {
			"UNKNOWN",
			"INVALID OPCODE",
			"INVALID READ",
			"INVALID WRITE"
		};
	printf("Error %d occurred: %s\n. Abort.\n",
		gb_err,
		gb_err >= GB_INVALID_MAX ?
		gb_err_str[0] : gb_err_str[gb_err]);
	abort();
#endif
}

uint8_t shift_x = 0;
uint8_t shift_y = 0;
uint8_t line_skip = 0;
uint8_t col_skip = 0;
void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[LCD_WIDTH],
		   const uint_fast8_t line) {


	if (line < shift_y) {
		return;
	}
	if (line_skip > 0 && (line - shift_y) % line_skip == 0) {
		return;
	}

	uint8_t effective_line = line - shift_y - (line_skip > 0 ? (line - shift_y) / line_skip : 0);

	if (effective_line >= SCREEN_HEIGHT) {
        return;
    }

	uint8_t skip_count = 0;
	uint8_t skipped = 0;
	for (uint_fast8_t x = shift_x; x<LCD_WIDTH; x++) {
        uint16_t color = palettes[palette][(pixels[x] & LCD_PALETTE_ALL) >> 4]
				[pixels[x] & 3];

		if (col_skip > 0 && skip_count == col_skip) {
			skip_count = 0;
			skipped++;
			continue;
		} else {
			skip_count++;
		}

		uint8_t effective_x = x - shift_x - skipped;
		if (effective_x >= SCREEN_WIDTH) {
			continue;
		}
        dmd.writePixel(effective_x, effective_line, color);
    }
}



struct gb_s gb;


void overclock() {
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(2);
	set_sys_clock_khz(264000, true);
}

void setup_gb(void) {
	enum gb_init_error_e ret;

	/* Overclock. */

	/* Initialise GB context. */
	ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read,
		      &gb_cart_ram_write, &gb_error, NULL);
	printf("GB \n");

	if(ret != GB_INIT_NO_ERROR) {
		printf("Error: %d\n", ret);
	}

#if ENABLE_LCD
	gb_init_lcd(&gb, &lcd_draw_line);

	printf("LCD ");
	//gb.direct.interlace = 1;
#endif
#if ENABLE_SOUND
	audio_init();
	printf("AUDIO ");
#endif
}

void loop_gb(void) {
#if ENABLE_SOUND
		static uint16_t stream[1098];
#endif

    gb.gb_frame = false;

    do {
        __gb_step_cpu(&gb);
        tight_loop_contents();
    } while(!(gb.gb_frame));

#if ENABLE_SOUND
    audio_callback(NULL, stream, 1098);
#endif

}