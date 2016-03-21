#include "my_stat.h"
#include <iostream>
#include <ctime>
#include <thread>
#include <iomanip>

#include "tbb/atomic.h"
#include "tbb/enumerable_thread_specific.h"

#define MAX_THREADS 8

struct stat_t
{
    event_t e;

    uint32_t push_cnt;
    uint32_t pop_cnt;
    uint32_t steal_cnt;
} stat[MAX_THREADS];

tbb::atomic<int> stat_size(0);
thread_local int me;

tbb::tbb_thread watchdog_handle;

event_t dx[] = {
    STAT_PUSH,
    STAT_PUSH,
    STAT_NOOP,
    STAT_POP,
    STAT_POP,
    STAT_POP,
    STAT_NOOP,
    STAT_PUSH,
    STAT_NOOP
};

event_t dy[] = {
    STAT_NOOP,
    STAT_POP,
    STAT_POP,
    STAT_POP,
    STAT_NOOP,
    STAT_PUSH,
    STAT_PUSH,
    STAT_PUSH,
    STAT_NOOP
};

uint32_t total[9];
uint64_t iterations = 0;

bool watchdog_stop;

void watchdog_exec()
{
    stat_t cur[2];

    while (!watchdog_stop) {
        cur[0] = stat[0];
        cur[1] = stat[1];

        for (int i = 0; i < 9; i++)
            if (cur[0].e == dx[i] && cur[1].e == dy[i])
                total[i]++;

        iterations++;
    }
}

void start_watchdog()
{
    watchdog_handle = tbb::tbb_thread(watchdog_exec);
}

void stop_watchdog()
{
    watchdog_stop = true;
    watchdog_handle.join();

    std::cerr << "\n\n====================== Deques Stats =====================\n";

    uint64_t sum = 0;
    for (int i = 0; i < 9; i++)
        sum += total[i];

    double prob[9];
    for (int i = 0; i < 9; i++)
        prob[i] = (double) total[i] / sum;

    std::cerr << "\n";
    std::cerr << "iterations: " << iterations << "\n";

    long cnt[2];
    cnt[0] = stat[0].push_cnt + stat[0].pop_cnt + stat[0].steal_cnt;
    cnt[1] = stat[1].push_cnt + stat[1].pop_cnt + stat[1].steal_cnt;
    std::cerr << "iterations per operation:\nth0: "
        << (double) iterations / cnt[0] << "  th1: " 
        << (double) iterations / cnt[1] << "\n";

    std::cerr << "\n";
    std::cerr.precision(15);
    std::cerr << "push-noop (p1):  \t" << std::setw(11) << total[0] << "\t" << std::fixed << prob[0] << "\n";
    std::cerr << "noop-push (p2):  \t" << std::setw(11) << total[6] << "\t" << std::fixed << prob[6] << "\n";
    std::cerr << "push-push (p12): \t" << std::setw(11) << total[7] << "\t" << std::fixed << prob[7] << "\n";
    std::cerr << "pop-noop (q1):   \t" << std::setw(11) << total[4] << "\t" << std::fixed << prob[4] << "\n";
    std::cerr << "noop-pop (q2):   \t" << std::setw(11) << total[2] << "\t" << std::fixed << prob[2] << "\n";
    std::cerr << "pop-pop (q12):   \t" << std::setw(11) << total[3] << "\t" << std::fixed << prob[3] << "\n";
    std::cerr << "push-pop (pq21): \t" << std::setw(11) << total[1] << "\t" << std::fixed << prob[1] << "\n";
    std::cerr << "pop-push (qp12): \t" << std::setw(11) << total[5] << "\t" << std::fixed << prob[5] << "\n";
    std::cerr << "noop-noop (r):   \t" << std::setw(11) << total[8] << "\t" << std::fixed << prob[8] << "\n";

    std::cerr << "\n";
    std::cerr << "Thread 0: \n";
    std::cerr << "push: \t"  << stat[0].push_cnt  << "\n";
    std::cerr << "pop: \t"   << stat[0].pop_cnt   << "\n";
    std::cerr << "steal: \t" << stat[0].steal_cnt << "\n";
    std::cerr << "\n";
    std::cerr << "Thread 1: \n";
    std::cerr << "push: \t"  << stat[1].push_cnt  << "\n";
    std::cerr << "pop: \t"   << stat[1].pop_cnt   << "\n";
    std::cerr << "steal: \t" << stat[1].steal_cnt << "\n";
    std::cerr << "\n";
    std::cerr << "Total: \n";
    std::cerr << "push: \t"  << stat[0].push_cnt + stat[1].push_cnt   << "\n";
    std::cerr << "pop: \t"   << stat[0].pop_cnt + stat[1].pop_cnt     << "\n";
    std::cerr << "steal: \t" << stat[0].steal_cnt + stat[1].steal_cnt << "\n";
    std::cerr << "\n";
}

void gather_stat(event_t e)
{

    stat[me].e = e;

    if (e == STAT_PUSH)
        stat[me].push_cnt++;
    if (e == STAT_POP)
        stat[me].pop_cnt++;
    if (e == STAT_STEAL)
        stat[me].steal_cnt++;

    // nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
}

void init_stat_thread()
{
    int old_size;
    do {
        old_size = stat_size;
    } while(stat_size.compare_and_swap(old_size + 1, old_size) != old_size);

    if (old_size >= MAX_THREADS)
        std::cerr << "MAX_THREADS constant is readched\n";

    me = old_size;

    if (old_size == 0)
        start_watchdog();
}
