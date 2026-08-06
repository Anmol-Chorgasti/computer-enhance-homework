
# High Level Mental Model of `clock_gettime` and `RDTSC(P)`

*Learnings from doing the Part 2 homework of the Performance Aware Programming Course by Casey Muratori.*

## Motivation

The main goal of this exercise is to understand how to get close enough wall-clock / human intuition level times when measuring the performance of a naive, unoptimized JSON Parser that can parse a JSON file containing 10 million pairs of latitude, longitude coordinates. 

At this stage, my knowledge is limited to the two common functions used for "measuring" SOMETHING on Linux: `clock_gettime()` and `rdtsc`. The text below contains my findings and thoughts on these functions. They are not guaranteed to be correct, but rather serve as a baseline for me to refer to when and if I need to in the future.

The measurements themselves, however, are not exact. I cannot claim that they are exact numbers as there is still a lot about performance and other hidden levers I am currently unaware of. At best, the measurements made by me are "trending" in the right direction.

## The Core Problem

To estimate exact time, I could use `clock_gettime`, take the difference, and get the estimates. Watching Casey Muratori's *Performance Aware Programming* series showed me, however, that if possible, rely on `rdtsc` at least at the beginning to get more accurate estimates. My hunch on the reason behind this recommendation was that `clock_gettime` is slower than `rdtsc`. (There is much more to this as I later found out.)

However, the problem I faced was that `rdtsc` does not give me a time value back. So then how do I go about measuring the time while relying on `rdtsc`?

## Theory I Learnt

Frequency would solve the problem. If `rdtsc` gives me back the value of some crystal X, and I had the frequency of X, I could take the difference of the values of X at different points in time, divide the difference by the frequency of X, and get back the time value. Unfortunately, it is not easy, or maybe even impossible, to GET the frequency of X directly.

All hope is not lost, though, as there is ANOTHER crystal Y whose frequency is known. 
So to get the frequency of X, maybe I can run Y until it meets some threshold, see how many times X ran in that period of time, and get its frequency then.

### For Example:
I know A runs at a constant speed of 10 meters per second, but I don't know B's speed. However, I do KNOW that B runs at a constant speed (crazy control).
I can let A AND B BOTH run UNTIL A covers 100 meters. 
I know for a fact that A and B have run for exactly 10 seconds, since I know A's speed, so I can just use that time (which I now know) and the distance covered by B (which I also KNOW) to get the frequency of B!

The question I had here was... wait, why care so much for B, or in this case crystal X? If I have Y's frequency, why can't I get ITS values and then just divide by its frequency and get the time?

