#ifndef CACHE_H
#define CACHE_H

#include "uint32.h"
#include "uint64.h"
#include "taia.h"

typedef struct cache_options {
	int allow_resize;         /* 1 means allow automatic resize, 0 means don't, default is 1 */
	uint32 target_cycle_time; /* in seconds, default is 86400 (24 hours) */
	int (*resize_callback)(double,uint32,uint32,int);
} cache_options;

extern uint64 cache_motion;
extern int cache_init(unsigned int,cache_options *);
extern void cache_set(const char *,unsigned int,const char *,unsigned int,uint32);
extern char *cache_get(const char *,unsigned int,unsigned int *,uint32 *);

typedef void*cache_t;

extern cache_t cache_t_new(unsigned int,cache_options *);
extern void cache_t_destroy(cache_t);
extern void cache_t_set(cache_t,const char *,unsigned int,const char *,unsigned int,uint32);
extern char *cache_t_get(cache_t,const char *,unsigned int,unsigned int *,uint32 *,struct tai *);
extern int cache_t_init(cache_t,unsigned int,cache_options *);

#endif
