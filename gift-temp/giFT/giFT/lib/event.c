/*
 * $Id: event.c,v 1.59 2003/05/04 20:53:50 rossta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "gift.h"

#include "network.h"
#include "event.h"
#include "tcpc.h"

#include <errno.h>

#ifdef HAVE_POLL
# include <sys/poll.h>
#else

struct pollfd
{
	int   fd;
	short events;
	short revents;
};

# define POLLIN   0x0001
# define POLLOUT  0x0004
# define POLLERR  0x0008
# define POLLHUP  0x0010
# define POLLNVAL 0x0020

#endif /* HAVE_POLL */

/*****************************************************************************/

/**
 * File descriptor handler.
 */
typedef struct
{
	int           fd;                  /**< file descriptor */
	input_id      id;
	input_id      poll_id;
	InputState    set;

	InputCallback cb;
	void         *udata;               /**< arbitrary data associated with
	                                    *   this input fd */
	time_t        timeout;             /**< see ::input_add */
	timer_id      validate;            /**< timeout for incomplete sockets */
	unsigned char complete : 1;        /**< complete is FALSE until first
	                                    *   state change.  used for
	                                    *   automated connection timeouts. */
	unsigned char suspended : 1;       /**< suspended is TRUE when input
	                                    *   should not be put in select loop */
	signed char   dirty : 2;           /**< write queue */
} Input;

/**
 * Timer event.
 */
typedef struct timer
{
	timer_id       id;                 /**< reference identifier */

	struct timeval expiration;         /**< exact time this timer will
	                                    *   expire */
	struct timeval interval;           /**< interval this timer is set for */

	TimerCallback  cb;
	void          *udata;              /**< arbitrary user data */
} Timer;

/*****************************************************************************/
/* blatantly ripped from sys/time.h for reasons somewhat beyond me */

#define TIME_LT  <
#define TIME_GT  >
#define TIME_EQ  ==

#define TIME_SET(t,m)                                       \
	do                                                      \
	{                                                       \
		(t)->tv_sec = m / 1000;                             \
		(t)->tv_usec = (m % 1000) * 1000;                   \
	} while (0)

#define TIME_CMP(t,v,__OP__)                                \
	(((t)->tv_sec == (v)->tv_sec) ?                         \
	 ((t)->tv_usec __OP__ (v)->tv_usec) :                   \
	 ((t)->tv_sec __OP__ (v)->tv_sec))

#define TIME_ADD(t,v,r)                                     \
	do                                                      \
	{                                                       \
		(r)->tv_sec  = (t)->tv_sec + (v)->tv_sec;           \
		(r)->tv_usec = (t)->tv_usec + (v)->tv_usec;         \
		if ((r)->tv_usec >= 1000000)                        \
		{                                                   \
			++(r)->tv_sec;                                  \
			(r)->tv_usec -= 1000000;                        \
		}                                                   \
	} while (0)

#define TIME_ADDI(t,i,r)                                    \
	do                                                      \
	{                                                       \
		(r)->tv_sec  = (t)->tv_sec;                         \
		(r)->tv_usec = (t)->tv_usec + (i);                  \
		if ((r)->tv_usec >= 1000000)                        \
		{                                                   \
			(r)->tv_sec += (r)->tv_usec / 1000000;          \
			(r)->tv_usec %= 1000000;                        \
		}                                                   \
	} while (0)

#define TIME_SUB(t, v, r)                                   \
	do                                                      \
	{                                                       \
		(r)->tv_sec = (t)->tv_sec - (v)->tv_sec;            \
		(r)->tv_usec = (t)->tv_usec - (v)->tv_usec;         \
		if ((r)->tv_usec < 0) {                             \
			--(r)->tv_sec;                                  \
			(r)->tv_usec += 1000000;                        \
		}                                                   \
	} while (0)

#define TIME_CLEAR(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define TIME_ISSET(t) ((t)->tv_sec ? 1 : (t)->tv_usec)

/*****************************************************************************/

/* this is used for error reporting only */
#ifdef HAVE_POLL
# define POLLFN "poll"
#else
# define POLLFN "select"
#endif

/* if you feel like being brave, change to #if 1 */
#if 0
# define POLL_TRACE(args) TRACE(args)
#else
# define POLL_TRACE(args)
#endif

static volatile int  polling       = FALSE; /* keeps event_loop alive */

static input_id      input_ids     = 0;     /* number of inputs active */
static input_id      input_ids_max = 0;     /* absolute highest input id */
static input_id      poll_ids      = 0;     /* same as input_ids for now */
static timer_id      timer_ids     = 0;     /* number of timers active */

