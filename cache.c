#include "alloc.h"
#include "byte.h"
#include "uint32.h"
#include "exit.h"
#include "tai.h"
#include "taia.h"
#include "cache.h"
#include "log.h"
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
    struct taia start;
    double last_ratio;
    uint32 max_ttl;
  } cycle;
  cache_options_t options;
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

#define DEFAULT_TARGET_CYCLE_TIME 86400  /* 24 hours */

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

static int init(struct cache *c,unsigned int cachesize,cache_options_t *options)
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

  taia_now(&c->cycle.start);
  c->cycle.last_ratio = 0.0;

  if (options) {
    byte_copy(&c->options,sizeof(cache_options_t),options);
  } else {
    c->options.resize_mode = TARGET_CYCLE_TIME;
    c->options.target_cycle_time = DEFAULT_TARGET_CYCLE_TIME;
  }

  return 1;
}

static int resize_cache(struct cache *c,unsigned int newsize)
{
  struct cache *new;
  unsigned int keylen;
  char *key;
  unsigned int datalen;
  char *data;
  struct tai expire;
  struct tai now;
  uint32 ttl;
  uint32 pos;
  uint32 end;
  resize_mode_t saved_mode;

  printf("*** resizing\n");fflush(stdout);

  /* is cache empty? */
  if (c->writer == c->hsize) {
    return init(c,newsize,&c->options);
  }

  new = (struct cache *)cache_t_new(newsize,&c->options);
  if (!new) return 0;

  saved_mode = new->options.resize_mode;
  new->options.resize_mode = NONE;

  pos = c->oldest == c->unused ? c->hsize : c->oldest;
  end = c->unused;

  printf("c->hsize %d\n",c->hsize);fflush(stdout);
  printf("c->oldest %d\n",c->oldest);fflush(stdout);
  printf("c->unused %d\n",c->unused);fflush(stdout);
  printf("pos %d\n",pos);fflush(stdout);
  printf("end %d\n",end);fflush(stdout);

  while (pos < end) {

    keylen = get4(c,pos + 4);
    key = c->x + pos + 20;

    tai_unpack(c->x + pos + 12,&expire);
    tai_now(&now);

    if (tai_less(&now,&expire)) {
      tai_sub(&expire,&expire,&now);
      ttl = tai_approx(&expire);

      datalen = get4(c,pos + 8);
      if (datalen > c->size - pos - 20 - keylen) cache_impossible();

      data = c->x + pos + 20 + keylen;

      cache_t_set(new,key,keylen,data,datalen,ttl);
    }

    pos += keylen + datalen + 20;
//printf("pos %d\n",pos);fflush(stdout);
    if (pos >= c->unused && c->oldest != c->unused) {
      printf("hey!\n");fflush(stdout);
      pos = c->hsize;
      end = c->writer;
    }

  }

  new->options.resize_mode = saved_mode;

  if (c->x) {
    alloc_free(c->x);
  }

  /* update old cache structure with new cacahe data */
  byte_copy(c,sizeof(struct cache),new);

  /* delete new cache structure */
  alloc_free(new);

  printf("*** done resizing\n");fflush(stdout);

  return 1;
}

int cache_t_resize(cache_t cache,unsigned int newsize)
{
  return resize_cache((struct cache *)cache,newsize);
}

/*
 * Determine target cycle time based on config options.
 */
static double get_target_cycle_time(struct cache *c)
{
  uint32 target = 0;

  switch(c->options.resize_mode) {

    case TARGET_CYCLE_TIME:
    target = c->options.target_cycle_time;
    break; 

    case MAX_TTL:
    target = c->cycle.max_ttl;
    break;

    default:
    target = 0;
    break;
  }

  return (double)target;
}

/*
 * Determine whether cache should be resized. This is by calculating the ratio of the
 * target cycle time to the actual cycle time.
 *
 * This function takes the last actual cycle time as input and returns the calculated ratio
 * and new size by reference. The return value is a flag indicating whether the cache should
 * be resized.
 */
static int should_resize(struct cache *c,double cycle_time,double *ratio,unsigned int *newsize)
{
  int resize = 0;
  double target_cycle_time;

  *ratio = 0;
  *newsize = 0;

  target_cycle_time = get_target_cycle_time(c);

  if (target_cycle_time && cycle_time) {

    *ratio = target_cycle_time / cycle_time;

    *newsize = c->size * (*ratio) * 1.1; /* add 10% */
    if (*newsize > MAXCACHESIZE) *newsize = MAXCACHESIZE;
    if (*newsize < MINCACHESIZE) *newsize = MINCACHESIZE;

    /*
     * Only consider resize if ratio has been high or low for two cycles in a row, to try
     * and avoid volatility in cache resizing from a spike or lull. Also, don't bother 
     * resizing if the current size is already at the defined limit.
     */ 
    if (c->cycle.last_ratio) {
      resize = (*ratio > 1.0 && c->cycle.last_ratio > 1.0 && c->size < MAXCACHESIZE)
            || (*ratio < 0.5 && c->cycle.last_ratio < 0.5 && c->size > MINCACHESIZE);
    }
  }

  return resize;
}

/*
 * Check whether cache should be resized, and if so, resize it. Returns 1 if cache
 * was resized, 0 otherwise.
 */
static int check_for_resize(struct cache *c)
{
  struct taia now;
  struct taia elapsed;
  double cycle_time;
  double ratio;
  int resize = 0;
  uint32 motion;
  unsigned int oldsize;
  unsigned int newsize;
  char *new;

  taia_now(&now);
  taia_sub(&elapsed,&now,&c->cycle.start);

  cycle_time = taia_approx(&elapsed);

  if (c->options.resize_mode != NONE && cycle_time) {

    resize = should_resize(c,cycle_time,&ratio,&newsize);

    c->cycle.last_ratio = ratio;

    if (resize) {
      oldsize = c->size; /* the init call will modify c->size */
      //resize = init(c,newsize,&c->options);
      resize = resize_cache(c,newsize);
      if (c->options.resize_callback) {
        (*c->options.resize_callback)(ratio,cycle_time,oldsize,newsize,resize);
      }
      if (resize) return 1;
    }
  }

  c->cycle.max_ttl = 0;
  taia_now(&c->cycle.start);

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
      if (check_for_resize(c)) {
        cache_t_set(cache,key,keylen,data,datalen,ttl); /* call again if cache was resized */
        return;
      }
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

  if (ttl > c->cycle.max_ttl) c->cycle.max_ttl = ttl;

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
cache_t cache_t_new(unsigned int cachesize,cache_options_t *options) {

  struct cache *c = (struct cache *)alloc(sizeof(struct cache));
  byte_zero(c,sizeof(struct cache));

  if (init(c,cachesize,options)) {
    return (cache_t)c;
  }

  return 0;
}

/*
 * Re-initialize existing cache.
 */
int cache_t_init(cache_t cache,unsigned int cachesize,cache_options_t *options) {
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

int cache_init(unsigned int cachesize,cache_options_t *options)
{
  if (!default_cache) {
    default_cache = cache_t_new(cachesize,options);
    return default_cache != 0;
  }

  return init(default_cache,cachesize,options);
}
