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

#define innerloop(colIndex) \
{ \
    asm("    LD A,%"#colIndex); \
\
    asm("    LD B,%40"); \
    asm("InnerLoop"#colIndex":"); \
\
    /* for (j = ITERATIONS/4 - 1; j >= 0; --j) */ \
    { \
        /* ang1 = ang1Start + v; */ \
        asm("    ADD.s IY,DE"); /* v from IY, ang1Start from DE' */ \
        asm("    EXX"); /* Leave alt-register mode */ \
\
        /* *(((char*)&ang1)+2) = (char)costable>>16; */ \
        asm("    ADD IY,DE"); \
\
        /* ang2 = ang2Start + u; */ \
        asm("    LEA HL,IX+0"); /* u from IX */ \
        asm("    ADD.s HL,SP"); /* ang2Start from SPS */ \
\
        /* *(((char*)&ang2)+2) = (char)costable>>16; */ \
        asm("    ADD HL,DE"); \
\
        /* u = *(int*)(ang1+COSTABLEENTRIES) + *(int*)(ang2+COSTABLEENTRIES); */ \
        asm("    LD BC,(IY)"); \
        asm("    LD IX,(HL)"); \
        asm("    ADD IX,BC"); \
\
        /* v = *(int*)ang1 + *(int*)ang2; */ \
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
    } \
    asm("    EXX"); /* Enter alt-register mode */ \
    asm("    DJNZ InnerLoop"#colIndex); \
}

#define sintable ((long*)0x50000) // 0x50000 - 0x5FFFF

#define bitmap ((char*)0x60000) // 0x60000 - 0x6FFFF
#define bitmapCentre ((char*)(bitmap+(HEIGHT+1)*HEIGHT/2))
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
        consolidatebuffer(BITMAPBUFFER);                        // Length 6
        createbitmapfrombuffer(HEIGHT, HEIGHT, BITMAPFORMAT);   // Length 8
        drawbitmap((WIDTH-HEIGHT)/2, 0);                        // Length 7
    }

    consolidatebuffer(COMMANDBUFFER);
}

void prependandappend()
{
    rleHeader[0] = 23;
    rleHeader[1] = 0;
    rleHeader[2] = 0xA0;
    *(short*)(rleHeader+3) = BITMAPBUFFER;
    rleHeader[5] = 0x19;

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

void createRLEbuffers()
{
    int lenA, lenB, bufferId;
    unsigned char colA, colB, colC, bitA, bitB, bitC, bitD, bitE, bitF, bitG, bitH, colour, coloursincblack[17], sequence[8];
    unsigned char* colours = coloursincblack+1;

    coloursincblack[0] = 0xc0;
    colours[0x0] = makecolourfromindex(0x0);
    colours[0x1] = makecolourfromindex(0x1);
    colours[0x2] = makecolourfromindex(0x2);
    colours[0x3] = makecolourfromindex(0x3);
    colours[0x4] = makecolourfromindex(0x4);
    colours[0x5] = makecolourfromindex(0x5);
    colours[0x6] = makecolourfromindex(0x6);
    colours[0x7] = makecolourfromindex(0x7);
    colours[0x8] = makecolourfromindex(0x8);
    colours[0x9] = makecolourfromindex(0x9);
    colours[0xA] = makecolourfromindex(0xA);
    colours[0xB] = makecolourfromindex(0xB);
    colours[0xC] = makecolourfromindex(0xC);
    colours[0xD] = makecolourfromindex(0xD);
    colours[0xE] = makecolourfromindex(0xE);
    colours[0xF] = makecolourfromindex(0xF);

    bufferId = 0;
    for (lenA = 1; lenA <= 256; ++lenA)
    {
        createbuffer(bufferId, lenA);
        adjustbuffermultiple(bufferId, 0x42, 0, lenA, 0xc0);
        ++bufferId;
    }

    for (colA = 0; colA < 16; ++colA)
    {
        colour = colours[colA];
        for (lenA = 1; lenA <= 256; ++lenA)
        {
            createbuffer(bufferId, lenA + 1);
            adjustbuffermultiple(bufferId, 0x42, 0, lenA, 0xc0);
            adjustbuffer(bufferId, 2, lenA, colour);
            ++bufferId;
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
                createbuffer(bufferId, lenA + 1 + lenB + 1);
                adjustbuffermultiple(bufferId, 0x42, 0, lenA + 1 + lenB, 0xc0);
                adjustbuffer(bufferId, 2, lenA, colour);
                adjustbuffer(bufferId, 2, lenA + 1 + lenB, colour);
                ++bufferId;
            }
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
        }
        bufferId += (1<<10) - (17<<5);
    }
}

volatile int ang1Start;
volatile int ang2Start;
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
printnum(gettime()-start); vdp_sendstring("\n\r");

    t = 0; // 128000 + 40*4*298;

start = gettime();

    while (getlastkey() != 125 && t < 400*16)
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

        asm("    LD C,%10");
        asm("OuterLoop1:");
        // for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LD IY,0"); // u = 0
            innerloop(1);
            innerloop(2);
            innerloop(3);
            innerloop(4);
           // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DEC C");
        asm("    JR NZ,OuterLoop1");

        asm("    LD C,%10");
        asm("OuterLoop2:");
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LD IY,0"); // u = 0
            innerloop(5);
            innerloop(6);
            innerloop(7);
            innerloop(8);
            // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DEC C");
        asm("    JR NZ,OuterLoop2");

        asm("    LD C,%10");
        asm("OuterLoop3:");
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LD IY,0"); // u = 0
            innerloop(9);
            innerloop(A);
            innerloop(B);
            innerloop(C);
            // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DEC C");
        asm("    JR NZ,OuterLoop3");

        asm("    LD C,%10");
        asm("OuterLoop4:");
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD IX,0"); // v = 0
            asm("    LD IY,0"); // u = 0
            innerloop(D);
            innerloop(E);
            innerloop(F);
            innerloop(10);
            // ang1Start += SCALEVALUE;
            asm("    LD HL,41720");
            asm("    ADD HL,DE");
            asm("    EX DE,HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    DEC C");
        asm("    JR NZ,OuterLoop4");

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
                //len = 0;
                asm("    LD A,B"); // BC Was set to zero before we started

                asm("    LD D,%3");

                {
                    asm("    INC A");

                    // do
                    {
                        asm("RLE_CountBlack3Loop:");

                        // ptr += 3;
                        asm("    LEA IY,IY+%3");
                    }
                    // while (*(unsigned int*)ptr == 0 && len < 255);
                    asm("    LD HL,(IY)");
                    asm("    SBC HL,BC"); // BC Was set to zero before we started
                    asm("    JR NZ,RLE_CountBlack3End");
                    // len += 3;
                    asm("    ADD A,D");
                    asm("    JR NZ,RLE_CountBlack3Loop");
                    asm("    SUB A,D");
                    asm("RLE_CountBlack3End:");

                    // --len; (INC at start then SUB 3 above then INC below equals -1)
                    asm("    LD E,A");
                    asm("    INC E");
                }

                // while (*++ptr == 0 && ++len < 255) {}
                asm("RLE_CountBlack1Loop:");
                asm("    LD A,(IY)");
                asm("    OR A,A");
                asm("    JR NZ,RLE_CountBlack1End");
                asm("    INC E");
                asm("    LD A,E");
                asm("    INC A"); // Instead of CP A,%FF
                asm("    JR Z,RLE_CountBlack1End");
                asm("    INC IY");
                asm("    LD A,(IY)");
                asm("    OR A,A");
                asm("    JR NZ,RLE_CountBlack1End");
                asm("    INC E");
                asm("    INC IY");
                asm("    LD A,(IY)");
                asm("    OR A,A");
                asm("    JR NZ,RLE_CountBlack1End");
                asm("    INC E");
                asm("    INC IY");
                asm("RLE_CountBlack1End:");

                {
                    // col = *ptr;
                    asm("    LD D,(IY)");

                    // if (col != 255)
                    asm("    LD A,D");
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

                            asm("    SUB A,%2");
                            asm("    ADD A,A");
                            asm("    ADD A,A");
                            asm("    ADD A,A");
                            asm("    ADD A,A");
                            asm("    ADD A,E");
                            asm("    SUB A,%2");
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
                    asm("    JR RLE_Sextuple_PixelTwo");
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
                    asm("    JR	Z,RLE_LastRun");

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

        vdp_sendblock(rleHeader, rle+2 - rleHeader);
        callbuffer(COMMANDBUFFER);

        t += 40*4; // ENSURE THIS IS A MULTIPLE OF 4

//        printnum(gettime()-start); vdp_sendstring("\r");
    }
printnum(gettime()-start); vdp_sendstring("\n\r");

//    cls();
    clearallbuffers();
    cursor(1);
    return 0;
}