#define              MAX_INPUTS      4096   /* maximum number of inputs */

static struct pollfd poll_fds[MAX_INPUTS];  /* poll argument */
static Input         inputs  [MAX_INPUTS];  /* maintain all inputs */
static List         *timers        = NULL;  /* sorted by expiration */

static Array        *inputs_add    = NULL;  /* add queue */
static Array        *inputs_remove = NULL;  /* remove queue */

static Dataset      *fds           = NULL;  /* index of input ids by fd */


/*****************************************************************************/

/*
 * Perl representation of this data structure to ease understanding.
 *
 * my $fds = { fd3 => { id1 => 'id',
 *                      id2 => 'id',
 *                      id3 => 'id' },
 *             fd4 => { id4 => 'id' },
 *             fd5 => { id5 => 'id',
 *                      id6 => 'id' } };
 *
 * NOTE:
 *
 * We are using dynamic allocation for the fd index as it is faulty to assume
 * that file descriptors can be the index to a staticly sized array in a
 * portable fashion.
 */
static Dataset *get_fd_index (int fd)
{
	assert (fd >= 0);

	return dataset_lookup (fds, &fd, sizeof (fd));
}

static void foreach_fd_index (int fd, DatasetForeach func, void *udata)
{
	Dataset *ids;

	if (!(ids = get_fd_index (fd)))
		return;

	dataset_foreach (ids, func, udata);
}

static void add_fd_index (int fd, input_id id)
{
	Dataset *ids = NULL;

	if (!(ids = get_fd_index (fd)))
	{
		if (!(ids = dataset_new (DATASET_LIST)))
			return;

		if (!fds)
			fds = dataset_new (DATASET_HASH);

		dataset_insert (&fds, &fd, sizeof (fd), ids, 0);
	}

	dataset_insert (&ids, &id, sizeof (id), "id", 0);
}

static void del_fd_index (int fd, input_id *id)
{
	Dataset      *ids = NULL;
	unsigned long ids_left = 0;

	if (!(ids = get_fd_index (fd)))
		return;

	/*
	 * If a valid id was specified, remove exactly that id.  If this happened
	 * to be the last id left for this fd, remove the fd as well.  In the
	 * event that an invalid id is supplied, ids_left will be left
	 * initialized to 0 to indicate that the set should be cleared and
	 * removed regardless of remaining entries.
	 */
	if (id)
	{
		dataset_remove (ids, id, sizeof (*id));
		ids_left = dataset_length (ids);
	}

	if (ids_left == 0)
	{
		dataset_clear (ids);
		dataset_remove (fds, &fd, sizeof (fd));
	}
}

/*****************************************************************************/

/*
 * Having this functionality in event.c was a mistake, but I haven't had the
 * time/energy to move to tcpc.[ch] and deal with all the backwards
 * compatability issues.  Lots of code now depends on this, and not all of it
 * written by myself :(
 */
static int validate_timeout (Input *input)
{
	InputCallback cb;
	void         *udata;

	/* set this just in case this input manages to find its way back into
	 * some of the functions here while its being destroyed */
	input->complete = TRUE;
	input->validate = 0;

	/* the input is long since dead, but this timer is still around? */
	if (input->fd < 0)
	{
		TRACE (("%d, %d [%d]: FIXME", input->fd, input->id, input->validate));
		return FALSE;
	}

	/* required to call `cb' after input_remove_all */
	cb    = input->cb;
	udata = input->udata;

	/*
	 * Cleanup this descriptor and implicitly remove from the loop as a
	 * simple helper.  We then raise the callback to notify the caller that
	 * the descriptor has reached the maximum alloted time to respond.
	 */
	net_close (input->fd);
	input_remove_all (input->fd);

	/*
	 * Raise the requested callback with an invalid socket and input id, as
	 * they are both invalid now.  This is a safe way for the caller to
	 * differentiate between a timeout and some other socket error.
	 */
	cb (-1, 0, udata);

	/* do not renew the timer, it's meant to be a one-shot deal */
	return FALSE;
}

static input_id next_input_id (void)
{
	input_id id = input_ids;

	while (inputs[id].fd >= 0)
		id++;

	if (input_ids_max < id)
		input_ids_max = id;

	input_ids++;
	return id;
}

static input_id next_poll_id (void)
{
	return poll_ids++;
}

