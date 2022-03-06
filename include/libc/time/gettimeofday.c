#include <stdint.h>
#include <sys/time.h>

#ifdef __P2__
#include <propeller2.h>
#else
#include <propeller.h>
extern uint32_t _getus();
#endif

// official time in "seconds" since the Unix Epoch
// this actually leaves out leap seconds, which is a big can of worms
// we're ignoring for now
static uint32_t lastsec = 1_000_000_000; // Sept. 9, 2001

// microsecond counter reading at last second
static uint32_t lastus = 0;

int gettimeofday(struct timeval *tv, void *unused)
{
    uint32_t now = _getus();  // current microsecond count
    uint32_t then = lastus;
    uint32_t elapsed = now - then;
    uint32_t secs, leftover;

    secs = elapsed / 1000000;
    leftover = elapsed % 1000000;
    if (secs) {
        lastsec += secs;
        lastus = now - leftover;
    }
    tv->tv_sec = lastsec;
    tv->tv_usec = leftover;
    return 0;
}

int
settimeofday(const struct timeval *tv, const void *unused)
{
    uint32_t now = _getus();
    lastsec = tv->tv_sec;
    lastus = now - tv->tv_usec;
    return 0;
}
