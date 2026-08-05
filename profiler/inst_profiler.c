#include "time_stats.c"
#include <sys/mman.h>
#define CPU_FREQ_LIMIT "2"
#define CPU_FREQ_UNIT "sec"
#define LIMIT 4096


struct Anchor {
    const char *Name;
    u64 TSCExclusiveElapsed;
    u64 TSCInclusiveElapsed;
    u64 HitCount;  
};


struct Profiler {
    struct Anchor Anchors[LIMIT];
    u64 TSCFreq;
    u64 StartTime;
    u64 EndTime;
};

struct FrameDetails {
    u32 CallingParent;
    u64 CallingTime;
    u32 AnchorIdx;
    u64 OldTscInclusive;
};

static struct Profiler Pr; /* Static takes care of zero initialization for all members of Pr */
static u32 GlobalParentIdx; /* 0 signals no parent open, Casey style thinking */


void ExitProg(char *Message){
    printf("%s\n", Message);
    exit(EXIT_FAILURE);
}

void PrintTime(char *Message, u64 Start, u64 End, u64 TSCF){
    printf("\n====================== Profiler Output =================\n");
    printf("%s Time: %.2f seconds, %.2f milliseconds\n", Message, (f64)(End - Start)/(f64)TSCF, 1000 * ((f64)(End - Start)/(f64)TSCF));
    printf("---------------------------\n");
}


#define BeginProfiler StartProfiler()

// Initialize Profiler
__attribute__((always_inline)) inline static void StartProfiler(){
    Pr.TSCFreq = ReadCPUFreq(CPU_FREQ_UNIT, CPU_FREQ_LIMIT);
    /* What do I need here? */
    Pr.StartTime = ReadCPUTimer();
}

#define EndProfiler EndAndPrintProfiler()
// Kind of Destroy Profiler 
__attribute__((always_inline)) inline static void EndAndPrintProfiler(){

    Pr.EndTime = ReadCPUTimer();
    struct Anchor *Others = &Pr.Anchors[0];

    u64 MainElapsed = Pr.EndTime - Pr.StartTime;
    PrintTime("Full Program",Pr.StartTime, Pr.EndTime, Pr.TSCFreq); // prints entire running time of program

    /* PRINT ALL BLOCKS */
    u32 i = 1;
    while(Pr.Anchors[i].HitCount != 0){
        printf(
            "%s[%lu] :- %lu(excl.) (%.2f%%)", Pr.Anchors[i].Name,
            Pr.Anchors[i].HitCount,
            Pr.Anchors[i].TSCExclusiveElapsed,
            100 * ((f64)Pr.Anchors[i].TSCExclusiveElapsed/(f64)MainElapsed)
        );
        if(Pr.Anchors[i].TSCExclusiveElapsed != Pr.Anchors[i].TSCInclusiveElapsed){
            printf(" %lu(incl.) (%.2f%%)\n",
                Pr.Anchors[i].TSCInclusiveElapsed,
                100 * ((f64)Pr.Anchors[i].TSCInclusiveElapsed/(f64)MainElapsed)
            );
        }else printf("\n");
        i++;
    }

    printf("Other[1] :- %lu(excl.) (%.2f%%)\n",
        MainElapsed + Others->TSCExclusiveElapsed, 100 * (f64)(MainElapsed + Others->TSCExclusiveElapsed)/(f64)MainElapsed);
}


__attribute__ ((always_inline)) inline static void CleanUp(struct FrameDetails *F){
   u64 Cost = ReadCPUTimer() - F->CallingTime; //elapsed time since frame opened

   // this belongs to exclusive cost of current zone
   struct Anchor *A = &Pr.Anchors[F->AnchorIdx];
   A->TSCExclusiveElapsed += Cost;
   struct Anchor *P = &Pr.Anchors[F->CallingParent];
   
   // Subtract this from the parent's exclusive costs however or this same
   // value will be double counted when we go and get Parent's frame costs
   P->TSCExclusiveElapsed -= Cost;

   // Reset Inclusive cost of current zone (at this particular exit point)
   A->TSCInclusiveElapsed = F->OldTscInclusive + Cost;

   // Reset GlobalParentIdx to parent
   GlobalParentIdx = F->CallingParent;
   
}

#define TimeFunction TimeSection(__func__, __COUNTER__+1)
#define TimeBlock(BlockName) TimeSection(BlockName, __COUNTER__+1)

#define TimeSection(BlockName, Idx) \
        struct FrameDetails Fd __attribute__((cleanup(CleanUp))); \
        Fd.AnchorIdx = Idx; \
        Fd.CallingParent = GlobalParentIdx; \
        struct Anchor *A = &Pr.Anchors[Idx]; \
        Fd.OldTscInclusive = A->TSCInclusiveElapsed; \
        A->Name = BlockName; \
        A->HitCount++; \
        GlobalParentIdx = Idx; \
        Fd.CallingTime = ReadCPUTimer(); \

        
        