static void input_add_queue (Input *input)
{
	/* this input was removed before the dirty flag was unset, so simply
	 * ignore since we know input_remove_queue will tackle the rest */
	if (input->dirty < 0)
		return;

	assert (input->fd >= 0);
	assert (input->dirty > 0);
	assert (poll_fds[input->poll_id].fd == input->fd);

	input->dirty = 0;
}

static void set_pollfd (input_id pid, Input *input)
{
	struct pollfd *pfd;

	assert (input->poll_id == pid);
	pfd = &poll_fds[pid];

	pfd->fd      = input->fd;
	pfd->events  = 0;
	pfd->revents = 0;

	if (input->set & INPUT_READ)
		pfd->events |= POLLIN;

	if (input->set & INPUT_WRITE)
		pfd->events |= POLLOUT;

	if (input->set & INPUT_ERROR)
		pfd->events |= POLLERR;
}

input_id input_add (int fd, void *udata, InputState set, InputCallback cb,
                    time_t timeout)
{
	Input   *input;
	input_id id;
	input_id pid;

	if (fd < 0)
		return 0;

	id = next_input_id();
	pid = next_poll_id();

	POLL_TRACE (("%d => %d, fd = %d", id, pid, fd));

	input = &inputs[id];
	input->dirty   = 1;
	input->id      = id;
	input->poll_id = pid;
	input->fd      = fd;
	input->udata   = udata;
	input->set     = set;
	input->cb      = cb;
	input->timeout = timeout;

	/* temporary hack to ensure that i don't miss any updates to the new api */
	assert (timeout != 1);

	if (!timeout)
	{
		input->complete = TRUE;
		input->validate = 0;
	}
	else
	{
		input->complete = FALSE;
		input->validate =
		    timer_add (timeout, (TimerCallback)validate_timeout, input);
	}

	/* maintain an index of all identifiers for a specific file descriptor to
	 * prevent fatal errors when using the api */
	add_fd_index (input->fd, input->id);

	/* after the next pass to poll, iterate through this write queue and
	 * remove the dirty elements from this input and finish it's population
	 * into the poll_fds array */
	push (&inputs_add, input);

	/* this is somewhat dangerous to do while the input is still dirty, but
	 * it is required to keep up the efficiency improvements when removing
	 * inputs, even if they are dirty */
	set_pollfd (pid, input);

	/* zero is an invalid id as far as the interface is concerned, but as an
	 * implementation it is perfectly valid */
	return id + 1;
}

static int eq_input (Input *input, input_id poll_id)
{
	return (input->poll_id == poll_id);
}

static int match_input (Dataset *d, DatasetNode *node, input_id *poll_id)
{
	input_id *id_ptr = node->key;
	input_id  id     = *id_ptr;

	return eq_input (&inputs[id], *poll_id);
}

static Input *find_input (input_id poll_id, input_id *id_ret)
{
	Dataset     *ids;
	DatasetNode *node;
	input_id     id;

	if (!(ids = get_fd_index (poll_fds[poll_id].fd)))
	{
		TRACE (("unable to locate fd index %d", poll_fds[poll_id].fd));
		return NULL;
	}

	if (!(node = dataset_find_node (ids, DATASET_FOREACH(match_input), &poll_id)))
	{
		TRACE (("unable to locate id %d in fd index %d",
		        poll_id, poll_fds[poll_id].fd));
		return NULL;
	}

	id = *((input_id *)node->key);

	if (id_ret)
		*id_ret = id;

	return &inputs[id];
}

static void move_pollfd (input_id dst_id, input_id src_id)
{
	Input         *input;
	struct pollfd *dst = &poll_fds[dst_id];
	struct pollfd *src = &poll_fds[src_id];

	/* if the id's are identical, we still want to invalidate the source, but
	 * we can optimize away the lookup and writes */
	if (dst_id != src_id)
	{
		/* update the poll_id (used for unique correlation between inputs and
		 * pollfds) */
		if ((input = find_input (src_id, NULL)))
		{
			POLL_TRACE (("adjusting %d to pollid %d", input->id, dst_id));
			input->poll_id = dst_id;
		}

		/* move the memory */
		dst->fd      = src->fd;
		dst->events  = src->events;
		dst->revents = 0;
	}

#ifdef DEBUG
	memset (src, 0, sizeof (struct pollfd));
#endif /* DEBUG */

	/* nullify the old location */
	src->fd = -1;
}

static void remove_pollfd (input_id poll_id)
{
	assert (poll_id >= 0);
	assert (poll_id < poll_ids);

	/* swap the last entry in the poll_fds array with the one being deleted so
	 * that we do not need to rebuild the entire array after each removal */
	move_pollfd (poll_id, --poll_ids);
}

