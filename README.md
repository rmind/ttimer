# Tick-based timer mechanism

[![Build Status](https://travis-ci.org/rmind/ttimer.svg?branch=master)](https://travis-ci.org/rmind/ttimer)

Tick-based timer implemented using the hierarchical timing wheel algorithm.
It has amortised O(1) time complexity for all operations (start/stop/tick).
The implementation is written in C99 and distributed under the 2-clause BSD
license.

Reference:

	G. Varghese, A. Lauck, Hashed and hierarchical timing wheels:
	efficient data structures for implementing a timer facility,
	IEEE/ACM Transactions on Networking, Vol. 5, No. 6, Dec 1997
	http://www.cs.columbia.edu/~nahum/w6998/papers/ton97-timing-wheels.pdf

## API

* `ttimer_t *ttimer_create(time_t maxtimeout, time_t now)`
  * Construct a new timer object.  The `maxtimeout` parameter specifies the
  maximum timeout value which can be used with this timer.  Zero should be
  used if there is no limit.  However, tuning this value to be as small as
  possible would allow the implementation to be more efficient.  The `now`
  parameter indicates the initial time value, e.g. `time(NULL)`.  Returns
  the timer object on success and `NULL` on failure.

* `void ttimer_destroy(ttimer_t *timer)`
  * Destroy the timer object.

* `void ttimer_setfunc(ttimer_ref_t *entry, ttimer_func_t handler, void *arg)`
  * Setup the timer entry and set a handler function with an arbitrary
  argument.  This function will be called on timeout event.  The timer entry
  structure `ttimer_ref_t` is typically embedded in the object associated
  with the timer event.  It must be treated as an opaque structure.
  * This function should only be called when the timer entry is not
  activated i.e. before invoking the `ttimer_start()`.

* `void ttimer_start(ttimer_t *timer, ttimer_ref_t *entry, time_t timeout)`
  * Start the timer for a given entry with a specified `timeout` value.
  The handler function must be set with `ttimer_setfunc` before activating
  the timer for given entry.

* `bool ttimer_stop(ttimer_t *timer, ttimer_ref_t *enttry)`
  * Stop the timer for the given timer entry.  It may also be called if
  the timer was not activated.  Returns `true` if the entry was activate
  (i.e. `ttimer_start()` was invoked) and `false` otherwise.
  * Stopping the timer will not reset the handler function set up by the
  `ttimer_setfunc()` call.  However, after the stop, the handler function
  may be changed if needed.

* `void ttimer_run_ticks(ttimer_t *timer, time_t now)`
  * Process all expired events and advance the "current time" up to the
  new time, specified by the `now` parameter.  Note that the processing
  includes any previously missed ticks since the last run.  This is the
  main "tick" operation which shall occur periodically.

## Notes

The timeout values would typically represent seconds.  However, other
time units can be used with the API as long as they can be represented by
the `time_t` type.  Internally, the mechanism does not assume UNIX time.

This is a tick-based mechanism and the accuracy, as well as the granularity,
depends on the tick period.  Depending on the use case case, for an optimal
tick rate, you might want to consider using the
[Nyquist frequency](https://en.wikipedia.org/wiki/Nyquist_frequency).
