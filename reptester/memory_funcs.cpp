
struct buffer {
    u64 Count;
    u8* Data; // Pointer to first valid memory location
};

static buffer AllocateMallocBuffer(u64 Size)
{
    buffer Result = {}; // Zero initialization

    Result.Data = (u8*)malloc(Size);

    /* Defensive Check */
    if(!Result.Data){
        fprintf(stderr, "Unable to Allocate %lu bytes\n", Size);
    }
    else Result.Count = Size;

    return Result;
}

static void FreeBuffer(buffer *B)
{
    if(B->Data){
        free(B->Data);
    }
    /* Could be that B->Data is just NULL */ 
}

/* Allocate Buffer using Mmap but without a file backing it 
   Can be read from and written to
*/
static buffer AllocateBuffer(u64 Size)
{
    buffer Result = {};

    Result.Data = (u8*)mmap(NULL, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if(Result.Data == MAP_FAILED){
        fprintf(stderr, "Unable to mmap %lu bytes\n", Size);
    }
    else Result.Count = Size;
    return Result;
}

/* Allocates Buffer and TRIES to populate it with File contents.
   No promises made on if the file contents were successfully read ahead.
   No write access provided to this buffer, only meant to be read from
*/
static buffer ReadFileIntoBuffer(u32 FD, u64 Size)
{
    buffer Result = {};

    Result.Data = (u8*)mmap(NULL, Size, PROT_READ, MAP_PRIVATE | MAP_FILE | MAP_POPULATE, FD, 0);

    if(Result.Data == MAP_FAILED){
        fprintf(stderr, "Unable to read %x File and mmap %lu bytes\n", FD, Size);
    }
    else Result.Count = Size;
    return Result;
}

static void UnmapBuffer(buffer *B){

    if(B->Data != MAP_FAILED){
        u8 Result = munmap(B->Data, B->Count); // Returns 0 on success
        if(Result){
            fprintf(stderr, "Failed to deallocate Buffer");
        }
    }
}

static buffer HandleAllocation(u32 i, u64 size){
    
    switch(i){
        case 1:
            printf("Preallocation using Malloc\n");
            return AllocateMallocBuffer(size);
        case 2:
            printf("Preallocation using Mmap\n");
            return AllocateBuffer(size);
        default:
            fprintf(stderr, "Unmatched case\n");
            return {};
    }
}

static void HandleDeallocation(u32 i, buffer *B){
    switch(i){
        case 1:
            return FreeBuffer(B);
        case 2:
            return UnmapBuffer(B);
        default:
            fprintf(stderr, "Unmatched case\n");
            return;
    }
}

static void EvictFileFromCache(int Fd){
    if(Fd != -1){
        fdatasync(Fd);

        b32 Result = posix_fadvise(Fd, 0,0,POSIX_FADV_DONTNEED);
        if(Result){
            fprintf(stderr, "Eviction failed\n");
        }

    }else fprintf(stderr, "Invalid File Descriptor\n");
}
