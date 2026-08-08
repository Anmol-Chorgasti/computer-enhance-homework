#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef __uint64_t u64;
typedef double f64;
typedef __uint8_t u8;
typedef __uint32_t u32;
typedef int32_t b32;
typedef long s64;

#include "time_stats.cpp"
#include "memory_funcs.cpp"
#include "rep_tester.cpp"
#include "read_tests.cpp"

#define ArrayCount(Array) sizeof(Array)/sizeof((Array)[0])

struct test_func {
    char const *Name;
    read_test_func *fptr; // this is a pointer to functions that share the signature defined by read_test_func
};

test_func Functions[] = {
    {"Read with fread", ReadWithFread},
    {"Read with system read", ReadWithSysRead},
};

int main(int Argc, char **Argv){
    read_params Param = {};

    if(Argc < 2){
        fprintf(stderr, "No file name provided\n");
        return 1;
    }

    Param.FileName = Argv[1];

    u64 CPUTimerFreq = GetCPUFreq(1);

    struct stat FileStat;
    stat(Param.FileName, &FileStat);

   
    b32 MmapBuffer = 0;
    if(Argc == 3) MmapBuffer = strcmp(Argv[2],"mmap")==0;

    if(MmapBuffer){
        Param.Dest = AllocateBuffer(FileStat.st_size);
    }else{
        Param.Dest = AllocateMallocBuffer(FileStat.st_size);
    }
    
    repetition_tester Testers[ArrayCount(Functions)] = {};

    // defensive check - which I missed, if I am reading nothing..then why even test
    if(Param.Dest.Count){
        for(;;){
            for(int i = 0; i < ArrayCount(Functions); ++i){
                test_func Tf = Functions[i];

                printf("%s --------------\n", Tf.Name);
                repetition_tester *Tester = Testers + i;

                InitializeTester(Tester, CPUTimerFreq, Param.Dest.Count);
                Tf.fptr(Tester, &Param);
                printf("\n\n");
            }
        }
    }
    else{
        fprintf(stderr, "File needs to have some bytes of data for testing\n");
    }
    


    if(MmapBuffer){
        UnmapBuffer(&Param.Dest);
    }else{
        FreeBuffer(&Param.Dest);
    }

}

