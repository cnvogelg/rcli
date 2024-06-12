#ifndef TIMER_H
#define TIMER_H

struct timer_handle;
typedef struct timer_handle timer_handle_t;

struct timer_job;
typedef struct timer_job timer_job_t;

extern timer_handle_t *timer_init(void);
extern void timer_exit(timer_handle_t *th);

extern ULONG timer_get_sig_mask(timer_handle_t *th);
extern timer_job_t *timer_get_next_done_job(timer_handle_t *th);

extern timer_job_t *timer_get_first_job(timer_handle_t *th);
extern timer_job_t *timer_get_next_job(timer_job_t *job);

extern timer_job_t *timer_start(timer_handle_t *th, ULONG secs, ULONG micros, APTR user_data);
extern void timer_stop(timer_job_t *job);

extern APTR timer_job_get_user_data(timer_job_t *job);
extern void timer_job_get_wait_time(timer_job_t *job, ULONG *wait_s, ULONG *wait_us);

#endif
