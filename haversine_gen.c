#define _POSIX_C_SOURCE 200809L
#include "haversine_fun.c"
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

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
    
    s64 bytes = snprintf(Offset, JSONSTRINGBYTES, "{\"x0\":%.12f, \"y0\":%.12f, \"x1\":%.12f, \"y1\":%.12f}%s\n", X0, Y0, X1, Y1, (flag) ? "" : ",");
    return bytes;
}

void ExitProg(char *Message){
    printf("%s\n", Message);
    exit(EXIT_FAILURE);
}

void LongConverter (char *Value, s64 *Number){
    errno = 0;
    char *EndPtr;
    *Number = strtol(Value, &EndPtr, 0);
    if(*EndPtr != '\0') ExitProg("Invalid count or seed input");
    if(errno == EINVAL) ExitProg("base value unsupported");
    if(errno == ERANGE && (*Number) == LONG_MIN) ExitProg("Input value out of range - underflow");
    if(errno == ERANGE && (*Number) == LONG_MAX) ExitProg("Input value out of range - overflow");
}

/* 
    The parameter values for mmap are fixed at a program level
    This function acts as a wrapper to ensure errno stays at 0 even after
    a mmap call, as if not, it exits the program.

*/
void* RequestMemory(size_t Size){
    void *ptr;
    ptr =  mmap(NULL, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(ptr == MAP_FAILED) ExitProg("Memory allocation failed");
    return ptr;
}

int main(s32 Argc, char **Argv){

    if(Argc != 3) ExitProg("Not enough arguments");

    s64 Seed, Count;
    LongConverter(*(++Argv), &Seed);
    LongConverter(*(++Argv), &Count);

    printf("Seed used: %lx\n", Seed);

    f64 Bench = 0;
    f64 Den = sqrt((f64)Count);
    srand(Seed);
    
    /* 100 bytes extra assumed for json wrapper text */
    const s64 JsonBufSz = (JSONSTRINGBYTES * Count) + 100;
    const s64 BinBufSz = sizeof(f64) * (Count + 1);

    /* requesting huge mmaped regions to write into */
    char *JsonBuffer = (char *)RequestMemory(JsonBufSz);
    f64 *BinBuffer = (f64 *)RequestMemory(BinBufSz);
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
    printf("Statistical Bench Mark : %.12f\n", Bench);
    WriteBinary(BinOffset, &Bench);
    BinOffset += 1;

    /* flush into the files */
    FILE *FJson = NULL;
    if ((FJson = fopen("build/input.json", "w")) == NULL)
        ExitProg("File failed to open");
    FILE *RBin = NULL;
    if ((RBin = fopen("build/results.bin", "wb")) == NULL)
        ExitProg("File failed to open");
    
    /* remember that 0ffset is always 1 past the last written byte */
    s64 BinWritten = fwrite(BinBuffer, sizeof(f64), BinOffset-BinBuffer, RBin);
    s64 JsonWritten = fwrite(JsonBuffer, sizeof(char), JsonOffset-JsonBuffer, FJson);

    if(BinWritten != (BinOffset-BinBuffer) || JsonWritten != (JsonOffset-JsonBuffer)) ExitProg("File write error\n");
   

    /* clean up shop */
    munmap(JsonBuffer, JsonBufSz);
    munmap(BinBuffer, BinBufSz);
    fclose(FJson); fclose(RBin);
    JsonBuffer = NULL; JsonOffset = NULL; 
    BinBuffer = NULL; BinOffset = NULL;
}

/*
    Thoughts
    - the bench does not vary too much across different seeds, around 100-200
      hopefully that is enough variance to go ahead with this assignment
    - early termination IS possible by checking snprintf, but for now using
      a simple final check for fwrite is sufficient given that I am the one
      generating the executable inputs.
*/