#include "inst_profiler.c"
#include <stdio.h>
#include <stdlib.h>

u64 RecSum(int n){
    TimeFunction;

    {
        TimeBlock("Inner recursive loop");
        for(int i = 1; i < 100; ++i){
            
        }
    }
    if(n == 0) return 0;
    return n + RecSum(n-1);
}

u64 RecSum2(int n){

    if(n == 0) return 0;
    return n + RecSum2(n-1);
}

int main(){
    u64 sum = 0;

    BeginProfiler;

    {
        TimeBlock("Loop");
        for(u64 i = 1; i < 1000000; ++i){
            sum += i;
        }
    }
    
    RecSum(1000);

    EndProfiler;

}