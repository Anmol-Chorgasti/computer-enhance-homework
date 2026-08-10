#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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
    //{"Read with system read", ReadWithSysRead},
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

    repetition_tester Testers[ArrayCount(Functions)+1] = {};
    Param.Dest.Count = FileStat.st_size;

    // defensive check - which I missed, if I am reading nothing..then why even test
    if(Param.Dest.Count){
        for(;;){        
            for(int i = 1; i <= 2; ++i){
                Param.Dest = HandleAllocation(i, FileStat.st_size);
                for(int i = 0; i < ArrayCount(Functions); ++i){
                    test_func Tf = Functions[i];

                    printf("%s --------------\n", Tf.Name);
                    repetition_tester *Tester = Testers + i;

                    InitializeTester(Tester, CPUTimerFreq, Param.Dest.Count, 2);
                    Tf.fptr(Tester, &Param);
                    printf("\n\n");
                }
                HandleDeallocation(i, &Param.Dest);
            }

            //file populate test - to do
            printf("Reading file with Mmap and MAP_POPULATE -----\n");
            repetition_tester  *Tester = Testers + ArrayCount(Functions); // last index
            InitializeTester(Tester, CPUTimerFreq, Param.Dest.Count);
            ReadWithMmapPop(Tester, &Param);
            printf("\n\n");
        }
    }
    else{
        fprintf(stderr, "File needs to have some bytes of data for testing\n");
    }
    


}

