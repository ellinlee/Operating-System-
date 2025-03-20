/**********************************************************************
 * Copyright (c) 2019-2024
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;

/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;

/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];

/**
 * Monotonically increasing ticks. Do not modify it
 */
extern unsigned int ticks;

/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;

/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
static bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_BLOCKED;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
static void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue))
	{
		struct process *waiter = list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter is in the wait status
		 */
		assert(waiter->status == PROCESS_BLOCKED);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

#include "sched.h"

/***********************************************************************
 * FCFS scheduler
 ***********************************************************************/
static int fcfs_initialize(void)
{
	return 0;
}

static void fcfs_finalize(void)
{
}

static struct process *fcfs_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/**
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, the current process can be blocked
	 * while acquiring a resource. In this case just pick the next as well.
	 */
	if (!current || current->status == PROCESS_BLOCKED)
	{
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan)
	{
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue))
	{
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);

		/**
		 * Detach the process from the ready queue. Note that we use
		 * list_del_init() over list_del() to maintain the list head tidy.
		 * Otherwise, the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the process to run next */
	return next;
}

struct scheduler fcfs_scheduler = {
	.name = "FCFS",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fcfs_initialize,
	.finalize = fcfs_finalize,
	.schedule = fcfs_schedule,
};

/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	struct process *next = NULL;

	if (!current || current->status == PROCESS_BLOCKED)
	{
		goto pick_next;
	}

	if (current->age < current->lifespan)
	{
		return current;
	}

pick_next:
	if (!list_empty(&readyqueue))
	{
		/*readyqueue가 비어있지않으면 shortest time 찾아서 next에 넣기*/
		/*readyqueue에 들어있는 프로세스 비교 - for lifespan이 가장 짧은 process 찾기*/
		struct process *tmp = NULL;
		unsigned int min = UINT_MAX;

		list_for_each_entry(tmp, &readyqueue, list)
		{
			if (tmp->lifespan < min)
			{
				min = tmp->lifespan;
				next = tmp;
			}
		}
		list_del_init(&next->list);
	}
	return next;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire,  /* Use the default FCFS acquire() */
	.release = fcfs_release,  /* Use the default FCFS release() */
	.schedule = sjf_schedule, /* TODO: Assign your schedule function
						 to this function pointer to activate
						 SJF in the simulation system */
};

/***********************************************************************
 * STCF scheduler - choose the one which has the shortest completion time
 * 					need to compare the remained time of current process and processes in the readyqueue
 ***********************************************************************/

static struct process *stcf_schedule(void)
{
	struct process *next = NULL;

	if (!current || current->status == PROCESS_BLOCKED)
	{
		goto pick_next;
	}

	if (current->age < current->lifespan)
	{
		if (!list_empty(&readyqueue))
		{
			struct process *tmp = NULL;
			unsigned int min = UINT_MAX;

			list_for_each_entry(tmp, &readyqueue, list)
			{
				if (tmp->lifespan - tmp->age < min)
				{
					min = tmp->lifespan;
					next = tmp;
				}
			}
			if (current->lifespan - current->age > min)
			{
				list_add_tail(&current->list, &readyqueue);
			}
			else
			{
				return current;
			}
			list_del_init(&next->list);
		}
		else
		{
			return current;
		}
	}
	else if (current->age == current->lifespan)
	{
		goto pick_next;
	}

	return next;

pick_next:
	if (!list_empty(&readyqueue))
	{
		struct process *tmp = NULL;
		unsigned int min = UINT_MAX;

		list_for_each_entry(tmp, &readyqueue, list)
		{
			if (tmp->lifespan - tmp->age < min)
			{
				min = tmp->lifespan - tmp->age;
				next = tmp;
			}
		}
		list_del_init(&next->list);
	}
	else if (!current && list_empty(&readyqueue))
	{
		return NULL;
	}
	return next;
}

struct scheduler stcf_scheduler = {
	.name = "Shortest Time-to-Complete First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = stcf_schedule,
	/* You need to check the newly created processes to implement STCF.
	 * Have a look at @forked() callback.
	 */

	/* Obviously, you should implement stcf_schedule() and attach it here */

};

/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process *rr_schedule(void)
{
	struct process *next = NULL;

	if (!current || current->status == PROCESS_BLOCKED)
	{
		goto pick_next;
	}

	if (current->age < current->lifespan)
	{
		list_add_tail(&current->list, &readyqueue);
	}

pick_next:
	if (!list_empty(&readyqueue))
	{
		next = list_first_entry(&readyqueue, struct process, list);
		list_del_init(&next->list);
	}
	return next;
}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = rr_schedule,
};

/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
static bool prio_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		r->owner = current;
		return true;
	}

	current->status = PROCESS_BLOCKED;
	list_add_tail(&current->list, &r->waitqueue);
	return false;
}

