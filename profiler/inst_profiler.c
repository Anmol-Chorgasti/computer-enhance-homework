#include "time_stats.c"
#include <sys/mman.h>
#define CPU_FREQ_LIMIT "2"
#define CPU_FREQ_UNIT "sec"
#define LIMIT 4096

/* A tree node 
    Created when we call TimeFunction / TimeBlock
    Pointer to this node - which should be same as CurrPtr in Profiler node
    start time stored, pointer pushed into stack and stored in a variable for gcc to work with on exit
    CurrPtr IS THEN incremented by one. 
    on exit, gcc calls end timer immediately, stores it in node, pops stack top (just decrement stackTop by 1, to point
    to the prev index.
*/
struct Anchor {
    const char *Name;
    u64 TotElapsed;
    u64 TSCChldrenElapsed;
    u64 HitCount; //can use this to check if hit count is 
    u64 Frames;
};


struct Profiler {
    struct Anchor Anchors[LIMIT];
    u64 AnchorIdx;
    u64 TSCFreq;
    u64 StartTime;
    u64 EndTime;
};

struct FrameDetails {
    u64 CallingParent;
    u64 CallingTime;
    struct Anchor *A;
};

static struct Profiler Pr; /* Static takes care of zero initialization for all members of Pr */
static u64 GlobalParentIdx = 0; /* 0 signals no parent open, Casey style thinking */
static u64 RecCount = 0;

/* Figure our recursion and loops later */

void ExitProg(char *Message){
    printf("%s\n", Message);
    exit(EXIT_FAILURE);
}

void PrintTime(char *Message, u64 Start, u64 End, u64 TSCF){
    printf("\n====================== Profiler Output =================\n");
    printf("%s Time: %.2f seconds, %.2f milliseconds\n", Message, (f64)(End - Start)/(f64)TSCF, 1000 * ((f64)(End - Start)/(f64)TSCF));
    printf("---------------------------\n");
}

void PrintElapsed(struct Anchor *APtr, u64 Base){
    /* print name, exclusive elapsed and total elapsed */
    if (APtr->TSCChldrenElapsed) 
        printf("%s[%lu] :- Exclusive = %lu(elapsed) (%.2f%%), Total = %lu(elapsed) (%.2f%%)\n", APtr->Name, APtr->HitCount,
        APtr->TotElapsed - APtr->TSCChldrenElapsed, 100 * ((f64)(APtr->TotElapsed - APtr->TSCChldrenElapsed)/(f64)Base),
        APtr->TotElapsed, 100*((f64)(APtr->TotElapsed)/(f64)Base));
    else
        printf("%s[%lu] :- %lu(elapsed) (%.2f%%)\n", APtr->Name, APtr->HitCount,
        APtr->TotElapsed, 100 * (f64)(APtr->TotElapsed)/(f64)Base);
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

    for(int i = 1; i <= Pr.AnchorIdx; ++i){
        PrintElapsed(&Pr.Anchors[i], MainElapsed);
    }

    printf("Other[1] :- %lu(elapsed) (%.2f%%)\n",
        MainElapsed - Others->TSCChldrenElapsed, 100 * (f64)(MainElapsed - Others->TSCChldrenElapsed)/(f64)MainElapsed);
}


__attribute__ ((always_inline)) inline static void CleanUp(struct FrameDetails *FPtr){
   
    u64 Cost = ReadCPUTimer() - FPtr->CallingTime; // THe entire duration this frame was open for
    u64 Parent = FPtr->CallingParent;
    GlobalParentIdx = Parent;
    
    FPtr->A->TotElapsed += Cost; // this cost gets added to the Anchor of this frame
    Pr.Anchors[Parent].TSCChldrenElapsed += Cost; // the parent of this frame gets the cost added to its children cost
}

__attribute__ ((always_inline)) inline static void CleanRecUp(struct FrameDetails *FPtr){
   // IF WE are on last frame, 
   /*
        We can dump the exclusive costs into the parent's child cost
        ON THE LAST FRAME.
        and also reset globalparentidx to the actual parent!
   */
    FPtr->A->Frames -= 1;
    RecCount -= 1;
    u64 Parent = FPtr->CallingParent;
    GlobalParentIdx = Parent;
    if(FPtr->A->Frames == 0){    
        u64 Cost =  ReadCPUTimer() - FPtr->CallingTime; 
        // the first frame of a recursive fun, how long was this open for? that's the total cost of this anchor
        FPtr->A->TotElapsed += Cost;
        Pr.Anchors[Parent].TSCChldrenElapsed += Cost;
    }
    
}

u64 GetIndex(const char *BName){
    for(int i = Pr.AnchorIdx; i > 0; i--){
        if(Pr.Anchors[i].Name == BName) return i;
    }
    Pr.AnchorIdx += 1;
    return Pr.AnchorIdx;
}

#define TimeFunction TimeBlock(__func__)

#define TimeBlock(BlockName) \
        u64 Idx = 0; \
        if(RecCount) Idx = GetIndex(BlockName); \
        else Idx = (Pr.AnchorIdx += 1); \
        struct FrameDetails Fd __attribute__((cleanup(CleanUp))); \
        Fd.CallingParent = GlobalParentIdx; \
        Fd.A = &Pr.Anchors[Idx]; \
        Fd.A->Name = BlockName; \
        Fd.A->HitCount += 1; \
        struct Anchor *T = &Pr.Anchors[GlobalParentIdx]; \
        GlobalParentIdx = Idx; \
        Fd.CallingTime = ReadCPUTimer(); \

#define TimeRecFunc(BlockName) \
        u64 Idx = GetIndex(BlockName); \
        struct FrameDetails F  __attribute__((cleanup(CleanRecUp))); \
        F.CallingParent = GlobalParentIdx; \
        F.A = &Pr.Anchors[Idx]; \
        F.A->Name = BlockName; \
        F.A->HitCount += 1; \
        F.A->Frames += 1; \
        struct Anchor *T = &Pr.Anchors[GlobalParentIdx]; \
        GlobalParentIdx = Idx; \
        RecCount += 1; \
        F.CallingTime = ReadCPUTimer(); \
        
        


