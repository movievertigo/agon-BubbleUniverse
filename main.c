#include "sintable.inc"

#include "agonmos.h"
#include "agonlib.h"

#define WIDTH 320
#define HEIGHT 240

#define BITMAPBUFFER 1
#define BITMAPFORMAT 1

#define COMMANDBUFFER 2

#define CURVECOUNT 256
#define CURVESTEP 4
#define ITERATIONS 256

#define SINTABLEPOWER 14
#define SINTABLEENTRIES (1<<SINTABLEPOWER)

#define SINTABLEMASK (SINTABLEENTRIES*4-1)

#define PI 3.1415926535897932384626433832795f
#define RVALUE (4*(int)((CURVESTEP * (1<<SINTABLEPOWER)) / 235))
#define SCALEVALUE (4*(int)((CURVESTEP * (1<<SINTABLEPOWER)) / (2*PI)))

static unsigned char rgbtoindex[64] = {
     0, 25,  1,  9, 17, 29, 42, 54,  2, 32,  3, 58, 10, 36, 48, 11,
    16, 26, 40, 52, 18,  8, 43, 55, 21, 33, 46, 59, 23, 37, 49, 62,
     4, 27,  5, 53, 19, 30, 44, 56,  6, 34,  7, 60, 24, 38, 50, 63,
    12, 28, 41, 13, 20, 31, 45, 57, 22, 35, 47, 61, 14, 39, 51, 15
};

static char gcolPointBuffer[9] = {18, 0, 0, 25, 69, 0, 0, 0, 0};
#define gcolpointmacro(colour, x, y) \
{ \
    gcolPointBuffer[2] = colour; \
    *(short*)(gcolPointBuffer+5) = x; \
    *(short*)(gcolPointBuffer+7) = y; \
    vdp_sendblock(gcolPointBuffer, 9); \
}

static char waitForVSYNCBuffer[3] = {23, 0, 0xc3};
#define waitforvsyncmacro() \
{ \
    vdp_sendblock(waitForVSYNCBuffer, 3); \
}

static char setOriginBuffer[5] = {29, 0, 0, 0, 0};
#define setorigin(x, y) \
{ \
    *(short*)(setOriginBuffer+1) = x; \
    *(short*)(setOriginBuffer+3) = y; \
    vdp_sendblock(setOriginBuffer, 5); \
}

static char clearBufferBuffer[6] = {23, 0, 0xA0, 0, 0, 2};
#define clearbuffer(bufferId) \
{ \
    *(short*)(clearBufferBuffer+3) = bufferId; \
    vdp_sendblock(clearBufferBuffer, 6); \
}

static char writeToBufferBuffer[8] = {23, 0, 0xA0, 0, 0, 0, 0, 0};
#define writetobuffer(bufferId, data, length) \
{ \
    *(short*)(writeToBufferBuffer+3) = bufferId; \
    *(short*)(writeToBufferBuffer+6) = length; \
    vdp_sendblock(writeToBufferBuffer, 8); \
    vdp_sendblock(data, length); \
}

static char callBufferBuffer[6] = {23, 0, 0xA0, 0, 0, 1};
#define callbuffer(bufferId) \
{ \
    *(short*)(callBufferBuffer+3) = bufferId; \
    vdp_sendblock(callBufferBuffer, 6); \
}

static char selectBufferForBitmapBuffer[5] = {23, 27, 0x20, 0, 0};
#define selectbufferforbitmap(bufferId) \
{ \
    *(short*)(selectBufferForBitmapBuffer+3) = bufferId; \
    vdp_sendblock(selectBufferForBitmapBuffer, 5); \
}

static char createBitmapFromBufferBuffer[8] = {23, 27, 0x21, 0, 0, 0, 0, 0};
#define createbitmapfrombuffer(width, height, format) \
{ \
    *(short*)(createBitmapFromBufferBuffer+3) = width; \
    *(short*)(createBitmapFromBufferBuffer+5) = height; \
    createBitmapFromBufferBuffer[7] = format; \
    vdp_sendblock(createBitmapFromBufferBuffer, 8); \
}


static char drawBitmapBuffer[7] = {23, 27, 3, 0, 0, 0, 0};
#define drawbitmap(x, y) \
{ \
    *(short*)(drawBitmapBuffer+3) = x; \
    *(short*)(drawBitmapBuffer+5) = y; \
    vdp_sendblock(drawBitmapBuffer, 7); \
}


