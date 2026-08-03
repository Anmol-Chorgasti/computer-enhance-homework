#include "time_stats.c"
#include <sys/mman.h>
#define CPU_FREQ_LIMIT "2"
#define CPU_FREQ_UNIT "sec"
#define LIMIT 1000

/* A tree node 
    Created when we call TimeFunction / TimeBlock
    Pointer to this node - which should be same as CurrPtr in Profiler node
    start time stored, pointer pushed into stack and stored in a variable for gcc to work with on exit
    CurrPtr IS THEN incremented by one. 
    on exit, gcc calls end timer immediately, stores it in node, pops stack top (just decrement stackTop by 1, to point
    to the prev index.
*/
struct PrNode {
    struct PrNode *Children[LIMIT];
    const char *Name;
    u64 StartTime;
    u64 EndTime;
    u32 CurrChildIdx;  //always points to the CURRENT FREE Index to insert a child node ptr in.
};


struct Profiler {
    struct PrNode *Stack[LIMIT];
    struct PrNode *Root; /* Points to the base of the Mmaped region as well as the root node */
    struct PrNode *MmapLimit;
    u64 TSCFreq;
    u32 StackTop;
};

static struct Profiler Pr = {}; /* Global variable due to many functions internally relying on it */
static int Count = 0; /* this keeps track of how many blocks or functions we have profiled */

void ExitProg(char *Message){
    printf("%s\n", Message);
    exit(EXIT_FAILURE);
}

void PrintTime(char *Message, u64 Start, u64 End, u64 TSCF){
    printf("\n====================== Profiler Output =================\n");
    printf("%s Time: %.2f seconds, %.2f milliseconds\n", Message, (f64)(End - Start)/(f64)TSCF, 1000 * ((f64)(End - Start)/(f64)TSCF));
    printf("---------------------------\n");
}

void PrintTree(struct PrNode *Node, int Depth, u64 Base){
    /* if Depth is a 1000 layers deep..no screen will be able to handle this print out..
       is it safe to say though that trying to profile something so nested in general is a bad habit?
    */
    for(int i = 0; i < Depth; i++)
        printf(" ");
    u64 Elapsed = Node->EndTime - Node->StartTime;
    printf("%s : %ld (elapsed) (%.2f%%)\n", Node->Name, Elapsed, 100 * ((f64)Elapsed/(f64)Base));

    // Recursive calls
    for(int i = 0; i < Node->CurrChildIdx; ++i){
        PrintTree(Node->Children[i], Depth+1, Base);
    }
    return;
}

void PrintTimeTree(struct PrNode *Node, int Depth, u64 Base, u64 TSCF){
    /* if Depth is a 1000 layers deep..no screen will be able to handle this print out..
       is it safe to say though that trying to profile something so nested in general is a bad habit?
    */
    
    for(int i = 0; i < Depth; i++)
       printf(" ");
    
    u64 Elapsed = Node->EndTime - Node->StartTime;
    printf("%s Time: %.2f seconds, %.2f milliseconds, (%.2f%%)\n", Node->Name, (f64)(Elapsed)/(f64)TSCF, 1000 * ((f64)(Elapsed)/(f64)TSCF), 100 * ((f64)Elapsed/(f64)Base));

    // Recursive calls
    for(int i = 0; i < Node->CurrChildIdx; ++i){
        PrintTimeTree(Node->Children[i], Depth+1, Base, TSCF);
    }
    return;
}

/* If success, returns a warmed up memory, avoiding PAGE FAULT cost later */
void* RequestMemory(size_t Size){
    void *ptr;
    ptr =  mmap(NULL, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    if(ptr == MAP_FAILED) ExitProg("Memory allocation failed");
    return ptr;
}

void FreeMemory(void *Root, size_t Size){
    int n = munmap(Root, Size);
    if(!n) return;
    ExitProg("Failed to free Memory. TERMINATING PROGRAM");
}

#define BeginProfiler StartProfiler()

// Initialize Profiler
__attribute__((always_inline)) inline static void StartProfiler(){
    Pr.TSCFreq = ReadCPUFreq(CPU_FREQ_UNIT, CPU_FREQ_LIMIT);
    Pr.Root = (struct PrNode *)RequestMemory(sizeof(struct PrNode)*LIMIT);
   
    Pr.MmapLimit = Pr.Root + (LIMIT); // Points to the LAST valid address in the mmapped region
    Pr.Stack[0] = Pr.Root;
    Pr.Root->StartTime = ReadCPUTimer();
    Pr.StackTop = 0; //points to current active running block of code
}

#define EndProfiler EndAndPrintProfiler()
// Kind of Destroy Profiler 
__attribute__((always_inline)) inline static void EndAndPrintProfiler(){

    Pr.Root->EndTime = ReadCPUTimer();
    Pr.Root->Name = "Root";

    u64 MainElapsed = Pr.Root->EndTime - Pr.Root->StartTime;
    PrintTime("Full Program",Pr.Root->StartTime, Pr.Root->EndTime, Pr.TSCFreq); // prints entire running time of program
    PrintTree(Pr.Root, 0, MainElapsed);

    u64 ChildrenCost = 0;
    for(int i = 0; i < Pr.Root->CurrChildIdx; ++i){
        ChildrenCost += (Pr.Root->Children[i]->EndTime - Pr.Root->Children[i]->StartTime);
    }
    s64 Balance = (MainElapsed - ChildrenCost);
    if(Balance >= 0) 
        printf("\nOther : %ld(elapsed) (%.2f%%)\n\n", Balance, 100 * ((f64)Balance/(f64)MainElapsed));
    else ExitProg("Profiler math is not mathing");

    printf("\n---------------------------------\n");
    
    PrintTimeTree(Pr.Root, 0, MainElapsed, Pr.TSCFreq);

    printf("\n%d blocks profiled.\n", Count+1);
    // Free memory
    FreeMemory(Pr.Root, sizeof(struct PrNode)*LIMIT);
    Pr.Root = Pr.MmapLimit = NULL;
    Pr.Stack[0] = NULL;
    Pr.StackTop = 0;
}

/* 
   The profiler does not have defensive checks baked into it, 
   so adding those in could further inflate the costs.
*/
__attribute__ ((always_inline)) inline static void CleanUp(struct PrNode **NodePtr){
    (*NodePtr)->EndTime = ReadCPUTimer(); //let this be the FIRST thing that happens

    // move stack top back by 1 as this node is POPPED off
    Pr.StackTop = (Pr.StackTop > 0) ? (Pr.StackTop - 1) : 0; 

    //Add this node to the children of the parent node - the one that called this Node
    struct PrNode *Parent = Pr.Stack[Pr.StackTop];
    Parent->Children[Parent->CurrChildIdx] = (*NodePtr);
    Parent->CurrChildIdx += 1;
}

#define TimeFunction \
        Count += 1; \
        struct PrNode *Node __attribute__((cleanup(CleanUp))) = Pr.Root + Count; \
        Pr.StackTop += 1; \
        Pr.Stack[Pr.StackTop] = Node; \
        Node->Name = __func__; \
        Node->CurrChildIdx = 0; \
        Node->StartTime = ReadCPUTimer(); \

#define TimeBlock(BlockName) \
        Count += 1; \
        struct PrNode *Node __attribute__((cleanup(CleanUp))) = Pr.Root + Count; \
        Pr.StackTop += 1; \
        Pr.Stack[Pr.StackTop] = Node; \
        Node->Name = BlockName; \
        Node->CurrChildIdx = 0; \
        Node->StartTime = ReadCPUTimer(); \



