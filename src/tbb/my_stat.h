#ifndef MY_STAT_H
#define MY_STAT_H

enum event_t {
	STAT_NOOP  = 0,
	STAT_PUSH  = 1,
	STAT_POP   = 2,
	STAT_STEAL = 3
};

void start_watchdog();
void stop_watchdog();

void init_thread_stat1();
void init_thread_stat2();

void gather_stat(event_t e);

#endif /* end of include guard: MY_STAT_H */

