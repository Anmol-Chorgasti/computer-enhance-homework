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
#define ASCII_DOT 46
#define ASCII_MINUS 45
#define ASCII_PLUS 43
#define ASCII_ZERO 48
#define ASCII_NINE 57
#define INVALIDJSON 190.0
#define ABS_ERR_MARGIN 1e-8

typedef double f64;
typedef int s32;
typedef long s64;
typedef __uint8_t u8;

struct Pairs {
    f64 X0, Y0, X1, Y1;
};

static s64 ParsedPairs = 0;
static f64 TotalBench = 0;
static char JsonBuffer[JSONSTRINGBYTES];
static s32 BufferBytes = 0; /* maintains CURRENT Valid bytes in buffer */

static f64 Doubles[4] = {};
static s32 DoubleErrFlag = 0;
static s32 CorruptBuffer = 0;


void ExitProg(char *Message){
    printf("%s\n", Message);
    exit(EXIT_FAILURE);
}

void FlushBuffer(){
    char Message[BufferBytes];
    snprintf(Message, BufferBytes, "%s", JsonBuffer);
    printf("%s\n", Message);
}

/*
    Returns 0 if char passed to it is a '.'
    Returns 1 if char passed to it is a '-'
    Returns 2 if char passed to it is a '+'
    Returns 3 if char passed to it is a digit
    Returns -1 for all other cases, aka invalid
*/
int IsValid (char Value){
    if (Value == ASCII_DOT) return 0;
    if (Value == ASCII_MINUS ) return 1;
    if (Value == ASCII_PLUS) return 2;
    if (Value >= ASCII_ZERO && Value <= ASCII_NINE) return 3;
    return -1;
}