static void input_remove_queue (Input *input)
{
	assert (input->fd >= 0);
	assert (input->dirty < 0);

#ifdef DEBUG
	memset (input, 0, sizeof (*input));
#endif /* DEBUG */

	input->dirty = 0;
	input->fd = -1;
}

static void remove_full (input_id id, int delete_idx)
{
	assert (inputs[id].fd >= 0);

	/* already removed, bow out gracefully */
	if (inputs[id].dirty < 0)
		return;

	/* decrement the input id counter */
	input_ids--;

	if (input_ids_max == id)
		input_ids_max = MAX ((id - 1), 0);

	/* remove this input from the global poll array */
	if (!inputs[id].suspended)
		remove_pollfd (inputs[id].poll_id);

	/* it's ok to delete the fd indexes for removed sources, we just need
	 * to be careful with find_input calls */
	if (delete_idx)
		del_fd_index (inputs[id].fd, &id);

	/* remove the validate timer if it exists */
	if (inputs[id].validate)
		timer_remove_zero (&(inputs[id].validate));

	inputs[id].dirty = -1;

	/* schedule this entry for complete removal */
	push (&inputs_remove, &inputs[id]);
}

void input_remove (input_id id)
{
	if (id == 0)
		return;

	/* subtract one for API reasons...see ::input_add */
	remove_full (id - 1, TRUE);
}

static int remove_by_fd (Dataset *d, DatasetNode *node, int *fd)
{
	input_id *id = node->key;

	/* it is important that we not disturb the index while we are walking
	 * along it, obviously */
	remove_full (*id, FALSE);

	return TRUE;
}

void input_remove_all (int fd)
{
	if (fd < 0)
		return;

	/* remove each individual entry, maintaining the index so that it may
	 * be removed more efficiently after all removals have taken place */
	foreach_fd_index (fd, DATASET_FOREACH(remove_by_fd), &fd);
	del_fd_index (fd, NULL);
}

static void input_suspend (Input *input)
{
	if (input->suspended)
		return;

	remove_pollfd (input->poll_id);

	input->poll_id   = 0;
	input->suspended = TRUE;

	if (input->validate)
		timer_remove_zero (&input->validate);
}

static int suspend_by_fd (Dataset *d, DatasetNode *node, int *fd)
{
	input_id *id = node->key;

	input_suspend (&inputs[*id]);

	return FALSE;
}

void input_suspend_all (int fd)
{
	if (fd < 0)
		return;

	foreach_fd_index (fd, DATASET_FOREACH(suspend_by_fd), &fd);
}

static void input_resume (Input *input)
{
	if (!input->suspended)
		return;

	assert (input->validate == 0);
	assert (input->poll_id == 0);

	/* acquire a new poll id */
	input->poll_id   = next_poll_id();
	input->suspended = FALSE;

	/* add the validate timer back if necessary */
	if (!input->complete && input->timeout)
	{
		assert (input->validate == 0);

		input->validate =
		    timer_add (input->timeout, (TimerCallback)validate_timeout, input);
	}

	/* reset the poll_fds data */
	set_pollfd (input->poll_id, input);
}

static int resume_by_fd (Dataset *d, DatasetNode *node, int *fd)
{
	input_id *id = node->key;

	input_resume (&inputs[*id]);

	return FALSE;
}

void input_resume_all (int fd)
{
	if (fd < 0)
		return;

	foreach_fd_index (fd, DATASET_FOREACH(resume_by_fd), &fd);
}

/*****************************************************************************/

static void time_current (struct timeval *curr_time)
{
	/* this abstraction is provided so that we may one day cache the value to
	 * avoid unnecessary gettimeofday calls */
	platform_gettimeofday (curr_time, NULL);
}

static int find_timer (Timer *timer, timer_id *id)
{
	return INTCMP (timer->id, *id);
}

static Timer *timer_find (timer_id id, List **link_ptr)
{
	List *link;

	link = list_find_custom (timers, &id, (CompareFunc)find_timer);

	if (link_ptr)
		*link_ptr = link;

	return list_nth_data (link, 0);
}

static timer_id next_timer_id (void)
{
	timer_id id = timer_ids;

	while (id == 0 || timer_find (id, NULL))
		id++;

	timer_ids++;
	return id;
}

