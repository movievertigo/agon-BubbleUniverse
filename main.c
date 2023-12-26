#include "agonmos.h"
#include "agonlib.h"

#include "sintable.inc"

#define WIDTH 320
#define HEIGHT 240

#define BITMAPBUFFER 1
#define BITMAPFORMAT 1

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


#define bitmap ((char*)0x60000)
#define bitmapEnd ((char*)(bitmap+HEIGHT*HEIGHT))

int main(void)
{
    long start = gettime();

    int t, ang1Start, ang2Start, ang1, ang2, u, v;
    int i, j;
    unsigned char* ptr;
    unsigned char oldColour;

    ptr = bitmap;
    while (ptr < bitmapEnd)
    {
        *((int*)(ptr)) = 123;
        ptr += 3;
    }

clearbuffer(BITMAPBUFFER);
writetobuffer(BITMAPBUFFER, bitmap, HEIGHT*HEIGHT);
selectbufferforbitmap(BITMAPBUFFER);
createbitmapfrombuffer(HEIGHT, HEIGHT, BITMAPFORMAT);
drawbitmap(0, 0);
printnum(gettime()-start); vdp_sendstring("\n\r");

return 0;

    setorigin((WIDTH-HEIGHT)/2, 0);

    ptr = bitmap;

    oldColour = 255;
    for (i = 0; i < CURVECOUNT; i += CURVESTEP)
    {
        unsigned char* chunk = ptr;

        for (j = 0; j < ITERATIONS; ++j)
        {
            unsigned char r = (i * 4) / CURVECOUNT;
            unsigned char g = (j * 4) / ITERATIONS;
            unsigned char b = 3 - (r+g) / 2;
            unsigned char colour = rgbtoindex[r + g*4 + b*16];

            if (colour != oldColour)
            {
                *ptr++ = 18; *ptr++ = 0; *ptr++ = colour;
                oldColour = colour;
            }

            *ptr++ = 25; *ptr++ = 69; *ptr++ = j; *ptr++ = 0; *ptr++ = i/CURVESTEP; *ptr++ = 0;
        }
        writetobuffer(1, chunk, ptr-chunk);
    }
    callbuffer(1);
    printnum(ptr-bitmap);
    t = 0;

    while (getlastkey() != 125 && t == 0)
    {
        ang1Start = t;
        ang2Start = t;
        for (i = CURVECOUNT/CURVESTEP-1; i >= 0; --i)
        {
            u = 0;
            v = 0;
            for (j = ITERATIONS - 1; j >= 0; --j)
            {
                ang1 = ang1Start + v;
                ang2 = ang2Start + u;
                u = *(int*)(((char*)sintable)+(ang1 & SINTABLEMASK)) + *(int*)(((char*)sintable)+(ang2 & SINTABLEMASK)); // sin
                v = *(int*)(((char*)sintable)+((ang1 + SINTABLEENTRIES) & SINTABLEMASK)) + *(int*)(((char*)sintable)+((ang2 + SINTABLEENTRIES) & SINTABLEMASK)); // cos

                if (j == 0)
                {
                    printnum(u);vdp_sendchar(' ');printnum(v);vdp_sendchar(' ');
                }
                gcolpointmacro(i, u>>7, v>>7);
            }

            ang1Start += SCALEVALUE;
            ang2Start += RVALUE;
        }

        t += 32; // ENSURE THIS IS A MULTIPLE OF 4
    }

    printnum(gettime()-start);
//    cls();
    cursor(1);
    return 0;
}