void vdp_sendchar(char character);
void vdp_sendstring(char* string);
void vdp_sendblock(char* block, int length);

unsigned char* mos_sysvars();
unsigned char* mos_getkbmap();
