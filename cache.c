#include "alloc.h"
#include "byte.h"
#include "uint32.h"
#include "exit.h"
#include "tai.h"
#include "cache.h"
#include <stdio.h>

uint64 cache_motion = 0;

struct cache {
  char *x;
  uint32 size;
  uint32 hsize;
  uint32 writer;
  uint32 oldest;
  uint32 unused;
  uint64 cache_motion;
  struct {
    struct tai start;
    int count;
    int low_ratio_count;
  } cycle;
  cache_options options;
};

static struct cache *default_cache = 0;

/*
100 <= size <= 1000000000.
4 <= hsize <= size/16.
hsize is a power of 2.

hsize <= writer <= oldest <= unused <= size.
If oldest == unused then unused == size.

x is a hash table with the following structure:
x[0...hsize-1]: hsize/4 head links.
x[hsize...writer-1]: consecutive entries, newest entry on the right.
x[writer...oldest-1]: free space for new entries.
x[oldest...unused-1]: consecutive entries, oldest entry on the left.
x[unused...size-1]: unused.

Each hash bucket is a linked list containing the following items:
the head link, the newest entry, the second-newest entry, etc.
Each link is a 4-byte number giving the xor of
the positions of the adjacent items in the list.

Entries are always inserted immediately after the head and removed at the tail.

Each entry contains the following information:
4-byte link; 4-byte keylen; 4-byte datalen; 8-byte expire time; key; data.
*/

#define MAXKEYLEN 1000
#define MAXDATALEN 1000000

#define MAXCACHESIZE 1000000000
#define MINCACHESIZE 100

#define DEFAULT_TARGET_CYCLETIME 86400  /* 24 hours */
#define DEFAULT_MIN_SAMPLE_TIME  300    /* 5 minutes */

static void log_cache_resize(unsigned int oldsize, unsigned int newsize)
{
  printf("cache resized from %d to %d\n", oldsize, newsize);
} 

static void cache_impossible(void)
{
  _exit(111);
}

static void set4(struct cache *c, uint32 pos,uint32 u)
{
  if (pos > c->size - 4) cache_impossible();
  uint32_pack(c->x + pos,u);
}

static uint32 get4(struct cache *c, uint32 pos)
{
  uint32 result;
  if (pos > c->size - 4) cache_impossible();
  uint32_unpack(c->x + pos,&result);
  return result;
}

static unsigned int hash(struct cache *c,const char *key,unsigned int keylen)
{
  unsigned int result = 5381;

  while (keylen) {
    result = (result << 5) + result;
    result ^= (unsigned char) *key;
    ++key;
    --keylen;
  }
  result <<= 2;
  result &= c->hsize - 4;
  return result;
}

static int init(struct cache *c,unsigned int cachesize,cache_options *options)
{
  char *mem;

  /* allocate memory first so failure won't leave c in inconsistent state */
  mem = alloc(cachesize);
  if (!mem) return 0;

  if (c->x) {
    alloc_free(c->x);
    c->x = 0;
  }

  if (cachesize > MAXCACHESIZE) cachesize = MAXCACHESIZE;
  if (cachesize < MINCACHESIZE) cachesize = MINCACHESIZE;
  c->size = cachesize;

  c->hsize = 4;
  while (c->hsize <= (c->size >> 5)) c->hsize <<= 1;

  c->x = mem;
  byte_zero(c->x,c->size);

  c->writer = c->hsize;
  c->oldest = c->size;
  c->unused = c->size;

  tai_now(&c->cycle.start);
  c->cycle.count = 0;
  c->cycle.low_ratio_count = 0;

  if (options) {
    byte_copy(&c->options,sizeof(cache_options),options);
  } else {
    c->options.allow_resize = 1;
    c->options.clear_on_resize = 0;
    c->options.target_cycle_time = DEFAULT_TARGET_CYCLETIME;
    c->options.min_sample_time = DEFAULT_MIN_SAMPLE_TIME;
  }

  return 1;
}

/*
 * Return true if ration < 0.5 for the third time in succession.
 */
static int low_ratio(struct cache *c, double ratio)
{
  if (ratio < 0.5) {
    if (++c->cycle.low_ratio_count > 2) return 1;
  } else {
    c->cycle.low_ratio_count = 0;
  }
  return 0;
}

static int check_motion(struct cache *c)
{
  struct tai now;
  struct tai elapsed;
  double ratio;
  uint32 motion;
  unsigned int newsize;
  char *new;

  tai_now(&now);
  tai_sub(&elapsed,&now,&c->cycle.start);

  c->cycle.count++;

  if (tai_approx(&elapsed) > c->options.min_sample_time) {

    ratio = (double)c->options.target_cycle_time / (tai_approx(&elapsed) / c->cycle.count);

    printf("ratio = %lf\n", ratio);fflush(stdout);

    if (c->options.allow_resize && (ratio > 1.0 || low_ratio(c,ratio))) {
      newsize = c->size * ratio * 1.1; /* add 10% */
      if (newsize < MAXCACHESIZE) {
        log_cache_resize(c->size, newsize);
        init(c,newsize,&c->options); /* TODO check for failure, and at least log? */
        return 1;
      }
    }

    tai_now(&c->cycle.start);
    c->cycle.count = 0;

  }

  return 0;
} 

/*
 * Get entry from cache. Remaining time to live in seconds is return via ttl parameter.
 * If stamp is not 0, it points to a struct tai containing the time to use as the
 * current time for determining cache expiry (optimization to avoid system call).
 */
