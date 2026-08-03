#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <x86intrin.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OS_TIMER_FREQ 1000000000


typedef __uint64_t u64;
typedef double f64;
typedef __uint8_t u8;
typedef __uint32_t u32;
typedef int s32;
typedef long s64;

static int LongConverterFlag = 0; /* set to -1 if something is off */
static int UnitFlag = 0; /* set to -1 if wrong unit passed */

/* Returns value in Nanoseconds at time of counting */
static u64 ReadOsTimer(void)
{
    struct timespec T;
    clock_gettime(CLOCK_MONOTONIC_RAW, &T);
    return (u64)(T.tv_sec * OS_TIMER_FREQ + T.tv_nsec);
}

static u64 ReadCPUTimer(void)
{
    return __rdtsc();
}


/* Takes in RunTime in seconds and returns Approximate CPU Frequency
   per second over that timeframe - calibrated against the known
   CLOCK_GETTIME frequency of 1 GHz.
*/
static u64 GetCPUFreq(f64 RunTime)
{
   
    u64 OSElapsed = 0, OSEnd = 0;
    u64 Target = OS_TIMER_FREQ * RunTime;
    u64 CPUStart = ReadCPUTimer();
    u64 OSStart = ReadOsTimer();

    while (OSElapsed < Target){
        OSEnd = ReadOsTimer();
        OSElapsed = OSEnd - OSStart;
    }
    u64 CPUEnd = ReadCPUTimer();

    f64 TimeSpent = (f64)OSElapsed / (f64)OS_TIMER_FREQ;
    u64 CPUElapsed = CPUEnd - CPUStart;

    /*
        This is a ratio style of calculation
        CPU E / OS E = CPU F / OS F
        CPU F = OS F * (CPU E / OS E)
        CPU F = CPU E * ( 1 / TimeSpent) as 
        1 / Timespent = OS F / OS E    
    */
    u64 CPUFreq = CPUElapsed/TimeSpent;

    /* Print running time stats*/
    printf("Requested Run Time: %.fms %.2fs\n", RunTime * 1e3, RunTime);
    printf("Approx Actual Run Time: %.fns %.fms %.2fs\n", TimeSpent*1e9, TimeSpent*1000, TimeSpent);

    /* Print OS & CPU Timer stats */
    printf("OS Freq : %ld\n", (u64)OS_TIMER_FREQ);
    printf("CPU Start Count : %ld\n", CPUStart);
    printf("CPU Ends Count : %ld\n", CPUEnd);
    printf("CPU Frequency(approx.) : %ld, %.3fGHz\n", CPUFreq, CPUFreq/1e9);

    return CPUFreq;
}

/* This function does the conversion into seconds based on unit 
   and runtime, calls GetCPUFreq based on runtime in nanoseconds and
   returns the (guessed)CPU Frequency
*/
static u64 ReadCPUFreq(char *Unit, char *RunTime){
    char *EndPtr = NULL;
    f64 Time = strtod(RunTime, &EndPtr);
    if(*EndPtr != '\0'){
        LongConverterFlag = -1;
        return 0;
    }


    if(strcmp(Unit,"sec")==0){
         /* no action required as time is already in sec */
    }
    else if(strcmp(Unit, "milli")==0){
        Time /= 1e3;
    }
    else if(strcmp(Unit, "nano")==0){
        Time /= 1e9; 
    }
    else {
        UnitFlag = -1;
        return 0;
    }

    return GetCPUFreq(Time);
}



