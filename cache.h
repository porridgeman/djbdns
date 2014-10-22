#ifndef CACHE_H
#define CACHE_H

#include "uint32.h"
#include "uint64.h"
#include "taia.h"

typedef enum {
	NONE,                 /* disable auto resizing */
	TARGET_CYCLE_TIME,    /* use value of target_cycle_time */
	MIN_TTL,              /* use minimum TTL for last cycle as target cycle time */
	MAX_TTL,              /* use maximum TTL for last cycle as target cycle time */
	AVG_TTL               /* use average TTL for last cycle as target cycle time */
} resize_mode_t;

typedef struct cache_options {
	resize_mode_t resize_mode;       /* default is TARGET_CYCLE_TIME */
	uint32 target_cycle_time;        /* in seconds, default is 86400 (24 hours) */
	void (*resize_callback)(double,  /* resize ratio */
							uint32,  /* old size */
							uint32,  /* new size */
							int);    /* 1 if resize was successful, 0 if not */
} cache_options_t;

extern uint64 cache_motion;
extern int cache_init(unsigned int,cache_options_t *);
extern void cache_set(const char *,unsigned int,const char *,unsigned int,uint32);
extern char *cache_get(const char *,unsigned int,unsigned int *,uint32 *);

typedef void*cache_t;

extern cache_t cache_t_new(unsigned int,cache_options_t *);
extern void cache_t_destroy(cache_t);
extern void cache_t_set(cache_t,const char *,unsigned int,const char *,unsigned int,uint32);
extern char *cache_t_get(cache_t,const char *,unsigned int,unsigned int *,uint32 *,struct tai *);
extern int cache_t_init(cache_t,unsigned int,cache_options_t *);

#endif
