class FastDMD : public DMD_BASE_CLASS {
    public:
	FastDMD(uint8_t *mux_list, byte _pin_nOE, byte _pin_SCLK, uint8_t *pinlist,
				   byte panelsWide, byte panelsHigh, bool d_buf = false) : 
				  DMD_BASE_CLASS(mux_list, _pin_nOE, _pin_SCLK, pinlist, panelsWide, panelsHigh, d_buf)
	{
	}

    void writePixelFast(int16_t x, int16_t y, uint8_t* color) {
        uint8_t r = color[0];
        uint8_t g = color[1];
        uint8_t b = color[2];
        uint8_t bit, limit, * ptr;


        DEBUG_TIME_MARK_333;
        DEBUG_TIME_MARK;

        uint16_t base_addr = (y % pol_displ) * WIDTH + x;
        ptr = &matrixbuff[backindex][base_addr]; // Base addr
        DEBUG_TIME_MARK;


        bit = 1;
        limit = 1 << nPlanes;
        if (y % DMD_PIXELS_DOWN < pol_displ) {
            // Data for the upper half of the display is stored in the lower
            // bits of each byte.
        
            // Data is stored in the low 6 bits so it can be quickly
            // copied to the DATAPORT register w/6 output lines.
            for (; bit < limit; bit <<= 1) {

                *ptr |= output_mask;

                * ptr &= ~0b000111;            // Mask out R,G,B in one op
                if (r & bit) *ptr |= 0b000001; // Plane N R: bit 2
                if (g & bit) *ptr |= 0b000010; // Plane N G: bit 3
                if (b & bit) *ptr |= 0b000100; // Plane N B: bit 4
                ptr += displ_len;                 // Advance to next bit plane
            }
        }
        else {
            // Data for the lower half of the display is stored in the upper
            // bits

            for (; bit < limit; bit <<= 1) {

                *ptr |= output_mask;

                * ptr &= ~0b111000;            // Mask out R,G,B in one op
                if (r & bit) *ptr |= 0b001000; // Plane N R: bit 5
                if (g & bit) *ptr |= 0b010000; // Plane N G: bit 6
                if (b & bit) *ptr |= 0b100000; // Plane N B: bit 7
                ptr += displ_len;                 // Advance to next bit plane
            }
        }
        DEBUG_TIME_MARK;

    }
};