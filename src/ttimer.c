/*
 * Copyright (c) 2016 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Use is subject to license terms, as specified in the LICENSE file.
 */

/*
 * Hierarchical timing wheel.  An efficient mechanism to implement
 * timers with frequent timeouts.  The mechanism has amortised O(1)
 * complexity for all operations (start/stop/tick).
 *
 * NOTE: This is a tick-based mechanism and the accuracy, as well
 * as the granularity, depends on the tick period.  Typically, we
 * will use an approximate 1 second tick.
 *
 * Reference:
 *
 *	G. Varghese, A. Lauck, Hashed and hierarchical timing wheels:
 *	efficient data structures for implementing a timer facility,
 *	IEEE/ACM Transactions on Networking, Vol. 5, No. 6, Dec 1997
 *
 * http://www.cs.columbia.edu/~nahum/w6998/papers/ton97-timing-wheels.pdf
 */

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "ttimer.h"
#include "utils.h"

/*
 * Each timing wheel in the hierarchy will be of equal size: 256 slots.
 * It is a convenient number for calculations.  Three levels of 256-slot
 * wheels is 256^3 = ~194 days.  Just cap the maximum level at 3 and let
 * the re-calculation happen for the greater values.
 */

#define	WHEEL_BUCKETS		(256)
#define	WHEEL_MAX_LEVELS	(3)
#define	DIV_BY_BUCKETS(x)	((x) >> 8)
#define	MOD_BY_BUCKETS(x)	((x) & 0xff)

typedef struct {
	unsigned		hand;
	LIST_HEAD(,ttimer_ref)	bucket[WHEEL_BUCKETS];
} twheel_t;

struct ttimer {
	unsigned		levels;
	time_t			lastrun;
	twheel_t		wheel[];
};

ttimer_t *
ttimer_create(time_t maxtimeout, time_t now)
{
	unsigned len, levels = 0;
	ttimer_t *timer;

	while (maxtimeout > 0) {
		maxtimeout = DIV_BY_BUCKETS(maxtimeout);
		levels++;
	}
	levels = levels ? MIN(levels, WHEEL_MAX_LEVELS) : WHEEL_MAX_LEVELS;
	len = offsetof(ttimer_t, wheel[levels]);

	if ((timer = calloc(1, len)) == NULL) {
		return NULL;
	}
	timer->levels = levels;
	timer->lastrun = now;
	return timer;
}

void
ttimer_destroy(ttimer_t *timer)
{
	free(timer);
}

void
ttimer_setfunc(ttimer_ref_t *ent, ttimer_func_t handler, void *arg)
{
	ent->scheduled = false;
	ent->func = handler;
	ent->arg = arg;
}

void
ttimer_start(ttimer_t *timer, ttimer_ref_t *ent, time_t timeout)
{
	unsigned level, r, multiplier;
	twheel_t *wheel;

	ASSERT(timeout > 0);
	ASSERT(!ent->scheduled);
	ASSERT(ent->func != NULL);

	/*
	 * The algorithm to find the target bucket and the remaining
	 * time is conceptually the same with the digital clock time.
	 *
	 * As an example, let the current time be 22:58:57 i.e. 2 min
	 * and 3 sec before midnight.  Now consider adding 192 seconds
	 * to it.  It would result in 23:02:09.  The logic being:
	 *
	 * L0 (seconds): (57 + 192) div 60 = 4 rem 9
	 * L1 (minutes): (58 + 4) div 60 = 1 rem 2
	 * L2 (hours): (22 + 1) div 24 = 0 rem 23
	 *
	 * Hence, the level to insert is L2 (because we reached zero)
	 * and the bucket to insert is 23 (the remainder).  The remaining
	 * time is a sum of the previous remainders: 9 + (2 * 60) = 129.
	 * In other words, once the clock will reach 23:00:00, there will
	 * be 129 seconds remaining until 23:02:09.
	 *
	 * Our timing wheel has 256 units, therefore the time before
	 * "midnight" is 255:255:255 and we divide and modulus by 256.
	 */

	level = 0;
	ent->remaining = 0;
	multiplier = 1;
next:
	wheel = &timer->wheel[level];
	r = MOD_BY_BUCKETS(wheel->hand + timeout);
	timeout = DIV_BY_BUCKETS(wheel->hand + timeout);

	/* Switch to the next level if time exceeds the level. */
	if (__predict_false(timeout > 0)) {
		ent->remaining += (r * multiplier);
		multiplier *= WHEEL_BUCKETS;
		level++;

		if (__predict_true(level < WHEEL_MAX_LEVELS)) {
			goto next;
		}

		/*
		 * If we reach the final level, just add the whole
		 * remaining time.
		 */
		ent->remaining += multiplier * (timeout - 1);
		level--;
	}

	/*
	 * Insert the entry into the wheel bucket and mark as scheduled.
	 */
	wheel = &timer->wheel[level];
	LIST_INSERT_HEAD(&wheel->bucket[r], ent, entry);
	ent->scheduled = true;
}

bool
ttimer_stop(ttimer_t *timer, ttimer_ref_t *ent)
{
	bool stop = ent->scheduled;

	if (stop) {
		LIST_REMOVE(ent, entry);
		ent->scheduled = false;
	}
	(void)timer;
	return stop;
}

/*
 * ttimer_tick: process any expired events for the given time value.
 */
void
ttimer_tick(ttimer_t *timer)
{
	unsigned ntimeouts = 0, level = 0, n;
	ttimer_ref_t *ent;
	twheel_t *wheel;

	/*
	 * Process the first level in the hierarchy.  We will process
	 * the next level if the whole level was processed.
	 */
next:
	wheel = &timer->wheel[level];
	n = MOD_BY_BUCKETS(wheel->hand + 1);
	while ((ent = LIST_FIRST(&wheel->bucket[n])) != NULL) {
		time_t remaining = ent->remaining;

		/*
		 * Remove the timer entry and:
		 * - If there is remaining time, re-schedule it.
		 * - Otherwise, run the callback function.
		 */
		ASSERT(ent->scheduled);
		LIST_REMOVE(ent, entry);
		ent->scheduled = false;

		if (remaining) {
			ttimer_start(timer, ent, remaining);
			continue;
		}
		ASSERT(ent->func != NULL);
		ent->func(ent, ent->arg);
		ntimeouts++;
	}
	wheel->hand = n;

	/*
	 * Completed processing the level?  Process the next one.
	 */
	if (n == 0 && ++level < timer->levels) {
		goto next;
	}
}

/*
 * ttimer_run_ticks: run the tick for the current time ("now"),
 * including any previously missed ticks since the last run.
 */
void
ttimer_run_ticks(ttimer_t *timer, time_t now)
{
	while (timer->lastrun < now) {
		ttimer_tick(timer);
		timer->lastrun++;
	}
	timer->lastrun = now;
}
