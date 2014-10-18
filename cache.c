#include "alloc.h"
#include "byte.h"
#include "uint32.h"
#include "exit.h"
#include "tai.h"
#include "cache.h"

uint64 cache_motion = 0;

struct cache {
  char *x;
  uint32 size;
  uint32 hsize;
  uint32 writer;
  uint32 oldest;
  uint32 unused;
  uint64 cache_motion;
};

static cache_t default_cache;

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

static int init(struct cache *c, unsigned int cachesize)
{
  if (c->x) {
    alloc_free(c->x);
    c->x = 0;
  }

  if (cachesize > 1000000000) cachesize = 1000000000;
  if (cachesize < 100) cachesize = 100;
  c->size = cachesize;

  c->hsize = 4;
  while (c->hsize <= (c->size >> 5)) c->hsize <<= 1;

  c->x = alloc(c->size);
  if (!c->x) return 0;
  byte_zero(c->x,c->size);

  c->writer = c->hsize;
  c->oldest = c->size;
  c->unused = c->size;

  return 1;
}

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

        /* if expire is 0, cache doesn't expire */
        if (tai_approx(&expire)) {
          /* use passed in timestamp, if available */
          if (stamp) {
            tai_uint(&now,stamp->x);
          } else {
            tai_now(&now);
          }
          if (tai_less(&expire,&now)) return 0;
        }

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

  /* if ttl is 0, cache doesn't expire */
  tai_uint(&expire,ttl);
  if (ttl) {
    tai_now(&now);
    tai_add(&expire,&expire,&now);
  }

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

int cache_t_init(cache_t cache, unsigned int cachesize) {
  return init((struct cache *)cache, cachesize);
}

cache_t cache_t_new(unsigned int cachesize) {

  struct cache *c = (struct cache *)alloc(sizeof(struct cache));
  byte_zero(c,sizeof(struct cache));

  if (init(c,cachesize)) {
    return (cache_t)c;
  }

  return 0;
}

void cache_t_delete(cache_t cache) {

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

int cache_init(unsigned int cachesize) {

  if (!default_cache) {
    default_cache = cache_t_new(cachesize);
    return default_cache != 0;
  }

  return init(default_cache,cachesize);
}
