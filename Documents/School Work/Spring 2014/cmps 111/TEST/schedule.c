/* This file contains the scheduling policy for SCHED
 *
 * The entry points are:
 *   do_noquantum:        Called on behalf of process' that run out of quantum
 *   do_start_scheduling  Request to start scheduling a proc
 *   do_stop_scheduling   Request to stop scheduling a proc
 *   do_nice		  Request to change the nice level on a proc
 *   init_scheduling      Called from main.c to set up/prepare scheduling
 */
#include "sched.h"
#include "schedproc.h"
#include <assert.h>
#include <minix/com.h>
#include <machine/archtypes.h>
#include "kernel/proc.h" /* for queue constants */


PRIVATE timer_t sched_timer;
PRIVATE unsigned balance_timeout;

#define BALANCE_TIMEOUT	5 /* how often to balance queues in seconds */

FORWARD _PROTOTYPE( int schedule_process, (struct schedproc * rmp)	);
FORWARD _PROTOTYPE( void balance_queues, (struct timer *tp)		);

#define DEFAULT_USER_TIME_SLICE 200


/*===========================================================================*
 *				do_noquantum				     *
 *===========================================================================*/

PUBLIC int do_noquantum(message *m_ptr)
{
	register struct schedproc *rmp;
	register struct schedproc *rmp_temp;
	int rv, proc_nr_n;

	if (sched_isokendpt(m_ptr->m_source, &proc_nr_n) != OK) {
		printf("SCHED: WARNING: got an invalid endpoint in OOQ msg %u.\n",
		m_ptr->m_source);
		return EBADEPT;
	}

	rmp = &schedproc[proc_nr_n];

     /* New Code */
    if (!(rmp->flags & USER_PROCESS) && rmp->priority < MIN_USER_Q) /* system process */ {
        rmp->priority++;
        printf("system process ran out of quantum\n");
    }
    else if ((rmp->flags & USER_PROCESS) && rmp->priority == (MIN_USER_Q - 1)) /* winner ran out of quantum */ {
        rmp->priority = MIN_USER_Q;
        change_tickets(rmp_temp, -1);
        printf("winning process ran out of quantum\n");
    }
    else /* a process other than a winning process ran out of quantum. this means that
              the winning processe(s) are IO bound, so increase their tickets */
        if (rmp->flags & USER_PROCESS) {
            for (proc_nr_n = 0, rmp_temp = schedproc; proc_nr_n < NR_PROCS; ++proc_nr_n, ++rmp_temp)
                if (rmp_temp->priority == (MIN_USER_Q - 1) && rmp_temp->flags == (IN_USE | USER_PROCESS))
                    change_tickets(rmp_temp, 1);
            printf("IO bound process detected, Adding tickets to WINNING process\n");
        }

    /* Run process */
    if ((rv = schedule_process(rmp)) != OK)
        return rv;

    /* Run lottery */
    if (rv = run_lottery() != OK)
        return rv;

	return OK;
}

/*===========================================================================*
 *				do_stop_scheduling			     *
 *===========================================================================*/
PUBLIC int do_stop_scheduling(message *m_ptr)
{
	register struct schedproc *rmp;
	int rv, proc_nr_n;

	/* check who can send you requests */
	if (!accept_message(m_ptr))
		return EPERM;

	if (sched_isokendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr_n) != OK) {
		printf("SCHED: WARNING: got an invalid endpoint in OOQ msg "
		"%ld\n", m_ptr->SCHEDULING_ENDPOINT);
		return EBADEPT;
	}

	rmp = &schedproc[proc_nr_n];
	rmp->flags = 0; /*&= ~IN_USE;*/

	/*NEW CODE */
	/*If the process is finished try another process */
	if (rv = run_lottery() != OK)
        return rv;

	return OK;
}

/*===========================================================================*
 *				do_start_scheduling			     *
 *===========================================================================*/