void PrintPair(struct Pairs *HPtr){
    printf("X0 : %.12f\n", HPtr->X0);
    printf("Y0 : %.12f\n", HPtr->Y0);
    printf("X1 : %.12f\n", HPtr->X1);
    printf("Y1 : %.12f\n\n", HPtr->Y1);
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

/*
    This function parses a float string into a signed float.
    Requirements:
        1. *Current should immediately be pointing to a '.' or a '-' or a number
        2. Limit should be pointing to the last valid byte in current buffer
    Raises:
        If buffer is corrupted, float itself is corrupted, or current is not pointing immediately upon entry to a valid number, returns the INVALIDJSON code to its caller.
*/

int IsEqual(f64 Val1, f64 Val2){
    if(Val1 == Val2) return 1;
    if (Val2 > Val1){
        f64 T = Val2;
        Val2 = Val1;
        Val1 = T;
    }

    return ((Val1 - Val2)) <= ((Val2) * ABS_ERR_MARGIN);
}
f64 GetDouble (char **Current, char *Limit){

    char Value = *(*Current);
    // Immediate check upon entry
    s32 Type = IsValid(Value);
    if (Type == -1){
        return INVALIDJSON;
    }

    /* First character has been parsed - need to see what it is */
    s32 DotLoc = 0, TotalChars = 1, Sign = 1;
    s64 Number = 0;
    if(Type == 0) DotLoc = 1; 
    else if(Type == 1) Sign = -1;
    else if(Type == 2) Sign = 1;
    else Number = (Number * 10) + (Value - '0'); // first digit
    
    *Current += 1;
    while(*Current <= Limit){
        Value = *(*Current);
        Type = IsValid(Value); 
        
         /* 
            Some Edge cases
            - 56
            -. 678
            The overall float count maintained by the caller will 
            go above 4 as "- 56" is parsed as 0 and 56.
         */
        if (Type == -1) break;

        /* having a sign value here is wrong, it can ONLY be in the first 
           pos, so if type is 1 or 2, flush and return Invalidjson
        */
        if (Type == 1 || Type == 2){
            return INVALIDJSON;
        }
        if (Type == 0){
            if (DotLoc){
                /* what float number has TWO dots? */
                return INVALIDJSON;
            }
            TotalChars += 1;
            DotLoc = TotalChars;
            *Current += 1;
            continue;
        }

        Number = (Number * 10) + (Value - '0');
        *Current += 1;
        TotalChars += 1;

    }

    if (!DotLoc) return (Sign * (f64)Number);
    s64 Den = 1;
    s32 DigitsAfterDot = (TotalChars) - DotLoc;
    for(int i = 0; i < DigitsAfterDot; ++i){
        Den *= 10;
    }
    f64 Result = ((f64)Number) / ((f64)Den);
    return (Sign * Result);
}

/*
    ParseBuffer parses the JSONBUFFER in the state at the moment when it is 
    called.

    It returns 0 if it has successfully parsed exactly 4 floats from Buffer
    and -1 if it fails for any reason.

    Side effect - fills in the Pairs struct passed to it and sets Error flags.
*/
s32 ParseBuffer(struct Pairs *HPtr){
    char *Current = JsonBuffer;
    char *Limit = Current + BufferBytes;

    DoubleErrFlag = 0;
    CorruptBuffer = 0;

    // Reset Doubles for safety reasons
    for(s32 i = 0; i < 4; ++i){
        Doubles[i] = 0;
    } 

    s32 DoublesParsed = -1; /* serves as count AND index for Doubles */
    while(Current < Limit){
        /* only when IsValid does not return -1, call GetDouble */
        char Value = *Current;
        if (IsValid(Value) != -1){
            if(*(Current-1) == 'X' || *(Current-1) == 'Y' || *(Current-1) == 'x' || *(Current-1) == 'y'){
                Current += 1;
                continue;
            }
            f64 Coordinate = GetDouble(&Current, Limit);
            if (Coordinate == INVALIDJSON){
                CorruptBuffer = 1;
                return -1;
            }
            DoublesParsed += 1;
            if (DoublesParsed >= 4){
                DoubleErrFlag = 1;
                return -1;
            }
            Doubles[DoublesParsed] = Coordinate;
            continue; 
            /* current is pointing to the first non numeric char after
               the just parsed double. Let the increment be handled outside
               this if loop in the NEXT iteration.
            */
        }

        Current += 1;
    }

    if (DoublesParsed != 3) {
        DoubleErrFlag = 1;
        return -1;
    }

    /* If here, two invariants true - 4 doubles parsed, all doubles valid */
    HPtr->X0 = Doubles[0]; 
    HPtr->Y0 = Doubles[1];
    HPtr->X1 = Doubles[2];
    HPtr->Y1 = Doubles[3];
    return 0;
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

    /* Free the file descriptors */
    close(JsonFD);
    close(BinFD);


    /* Base pointers initialized for unmapping areas later */
    char *JsonBasePtr = JsonPtr;
    f64 *BinBasePtr = BinPtr;

    struct Pairs HavPairs;
    struct Pairs *HPtr = &HavPairs;
    char *BuffPtr = JsonBuffer; // remember JsonBuffer is going to be &JsonBuffer[0] here.

    // initial push of JsonPtr
    while(*JsonPtr != '[') JsonPtr += 1;
    while(*JsonPtr != '{' && JsonPtr < (JsonBasePtr + JsonSize)) 
        JsonPtr += 1;
    

    while(*JsonPtr == '{'){
        BufferBytes = 1; 

        // we want all bytes from { to } inclusive
        while(*JsonPtr != '}'){
            BufferBytes += 1;
            JsonPtr += 1;
        }
        

        JsonPtr += 1;
        BufferBytes += 1; //to include the last }
       
    
        memcpy(BuffPtr, JsonPtr-BufferBytes+1, BufferBytes);

        // Parse buffer
        s32 Success = ParseBuffer(HPtr);
        if (Success == -1){
            FlushBuffer();
            printf("Pair %ld:\n", ParsedPairs+1);
            printf("Double Flag : %x\nBuffer Flag : %x\n", DoubleErrFlag, CorruptBuffer);
            ExitProg("Above Json string is not parsable");
        }
        
        /*
        printf("Pair %ld:\n", ParsedPairs);
        PrintPair(HPtr);
        */

        // Test Harness
        f64 HavDist = ReferenceHaversine(HPtr->X0, HPtr->Y0, HPtr->X1, HPtr->Y1, EarthRadius);
        f64 CorrectDist = *(BinBasePtr + ParsedPairs);
        if ( !(IsEqual(HavDist, CorrectDist)) ){
            FlushBuffer();
            printf("Pair %ld:\n", ParsedPairs+1);
            PrintPair(HPtr);
            printf("Parsed HD : %.12f\nExpected HD : %.12f\n", HavDist, CorrectDist);
            ExitProg("Haversine distance does not match the expected result.");
        }
        TotalBench += HavDist;

        ParsedPairs += 1;
        // array of pairs has ended - probably never touched
        if(*JsonPtr == ']') break;
        
        // control flow coming here indicates JsonOfft is at the , between two pairs. Make JsonPtr point to next pair's {
        while(*JsonPtr != '{' && JsonPtr < (JsonBasePtr + JsonSize)){
            JsonPtr += 1;
        }
    }

    /* milestone 1 - print parsed pairs */
    printf("%ld\n", ParsedPairs);


    f64 Den = sqrt(ParsedPairs);
    TotalBench = TotalBench/Den;

    /* comparision of final benchmark */
    f64 CorrectBench = *(BinBasePtr + ParsedPairs);
    if ( !(IsEqual(TotalBench, CorrectBench)) ){
        printf("Parsed Bench : %.12f\nExpected Bench : %.12f\n", TotalBench, CorrectBench);
        ExitProg("Final statistical benches don't match");
    }

    printf("Parse succeeded\n");
    printf("Parsed Bench : %.12f\nExpected Bench : %.12f\n", TotalBench, CorrectBench);

    // Close up shop
    munmap(JsonBasePtr, JsonSize);
    munmap(BinBasePtr, BinSize);
    JsonPtr = NULL, BinPtr = NULL;
    JsonBasePtr = NULL, BinBasePtr = NULL;

}

 /*

    note edge cases here
    a. what if json file is just empty array - exit prog and say no pairs found
    b. what if json file is just [ {} ], so have a check for blank pairs
        this should however be tested at the Parse buffer level and scaled to handle any count of floats that are not 4 per buffer - not in the outer loop in main


    a memcpy is always successful in ITS task
    I should be safe in using it, as my memory areas
    don't overlap
    and my destination points to a stack region buffer
    and source points to a mapped region, on the Heap



    */

