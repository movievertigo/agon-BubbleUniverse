#include "agonlib.h"
#include "agonmos.h"

#define MALLOC_BASE 0x50000
#define MALLOC_MAX 0xB8000

static int malloc_base = MALLOC_BASE;
static int malloc_max = MALLOC_MAX;

void* malloc(int bytes)
{
    malloc_base += bytes;
    if (malloc_base > malloc_max)
    {
        vdp_sendstring("Malloc failed. "); printnum(malloc_base - malloc_max); vdp_sendstring(" bytes over\n\r");
    }
    return (void*)(malloc_base > malloc_max ? 0 : malloc_base - bytes);
}

void freeall()
{
    malloc_base = MALLOC_BASE;
}

long gettime()
{
    char* sysvars = mos_sysvars();
    long frameTimer = *(long*)(sysvars + 0);
    return (frameTimer * 5) / 6;
}

unsigned char getkey()
{
    return mos_sysvars()[0x18] ? mos_sysvars()[0x17] : 0;
}

unsigned char getlastkey()
{
    return mos_sysvars()[0x17];
}
void printnum(long num)
{
    long mag;
    char skip = 1;
    if (num < 0) { num = -num; vdp_sendchar('-'); }
    for (mag = 1000000000l; mag > 0; mag /= 10)
    {
        char digit = num / mag;
        num -= digit * mag;
        if (digit != 0 || !skip || mag == 1)
        {
            vdp_sendchar('0' + digit);
            skip = 0;
        }
    }
}

void cls()
{
    vdp_sendchar(12);
}

void cursor(char state)
{
    static char cursorBuffer[3] = {23, 1, 0};
    cursorBuffer[2] = state;
    vdp_sendblock(cursorBuffer, 3);
}

void plot(char mode, short x, short y)
{
    static char plotBuffer[6] = {25, 0, 0, 0, 0, 0};
    *(plotBuffer+1) = mode;
    *(short*)(plotBuffer+2) = x;
    *(short*)(plotBuffer+4) = y;
    vdp_sendblock(plotBuffer, 6);
}

void gcol(char colour)
{
    static char gcolBuffer[3] = {18, 0, 0};
    gcolBuffer[2] = colour;
    vdp_sendblock(gcolBuffer, 3);
}

void colour(char colour)
{
    static char colourBuffer[2] = {17, 0};
    colourBuffer[1] = colour;
    vdp_sendblock(colourBuffer, 2);
}
