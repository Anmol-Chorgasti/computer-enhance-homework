
enum testmode : u32 {
    TestMode_Uninitialized,
    TestMode_Testing,
    TestMode_Completed,
    TestMode_Error,
};

/* The aggregation of all test runs a single rep tester makes */
struct repetition_test_results {
    u64 TestCount; // number of tests ran
    u64 TotalTime; // Not to be confused with TRY for time, this is the actual time elapsed
    f64 MinTime;
    f64 MaxTime;
};

struct repetition_tester {
    testmode TestStatus;
    u64 CPUTimerFreq; // why not global? by making it local to each tester, we get flexibility to use different cputimers

    u64 TargetProcessedByteCount; // bytes expected to be read
    u64 BytesProcessedCount; // bytes actually read

    u64 TestStartTime;
    u64 TryForTime; // how long should the tests run for?
    u64 TestTime; // what we actually measure PER test

    u64 OpenBlockCount; // These tests will be used on blocks we intend to time
    u64 CloseBlockCount;

    b32 PrintNewMins; 
    // This should not be a default assumption, rather it should be set the programmer
    // sometimes we desire to see what the new min is and sometimes we don't
     
    repetition_test_results Results;
};

static f64 TimeInSeconds(u64 CPUTimerFrequency, f64 Time){
    f64 Result = 0.0;

    if(CPUTimerFrequency){
        Result = Time/(f64)CPUTimerFrequency;
    }
    return Result;
}


static void PrintTime(char const *Name, u64 ElapsedTicks, u64 CPUTimerFrequency, u64 ByteCount){
    printf("%s : %lu", Name, ElapsedTicks);

    // ONLY if cputimerfrequency is passed should we print out the wallclock times
    // it can also be that a user has just passed in 0 for CPUTimerFrequency, signalling they
    // only want the raw elapsedticks value
    if(CPUTimerFrequency){
        f64 Seconds = TimeInSeconds(CPUTimerFrequency, (f64)ElapsedTicks);
        printf(" %.2f (seconds), %.2f (ms)", Seconds, 1000 * Seconds);

        // BandWidth
        if(ByteCount){
            f64 GB = (1024.0 * 1024.0 * 1024.0);
            f64 GBPerSec = ByteCount / (GB * Seconds);
            printf(" Bandwidth: %.2f GB/s", GBPerSec);
        }
    }
}  

/*
    When should this print? ONLY if testmode is completed
    Buf if a test is running infinitely?
    Then it would never be set to completed - so this is not a necessary test condition
*/
static void PrintResults(repetition_test_results *Results, u64 CPUFreq, u64 ByteCount){
    PrintTime("Min", Results->MinTime, CPUFreq, ByteCount);
    printf("\n");

    PrintTime("Max", Results->MaxTime, CPUFreq, ByteCount);
    printf("\n");

    if(Results->TestCount){
        f64 AverageTime = Results->TotalTime/(f64)Results->TestCount;

        PrintTime("Average", AverageTime, CPUFreq, ByteCount);
        printf("\n");
    }
}

static void BeginTime(repetition_tester *Tester){
    Tester->OpenBlockCount += 1;
    Tester->TestTime -= ReadCPUTimer();
}

static void EndTime(repetition_tester *Tester){
    Tester->CloseBlockCount += 1;
    Tester->TestTime += ReadCPUTimer();
}

static void CountBytes(repetition_tester *Tester, u64 ByteCount){
    Tester->BytesProcessedCount += ByteCount;
}

static void Error(repetition_tester *Tester, char const *Message){
    Tester->TestStatus = TestMode_Error;
    fprintf(stderr, "ERROR: %s\n", Message);
}

/*
    Rookie mistake I made - thinking this can be called only when Status is Uninitialized
    A completed Test Status can also be reset but we then need to flag if the stats have changed
    such as TargetByteCounts or CPUFreq
*/
static void InitializeTester(repetition_tester *Tester ,u64 CPUFreq, u64 TargetBytesCount = 0, u64 TimeToRun = 10){

    if(Tester->TestStatus == TestMode_Uninitialized){
        Tester->CPUTimerFreq = CPUFreq;
        Tester->TargetProcessedByteCount = TargetBytesCount;
        Tester->TestStatus = TestMode_Testing;
        Tester->PrintNewMins = true;
        Tester->Results.MinTime = (u64)-1;
    }
    else if(Tester->TestStatus == TestMode_Completed){
        Tester->TestStatus = TestMode_Testing;

        if(Tester->CPUTimerFreq != CPUFreq)
            Error(Tester, "CPU Frequency changed");

        if(Tester->TargetProcessedByteCount != TargetBytesCount)
            Error(Tester, "Bytes to process count changed");
    }

    Tester->TryForTime = TimeToRun * CPUFreq;
    Tester->TestStartTime = ReadCPUTimer();
}

/*
    What did I miss? the testing testmode check, none of the operations should happen
    if at entry we aren't even in testing testmode

*/

static b32 IsTesting(repetition_tester *Tester){

    if(Tester->TestStatus == TestMode_Testing){
        // There needs to be an openblockcount test here otherwise the bytesprocessedcount will always fail
        u64 CurrentTime = ReadCPUTimer();
        u64 ElapsedTestTime = CurrentTime - Tester->TestStartTime;

        if(Tester->OpenBlockCount){

            if(Tester->BytesProcessedCount != Tester->TargetProcessedByteCount){
                Error(Tester, "Bytes read do not equal target bytes count");
            }
            
            if(Tester->OpenBlockCount != Tester->CloseBlockCount)
                Error(Tester, "Open and Close blocks don't match");

            if(Tester->TestStatus == TestMode_Testing){ 

                Tester->Results.TotalTime += Tester->TestTime;                   
                Tester->TestStatus = TestMode_Testing;
                repetition_test_results *Rptr = &Tester->Results;
                Rptr->TestCount += 1;

                // Update Results
                if(Tester->TestTime > Rptr->MaxTime)
                    Rptr->MaxTime = Tester->TestTime;
                
                if(Tester->TestTime < Rptr->MinTime){
                    Rptr->MinTime = Tester->TestTime;

                    // Reset test
                    Tester->TestStartTime = CurrentTime;
                    
                    if(Tester->PrintNewMins){
                        PrintTime("Min", Tester->TestTime, Tester->CPUTimerFreq, Tester->BytesProcessedCount);
                        printf("\n\n");
                    }
                }  

                Tester->BytesProcessedCount = Tester->TestTime = 0;
                Tester->OpenBlockCount = Tester->CloseBlockCount = 0;
            }   
        }
       
        if(ElapsedTestTime >= Tester->TryForTime){
            Tester->TestStatus = TestMode_Completed;
            PrintResults(&Tester->Results, Tester->CPUTimerFreq, Tester->TargetProcessedByteCount);

        }
    }

    return (Tester->TestStatus == TestMode_Testing);  
}






