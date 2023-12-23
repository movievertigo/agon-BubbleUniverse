#include "agonmos.h"
#include "agonlib.h"

#include "sintable.inc"

#define CURVECOUNT 256
#define CURVESTEP 4
#define ITERATIONS 256

#define SINTABLEPOWER 14
#define SINTABLEENTRIES (1<<SINTABLEPOWER)

#define SINTABLEMASK (SINTABLEENTRIES*4-1)

#define PI 3.1415926535897932384626433832795f
#define RVALUE (4*(int)((CURVESTEP * (1<<SINTABLEPOWER)) / 235))
#define SCALEVALUE (4*(int)((CURVESTEP * (1<<SINTABLEPOWER)) / (2*PI)))

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

#define sin() \
{ \
    vdp_sendblock(waitForVSYNCBuffer, 3); \
}

int main(void)
{
    long start = gettime();

    int t, ang1Start, ang2Start, ang1, ang2, u, v;
    int i, j;

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

                gcolpointmacro(i, 160 + (u>>7), 120 + (v>>7));
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