#include "sintable.inc"

#include "agonmos.h"
#include "agonlib.h"

#define SCREENWIDTH 320
#define SCREENHEIGHT 240

#define HEIGHT 238

#define BITMAPBUFFER 65534
#define BITMAPFORMAT 1

#define COMMANDBUFFER 65533
#define COPYBUFFER 65532

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
#define preparetowritetobuffer(bufferId,length) \
{ \
    *(short*)(writeToBufferBuffer+3) = bufferId; \
    *(short*)(writeToBufferBuffer+6) = length; \
    vdp_sendblock(writeToBufferBuffer, 8); \
}
#define writetobuffer(bufferId, data, length) \
{ \
    preparetowritetobuffer(bufferId,length); \
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

static char copyBufferBuffer[12] = {23, 0, 0xA0, 0, 0, 13, 0, 0, 0, 0, 0, 0};
#define copybuffer(destinationBufferId, sourceBufferId) \
{ \
    *(short*)(copyBufferBuffer+3) = destinationBufferId; \
    *(short*)(copyBufferBuffer+6) = sourceBufferId; \
    *(short*)(copyBufferBuffer+8) = 65535; \
    vdp_sendblock(copyBufferBuffer, 10); \
}
#define combine2buffers(destinationBufferId, sourceBufferId1, sourceBufferId2) \
{ \
    *(short*)(copyBufferBuffer+3) = destinationBufferId; \
    *(short*)(copyBufferBuffer+6) = sourceBufferId1; \
    *(short*)(copyBufferBuffer+8) = sourceBufferId2; \
    *(short*)(copyBufferBuffer+10) = 65535; \
    vdp_sendblock(copyBufferBuffer, 12); \
}

static char consolidateBufferBuffer[6] = {23, 0, 0xA0, 0, 0, 14};
#define consolidatebuffer(bufferId) \
{ \
    *(short*)(consolidateBufferBuffer+3) = bufferId; \
    vdp_sendblock(consolidateBufferBuffer, 6); \
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
#define makecolourfromindex(index) makecolour(index>>2, index&3)

#define unrollcountblack3a(offset) \
{ \
    asm("    LD HL,(IY+"#offset")"); \
    asm("    SBC HL,BC"); \
    asm("    JR NZ,RLE_CountBlack3End"#offset); \
}
#define unrollcountblack3b(offset) \
{ \
    asm("    LD HL,(IY+"#offset"-126)"); \
    asm("    SBC HL,BC"); \
    asm("    JR NZ,RLE_CountBlack3End"#offset); \
}
#define unrollcountblack3d(offset) \
{ \
    asm("RLE_CountBlack3End"#offset":"); \
    asm("    LEA IY,IY+"#offset); \
    asm("    LD E,"#offset"-1"); \
    asm("    JR RLE_CountBlack3End"); \
}
#define unrollcountblack3e(offset) \
{ \
    asm("RLE_CountBlack3End"#offset":"); \
    asm("    LEA IY,IY+"#offset"-126"); \
    asm("    LD E,"#offset"-1"); \
    asm("    JR RLE_CountBlack3End"); \
}

#define u() \
{ \
    /* ang1 = ang1Start + v; */ \
    asm("    ADD.s IY,DE"); /* v from IY, ang1Start from DE' */ \
    asm("    EXX"); /* Leave alt-register mode */ \
\
    /* *(((char*)&ang1)+2) = (char)sintable>>16; */ \
    asm("    ADD IY,DE"); \
\
    /* ang2 = ang2Start + u; */ \
    asm("    LEA HL,IX+0"); /* u from IX */ \
    asm("    ADD.s HL,SP"); /* ang2Start from SPS */ \
\
    /* *(((char*)&ang2)+2) = (char)sintable>>16; */ \
    asm("    ADD HL,DE"); \
\
    /* u = *(int*)ang1 + *(int*)ang2; */ \
    asm("    LD BC,(IY)"); \
    asm("    LD IX,(HL)"); \
    asm("    ADD IX,BC"); \
\
    /* v = *(int*)(ang1+costable) + *(int*)(ang2+costable); */ \
    asm("    ADD IY,DE"); \
    asm("    ADD HL,DE"); \
    asm("    LD BC,(IY)"); \
    asm("    LD IY,(HL)"); \
    asm("    ADD IY,BC"); \
\
    /* *(unsigned char*)(scaleYTable[v] + scaleXTable[u]) = colIndex; */ \
    asm("    LD BC,(IY+1)"); \
    asm("    LD L,(IX)"); \
    asm("    LD H,E"); /* Assuming E is 0 as it's an address constant (quicker than LD H,%0) */ \
    asm("    ADD HL,BC"); /* Top byte of HL isn't clear but we compensate with -costable in the scale table */ \
    asm("    LD (HL),A"); \
\
    asm("    EXX"); /* Enter alt-register mode */ \
}

#define sintable ((long*)0x50000) // 0x50000 - 0x5FFFF

#define bitmap ((char*)0x60000) // 0x60000 - 0x6FFFF
#define bitmapEnd ((char*)(bitmap+HEIGHT*HEIGHT))

#define scaleTable ((unsigned char*)0x80000) // 0x78000 - 0x87FFF

#define rleHeader ((char*)0x90000) // 0x90000 - max length of RLE data
#define rleData ((char*)(rleHeader+6))

#define costable ((long*)(((int)sintable)*2)) // 0xA0000 - 0xAFFFF

#define scaleDiv 176
void generatescaletables()
{
    int i, step, xv = -32768 / scaleDiv + HEIGHT/2, yv = (long)bitmap - (long)costable + (-8192 / (scaleDiv/4) + HEIGHT/2) * HEIGHT;
    for (step = 0, i = -32768; i < 32768; i += 4)
    {
        scaleTable[i] = xv;
        *(int*)(scaleTable + i + 1) = yv;
        if (++step == scaleDiv/4)
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
        sintable[i] = sintable[SINTABLEENTRIES/2 - i - 1] = compactsintable[i] + ((int)scaleTable/2);
        sintable[SINTABLEENTRIES/2 + i] = sintable[SINTABLEENTRIES - i - 1] = -compactsintable[i] + ((int)scaleTable/2);
        costable[3*SINTABLEENTRIES/4 +i] = costable[SINTABLEENTRIES/4 - i - 1] = compactsintable[i] + ((int)scaleTable/2);
        costable[SINTABLEENTRIES/4 + i] = costable[3*SINTABLEENTRIES/4 - i - 1] = -compactsintable[i] + ((int)scaleTable/2);
    }
}

void makecommandbuffer()
{
    preparetowritetobuffer(COMMANDBUFFER, 6+8+7)
    {
        consolidatebuffer(BITMAPBUFFER);                             // Length 6
        createbitmapfrombuffer(HEIGHT, HEIGHT, BITMAPFORMAT);        // Length 8
        drawbitmap((SCREENWIDTH-HEIGHT)/2, (SCREENHEIGHT-HEIGHT)/2); // Length 7
    }

    consolidatebuffer(COMMANDBUFFER);
}

char checkCopyBlocksByReferenceWorks()
{
    char tryCommand[] = {30, 23, 0, 0xA0, 255, 255, 0x19, 17, 0, 255, 255, 17, 15, 10, 23, 0, 0x82};
    char* sysvars = mos_sysvars();
    char result;

    sysvars[8] = 0;
    vdp_sendblock(tryCommand, 17);
    while (!sysvars[8]) {} // Wait until the cursor Y position is reported as 1 (due to the VDU 10) 

     // If the command exists it will have swallowed the 255,255 and the cursor X will still be 0
    result = sysvars[7] == 0;

    vdp_sendchar(30); // Reset the cursor
    return result;
}

void prependandappend()
{
    rleHeader[0] = 23;
    rleHeader[1] = 0;
    rleHeader[2] = 0xA0;
    *(short*)(rleHeader+3) = BITMAPBUFFER;
    rleHeader[5] = checkCopyBlocksByReferenceWorks() ? 0x19 : 0x0D;

    bitmapEnd[0] = 255;
}

void clearbitmap()
{
    unsigned int* ptr = (unsigned int*)bitmap;
    while (ptr < bitmapEnd)
    {
        *ptr++ = 0;
    }
}

static unsigned long introPosition;
static unsigned int introPercentage;
static char* introText = "\n\n\n\n\n\n\n\n            Bubble Universe\r\n\n\n\n\n\n\n\n\
   Agon Light v1.01 by Movie Vertigo\r\n\n\
        twitter.com/movievertigo\r\n\
        youtube.com/movievertigo";
static unsigned char rgbtoindex[64] = {
     0, 25,  1,  9, 17, 29, 42, 54,  2, 32,  3, 58, 10, 36, 48, 11,
    16, 26, 40, 52, 18,  8, 43, 55, 21, 33, 46, 59, 23, 37, 49, 62,
     4, 27,  5, 53, 19, 30, 44, 56,  6, 34,  7, 60, 24, 38, 50, 63,
    12, 28, 41, 13, 20, 31, 45, 57, 22, 35, 47, 61, 14, 39, 51, 15
};

static char introViewport[] = {28, 18, 14, 22, 12};
void initIntro()
{
    int charIndex, stage = 3, printing = 0;
    char r,g,b;
    introPosition = 0;
    introPercentage = -1;

    for (charIndex = 0; charIndex < strlen(introText); ++charIndex)
    {
        if (printing && introText[charIndex] < 32)
        {
            --stage;
        }
        printing = introText[charIndex] >= 32;
        if (printing)
        {
            r = stage; g = (charIndex%7)-3; g = g < 0 ? -g : g; b = 3 - (r+g)/2;
            colour(rgbtoindex[(b<<4)+(g<<2)+r]);
        }
        vdp_sendchar(introText[charIndex]);
    }
    vdp_sendblock(introViewport, 5);
}

void updateIntro(int weight)
{
    int newPercentage = (introPosition * 100) / 10223536L;
    introPosition += weight;

    if (newPercentage != introPercentage)
    {
        introPercentage = newPercentage;
        printnum(introPercentage); vdp_sendstring("%\r");
    }
}

void endIntro()
{
    vdp_sendchar(26);
    colour(15);
    cls();
}

void createRLEbuffers()
{
    int lenA, lenB, bufferId;
    unsigned char colA, colB, colC, bitA, bitB, bitC, bitD, bitE, bitF, bitG, bitH, colour, coloursincblack[17], sequence[8];
    unsigned char* colours = coloursincblack+1;

    coloursincblack[0] = 0xc0;
    colours[0x0] = makecolourfromindex(0x0); colours[0x1] = makecolourfromindex(0x1); colours[0x2] = makecolourfromindex(0x2); colours[0x3] = makecolourfromindex(0x3);
    colours[0x4] = makecolourfromindex(0x4); colours[0x5] = makecolourfromindex(0x5); colours[0x6] = makecolourfromindex(0x6); colours[0x7] = makecolourfromindex(0x7);
    colours[0x8] = makecolourfromindex(0x8); colours[0x9] = makecolourfromindex(0x9); colours[0xA] = makecolourfromindex(0xA); colours[0xB] = makecolourfromindex(0xB);
    colours[0xC] = makecolourfromindex(0xC); colours[0xD] = makecolourfromindex(0xD); colours[0xE] = makecolourfromindex(0xE); colours[0xF] = makecolourfromindex(0xF);

    initIntro();

    bufferId = 0;
    for (lenA = 1; lenA <= 256; ++lenA)
    {
        if (lenA == 1)
        {
            sequence[0] = 0xc0;
            writetobuffer(bufferId, sequence, lenA);
        }
        else
        {
            combine2buffers(bufferId, lenA-2, 0);
            consolidatebuffer(bufferId);
        }
        ++bufferId;
        updateIntro(2*256*256/256);
    }

    for (colA = 0; colA < 16; ++colA)
    {
        colour = colours[colA];
        for (lenA = 1; lenA <= 256; ++lenA)
        {
            if (lenA < 256)
            {
                copybuffer(bufferId, lenA); // Copy existing black buffer of length lenA+1
            }
            else
            {
                combine2buffers(bufferId, lenA-1, 0);
                consolidatebuffer(bufferId);
            }
            adjustbuffer(bufferId, 2, lenA, colour);
            ++bufferId;
            updateIntro(43*256/16);
        }
    }

    bufferId = 0x2000;
    for (colA = 0; colA < 16; ++colA)
    {
        colour = colours[colA];

        for (lenA = 1+2; lenA <= 16+2; ++lenA)
        {
            for (lenB = 1+2; lenB <= 16+2; ++lenB)
            {
                copybuffer(bufferId, lenA + 1 + lenB); // Copy existing black buffer of length lenA+1+lenB+1
                adjustbuffer(bufferId, 2, lenA, colour);
                adjustbuffer(bufferId, 2, lenA + 1 + lenB, colour);
                ++bufferId;
            }
            updateIntro(58*16*256/16);
        }
    }

    bufferId = 0x3000;
    for (colA = 0; colA < 16; ++colA)
    {
        colour = colours[colA];
        for (bitA = 0; bitA < 2; ++bitA)
        {
            sequence[7] = bitA ? colour : 0xc0;
            for (bitB = 0; bitB < 2; ++bitB)
            {
                sequence[6] = bitB ? colour : 0xc0;
                for (bitC = 0; bitC < 2; ++bitC)
                {
                    sequence[5] = bitC ? colour : 0xc0;
                    for (bitD = 0; bitD < 2; ++bitD)
                    {
                        sequence[4] = bitD ? colour : 0xc0;
                        for (bitE = 0; bitE < 2; ++bitE)
                        {
                            sequence[3] = bitE ? colour : 0xc0;
                            for (bitF = 0; bitF < 2; ++bitF)
                            {
                                sequence[2] = bitF ? colour : 0xc0;
                                for (bitG = 0; bitG < 2; ++bitG)
                                {
                                    sequence[1] = bitG ? colour : 0xc0;
                                    for (bitH = 0; bitH < 2; ++bitH)
                                    {
                                        sequence[0] = bitH ? colour : 0xc0;
                                        writetobuffer(bufferId, sequence, 8);
                                        ++bufferId;
                                    }
                                }
                            }
                        }
                    }
                }
                updateIntro(24*64*256/16);
            }
        }
    }

    bufferId = 0x8000;
    for (colC = 0; colC <= 16; ++colC)
    {
        sequence[2] = coloursincblack[colC];
        for (colB = 0; colB <= 16; ++colB)
        {
            sequence[1] = coloursincblack[colB];
            for (colA = 0; colA <= 16; ++colA)
            {
                sequence[0] = coloursincblack[colA];
                writetobuffer(bufferId, sequence, 3);
                ++bufferId;
            }
            bufferId += (1<<5) - 17;
            updateIntro(29*256*256/17/17);
        }
        bufferId += (1<<10) - (17<<5);
    }

    endIntro();
}

volatile unsigned char* rle;
volatile int t;

int main(void)
{
    long start = gettime();

    clearallbuffers();
    selectbufferforbitmap(BITMAPBUFFER);

    generatescaletables();
    expandsintable();
    clearbitmap();
    prependandappend();
    makecommandbuffer();
    createRLEbuffers();
//printnum(gettime()-start); vdp_sendstring("\n\r");
    t = 0; // 128000 + 40*4*298;

start = gettime();

    while (getlastkey() != 125 /* && t < 400*16*/)
    {
//        start = gettime();

        asm("    PUSH IX"); // Store current IX

        // ang1Start = t;
        asm("    EXX"); // Enter alt-register mode
        asm("    LD DE,(_t)");

        // ang2Start = t;
        asm("    LD HL,(_t)");
        asm("    LD.s SP,HL");

        // Pre-load some constants into registers
        asm("    EXX"); // Leave alt-register mode
        asm("    LD	DE,50000h");
        asm("    EXX"); // Enter alt-register mode

        asm("    LD B,%10");
        asm("OuterLoop1:");
        // for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LEA IY,IX+0"); // u = 0
            asm("    LD A,%1");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%2");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%3");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%4");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
           // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DJNZ OuterLoop1");

        asm("    LD B,%10");
        asm("OuterLoop2:");
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LEA IY,IX+0"); // u = 0
            asm("    LD A,%5");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%6");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%7");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%8");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
           // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DJNZ OuterLoop2");

        asm("    LD B,%10");
        asm("OuterLoop3:");
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LEA IY,IX+0"); // u = 0
            asm("    LD A,%9");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%A");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
           asm("    LD A,%B");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%C");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DJNZ OuterLoop3");

        asm("    LD B,%10");
        asm("OuterLoop4:");
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LEA IY,IX+0"); // u = 0
            asm("    LD A,%D");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%E");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%F");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            asm("    LD A,%10");
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();u();
            // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DJNZ OuterLoop4");

        //rle = rleData;
        asm("    LD IX,90006h");

        //ptr = bitmap;
        asm("    LD IY,60000h");

        asm("    LD BC,0");
        asm("    EXX"); // Swap register mode
        asm("    LD BC,0");

        // while (1)
        {
            asm("RLE_Loop:");
            asm("    EXX"); // Store the old run values so we can check for consolidation
            asm("    OR A,A"); // Clear carry

            // if (*(unsigned int*)ptr == 0)
            asm("    LD HL,(IY)");
            asm("    SBC HL,BC");
            asm("    JR NZ,RLE_Sextuple");

            {
                asm("RLE_CountBlack3Unrolled:");
                unrollcountblack3a(  3); unrollcountblack3a(  6); unrollcountblack3a(  9); unrollcountblack3a( 12); unrollcountblack3a( 15);
                unrollcountblack3a( 18); unrollcountblack3a( 21); unrollcountblack3a( 24); unrollcountblack3a( 27); unrollcountblack3a( 30);
                unrollcountblack3a( 33); unrollcountblack3a( 36); unrollcountblack3a( 39); unrollcountblack3a( 42); unrollcountblack3a( 45);
                unrollcountblack3a( 48); unrollcountblack3a( 51); unrollcountblack3a( 54); unrollcountblack3a( 57); unrollcountblack3a( 60);
                unrollcountblack3a( 63); unrollcountblack3a( 66); unrollcountblack3a( 69); unrollcountblack3a( 72); unrollcountblack3a( 75);
                unrollcountblack3a( 78); unrollcountblack3a( 81); unrollcountblack3a( 84); unrollcountblack3a( 87); unrollcountblack3a( 90);
                unrollcountblack3a( 93); unrollcountblack3a( 96); unrollcountblack3a( 99); unrollcountblack3a(102); unrollcountblack3a(105);
                unrollcountblack3a(108); unrollcountblack3a(111); unrollcountblack3a(114); unrollcountblack3a(117); unrollcountblack3a(120);
                unrollcountblack3a(123); unrollcountblack3a(126);
                                        asm("    LEA IY,IY+126"); unrollcountblack3b(129); unrollcountblack3b(132); unrollcountblack3b(135);
                unrollcountblack3b(138); unrollcountblack3b(141); unrollcountblack3b(144); unrollcountblack3b(147); unrollcountblack3b(150);
                unrollcountblack3b(153); unrollcountblack3b(156); unrollcountblack3b(159); unrollcountblack3b(162); unrollcountblack3b(165);
                unrollcountblack3b(168); unrollcountblack3b(171); unrollcountblack3b(174); unrollcountblack3b(177); unrollcountblack3b(180);
                unrollcountblack3b(183); unrollcountblack3b(186); unrollcountblack3b(189); unrollcountblack3b(192); unrollcountblack3b(195);
                unrollcountblack3b(198); unrollcountblack3b(201); unrollcountblack3b(204); unrollcountblack3b(207); unrollcountblack3b(210);
                unrollcountblack3b(213); unrollcountblack3b(216); unrollcountblack3b(219); unrollcountblack3b(222); unrollcountblack3b(225);
                unrollcountblack3b(228); unrollcountblack3b(231); unrollcountblack3b(234); unrollcountblack3b(237); unrollcountblack3b(240);
                unrollcountblack3b(243); unrollcountblack3b(246); unrollcountblack3b(249); unrollcountblack3b(252);
                asm("    LEA IY,IY+126");
                asm("    LEA IY,IY+255-126-126");
                asm("    LD E,255-1");
                asm("    JR RLE_CountBlack3End");
                unrollcountblack3d(  3); unrollcountblack3d(  6); unrollcountblack3d(  9); unrollcountblack3d( 12); unrollcountblack3d( 15);
                unrollcountblack3d( 18); unrollcountblack3d( 21); unrollcountblack3d( 24); unrollcountblack3d( 27); unrollcountblack3d( 30);
                unrollcountblack3d( 33); unrollcountblack3d( 36); unrollcountblack3d( 39); unrollcountblack3d( 42); unrollcountblack3d( 45);
                unrollcountblack3d( 48); unrollcountblack3d( 51); unrollcountblack3d( 54); unrollcountblack3d( 57); unrollcountblack3d( 60);
                unrollcountblack3d( 63); unrollcountblack3d( 66); unrollcountblack3d( 69); unrollcountblack3d( 72); unrollcountblack3d( 75);
                unrollcountblack3d( 78); unrollcountblack3d( 81); unrollcountblack3d( 84); unrollcountblack3d( 87); unrollcountblack3d( 90);
                unrollcountblack3d( 93); unrollcountblack3d( 96); unrollcountblack3d( 99); unrollcountblack3d(102); unrollcountblack3d(105);
                unrollcountblack3d(108); unrollcountblack3d(111); unrollcountblack3d(114); unrollcountblack3d(117); unrollcountblack3d(120);
                unrollcountblack3d(123); unrollcountblack3d(126); unrollcountblack3e(129); unrollcountblack3e(132); unrollcountblack3e(135);
                unrollcountblack3e(138); unrollcountblack3e(141); unrollcountblack3e(144); unrollcountblack3e(147); unrollcountblack3e(150);
                unrollcountblack3e(153); unrollcountblack3e(156); unrollcountblack3e(159); unrollcountblack3e(162); unrollcountblack3e(165);
                unrollcountblack3e(168); unrollcountblack3e(171); unrollcountblack3e(174); unrollcountblack3e(177); unrollcountblack3e(180);
                unrollcountblack3e(183); unrollcountblack3e(186); unrollcountblack3e(189); unrollcountblack3e(192); unrollcountblack3e(195);
                unrollcountblack3e(198); unrollcountblack3e(201); unrollcountblack3e(204); unrollcountblack3e(207); unrollcountblack3e(210);
                unrollcountblack3e(213); unrollcountblack3e(216); unrollcountblack3e(219); unrollcountblack3e(222); unrollcountblack3e(225);
                unrollcountblack3e(228); unrollcountblack3e(231); unrollcountblack3e(234); unrollcountblack3e(237); unrollcountblack3e(240);
                unrollcountblack3e(243); unrollcountblack3e(246); unrollcountblack3e(249); unrollcountblack3e(252);
                asm("RLE_CountBlack3End:");

                // while (*++ptr == 0 && ++len < 255) {}
                asm("RLE_CountBlack1Unrolled:");
                asm("    LD A,B"); // BC Was set to zero before we started
                asm("    OR A,(IY)");
                asm("    JR NZ,RLE_CountBlack1End");
                asm("    INC E");
                asm("    LD D,E");
                asm("    INC D"); // Instead of CP A,%FF
                asm("    JR Z,RLE_CountBlack1End");
                asm("    INC IY");
                asm("    OR A,(IY)");
                asm("    JR NZ,RLE_CountBlack1End");
                asm("    INC E");
                asm("    INC IY");
                asm("    OR A,(IY)");
                asm("    JR NZ,RLE_CountBlack1End");
                asm("    INC E");
                asm("    INC IY");
                asm("    LD A,(IY)");
                asm("RLE_CountBlack1End:");

                {
                    // col = *ptr;
                    asm("    LD D,A");

                    // if (col != 255)
                    asm("    INC A");
                    asm("    JR	Z,RLE_LastRun");

                    {
                        // Can we consolidate with the previous entry
                        {
                            // Is the current run length < 16
                            asm("    LD A,E");
                            asm("    CP A,16+2");
                            asm("    JR NC,RLE_NoConsolidation");
                            // Are the colours the same
                            asm("    LD A,D");
                            asm("    EXX");
                            asm("    CP A,D");
                            asm("    LD A,E"); // Load the previous length while we are in alt-register mode
                            asm("    EXX");
                            asm("    JR NZ,RLE_NoConsolidation");
                           // Is the previous run length < 16
                            asm("    CP A,16+2");
                            asm("    JR NC,RLE_NoConsolidation");

                            asm("    ADD A,A");
                            asm("    ADD A,A");
                            asm("    ADD A,A");
                            asm("    ADD A,A");
                            asm("    ADD A,E");
                            asm("    SUB A,%22");
                            asm("    LD E,A");
                            asm("    LD A,D");
                            asm("    ADD A,%20-1");
                            asm("    LD D,A");

                            // We'll overwrite the previous entry with the consolidated entry
                            asm("    LD	(IX-2),DE"); // The high (3rd) byte gets written but doesn't affect anything

                            asm("    LD (IY),B"); // BC Was set to zero before we started
                            asm("    INC IY");

                            asm("    JR RLE_Loop");

                        }

                        asm("RLE_NoConsolidation:");

                        // *(rle+1) = col; *rle = len;
                        asm("    LD	(IX),DE"); // The high (3rd) byte gets written but doesn't affect anything

                        // rle += 2;
                        asm("    LEA IX,IX+%2");

                        // *ptr = 0;
                        asm("    LD (IY),B"); // BC Was set to zero before we started

                        // ++ptr;
                        asm("    INC IY");

                        asm("    JR RLE_Loop");
                    }
                    // else
                    {
                        asm("RLE_LastRun:");

                        // *(rle+1) = 0; *rle = len;
                        asm("    LD D,B"); // BC Was set to zero before we started
                        asm("    LD	(IX),DE"); // The high (3rd) byte gets written but doesn't affect anything

                        //rle += 2;
                        asm("    LEA IX,IX+%2");

                        //break;
                        asm("    JR RLE_End");
                    }
                }
            }
            // else
            {
                {
                    asm("RLE_Sextuple:");

                    asm("    LD E,B"); // BC Was set to zero before we started

                    asm("    LD A,L");
                    asm("    OR A,A");
                    asm("    JR Z,RLE_Sextuple_CheckPixelTwo");
                    asm("    LD D,A");
                    asm("    INC A"); // Compare first pixel with 255
                    asm("    JR	Z,RLE_End"); // If the first pixel is 255 we're at the end already
                    asm("    INC E");

                    asm("RLE_Sextuple_PixelTwo:");
                    asm("    LD A,H");
                    asm("    OR A,A");
                    asm("    LEA HL,IY+2");
                    asm("    JR Z,RLE_Sextuple_PixelThree");
                    asm("    CP A,D");
                    asm("    JR NZ,RLE_ColourTriple");
                    asm("    SET 1,E");
                    asm("RLE_Sextuple_PixelThree:");
                    asm("    LD A,(HL)");
                    asm("    OR A,A");
                    asm("    JR Z,RLE_Sextuple_PixelFour");
                    asm("    CP A,D");
                    asm("    JR NZ,RLE_ColourTriple");
                    asm("    SET 2,E");
                    asm("RLE_Sextuple_PixelFour:");
                    asm("    INC HL");
                    asm("    LD A,(HL)");
                    asm("    OR A,A");
                    asm("    JR Z,RLE_Sextuple_PixelFive");
                    asm("    CP A,D");
                    asm("    JR NZ,RLE_ColourTriple");
                    asm("    SET 3,E");
                    asm("RLE_Sextuple_PixelFive:");
                    asm("    INC HL");
                    asm("    LD A,(HL)");
                    asm("    OR A,A");
                    asm("    JR Z,RLE_Sextuple_PixelSix");
                    asm("    CP A,D");
                    asm("    JR NZ,RLE_ColourTriple");
                    asm("    SET 4,E");
                    asm("RLE_Sextuple_PixelSix:");
                    asm("    INC HL");
                    asm("    LD A,(HL)");
                    asm("    OR A,A");
                    asm("    JR Z,RLE_Sextuple_PixelSeven");
                    asm("    CP A,D");
                    asm("    JR NZ,RLE_ColourTriple");
                    asm("    SET 5,E");
                    asm("RLE_Sextuple_PixelSeven:");
                    asm("    INC HL");
                    asm("    LD A,(HL)");
                    asm("    OR A,A");
                    asm("    JR Z,RLE_Sextuple_PixelEight");
                    asm("    CP A,D");
                    asm("    JR NZ,RLE_ColourTriple");
                    asm("    SET 6,E");
                    asm("RLE_Sextuple_PixelEight:");
                    asm("    INC HL");
                    asm("    LD A,(HL)");
                    asm("    OR A,A");
                    asm("    JR Z,RLE_Sextuple_PixelsFinished");
                    asm("    CP A,D");
                    asm("    JR NZ,RLE_ColourTriple");
                    asm("    SET 7,E");

                    asm("RLE_Sextuple_PixelsFinished:");
                    asm("    LD A,D");
                    asm("    ADD A,%30-1");
                    asm("    LD D,A");
                    asm("    LD (IX),DE"); // The high (3rd) byte gets written but doesn't affect anything
                    asm("    LEA IX,IX+%2");
                    asm("    LD (IY),BC");
                    asm("    LD (IY+2),BC");
                    asm("    LD (IY+5),BC");
                    asm("    LEA IY,IY+%8");
                    asm("    JR RLE_Loop");

                    // CheckPixelTwo and CheckPixelThree moved here so RLE_Sextuple can flow into RLE_Sextuple_PixelTwo without a jump
                    asm("RLE_Sextuple_CheckPixelTwo:");
                    asm("    LD A,H");
                    asm("    OR A,A");
                    asm("    LEA HL,IY+2");
                    asm("    JR Z,RLE_Sextuple_CheckPixelThree");
                    asm("    LD D,A");
                    asm("    INC A"); // Compare second pixel with 255
                    asm("    JR	Z,RLE_LastRun"); // If the second pixel is 255 we need a run of 1 black pixel
                    asm("    SET 1,E");
                    asm("    JR RLE_Sextuple_PixelThree");
                    asm("RLE_Sextuple_CheckPixelThree:");
                    asm("    LD D,(HL)");
                    asm("    SET 2,E");
                    asm("    LD A,D");
                    asm("    INC A"); // Compare third pixel with 255
                    asm("    JR	NZ,RLE_Sextuple_PixelFour");
                    asm("    LD E,%1"); // If the third pixel is 255 we need a run of 2 black pixel
                    asm("    JR	RLE_LastRun");
                }

                asm("RLE_ColourTriple:");

//                *(unsigned short*)rle = 0x2000 + ptr[0] + ptr[1]<<5 + ptr[2]<<10;
                asm("    LD L,(IY+1)");
                asm("    ADD HL,HL");
                asm("    ADD HL,HL");
                asm("    ADD HL,HL");
                asm("    LD A,%80>>2"); // This is the top byte of 0x8000 >>2 as it will be <<2 below
                asm("    ADD A,(IY+2)");
                asm("    LD H,A"); 
                asm("    ADD HL,HL");
                asm("    ADD HL,HL");
                asm("    LD A,L");
                asm("    ADD A,(IY)");
                asm("    LD L,A");

                asm("    LD (IX),HL"); // The high (3rd) byte gets written but doesn't affect anything

//                rle += 2;
                asm("    LEA IX,IX+%2");

                // *(unsigned int*)ptr = 0;
                asm("    LD (IY),BC");

                // ptr += 3;
                asm("    LEA IY,IY+%3");

                asm("    LD D,H"); // Prevent this tripple being consolidated
                asm("    JR RLE_Loop");
            }
        }
        asm("RLE_End:");
        asm("    LD (_rle),IX");
        asm("    POP IX"); // Restore IX

        *(unsigned short*)rle = 0xFFFF;

        clearbuffer(COPYBUFFER);
        writetobuffer(COPYBUFFER, rleHeader, rle+2 - rleHeader);
        callbuffer(COPYBUFFER);
        callbuffer(COMMANDBUFFER);

        t += 40*4; // ENSURE THIS IS A MULTIPLE OF 4

//        printnum(gettime()-start); vdp_sendstring("\r");
    }
// printnum(gettime()-start); vdp_sendstring("\n\r");

    clearallbuffers();
    cls();
    cursor(1);
    return 0;
}
