#define _POSIX_C_SOURCE 200809L
#include "haversine_fun.c"
#include "profiler/inst_profiler.c"
#include <unistd.h>
#include <sys/stat.h>
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
#define ABS_ERR_MARGIN 1e-7
#define TENMILL 10000000

typedef int s32;
typedef long s64;

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


void FlushBuffer(){
    char Message[BufferBytes];
    snprintf(Message, BufferBytes, "%s", JsonBuffer);
    printf("%s\n", Message);
}

s64 IsValid (char Value){
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


/* Part of test harness */
s64 IsEqual(f64 Val1, f64 Val2){
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
    s32 Type = IsValid(Value);
    if (Type == -1){
        return INVALIDJSON;
    }

    s32 DotLoc = 0, TotalChars = 1, Sign = 1;
    s64 Number = 0;
    if(Type == 0) DotLoc = 1; 
    else if(Type == 1) Sign = -1;
    else if(Type == 2) Sign = 1;
    else Number = (Number * 10) + (Value - '0'); 
    
    *Current += 1;
    while(*Current <= Limit){
        Value = *(*Current);
        Type = IsValid(Value); 
        
        if (Type == -1) break;

        if (Type == 1 || Type == 2){
            return INVALIDJSON;
        }
        if (Type == 0){
            if (DotLoc){
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
    for(s64 i = 0; i < DigitsAfterDot; ++i){
        Den *= 10;
    }
    f64 Result = ((f64)Number) / ((f64)Den);
    return (Sign * Result);
}


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

void TestDistances(char *Oracle, f64 *PD, f64 Benchmark){
    s32 BinFD = open(Oracle, O_RDONLY);
    if(BinFD == -1){
        printf("%s ", Oracle);
        ExitProg("failed to open");
    }
    s64 BinSize;
    f64 *BinPtr = (f64 *)DumpFile(BinFD, Oracle, &BinSize);
    close(BinFD);
    f64 *BinBasePtr = BinPtr;

    for(s64 i = 0; i < TENMILL; ++i){
        if (IsEqual(*(PD + i), *(BinBasePtr + i))) continue;
        printf("Match failed on %ld pair\nExpected : %.12f, Received : %.12f\n", (i+1), *(BinBasePtr + i), *(PD + i));
        ExitProg("Test failed");
    }

    s32 flag = 0;
    f64 CorrectBench = *(BinBasePtr + TENMILL);
    munmap(BinBasePtr, BinSize);
    BinPtr = NULL;
    BinBasePtr = NULL;

    if(IsEqual(Benchmark, CorrectBench)) return;
    printf("Match failed on Benchmark.\nExpected : %.12f, Received : %.12f\n",CorrectBench, Benchmark);
    ExitProg("Test failed");
}

f64 Rand(s32 Min, s32 Max){
    f64 Base = (1.0 * rand())/RAND_MAX;
    Base *= (Max - Min);
    Base += Min;
    return Base;
}

int main(s32 Argc, char **Argv)
{
     // Arguments are ./program unit timeval JSONFILENAME testfile(optional). 

    /*
        setup clock timer here - expect time to use for cpufreq
        to be provided by program user
    */

    // for now expect arguments to be program name, JSONNAME Oracle
    if(Argc != 2 && Argc != 3) ExitProg("Wrong argument count");

    BeginProfiler;

    /* Define all variables needed */
    s32 JsonFD;
    s64 JsonSize, BinSize;
    char *JsonPtr = NULL;
    struct Pairs *HP = NULL;
    struct Pairs *HPLimit = NULL;
    f64 *ParsedDistances = NULL;
    f64 *PDLimit = NULL;
    char *JsonBasePtr = NULL;
    char *BuffPtr = NULL;
    f64 Sum = 0; f64 Den = sqrt((f64)TENMILL);

    /* start timer for mmap and file open here */
    {
        TimeBlock("Mem Alloc and File Load");

        JsonFD = open(Argv[1], O_RDONLY);
        if(JsonFD == -1){
            printf("%s ", Argv[1]);
            ExitProg("failed to open");
        }
        JsonPtr = (char *)DumpFile(JsonFD, Argv[1], &JsonSize);
        close(JsonFD);

        /* Start struct memory request timer here */
        HP = (struct Pairs *)RequestMemory(sizeof(struct Pairs)*TENMILL);
        HPLimit = HP + TENMILL; //One past the last valid mem addr.
        ParsedDistances = (f64 *)RequestMemory(sizeof(f64)*TENMILL);
        PDLimit = ParsedDistances + TENMILL;

        JsonBasePtr = JsonPtr;
        BuffPtr = JsonBuffer;
    }  


    {
         /* Start Parse timer here */
        TimeBlock("Parser");
         // initial push of JsonPtr
        while(*JsonPtr != '[') JsonPtr += 1;
        while(*JsonPtr != '{' && JsonPtr < (JsonBasePtr + JsonSize)) 
            JsonPtr += 1;
        while(*JsonPtr == '{' && (ParsedPairs < TENMILL)){

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
            s32 Success = ParseBuffer(HP + ParsedPairs);
            if (Success == -1){
                FlushBuffer();
                printf("Pair %ld:\n", ParsedPairs+1);
                printf("Double Flag : %x\nBuffer Flag : %x\n", DoubleErrFlag, CorruptBuffer);
                ExitProg("Above Json string is not parsable");
            }

            ParsedPairs += 1;
            // array of pairs has ended - probably never touched
            if(*JsonPtr == ']') break;
            
            // control flow coming here indicates JsonOfft is at the , between two pairs. Make JsonPtr point to next pair's {
            while(*JsonPtr != '{' && JsonPtr < (JsonBasePtr + JsonSize)){
                JsonPtr += 1;
            }
        }
    }
   

    // Test if 10million pairs parsed
    if(ParsedPairs != TENMILL){
        ExitProg("Input file has lesser than 10mill pairs. Exiting as further results are suspect.");
    }

    /* Start haversine distance calculation timer here */
   
    {
        TimeBlock("Haversine Function");
        for(s64 i = 0; i < TENMILL; ++i){
            struct Pairs *HPtr = HP + i;
            *(ParsedDistances + i) = ReferenceHaversine(
                HPtr->X0, HPtr->Y0,
                HPtr->X1, HPtr->Y1,
                EarthRadius
            );
        }
    }
   

    
    /* Start sum and benchmark timer here */
    {
        TimeBlock("Benchmark calc");
        for(s64 i = 0; i < TENMILL; ++i){
            Sum += *(ParsedDistances + i);
        }
        Sum /= Den;
    }
  

    // free memory timer here
    munmap(JsonBasePtr, JsonSize);
    munmap(HP, sizeof(struct Pairs)*TENMILL);
    if(Argc == 2) munmap(ParsedDistances, sizeof(f64)*TENMILL);
    JsonPtr = NULL, JsonBasePtr = NULL;
    HPLimit = NULL, PDLimit = NULL;
   
    /* End program timer here! */
    EndProfiler;

    if(Argc == 3){
        //..how are you going to test the distances if the freaking 
        // parsed distances memory is out man
        TestDistances(Argv[2], ParsedDistances, Sum);
        printf("\nALL TESTS PASSED!\n");
        printf("Benchmark : %.12f\n\n", Sum);
        munmap(ParsedDistances, sizeof(f64)*TENMILL);
    }

}