static Timer *timer_new (time_t interval, TimerCallback cb, void *udata)
{
	Timer         *timer;
	timer_id       id;
	struct timeval current_time;

	if (!(timer = MALLOC (sizeof (Timer))))
		return 0;

	id = next_timer_id ();
	assert (id != 0);

	timer->id    = id;
	timer->cb    = cb;
	timer->udata = udata;

	/*
	 * TIME_SET corrects a problem with the old method of initializing timers
	 * (TIME_CLEAR, TIME_ADDI).  Timers on the order of magnitude of hours
	 * overflowed the tv_usec value resulting in timers with incorrect
	 * intervals.
	 */
	TIME_SET (&timer->interval, interval);

	/* determine the expiration time */
	time_current (&current_time);
	TIME_ADD (&current_time, &timer->interval, &timer->expiration);

	return timer;
}

static void timer_free (Timer *timer)
{
	if (!timer)
		return;

	free (timer);
}

static int sort_timer (Timer *a, Timer *b)
{
	if (TIME_CMP (&a->expiration, &b->expiration, TIME_GT))
		return 1;
	else if (TIME_CMP (&a->expiration, &b->expiration, TIME_LT))
		return -1;

	return 0;
}

timer_id timer_add (time_t interval, TimerCallback cb, void *udata)
{
	Timer *timer;

	if (!(timer = timer_new (interval, cb, udata)))
		return 0;

	/* insert so that the closest expiration to the current time is placed
	 * first for efficiency purposes */
	timers = list_insert_sorted (timers, (CompareFunc)sort_timer, timer);

	return timer->id;
}

void timer_reset (timer_id id)
{
	Timer         *timer;
	struct timeval current_time;

	if (!(timer = timer_find (id, NULL)))
		return;

	time_current (&current_time);
	TIME_ADD (&current_time, &timer->interval, &timer->expiration);
}

static void remove_timer (Timer *timer, List *link)
{
	assert (timer != NULL);
	assert (link != NULL);

	assert (list_nth_data (link, 0) == timer);

	timer_ids--;
	timers = list_remove_link (timers, link);
	timer_free (timer);
}

void timer_remove (timer_id id)
{
	Timer *timer;
	List  *link = NULL;

	if (id == 0)
		return;

	if (!(timer = timer_find (id, &link)))
		return;

	remove_timer (timer, link);
}

void timer_remove_zero (timer_id *id)
{
	if (id && *id)
	{
		timer_remove (*id);
		*id = 0;
	}
}

/*****************************************************************************/

void event_init (void)
{
	int i;

	for (i = 0; i < MAX_INPUTS; i++)
	{
#ifdef DEBUG
		memset (&inputs[i], 0, sizeof (inputs[i]));
		memset (&poll_fds[i], 0, sizeof (poll_fds[i]));
#endif /* DEBUG */

		inputs[i].fd   = -1;
		poll_fds[i].fd = -1;
	}
}

void event_quit (int sig)
{
	polling = FALSE;
}

static int calc_timeout (Timer *t)
{
	struct timeval current_time;
	struct timeval timeout;
	int next_msec;

	time_current (&current_time);
	TIME_SUB(&t->expiration, &current_time, &timeout);

	/* get the next timeout in milliseconds instead of microseconds */
	next_msec = ((timeout.tv_sec * 1000000) + timeout.tv_usec) / 1000;
	return next_msec;
}

static int event_poll (struct pollfd *pfds, unsigned int nfds, int timeout)
{
	int             ret;
#ifndef HAVE_POLL
	int             i;
	int             maxfd = -1;
	fd_set          rset;
	fd_set          wset;
	fd_set          xset;
	struct timeval  timeout_tv;
	struct timeval *tvp = NULL;
#endif /* !HAVE_POLL */

#ifdef HAVE_POLL
	ret = poll (pfds, nfds, timeout);
#else
	if (timeout >= 0)
	{
		timeout_tv.tv_sec = timeout / 1000;
		timeout_tv.tv_usec = (timeout % 1000) * 1000;
		tvp = &timeout_tv;
	}

	FD_ZERO (&rset);
	FD_ZERO (&wset);
	FD_ZERO (&xset);

	for (i = 0; i < nfds; i++)
	{
		if (pfds[i].events == 0)
			continue;

		if (pfds[i].fd > maxfd)
			maxfd = pfds[i].fd;

		if (pfds[i].events & POLLIN)
			FD_SET (pfds[i].fd, &rset);

		if (pfds[i].events & POLLOUT)
			FD_SET (pfds[i].fd, &wset);

		if (pfds[i].events & POLLERR)
			FD_SET (pfds[i].fd, &xset);
	}

	ret = select (maxfd + 1, &rset, &wset, &xset, tvp);

	for (i = 0; i < nfds; i++)
	{
		pfds[i].revents = 0;

		if (pfds[i].events == 0)
			continue;

		if (FD_ISSET (pfds[i].fd, &rset))
			pfds[i].revents |= POLLIN;

		if (FD_ISSET (pfds[i].fd, &wset))
			pfds[i].revents |= POLLOUT;

		if (FD_ISSET (pfds[i].fd, &xset))
			pfds[i].revents |= POLLERR;
	}
#endif /* HAVE_POLL */

	return ret;
}

