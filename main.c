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
#define makecolourindex(outer, inner) ((outer<<2) + inner + 1)

#define innerloop(colIndex) \
{ \
    asm("    LD A,%40"); \
    asm("InnerLoop"#colIndex":"); \
\
    /* for (j = ITERATIONS/4 - 1; j >= 0; --j) */ \
    { \
        /* ang2 = ang2Start + u; */ \
        asm("    POP HL"); /* u from stack */ \
        asm("    ADD.s HL,SP"); /* ang2Start from SPS */\
\
        /* *(((char*)&ang2)+2) = (char)costable>>16; */ \
        asm("    ADD HL,DE"); \
\
        /* ang1 = ang1Start + v; */ \
        asm("    POP BC"); \
        asm("    LD IY,(_ang1Start)"); \
        asm("    ADD.s IY,BC"); \
\
        /* *(((char*)&ang1)+2) = (char)costable>>16; */ \
        asm("    ADD IY,DE"); \
\
        /* v = *(int*)ang1 + *(int*)ang2; */ \
        asm("    LD BC,(IY)"); \
        asm("    LD IX,(HL)"); \
        asm("    ADD IX,BC"); \
        asm("    PUSH IX"); \
\
        /* u = *(int*)(ang1-SINTABLEENTRIES) + *(int*)(ang2-SINTABLEENTRIES); */ \
        asm("    ADD IY,DE"); \
        asm("    ADD HL,DE"); \
        asm("    LD BC,(IY)"); \
        asm("    LD IY,(HL)"); \
        asm("    ADD IY,BC"); \
        asm("    PUSH IY"); \
\
        /* *(unsigned char*)(scaleYTable[v] + scaleXTable[u]) = colIndex; */ \
        asm("    LD BC,(IX+1)"); \
        asm("    LD L,(IY)"); \
        asm("    LD H,E"); /* Assuming E is 0 as it's an address constant (quicker than LD H,%0) */ \
        asm("    ADD HL,BC"); /* Top byte of HL isn't clear but we compensate with -sintable in the scale table */ \
        asm("    LD (HL),%"#colIndex); \
    } \
    asm("    DEC A"); \
    asm("    JR NZ,InnerLoop"#colIndex); \
}

#define costable ((long*)0x50000) // 0x50000 - 0x5FFFF

#define bitmap ((char*)0x60000) // 0x60000 - 0x6FFFF
#define bitmapCentre ((char*)(bitmap+(HEIGHT+1)*HEIGHT/2))
#define bitmapEnd ((char*)(bitmap+HEIGHT*HEIGHT))

#define scaleTable ((unsigned char*)0x80000) // 0x78000 - 0x87FFF

#define sintable ((long*)(((int)costable)*2)) // 0xA0000 - 0xAFFFF

#define rleHeader ((char*)0xB0000) // 0xB0000 - max length of RLE data
#define rleData ((char*)(rleHeader+6))

#define scaleDiv 176
void generatescaletables()
{
    int i, step, xv = -32768 / scaleDiv + HEIGHT/2, yv = (long)bitmap - (long)sintable + (-8192 / (scaleDiv/4) + HEIGHT/2) * HEIGHT;
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
    char commandBuffer[21] = {23, 0, 0xA0, 0, 0, 14,    23, 27, 0x21, 0, 0, 0, 0, 0,    23, 27, 3, 0, 0, 0, 0};

    *(short*)(commandBuffer+0 + 3) = BITMAPBUFFER;

    *(short*)(commandBuffer+6+3) = HEIGHT;
    *(short*)(commandBuffer+6+5) = HEIGHT;
    commandBuffer[6+7] = BITMAPFORMAT;

    *(short*)(commandBuffer+14+3) = (WIDTH-HEIGHT)/2;
    *(short*)(commandBuffer+14+5) = 0;

    writetobuffer(COMMANDBUFFER, commandBuffer, 21);
}

void prependandappend()
{
    rleHeader[0] = 23;
    rleHeader[1] = 0;
    rleHeader[2] = 0xA0;
    *(short*)(rleHeader+3) = BITMAPBUFFER;
    rleHeader[5] = 13;

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
    int len, bufferId;
    unsigned char colA, colB, colour, sequence[2];

    bufferId = 0;
    for (len = 1; len <= 256; ++len)
    {
        createbuffer(bufferId, len);
        adjustbuffermultiple(bufferId, 0x42, 0, len, 0xc0);
        ++bufferId;
    }

    for (colA = 0; colA < 16; ++colA)
    {
        colour = makecolour(colA >> 2, colA & 3);

        for (len = 1; len <= 256; ++len)
        {
            createbuffer(bufferId, len + 1);
            adjustbuffermultiple(bufferId, 0x42, 0, len, 0xc0);
            adjustbuffer(bufferId, 2, len, colour);
            ++bufferId;
        }
    }

    for (colB = 0; colB < 16; ++colB)
    {
        sequence[1] = makecolour(colB >> 2, colB & 3);
        bufferId = ((colB+1)<<8) + 0x2001;
        for (colA = 0; colA < 16; ++colA)
        {
            sequence[0] = makecolour(colA >> 2, colA & 3);
            writetobuffer(bufferId + colA, sequence, 2);
        }
    }

    sequence[1] = 0xc0;
    for (colA = 0; colA < 16; ++colA)
    {
        sequence[0] = makecolour(colA >> 2, colA & 3);
        writetobuffer(colA + 0x2001, sequence, 2);
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

    t = 0;

start = gettime();

    while (getlastkey() != 125 && t < 400*16)
    {
//        start = gettime();

        asm("    PUSH IX"); // Store current IX

        ang1Start = t;
        // ang2Start = t;
        asm("    LD HL,(_t)");
        asm("    LD.s SP,HL");

        // Pre-load some constants into registers
        asm("    LD	DE,50000h");

        asm("    LD A,%10");
        asm("OuterLoop1:");
        asm("    EX AF,AF'"); // Switch A registers
        // for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD BC,0");
            asm("    PUSH BC"); // v = 0
            asm("    PUSH BC"); // u = 0
            innerloop(1);
            innerloop(2);
            innerloop(3);
            innerloop(4);
            asm("    POP BC"); // Balance the stack
            asm("    POP BC"); // Balance the stack
            // ang1Start += SCALEVALUE;
            asm("    LD BC,41720");
            asm("    LD HL,(_ang1Start)");
            asm("    ADD HL,BC");
            asm("    LD (_ang1Start),HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    EX AF,AF'"); // Switch A registers
        asm("    DEC A");
        asm("    JR NZ,OuterLoop1");

        asm("    LD A,%10"); // A' (Alt register)
        asm("OuterLoop2:");
        asm("    EX AF,AF'"); // Switch A registers
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD BC,0");
            asm("    PUSH BC"); // v = 0
            asm("    PUSH BC"); // u = 0
            innerloop(5);
            innerloop(6);
            innerloop(7);
            innerloop(8);
            asm("    POP BC"); // Balance the stack
            asm("    POP BC"); // Balance the stack
            // ang1Start += SCALEVALUE;
            asm("    LD BC,41720");
            asm("    LD HL,(_ang1Start)");
            asm("    ADD HL,BC");
            asm("    LD (_ang1Start),HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    EX AF,AF'"); // Switch A registers
        asm("    DEC A");
        asm("    JR NZ,OuterLoop2");

        asm("    LD A,%10"); // A' (Alt register)
        asm("OuterLoop3:");
        asm("    EX AF,AF'"); // Switch A registers
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD BC,0");
            asm("    PUSH BC"); // v = 0
            asm("    PUSH BC"); // u = 0
            innerloop(9);
            innerloop(A);
            innerloop(B);
            innerloop(C);
            asm("    POP BC"); // Balance the stack
            asm("    POP BC"); // Balance the stack
            // ang1Start += SCALEVALUE;
            asm("    LD BC,41720");
            asm("    LD HL,(_ang1Start)");
            asm("    ADD HL,BC");
            asm("    LD (_ang1Start),HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    EX AF,AF'"); // Switch A registers
        asm("    DEC A");
        asm("    JR NZ,OuterLoop3");

        asm("    LD A,%10"); // A' (Alt register)
        asm("OuterLoop4:");
        asm("    EX AF,AF'"); // Switch A registers
        //        for (i = CURVECOUNT/CURVESTEP/4-1; i >= 0; --i)
        {
            asm("    LD BC,0");
            asm("    PUSH BC"); // u = 0
            asm("    PUSH BC"); // u = 0
            innerloop(D);
            innerloop(E);
            innerloop(F);
            innerloop(10);
            asm("    POP BC"); // Balance the stack
            asm("    POP BC"); // Balance the stack
            // ang1Start += SCALEVALUE;
            asm("    LD BC,41720");
            asm("    LD HL,(_ang1Start)");
            asm("    ADD HL,BC");
            asm("    LD (_ang1Start),HL");
                // ang2Start += RVALUE;
            asm("    LD HL,1112");
            asm("    ADD.s HL,SP");
            asm("    LD.s SP,HL");
        }
        asm("    EX AF,AF'"); // Switch A registers
        asm("    DEC A");
        asm("    JR NZ,OuterLoop4");

        //rle = rleData;
        asm("    LD IX,B0006h");

        //ptr = bitmap;
        asm("    LD IY,60000h");

        // while (1)
        {
            asm("RLE_Loop:");

            // if (*ptr == 0)
            asm("    LD A,(IY)");
            asm("    OR A,A");
            asm("    JR NZ,RLE_ColourPair");

            {
                //len = 0;
                asm("    LD E,%0");

                asm("    LD D,%3");

                // if (*(unsigned int*)ptr == 0)
                asm("    LD BC,0");
                asm("    LD HL,(IY)");
                asm("    SBC HL,BC");
                asm("    JR NZ,RLE_Not3Black");

                {
                    asm("    LD A,E");
                    asm("    INC A");

                    // do
                    {
                        asm("RLE_CountBlack3Loop:");

                        // ptr += 3;
                        asm("    LEA IY,IY+%3");
                    }
                    // while (*(unsigned int*)ptr == 0 && len < 255);
                    asm("    LD HL,(IY)");
                    asm("    SBC HL,BC"); // BC Was set to zero before we started the loop
                    asm("    JR NZ,RLE_CountBlack3End");
                    // len += 3;
                    asm("    ADD A,D");
                    asm("    JR NZ,RLE_CountBlack3Loop");
                    asm("    SUB A,D");
                    asm("RLE_CountBlack3End:");

                    // --len; (INC at start then SUB 3 above then INC below equals -1)
                    asm("    INC A");
                    asm("    LD E,A");

                    // Skip "decrementing then incrementing";
                    asm("    JR RLE_From3Black");
                }

                asm("RLE_Not3Black:");

                // while (*++ptr == 0 && ++len < 255) {}
                asm("RLE_CountBlack1Loop:");
                asm("    INC IY");
                asm("RLE_From3Black:");
                asm("    LD A,(IY)");
                asm("    OR A,A");
                asm("    JR NZ,RLE_CountBlack1End");
                asm("    INC E");
                asm("    LD A,E");
                asm("    INC A"); // Instead of CP A,%FF
                asm("    JR NZ,RLE_CountBlack1Loop");
                asm("RLE_CountBlack1End:");

                {
                    // col = *ptr;
                    asm("    LD D,(IY)");

                    // if (col != 255)
                    asm("    LD A,D");
                    asm("    INC A");
                    asm("    JR	Z,RLE_LastRun");

                    {
                        // *ptr = 0;
                        asm("    LD (IY),%0");

                        // ++ptr;
                        asm("    INC IY");

                        // *rle = len;
                        // *(rle+1) = col;
                        asm("    LD	(IX),DE"); // The high (3rd) byte gets written but doesn't affect anything

                        // rle += 2;
                        asm("    LEA IX,IX+%2");

                        asm("    JR RLE_Loop");
                    }
                    // else
                    {
                        asm("RLE_LastRun:");

                        //*rle = len;
                        //*(rle+1) = 0;
                        asm("    LD D,%0");
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
                asm("RLE_ColourPair:");

//                *(unsigned short*)rle = 0x2000 + *(unsigned short*)ptr;
                asm("    LD BC,(IY)");
                asm("    LD A,%20");
                asm("    ADD A,B");
                asm("    LD B,A");
                asm("    LD (IX),BC"); // The high (3rd) byte gets written but doesn't affect anything

//                rle += 2;
                asm("    LEA IX,IX+%2");

                // *(unsigned short*)ptr = 0;
                asm("    LD (IY),%0");
                asm("    LD (IY+1),%0");

                // ptr += 2;
                asm("    LEA IY,IY+%2");

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
    cursor(1);
    return 0;
}