PUBLIC int do_start_scheduling(message *m_ptr)
{
	register struct schedproc *rmp;
	int rv, proc_nr_n, parent_nr_n, nice;

	/* we can handle two kinds of messages here */
	assert(m_ptr->m_type == SCHEDULING_START ||
		m_ptr->m_type == SCHEDULING_INHERIT);

	/* check who can send you requests */
	if (!accept_message(m_ptr))
		return EPERM;

	/* Resolve endpoint to proc slot. */
	if ((rv = sched_isemtyendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr_n))
			!= OK) {
		return rv;
	}
	rmp = &schedproc[proc_nr_n];

	/* Populate process slot */
	rmp->endpoint     = m_ptr->SCHEDULING_ENDPOINT;
	rmp->parent       = m_ptr->SCHEDULING_PARENT;
	rmp->max_priority = (unsigned) m_ptr->SCHEDULING_MAXPRIO;
    /*New Code */
    rmp->nTickets = 20; /* Sets the starting amount as per stated by PDF */
    rmp->flags = 0; /* Clears the flag bits */


	if (rmp->max_priority >= NR_SCHED_QUEUES) {
		return EINVAL;
	}

	switch (m_ptr->m_type) {

	case SCHEDULING_START:
		/* We have a special case here for system processes, for which
		 * quanum and priority are set explicitly rather than inherited
		 * from the parent */
		rmp->priority   = rmp->max_priority;
		rmp->time_slice = (unsigned) m_ptr->SCHEDULING_QUANTUM;
		break;

	case SCHEDULING_INHERIT:
		/* Inherit current priority and time slice from parent. Since there
		 * is currently only one scheduler scheduling the whole system, this
		 * value is local and we assert that the parent endpoint is valid */
		if ((rv = sched_isokendpt(m_ptr->SCHEDULING_PARENT,
				&parent_nr_n)) != OK)
			return rv;

		/*rmp->priority = schedproc[parent_nr_n].priority; */

		/* NEW CODE */
		rmp->priority = MIN_USER_Q;
		rmp->flags |= USER_PROCESS; /* CHANGE LATER */

		rmp->time_slice = schedproc[parent_nr_n].time_slice;
		break;

	default:
		/* not reachable */
		assert(0);
	}

	/* Take over scheduling the process. The kernel reply message populates
	 * the processes current priority and its time slice */
	if ((rv = sys_schedctl(0, rmp->endpoint, 0, 0)) != OK) {
		printf("Sched: Error taking over scheduling for %d, kernel said %d\n",
			rmp->endpoint, rv);
		return rv;
	}
	/* NEW CODE */
	rmp->flags |= IN_USE; /* CHANGE LATER */

	/* Schedule the process, giving it some quantum */
	if ((rv = schedule_process(rmp)) != OK) {
		printf("Sched: Error while scheduling process, kernel replied %d\n",
			rv);
		return rv;
	}

	/* Mark ourselves as the new scheduler.
	 * By default, processes are scheduled by the parents scheduler. In case
	 * this scheduler would want to delegate scheduling to another
	 * scheduler, it could do so and then write the endpoint of that
	 * scheduler into SCHEDULING_SCHEDULER
	 */

	m_ptr->SCHEDULING_SCHEDULER = SCHED_PROC_NR;

	return OK;
}

/*===========================================================================*
 *				do_nice					     *
 *===========================================================================*/
PUBLIC int do_nice(message *m_ptr)
{
	struct schedproc *rmp;
	int rv;
	int proc_nr_n;
	unsigned new_q, old_q, old_max_q;

	/* check who can send you requests */
	if (!accept_message(m_ptr))
		return EPERM;

	if (sched_isokendpt(m_ptr->SCHEDULING_ENDPOINT, &proc_nr_n) != OK) {
		printf("SCHED: WARNING: got an invalid endpoint in OOQ msg "
		"%ld\n", m_ptr->SCHEDULING_ENDPOINT);
		return EBADEPT;
	}

	rmp = &schedproc[proc_nr_n];
	new_q = (unsigned) m_ptr->SCHEDULING_MAXPRIO;
	if (new_q >= NR_SCHED_QUEUES) {
		return EINVAL;
	}

	/* Store old values, in case we need to roll back the changes */
	old_q     = rmp->priority;
	old_max_q = rmp->max_priority;

	/* Update the proc entry and reschedule the process */
	rmp->max_priority = rmp->priority = new_q;

	if ((rv = schedule_process(rmp)) != OK) {
		/* Something went wrong when rescheduling the process, roll
		 * back the changes to proc struct */
		rmp->priority     = old_q;
		rmp->max_priority = old_max_q;
	}

	return rv;
}

