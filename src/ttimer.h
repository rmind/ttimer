/*
 * Copyright (c) 2016 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#ifndef	_TTIMER_H_
#define	_TTIMER_H_

__BEGIN_DECLS

struct ttimer_ref;
typedef struct ttimer ttimer_t;
typedef void (*ttimer_func_t)(struct ttimer_ref *, void *);

typedef struct ttimer_ref {
	/* Private members: */
	LIST_ENTRY(ttimer_ref)	entry;
	time_t			remaining;
	ttimer_func_t		func;
	void *			arg;
	bool			scheduled;
} ttimer_ref_t;

ttimer_t *	ttimer_create(time_t, time_t);
void		ttimer_destroy(ttimer_t *);

void		ttimer_setfunc(ttimer_ref_t *, ttimer_func_t, void *);
void		ttimer_start(ttimer_t *, ttimer_ref_t *, time_t);
bool		ttimer_stop(ttimer_t *, ttimer_ref_t *);
void		ttimer_run_ticks(ttimer_t *, time_t);
void		ttimer_tick(ttimer_t *);

#endif