static void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;
	assert(r->owner == current);
	r->owner = NULL;

	if (!list_empty(&r->waitqueue))
	{
		struct process *tmp = NULL;
		struct process *waiter = NULL;
		waiter = list_first_entry(&r->waitqueue, struct process, list);

		list_for_each_entry(tmp, &r->waitqueue, list)
		{
			if (tmp->prio > waiter->prio)
			{
				waiter = tmp;
			}
		}

		assert(waiter->status == PROCESS_BLOCKED);
		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}

static struct process *prio_schedule(void)
{
	struct process *next = NULL;

	if (!current || current->status == PROCESS_BLOCKED)
	{
		goto pick_next;
	}

	if (current->age < current->lifespan)
	{
		list_add_tail(&current->list, &readyqueue);
	}

pick_next:
	if (!list_empty(&readyqueue))
	{
		struct process *tmp = NULL;
		unsigned int max = 0;

		list_for_each_entry(tmp, &readyqueue, list)
		{
			if (tmp->prio >= max)
			{
				max = tmp->prio;
				next = tmp;
			}
		}
		list_del_init(&next->list);
	}
	return next;
}

struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule = prio_schedule,
};

/***********************************************************************
 * Priority scheduler with aging
 ***********************************************************************/
static struct process *pa_schedule(void)
{

	struct process *next = NULL;

	if (!current || current->status == PROCESS_BLOCKED)
	{
		goto pick_next;
	}

	if (current->age < current->lifespan)
	{
		list_add_tail(&current->list, &readyqueue);
	}

pick_next:

	if (!list_empty(&readyqueue))
	{
		struct process *tmp = NULL;
		unsigned int max = 0;

		list_for_each_entry(tmp, &readyqueue, list)
		{
			tmp->prio++;
			if (tmp->prio > max && tmp->prio < MAX_PRIO)
			{
				max = tmp->prio;
				next = tmp;
			}
		}
		next->prio = next->prio_orig;
		list_del_init(&next->list);
	}
	return next;
}

struct scheduler pa_scheduler = {
	.name = "Priority + aging",
	.acquire = prio_acquire,
	.release = prio_release,
	.schedule = pa_schedule,
};

/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 * acquire/release 수정 필요
 * acquire할 때 priority max_prio까지 높이기
 * release할 때 원래 priority로 복귀 필요
 ***********************************************************************/
static bool pcp_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		r->owner = current;
		r->owner->prio = MAX_PRIO;
		return true;
	}

	current->status = PROCESS_BLOCKED;
	list_add_tail(&current->list, &r->waitqueue);
	return false;
}

static void pcp_release(int resource_id)
{
	struct resource *r = resources + resource_id;
	assert(r->owner == current);

	r->owner = NULL;

	if (!list_empty(&r->waitqueue))
	{
		struct process *tmp = NULL;
		struct process *waiter = NULL;
		waiter = list_first_entry(&r->waitqueue, struct process, list);

		list_for_each_entry(tmp, &r->waitqueue, list)
		{
			if (tmp->prio > waiter->prio)
			{
				waiter = tmp;
				waiter->prio = waiter->prio_orig;
			}
		}

		assert(waiter->status == PROCESS_BLOCKED);
		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}

struct scheduler pcp_scheduler = {
	.name = "Priority + PCP Protocol",
	.acquire = pcp_acquire,
	.release = pcp_release,
	.schedule = prio_schedule,
};

/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
static bool pip_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner)
	{
		r->owner = current;
		return true;
	}

	current->status = PROCESS_BLOCKED;

	if (current->prio > r->owner->prio)
	{
		r->owner->prio = current->prio;
	}

	list_add_tail(&current->list, &r->waitqueue);
	return false;
}

static void pip_release(int resource_id)
{
	struct resource *r = resources + resource_id;
	assert(r->owner == current);
	r->owner = NULL;

	if (!list_empty(&r->waitqueue))
	{
		struct process *tmp = NULL;
		struct process *waiter = NULL;
		waiter = list_first_entry(&r->waitqueue, struct process, list);

		list_for_each_entry(tmp, &r->waitqueue, list)
		{
			if (tmp->prio > waiter->prio)
			{
				waiter = tmp;
				waiter->prio = waiter->prio_orig;
			}
		}

		assert(waiter->status == PROCESS_BLOCKED);
		list_del_init(&waiter->list);
		waiter->status = PROCESS_READY;
		list_add_tail(&waiter->list, &readyqueue);
	}
}

struct scheduler pip_scheduler = {
	.name = "Priority + PIP Protocol",
	.acquire = pip_acquire,
	.release = pip_release,
	.schedule = prio_schedule,
};