char *cache_t_get(cache_t cache,const char *key,unsigned int keylen,unsigned int *datalen,uint32 *ttl,struct tai *stamp)
{
  struct tai expire;
  struct tai now;
  uint32 pos;
  uint32 prevpos;
  uint32 nextpos;
  uint32 u;
  unsigned int loop;
  double d;

  struct cache *c = (struct cache *)cache;

  if (!c) return 0;
  if (!c->x) return 0;
  if (keylen > MAXKEYLEN) return 0;

  prevpos = hash(c,key,keylen);
  pos = get4(c,prevpos);
  loop = 0;

  while (pos) {
    if (get4(c,pos + 4) == keylen) {
      if (pos + 20 + keylen > c->size) cache_impossible();
      if (byte_equal(key,keylen,c->x + pos + 20)) {
        tai_unpack(c->x + pos + 12,&expire);
        tai_now(&now);
        if (tai_less(&expire,&now)) return 0;

        tai_sub(&expire,&expire,&now);
        d = tai_approx(&expire);
        if (d > 604800) d = 604800;
        *ttl = d;

        u = get4(c,pos + 8);
        if (u > c->size - pos - 20 - keylen) cache_impossible();
        *datalen = u;

        return c->x + pos + 20 + keylen;
      }
    }
    nextpos = prevpos ^ get4(c,pos);
    prevpos = pos;
    pos = nextpos;
    if (++loop > 100) return 0; /* to protect against hash flooding */
  }

  return 0;
}

/*
 * Add entry to cache, ttl is time to live in seconds. 
 */
void cache_t_set(cache_t cache,const char *key,unsigned int keylen,const char *data,unsigned int datalen,uint32 ttl)
{
  struct tai now;
  struct tai expire;
  unsigned int entrylen;
  unsigned int keyhash;
  uint32 pos;

  struct cache *c = (struct cache *)cache;

  if (!c) return;
  if (!c->x) return;
  if (keylen > MAXKEYLEN) return;
  if (datalen > MAXDATALEN) return;

  if (ttl > 604800) ttl = 604800;

  entrylen = keylen + datalen + 20;

  while (c->writer + entrylen > c->oldest) {
    if (c->oldest == c->unused) {
      if (c->writer <= c->hsize) return;
      if (check_motion(c)) return;
      c->unused = c->writer;
      c->oldest = c->hsize;
      c->writer = c->hsize;
    }

    pos = get4(c,c->oldest);
    set4(c,pos,get4(c,pos) ^ c->oldest);
  
    c->oldest += get4(c,c->oldest + 4) + get4(c,c->oldest + 8) + 20;
    if (c->oldest > c->unused) cache_impossible();
    if (c->oldest == c->unused) {
      c->unused = c->size;
      c->oldest = c->size;
    }
  }

  keyhash = hash(c,key,keylen);

  tai_now(&now);
  tai_uint(&expire,ttl);
  tai_add(&expire,&expire,&now);

  pos = get4(c,keyhash);
  if (pos)
    set4(c,pos,get4(c,pos) ^ keyhash ^ c->writer);
  set4(c,c->writer,pos ^ keyhash);
  set4(c,c->writer + 4,keylen);
  set4(c,c->writer + 8,datalen);
  tai_pack(c->x + c->writer + 12,&expire);
  byte_copy(c->x + c->writer + 20,keylen,key);
  byte_copy(c->x + c->writer + 20 + keylen,datalen,data);

  set4(c,keyhash,c->writer);
  c->writer += entrylen;
  c->cache_motion += entrylen;
  if (c == default_cache) {
    cache_motion += entrylen;
  }
}

/*
 * Create and return cache, cachesize is total size to allocate
 * in bytes (not including size of struct cache)
 */
cache_t cache_t_new(unsigned int cachesize,cache_options *options) {

  struct cache *c = (struct cache *)alloc(sizeof(struct cache));
  byte_zero(c,sizeof(struct cache));

  if (init(c,cachesize,options)) {
    return (cache_t)c;
  }

  return 0;
}

void cache_set_options(cache_options options)
{
  // TODO this is not ideal, could be misleading if someone sets options first
  if (default_cache) {
    byte_copy(default_cache->options,sizeof(cache_options),&options);
  }
}

/*
 * Re-initialize existing cache.
 */
int cache_t_init(cache_t cache,unsigned int cachesize,cache_options *options) {
  if (!cache) return 0;
  return init((struct cache *)cache,cachesize,options);
}

/*
 * Destroy cache, freeing all allocated memory.
 */
void cache_t_destroy(cache_t cache) {

  struct cache *c = (struct cache *)cache;

  if (c->x) {
    alloc_free(c->x);
  }
  alloc_free(c);
}

char *cache_get(const char *key,unsigned int keylen,unsigned int *datalen,uint32 *ttl)
{
  return cache_t_get(default_cache, key, keylen, datalen, ttl, 0);
}

void cache_set(const char *key,unsigned int keylen,const char *data,unsigned int datalen,uint32 ttl)
{
  cache_t_set(default_cache, key, keylen, data, datalen, ttl);
}

int cache_init(unsigned int cachesize,cache_options *options)
{
  if (!default_cache) {
    default_cache = cache_t_new(cachesize,options);
    return default_cache != 0;
  }

  return init(default_cache,cachesize,options);
}
