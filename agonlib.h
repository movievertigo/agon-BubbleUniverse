void* malloc(int bytes);
void freeall();

void cls();
void cursor(char state);

void plot(char mode, short x, short y);
void gcol(char colour);
void colour(char colour);

long gettime();
unsigned char getkey();
unsigned char getlastkey();

void printnum(long num);
