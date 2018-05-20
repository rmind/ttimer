/*
 * Copyright (c) 2016 Mindaugas Rasiukevicius <rmind at netbsd org>
 * All rights reserved.
 *
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <assert.h>

#include "ttimer.h"

static unsigned gotval = 0;
static unsigned setval = 0;

static void
timeout_handler(ttimer_ref_t *ent, void *arg)
{
	gotval = *(unsigned *)arg;
	(void)ent;
}

static ttimer_t *
ttimer_setup(time_t maxtimeout, ttimer_ref_t *ent)
{
	ttimer_t *timer;

	/* On timeout: gotval = setval. */
	ttimer_setfunc(ent, timeout_handler, &setval);
	timer = ttimer_create(maxtimeout, time(NULL));
	assert(timer);
	return timer;
}

static void
ttimer_basic(void)
{
	ttimer_t *timer;
	ttimer_ref_t ent;
	unsigned steps;

	/* 2 levels, basic test */
	timer = ttimer_setup(512, &ent);

	/* Timer after 1 step ("second"). */
	gotval = 0, setval = 1;
	ttimer_start(timer, &ent, 1);

	/* Tick and check. */
	ttimer_tick(timer);
	assert(gotval == 1);
	assert(!ent.scheduled);

	/* Timer after 255 steps (boundary check). */
	gotval = 0, setval = 2, steps = 255;
	ttimer_start(timer, &ent, steps);
	for (unsigned i = 0; i < steps ; i++) {
		assert(gotval == 0);
		assert(ent.scheduled);
		ttimer_tick(timer);
	}
	assert(gotval == 2);

	/* After 256 steps (wrap around check). */
	gotval = 0, setval = 2, steps = 256;
	ttimer_start(timer, &ent, steps);
	for (unsigned i = 0; i < steps; i++) {
		assert(gotval == 0);
		ttimer_tick(timer);
	}
	assert(gotval == 2);

	ttimer_destroy(timer);
}

static void
ttimer_overflow(void)
{
	ttimer_t *timer;
	ttimer_ref_t ent;
	unsigned steps;

	/* 3 levels, exceeding. */
	timer = ttimer_setup(256UL * 256 * 256 * 256, &ent);

	/* After 256^2 + 17 steps: pass two levels. */
	gotval = 0, setval = 2, steps = 256UL * 256 + 17;
	ttimer_start(timer, &ent, steps);
	for (unsigned i = 0; i < steps; i++) {
		assert(gotval == 0);
		ttimer_tick(timer);
	}
	assert(gotval == 2);

	/* After 256^3 + 19 steps: exceed the maximum size. */
	gotval = 0, setval = 2, steps = (256UL * 256 * 256) + 19;
	ttimer_start(timer, &ent, steps);
	for (unsigned i = 0; i < steps; i++) {
		assert(gotval == 0);
		ttimer_tick(timer);
	}
	assert(gotval == 2);

	ttimer_destroy(timer);
}

static void
ttimer_random(void)
{
	const unsigned maxt = 256 * 256;
	ttimer_t *timer;
	ttimer_ref_t ent;
	unsigned steps;

	timer = ttimer_setup(maxt, &ent);
	for (unsigned n = 0; n < 10000; n++) {
		gotval = 0, setval = 2, steps = (random() % maxt) + 1;
		ttimer_start(timer, &ent, steps);
		for (unsigned i = 0; i < steps; i++) {
			assert(gotval == 0);
			ttimer_tick(timer);
		}
		assert(gotval == 2);
	}
	ttimer_destroy(timer);
}

int
main(void)
{
	ttimer_basic();
	ttimer_overflow();
	ttimer_random();
	puts("ok");
	return 0;
}
