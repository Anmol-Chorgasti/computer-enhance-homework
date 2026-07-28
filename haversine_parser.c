#define _POSIX_C_SOURCE 200809L
#include "haversine_fun.c"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>

#define EarthRadius 6372.8
#define JSONSTRINGBYTES 130

typedef double f64;
typedef int s32;
typedef long s64;

struct Pairs {
    f64 X0, Y0, X1, Y1;
};

static f64 ParsedPairs = 0;
static f64 TotalBench = 0;
static char JsonBuffer[JSONSTRINGBYTES];


void ExitProg(char *Message){
    printf("%s\n", Message);
    exit(EXIT_FAILURE);
}



void *DumpFile (s32 FD, char *FileName, s64 *SizePtr){
    struct stat st;
    if( fstat(FD, &st) < 0 )
        ExitProg("failed to get size of file");

    *SizePtr = st.st_size;

    /* Empty file exception */
    if (!(*SizePtr)) 
        ExitProg("Empty file forces termination of program.");

    void *Ptr = NULL;
    Ptr = mmap(NULL, *SizePtr, PROT_READ, MAP_SHARED, FD, 0);

    if (Ptr == MAP_FAILED){
        printf("%s...", FileName);
        ExitProg("failed to dump into memory");
    }
    return Ptr;
}



int main(s32 Argc, char **Argv){

    /* input verification and file opening */
    if(Argc != 3) ExitProg("Wrong argument count");

    s32 JsonFD = open(Argv[1], O_RDONLY);
    if(JsonFD == -1){
        printf("%s ", Argv[1]);
        ExitProg("failed to open");
    }

    s32 BinFD = open(Argv[2], O_RDONLY);
    if(BinFD == -1){
        printf("%s ", Argv[1]);
        ExitProg("failed to open");
    }


    /* Now to dump both files into mmaped regions and get pointers back */
    s64 JsonSize, BinSize;
    char *JsonPtr = (char *)DumpFile(JsonFD, Argv[1], &JsonSize);
    f64 *BinPtr = (f64 *)DumpFile(BinFD, Argv[2], &BinSize);

    /* files don't serve any purpose after region is mmapped */
    close(JsonFD);
    close(BinFD);

    char *JsonOfft = JsonPtr;
    f64 *BinOfft = BinPtr;

    /* Base pointers initialized for unmapping areas later */
    char *JsonBasePtr = JsonPtr;
    f64 *BinBasePtr = BinPtr;

    struct Pairs HavPairs;
    struct Pairs *HPtr = &HavPairs;
    char *BuffPtr = JsonBuffer; // remember JsonBuffer is going to be &JsonBuffer[0] here.

    // initial push of JsonPtr
    while(*JsonPtr != '[') JsonPtr += 1;
    JsonOfft = ++JsonPtr; // both point to first pair's { bracket
    
    while(*JsonPtr != ']'){
        // Populate buffer
        s32 Bytes = 0; 

        // we want all bytes from { to } inclusive
        while(*(JsonOfft++) != '}'){
            Bytes += 1;
        }
        
        Bytes += 1; //to include the last }
       
        memcpy(BuffPtr, JsonPtr, Bytes);

        // update JsonPtr
        if(*JsonOfft == ']') break;
        
        // control flow coming here indicates JsonOfft is at the , between two pairs
        JsonPtr = ++JsonOfft;
        ParsedPairs += 1;
    }

    /* milestone 1 - print parsed pairs */
    printf("%f\n", ParsedPairs);


    f64 Den = sqrt(ParsedPairs);
    TotalBench = TotalBench/Den;

    /* comparision of final benchmark */

    // Close up shop
    munmap(JsonBasePtr, JsonSize);
    munmap(BinBasePtr, BinSize);
    JsonPtr = NULL, BinPtr = NULL;
    JsonOfft = NULL, BinOfft = NULL;

}

 /*
    Todo
    1) Bring in float extractor - use old float parser exercise as inspiration
    2) Address cases thoroughly if the amount of floats extracted are not 4
    3) Bring in test harness and verification against the binary results file
    4) run multiple tests and checks


    JsonOfft should be at the first [ in the json file
    JsonPtr is then this + 1 -> pointing to the first pair's {
    note edge cases here
    a. what if json file is just empty array - exit prog and say no pairs found
    b. what if json file is just [ {} ], so have a check for blank pairs
        this should however be tested at the Parse buffer level - not in the outer loop in main


    a memcpy is always successful in ITS task
    I should be safe in using it, as my memory areas
    don't overlap
    and my destination points to a stack region buffer
    and source points to a mapped region

    */

/* 
*/