#define sintable ((long*)0x4C000)
#define costable ((long*)(((char*)sintable)+SINTABLEENTRIES))

#define bitmap ((char*)0x60000)
#define bitmapCentre ((char*)(bitmap+(HEIGHT+1)*HEIGHT/2))
#define bitmapEnd ((char*)(bitmap+HEIGHT*HEIGHT))

#define scaleXTable ((char*)0x78000)
#define scaleYTable ((int*)0x98000)

#define scaleDiv 174
void generatescaletables()
{
    int i, step = 0, xv = -32768 / scaleDiv, yv = xv * HEIGHT;
    for (i = -32768; i < 32768; ++i)
    {
        scaleXTable[i] = xv;
        scaleYTable[i] = yv;
        if (++step == scaleDiv)
        {
            step = 0;
            ++xv;
            yv += HEIGHT;
        }
    }
}

void expandsintable()
{
    int i;
    for (i = 0; i < SINTABLEENTRIES/4; ++i)
    {
        sintable[i] = sintable[SINTABLEENTRIES/2 - i - 1] = sintable[SINTABLEENTRIES + i] = compactsintable[i];
        sintable[SINTABLEENTRIES/2 + i] = sintable[SINTABLEENTRIES - i - 1] = -compactsintable[i];
    }
}

void makecommandbuffer()
{
    char commandBuffer[21] = {23, 27, 0x21, 0, 0, 0, 0, 0,    23, 27, 3, 0, 0, 0, 0,    23, 0, 0xA0, 0, 0, 2};
    *(short*)(commandBuffer+3) = HEIGHT;
    *(short*)(commandBuffer+5) = HEIGHT;
    commandBuffer[7] = BITMAPFORMAT;

    *(short*)(commandBuffer+8+3) = (WIDTH-HEIGHT)/2;
    *(short*)(commandBuffer+8+5) = 0;

    *(short*)(commandBuffer+15 + 3) = BITMAPBUFFER;

    writetobuffer(COMMANDBUFFER, commandBuffer, 21);
}

void appendcallbuffertobitmap()
{
    bitmap[-8] = 23;
    bitmap[-7] = 0;
    bitmap[-6] = 0xA0;
    *(short*)(bitmap-5) = BITMAPBUFFER;
    bitmap[-3] = 0;
    *(short*)(bitmap-2) = HEIGHT*HEIGHT;

    bitmapEnd[0] = 23;
    bitmapEnd[1] = 0;
    bitmapEnd[2] = 0xA0;
    *(short*)(bitmapEnd+3) = COMMANDBUFFER;
    bitmapEnd[5] = 1;
}

int main(void)
{
    long start = gettime();

    int t, ang1Start, ang2Start, ang1, ang2, u, v;
    int i, j;
    unsigned char* ptr;
    unsigned char oldColour;

    clearbuffer(BITMAPBUFFER);
    clearbuffer(COMMANDBUFFER);
    selectbufferforbitmap(BITMAPBUFFER);

    generatescaletables();
    expandsintable();
    makecommandbuffer();
    appendcallbuffertobitmap();

    t = 0;

    while (getlastkey() != 125)
    {
        start = gettime();

        ptr = bitmap;
        while (ptr < bitmapEnd)
        {
            *((int*)(ptr)) = 0xc0c0c0;
            ptr += 3;
        }

        ang1Start = t;
        ang2Start = t;
        for (i = CURVECOUNT/CURVESTEP-1; i >= 0; --i)
        {
            u = 0;
            v = 0;
            for (j = ITERATIONS - 1; j >= 0; --j)
            {
                ang1 = ang1Start + v;
                *(((char*)&ang1)+2) = 0;
                ang2 = ang2Start + u;
                *(((char*)&ang2)+2) = 0;
                u = *(int*)(((char*)sintable)+ang1) + *(int*)(((char*)sintable)+ang2); // sin
                v = *(int*)(((char*)costable)+ang1) + *(int*)(((char*)costable)+ang2); // cos

                bitmapCentre[scaleXTable[u] + scaleYTable[v]] = 0xc0+i;
            }

            ang1Start += SCALEVALUE;
            ang2Start += RVALUE;
        }

        t += 32*8; // ENSURE THIS IS A MULTIPLE OF 4

        vdp_sendblock(bitmap - 8, HEIGHT*HEIGHT + 8 + 6); // +8 and +6 to include the write to bitmap buffer and call to command buffer

        printnum(gettime()-start); vdp_sendstring("\r");
    }


//    cls();
    cursor(1);
    return 0;
}