Turns out, there are tradeoffs:
1. **Location:** The location of Y is farther away from the CPU (it's on the motherboard) than X (which is way closer to the CPU). This would make calls to get the value of Y slower, potentially inflating the timing measurements. This inflation would then scale proportionally with the number of calls made.
2. **Granularity:** The granularity is lost. If X is faster than Y, we end up losing some information (for example, we may have to give up on nanosecond level granularity and instead settle for microsecond granularity).

As an aside, turns out that Linux DOES fall back on crystal Y when X is behaving funky—though I do not know what qualifies as funky behavior yet.

So, we need to rely on X, a.k.a. the timing crystal measured by the `rdtsc` call.

### Approximating Crystal X's Frequency

Enter `clock_gettime` and why it's the savior.

`clock_gettime` takes in parameters that allow callers to specify what time they actually want. For my purpose, `CLOCK_MONOTONIC_RAW` was the best pick as it is the most consistent. (Asking AI for the reason revealed a few things, such as a Network Daemon not being able to manipulate `MONOTONIC_RAW`. However, I don't know what that daemon is or why it would even bother to change clock values. For the purpose of this exercise, I abstracted away the reason and took it on good faith that `CLOCK_MONOTONIC_RAW` is stable and what I need, though I expect to lose myself in this rabbit hole later.)

A simple way to calculate the frequency of X is if I pass in the time period and call `rdtsc` at the start and end of this period, then the frequency is easily found. The main goal of `clock_gettime` is to then ensure that the time period set by me has indeed passed.

The logic used was:
* `rdtsc_start = rdtsc()`
* `start_time = clock_gettime()`
* Wait until set time (set by me) passes—found by calling `clock_gettime` on repeat and comparing returned value versus the `start_time`
* `end_time = clock_gettime()`
* `rdtsc_end = rdtsc()`

And now we have the frequency as `(rdtsc_end - rdtsc_start) / (set time passed by user)`. Hence, the main purpose of `clock_gettime` was to simply ensure that the time passed in BY the user was actually the time gap between the two `rdtsc` calls.

The frequency measured, however, IS NOT an exact value. One possible reason is the overhead from calling `clock_gettime()` itself, which COULD BE slower than `rdtsc`. Chances are very high that `RDTSC` ran LONGER than the set time by a small margin, which would inflate its calculated frequency. However, given that the cost of calling `clock_gettime` does not increase with an increase in the time gap, we can say that the larger the time gap, the better our chances to reduce this inflation.

In my `time_stats` program, I always saw values near 2.995 GHz as the frequency when I calculated it over a time gap of 5 seconds. Interestingly, this is very close to my AMD chip's base clock rate, which is 3 GHz.

Great, so we have the frequency of X and I don't need the heavier, slower `clock_gettime` calls anymore. I can call `rdtsc` and divide the difference by the frequency and get the time values in my desired unit.

SO, how did `clock_gettime()` do it, though? How is it able to get me the time in nanoseconds? It MUST access something on the hardware to get the time. What if I could reduce the overhead from calling it by calling that hardware entity directly, and IS IT slower than my `rdtsc()` calls?

## Peeking into `clock_gettime`

Everything that follows below is my assumptions / learnings from being lost in Assembly land as I TRIED to chase the assembly instructions for `clock_gettime()`.

My initial hunch was that `clock_gettime` is going to crystal Y (HPET as it is called on most systems) and getting its values. Since its frequency is known, we can just take the value, divide it by the known frequency, get the time, and maybe because Y is farther away from the CPU, `clock_gettime` is slower. Well, looking at the assembly proved that to be wrong (unless, of course, X is funky, in which case my hunch is then right).

On Linux, it takes a couple of jumps to get to the actual area where `clock_gettime`'s internals are exposed. It turns out these assembly instructions are sitting in some read-only section of memory (called vDSO... yet another rabbit hole I must explore). The BIGGEST surprise here, however, was that I saw a `rdtscp()` call being made.

*( `rdtsc` vs `rdtscp` — For the sake of conciseness, consider that `rdtscp` is the slightly slower cousin of `rdtsc`. They both read X, but `rdtscp` does a few things MORE. It stores the 'core number' of the crystal it read, and also has more protection against certain CPU mechanisms, which I am currently unaware of. For context — all CPU cores have their own crystal X, but the values across these crystals should ideally be synchronized and the same!)*

So, `clock_gettime()` gets us the time values... by reading crystal X? However, this whole issue started because we could not GET the time value from calling `rdtsc`, which also reads crystal X?

It is safe to assume then that `clock_gettime()` does some extra steps which CONVERT the reading from crystal X into a time value. The neighboring assembly instructions around the `rdtscp` call confirmed that hunch, though I was not able to figure out exactly what those extra steps are.

I could see that we were loading a constant in from some memory location, doing a multiplication, and also a right shift by some value held in the low 8 bits of a register. Watching Casey Muratori walk through how `QueryPerformanceCounter` (Windows' `clock_gettime()` version, I suppose) has a similar behavior made me guess that we are actually multiplying the value from crystal X with SOME ratio, but what is that ratio, who calculated it, and why do I know that it's right?

At this point, I was stuck and had to go to my beloved LLM friend and ask for what that ratio could be.

*(The below points are what I took away from my conversation with an LLM; I DID NOT verify them myself.)*

Turns out, at boot time, the kernel does a few things to store said ratio. Let's call it Z. Y and X start running at the same time and they are both paused when Y hits a certain threshold (just like in the running example of A and B... sorry for all the alphabets).

We check the value of X and the value of Y, and the magic ratio Z is then `(Val of Y) / (Val of X)`. It tells us: how much of X can I fit into Y, or staying with our running analogy, for every 1 meter covered by Y, how many meters did X cover?

Now if we know this ratio, we have a multiplier that we can use to convert X's value to Y's value! If I say, *"Hey, B ran 200 meters. If A was running, how much do you think A would have run for?"* I know that for every meter A covers, B covers two, so it must be that A has probably covered half the distance—so A's value is probably 100 meters.

Now we have Y's value and its frequency; therefore, we have a way to get the time. That's what `clock_gettime` does for us MATHEMATICALLY. It takes X's value, uses the magic ratio Z that the kernel was kind enough to store at some memory location for us, converts X's value to Y's value, uses Y's known frequency, and gives us back the time! Though in reality multiplying by the magic ratio itself gets us the nanoseconds value.

Assuming at the start the threshold the HPET is run for is EQUAL to its known Frequency.
RDTSC Val x (HPET Frequency)/(TSC Ticks) x (1 Ghz ns)/(HPET Frequency). You see how the HPET frequency gets cancelled out?
so our magic ratio just becomes (1 GHz ns)/(TSC Ticks). So in theory, the conversion could happen just by multiplying the RDTSC Value with this ratio.

This adventure also confirms that `clock_gettime` is slower than `rdtsc` and `rdtscp` because it is literally a function that CALLS `rdtscp` AND does some extra conversion and storing work. (Quite interesting to note, however, that this approach was still considered more efficient than going out to Y and getting its value and converting that to a time value instead, though I EXPECT the granularity had a role to play here as well.)

So we use it once to help us get the frequency of X, and then we ideally never have to call it again and can make do with `rdtsc` calls and the known frequency of X.