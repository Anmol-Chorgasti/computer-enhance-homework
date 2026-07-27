#define _POSIX_C_SOURCE 200809L
#include "../haversine_fun.c"
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#define XB -180
#define XE 180
#define YB -90
#define YE 90
#define EarthRadius 6372.8
#define JSONSTRINGBYTES 130


typedef double f64;
typedef int s32;
typedef long s64;

/* How much of the range from Min to Max do we want to scale into
   shift that scale by Min. 
   so 60% of range from 4 to 8, is how much of 4 numbers do I want to eatup
   I say 2.4 -> now I want to be 2.4 into the range from 4 to 8
   so I add 4 to it, getting 6.4
*/
f64 Rand(s32 Min, s32 Max){
    f64 Base = (1.0 * rand())/RAND_MAX;
    Base *= (Max - Min);
    Base += Min;
    return Base;
}

/* we know each write is an 8 byte binary */
void WriteBinary(f64 *Memptr, f64 *Value){
    memcpy(Memptr, Value, sizeof(f64));
}

/* number of bytes written have to be written as it is not fixed
   Offset will be incremented by those many bytes upon function return.
*/
s64 WriteJson(f64 X0, f64 Y0, f64 X1, f64 Y1, char *Offset, s32 flag){
    
    s64 bytes = snprintf(Offset, JSONSTRINGBYTES, "{\"x0\":%.17g, \"y0\":%.17g, \"x1\":%.17g, \"y1\":%.17g}%s\n", X0, Y0, X1, Y1, (flag) ? "" : ",");
    return bytes;
}


int main(s32 Argc, char **Argv){

    s64 Seed = strtol(*(++Argv), NULL, 0);
    s64 Count = strtol(*(++Argv), NULL, 0);
    f64 Bench = 0;
    f64 Den = sqrt((f64)Count);
    srand(Seed);

    /* requesting huge mmaped regions to write into */
    char *JsonBuffer = mmap(NULL, JSONSTRINGBYTES*Count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    f64 *BinBuffer = mmap(NULL, sizeof(f64)*Count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char *JsonOffset = JsonBuffer;
    f64 *BinOffset = BinBuffer;

    /* initial Json formatting write */
    s64 IntroBytes = snprintf(JsonOffset, JSONSTRINGBYTES, "{\"pairs\":[\n");
    JsonOffset += IntroBytes;

    for(s32 i = 0; i < Count; ++i){
        f64 X0 = Rand(XB, XE), X1 = Rand(XB, XE);
        f64 Y0 = Rand(YB, YE), Y1 = Rand(YB, YE);

        f64 Res = ReferenceHaversine(X0, Y0, X1, Y1, EarthRadius);
        Bench += Res;

        /* do the writes */
        WriteBinary(BinOffset, &Res);
        s64 Bytes = WriteJson(X0, Y0, X1, Y1, JsonOffset, ((i+1)==Count) ? 1 : 0);
        BinOffset += 1;
        JsonOffset += Bytes;
    }

    s64 EndBytes = snprintf(JsonOffset, JSONSTRINGBYTES, "]}");
    JsonOffset += EndBytes;

    Bench = Bench / Den;
     /* Print average for me to check */
    printf("Statistical Bench Mark : %.17g\n", Bench);

    /* flush into the files - pending */
    FILE *FJson = fopen("input.json", "w");
    FILE *RBin = fopen("results.bin", "wb");
    
    /* remember that 0ffset is always 1 past the last written byte */
    fwrite(BinBuffer, sizeof(f64), BinOffset-BinBuffer, RBin);
    fwrite(JsonBuffer, sizeof(char), JsonOffset-JsonBuffer, FJson);
   

    /* clean up shop */
    munmap(JsonBuffer, JSONSTRINGBYTES*Count);
    munmap(BinBuffer, sizeof(f64)*Count);
    fclose(FJson); fclose(RBin);
    JsonBuffer = NULL; JsonOffset = NULL; 
    BinBuffer = NULL; BinOffset = NULL;
}

/*
    add in exit mechanisms, and failsafes
    - how to exit early if no command line terminals provided
    - how to exit if os does not oblige with mmap requests

*/