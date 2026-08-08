
#include <fcntl.h>
#include <unistd.h>


// Filename to open and memory location to read into
struct read_params {
    char const *FileName;
    buffer Dest;
};

typedef void read_test_func(repetition_tester *Tester, read_params *Param);

static void ReadWithFread(repetition_tester *Tester, read_params *Param){
    
    while(IsTesting(Tester)){
        FILE *F = fopen(Param->FileName, "rb");
        if(F){
            buffer Dptr = Param->Dest;   
            BeginTime(Tester);
            size_t Result = fread(Dptr.Data, Dptr.Count, 1, F);
            EndTime(Tester);
            /*
                The way this fread has been used is elegant
                Dptr.Count, due to how the buffer struct is made contains the total byte count of the file
                Treat the entire file as 1 unit, and pass 1 as number of units to read to fread
                Since fread returns the number of UNITS successfully read, it can either be 1 or 0, making it easy to check the 
                success 
            */

            if(Result == 1){
                CountBytes(Tester, Dptr.Count);
            }
            else{
                Error(Tester, "Unable to read file with fread");
            }
            fclose(F);          
        } 
        else{
            fprintf(stderr, "Unable to open file with fopen\n");
        } 
    }
}

/*
    Rookie mistake I made : ASSUMING that if read fails to read all the bytes requested
    in one go, its a guaranteed fail. That's not always true as it could have been interrupted
    or cut short due to other kernel level details.
    if after hitting EOF, the next Read returns a 0, why not use THAT as a condition
    inside my while loop? - because it skews the results of the timer by including the cost
    of another read call which IS NOT necessary to actually read the file.
*/
static void ReadWithSysRead(repetition_tester *Tester, read_params *Param){
    
    while(IsTesting(Tester)){
        b32 Fd = open(Param->FileName, O_RDONLY);
        if(Fd != -1){
            buffer Dptr = Param->Dest;
            u64 BytesToRead = Dptr.Count;
            u64 BufferLoc = 0;
            while(BytesToRead){

                BeginTime(Tester);
                ssize_t Result = read(Fd, Dptr.Data + BufferLoc, BytesToRead);
                EndTime(Tester);

                if(Result == -1){
                    Error(Tester, "Unable to read file with read system call");
                    break;
                }
                else{
                    CountBytes(Tester, Result);
                    BufferLoc += Result;
                    BytesToRead -= Result;
                }
            }
            close(Fd);     
        }
        else{
            fprintf(stderr, "Unable to open file with fopen\n");
        }    
    }

}