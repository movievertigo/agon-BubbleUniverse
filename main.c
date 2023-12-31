#include "sintable.inc"

#include "agonmos.h"
#include "agonlib.h"

#define WIDTH 320
#define HEIGHT 240

#define BITMAPBUFFER 65534
#define BITMAPFORMAT 1

#define COMMANDBUFFER 65533

#define CURVECOUNT 256
#define CURVESTEP 4
#define ITERATIONS 256

#define SINTABLEPOWER 14
#define SINTABLEENTRIES (1<<SINTABLEPOWER)

#define SINTABLEMASK (SINTABLEENTRIES*4-1)

#define PI 3.1415926535897932384626433832795f
#define RVALUE (4*(int)((CURVESTEP * (1<<SINTABLEPOWER)) / 235))
#define SCALEVALUE (4*(int)((CURVESTEP * (1<<SINTABLEPOWER)) / (2*PI)))

static char waitForVSYNCBuffer[3] = {23, 0, 0xc3};
#define waitforvsyncmacro() \
{ \
    vdp_sendblock(waitForVSYNCBuffer, 3); \
}

static char clearBufferBuffer[6] = {23, 0, 0xA0, 0, 0, 2};
#define clearbuffer(bufferId) \
{ \
    *(short*)(clearBufferBuffer+3) = bufferId; \
    vdp_sendblock(clearBufferBuffer, 6); \
}

#define clearallbuffers() \
{ \
    clearbuffer(65535); \
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

static char createBufferBuffer[8] = {23, 0, 0xA0, 0, 0, 3, 0, 0};
#define createbuffer(bufferId, length) \
{ \
    *(short*)(createBufferBuffer+3) = bufferId; \
    *(short*)(createBufferBuffer+6) = length; \
    vdp_sendblock(createBufferBuffer, 8); \
}

static char adjustBufferBuffer[12] = {23, 0, 0xA0, 0, 0, 5, 0, 0, 0, 0, 0, 0};
#define adjustbuffer(bufferId, operation, offset, operand) \
{ \
    *(short*)(adjustBufferBuffer+3) = bufferId; \
    adjustBufferBuffer[6] = operation; \
    *(short*)(adjustBufferBuffer+7) = offset; \
    adjustBufferBuffer[9] = operand; \
    vdp_sendblock(adjustBufferBuffer, 10); \
}
#define adjustbuffermultiple(bufferId, operation, offset, count, operand) \
{ \
    *(short*)(adjustBufferBuffer+3) = bufferId; \
    adjustBufferBuffer[6] = operation; \
    *(short*)(adjustBufferBuffer+7) = offset; \
    *(short*)(adjustBufferBuffer+9) = count; \
    adjustBufferBuffer[11] = operand; \
    vdp_sendblock(adjustBufferBuffer, 12); \
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

#define makecolour(outer, inner) (0xc0 + (outer) + (inner)*4 + (3-(((outer)+(inner))>>1)) * 16)

#define innerloop(outer, inner) \
{ \
    for (j = ITERATIONS/4 - 1; j >= 0; --j) \
    { \
        ang1 = ang1Start + v; \
        *(((char*)&ang1)+2) = 0; \
        ang2 = ang2Start + u; \
        *(((char*)&ang2)+2) = 0; \
        u = *(int*)(((char*)sintable)+ang1) + *(int*)(((char*)sintable)+ang2); \
        v = *(int*)(((char*)costable)+ang1) + *(int*)(((char*)costable)+ang2); \
 \
        bitmapCentre[scaleXTable[u] + scaleYTable[v]] = makecolour(outer,inner); \
    } \
}

#define outerloop(outer) \
{ \
    for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i) \
    { \
        u = 0; \
        v = 0; \
        innerloop(outer,0); \
        innerloop(outer,1); \
        innerloop(outer,2); \
        innerloop(outer,3); \
        ang1Start += SCALEVALUE; \
        ang2Start += RVALUE; \
    } \
}

#define sintable ((long*)0x4C000) // 0x4C000 - 0x5FFFF
#define costable ((long*)(((char*)sintable)+SINTABLEENTRIES))

#define bitmap ((char*)0x60000) // 0x60000 - 0x6FFFF
#define bitmapCentre ((char*)(bitmap+(HEIGHT+1)*HEIGHT/2))
#define bitmapEnd ((char*)(bitmap+HEIGHT*HEIGHT))

#define scaleXTable ((char*)0x78000) // 0x70000 - 0x7FFFF
#define scaleYTable ((int*)0x98000) // 0x80000 - 0xAFFFF

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

void createRLEbuffers()
{
    int len, bufferId;
    unsigned char colA, colB, colour, sequence[2];

    bufferId = 0;
    for (colA = 0; colA < 16; ++colA)
    {
        colour = makecolour(colA >> 2, colA & 3);

        for (len = 1; len <= 256; ++len)
        {
            createbuffer(bufferId, len + 1);
            if (len != 1)
            {
                adjustbuffermultiple(bufferId, 0x42, 0, len, 0xc0);
            }
            else
            {
                adjustbuffer(bufferId, 2, 0, 0xc0);
            }
            adjustbuffer(bufferId, 2, len, colour);
            ++bufferId;
        }
    }

    for (len = 2; len <= 257; ++len)
    {
        createbuffer(bufferId, len);
        adjustbuffermultiple(bufferId, 0x42, 0, len, 0xc0);
        ++bufferId;
    }

    for (colB = 0; colB < 16; ++colB)
    {
        for (colA = 0; colA < 16; ++colA)
        {
            sequence[0] = makecolour(colA >> 2, colA & 3);
            sequence[1] = makecolour(colB >> 2, colB & 3);
            writetobuffer(bufferId, sequence, 2);
            ++bufferId;
        }
    }

    for (colA = 0; colA < 16; ++colA)
    {
        sequence[0] = makecolour(colA >> 2, colA & 3);
        sequence[1] = 0xc0;
        writetobuffer(bufferId, sequence, 2);
        ++bufferId;
    }
}

int main(void)
{
    long start = gettime();

    int t, ang1Start, ang2Start, ang1, ang2, u, v;
    char i, j;
    unsigned char* ptr;
    unsigned char oldColour;

    clearallbuffers();
    selectbufferforbitmap(BITMAPBUFFER);

    generatescaletables();
    expandsintable();
    makecommandbuffer();
    appendcallbuffertobitmap();
    createRLEbuffers();
printnum(gettime()-start); vdp_sendstring("\n\r");

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
        outerloop(0);
        outerloop(1);
        outerloop(2);
        outerloop(3);

        t += 32*8; // ENSURE THIS IS A MULTIPLE OF 4

        vdp_sendblock(bitmap - 8, HEIGHT*HEIGHT + 8 + 6); // +8 and +6 to include the write to bitmap buffer and call to command buffer

        printnum(gettime()-start); vdp_sendstring("\r");
    }


//    cls();
    cursor(1);
    return 0;
}