static void dispatch_timer (Timer *timer)
{
	struct timeval current_time;
	List          *link;
	int            ret;

	if (!timer)
		return;

	/* raise the callback before we remove this from the timers list to
	 * ensure that we do not disturb next_input_id */
	ret = timer->cb (timer->udata);

	/* the timer cb appears to have removed the timer without use of the return value, bastards */
	if (!(link = list_find (timers, timer)))
		return;

	if (!ret)
		remove_timer (timer, link);
	else
	{
		/* WARNING: duplicated in timer_reset */
		time_current (&current_time);
		TIME_ADD (&current_time, &timer->interval, &timer->expiration);

		/* add the timer back into the list in the same sorted order */
		timers = list_remove (timers, timer);
		timers = list_insert_sorted (timers, (CompareFunc)sort_timer, timer);
	}
}

static void dispatch_input (Input *input)
{
	if (input->suspended)
	{
		assert (input->poll_id == 0);
		return;
	}

	if (input->validate)
	{
		timer_remove_zero (&input->validate);
		input->complete = TRUE;
	}

	assert (input->fd == poll_fds[input->poll_id].fd);
	input->cb (input->fd, input->id + 1, input->udata);
}

static int poll_once (void)
{
	Timer *t       = NULL;
	int    timeout = -1;
	int    ret;

	if (timer_ids > 0)
	{
		t = list_nth_data (timers, 0);
		assert (t != NULL);

		/* if the timer should've already been called by now, completely
		 * bypass poll and handle the event */
		if ((timeout = calc_timeout (t)) <= 0)
		{
			dispatch_timer (t);
			return 0;
		}
	}

	ret = event_poll (poll_fds, poll_ids, timeout);

	switch (ret)
	{
	 case -1:
		GIFT_ERROR ((POLLFN ": %s", GIFT_NETERROR()));
		break;
	 case 0:
		dispatch_timer (t);
		break;
	 default:
		{
			int i;
			int nfds = ret;
			input_id ids;
			struct pollfd *pfd;

			/* we need to make a copy of this as a dispatched input may
			 * add or remove an input, changing this value */
			ids = input_ids_max + 1;

			for (i = 0; i < ids && nfds > 0; i++)
			{
				if (inputs[i].fd < 0 || inputs[i].dirty || inputs[i].suspended)
					continue;

				pfd = &poll_fds[inputs[i].poll_id];

				if (pfd->revents & pfd->events ||
				    pfd->revents & (POLLERR | POLLHUP | POLLNVAL))
				{
					dispatch_input (&inputs[i]);
					nfds--;
				}
			}

			POLL_TRACE (("%d/%d/%d", nfds, ret, ids));
		}
		break;
	}

	return ret;
}

void event_poll_once (void)
{
	Input *input;

	poll_once ();

	/* finish inserting all queued input additions */
	while ((input = shift (&inputs_add)))
		input_add_queue (input);

	/* ...and then removed inputs */
	while ((input = shift (&inputs_remove)))
		input_remove_queue (input);
}

void event_loop (void)
{
	polling = TRUE;

	while (polling)
	{
		if (!input_ids && !timer_ids)
			break;

		event_poll_once ();
	}
}

/*****************************************************************************/

#if 0
static void read_stdin (int fd, input_id id, void *udata)
{
	static char buf[1024];
	ssize_t     n;

	if ((n = read (fd, buf, sizeof (buf) - 1)) <= 0)
	{
		fprintf (stderr, "fd=%d exhausted\n", fd);
		input_remove (id);
		return;
	}

	buf[n] = 0;
	printf ("%s", buf);
}

int main (int argc, char **argv)
{
	input_id id;

	event_init ();

	id = input_add (fileno (stdin), NULL, INPUT_READ, read_stdin, FALSE);
	assert (id != 0);

	event_loop ();
	printf ("loop finished...\n");

	return 0;
}
#endif
