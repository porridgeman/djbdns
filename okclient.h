#ifndef OKCLIENT_H
#define OKCLIENT_H

#include "uint32.h"
#include "taia.h"

struct okclient_stats {
	uint32 requests;
	uint32 cache_hits;
	uint32 cache_misses;
	uint32 stat_calls;
};

extern int okclient(char *,struct taia *);

extern void okclient_get_stats(struct okclient_stats *);
extern void okclient_clear_stats();
extern void okclient_clear_cache();
extern void okclient_load_cache();

#endif