/*===========================================================================*
 *				schedule_process			     *
 *===========================================================================*/
PRIVATE int schedule_process(struct schedproc * rmp)
{
	int rv;

	if ((rv = sys_schedule(rmp->endpoint, rmp->priority,
			rmp->time_slice)) != OK) {
		printf("SCHED: An error occurred when trying to schedule %d: %d\n",
		rmp->endpoint, rv);
	}

	return rv;
}


/*===========================================================================*
 *				start_scheduling			     *
 *===========================================================================*/

PUBLIC void init_scheduling(void)
{
    /*The <minix/u64.h> include file
     defines  a  64 bit data type, u64_t, and a number of functions to operate
     on them.  Note that these functions are geared towards common disk offset
     and  block  computations,  and  do  not  provide  a  full  set  of 64 bit
     operations.*/

    u64_t disk; /* New Code */

	balance_timeout = BALANCE_TIMEOUT * sys_hz();
	init_timer(&sched_timer);
	set_timer(&sched_timer, balance_timeout, balance_queues, 0);

	/* Use the free running clock as TSC */
	read_tsc_64(&disk); /* New Code */

	/*Random # generator */
	srand((unsigned)disk.lo); /* New Code */
}

/*===========================================================================*
 *				balance_queues				     *
 *===========================================================================*/

/* This function in called every 100 ticks to rebalance the queues. The current
 * scheduler bumps processes down one priority when ever they run out of
 * quantum. This function will find all proccesses that have been bumped down,
 * and pulls them back up. This default policy will soon be changed.
 */
PRIVATE void balance_queues(struct timer *tp)
{
	struct schedproc *rmp;
	int proc_nr;
	int rv;

	/* NEW CODE */
    for (proc_nr = 0, rmp = schedproc; proc_nr < NR_PROCS; proc_nr++, rmp++)
        if (rmp->priority > rmp->max_priority && rmp->flags & IN_USE) {
            rmp->priority--;
            schedule_process(rmp);
        }

	set_timer(&sched_timer, balance_timeout, balance_queues, 0);
}

PRIVATE int run_lottery()
{
    struct schedproc *rmp;
    int rv;
	int proc_nr;
	int lucky = 0;
	int total = 0;

    /* Copied from balance_queues. Goes through each process being run
    and counts the number of tickets stored in each process and stores the
    total number of tickets. */
    for (proc_nr=0, rmp=schedproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
        /* if process active and the queue selected is the highest prior */
       if((rmp->flags & IN_USE) && rmp->priority == MIN_USER_Q ){
           total += rmp->nTickets;
       }
    }

    if (total == 0) /* there were no winnable processes */
       return OK;

    /* The winning # is chosen randomly between 1 and N (how many total
    tickets in the processes) */
    lucky = rand() % total + 1;

    /* Another loop to find the process that has the lucky # */
    for (proc_nr=0, rmp=schedproc; proc_nr < NR_PROCS; proc_nr++, rmp++) {
        if((rmp->flags & IN_USE) && rmp->priority == MIN_USER_Q ){
            if (lucky > -1)
               lucky -= rmp->nTickets;
            if (lucky < 0) /* fail-safe so there is no infinite loop*/
                break;
        }
    }
    /* Change later. */
    printf("Process %d won with %d of %d tickets\n", proc_nr, rmp->nTickets, total);


    rmp->priority = (MIN_USER_Q - 1); /* processes that won */
    rmp->time_slice = USER_QUANTUM; /*Sets time_slice back to default scheduling quanta */

    /* Run the process */
    if ((rv = schedule_process(rmp)) != OK)
        return rv;

    return OK;

}

PRIVATE void change_tickets(struct schedproc *rmp, int qty) {
    rmp->nTickets += qty;
    if (rmp->nTickets > 100)
        rmp->nTickets = 100;
    if (rmp->nTickets < 1)
        rmp->nTickets = 1;
}


