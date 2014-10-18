#ifndef CACHE_H
#define CACHE_H

#include "uint32.h"
#include "uint64.h"
#include "taia.h"

extern uint64 cache_motion;
extern int cache_init(unsigned int);
extern void cache_set(const char *,unsigned int,const char *,unsigned int,uint32);
extern char *cache_get(const char *,unsigned int,unsigned int *,uint32 *);

typedef void*cache_t;

extern cache_t cache_t_new(unsigned int);
extern void cache_t_delete(cache_t);
extern void cache_t_set(cache_t,const char *,unsigned int,const char *,unsigned int,uint32);
extern char *cache_t_get(cache_t,const char *,unsigned int,unsigned int *,uint32 *,struct tai *);
extern int cache_t_init(cache_t cache, unsigned int cachesize);

#endif
