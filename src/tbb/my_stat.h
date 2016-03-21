#ifndef MY_STAT_H
#define MY_STAT_H

enum event_t {
	STAT_NOOP  = 0,
	STAT_PUSH  = 1,
	STAT_POP   = 2,
	STAT_STEAL = 3
};

void init_stat_thread();

void gather_stat(event_t e);

void start_watchdog();
void stop_watchdog();

#endif /* end of include guard: MY_STAT_H */

