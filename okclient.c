#include <sys/types.h>
#include <sys/stat.h>
#include "str.h"
#include "ip4.h"
#include "okclient.h"
#include "cache.h"
#include "byte.h"
#include <stdio.h>
#include <dirent.h>
#include <errno.h>

#define CACHESIZE 1000000
#define DEFAULT_TTL 300    /* 5 minutes */
static int initialized = 0;
static cache_t cache;
static uint32 ttl = DEFAULT_TTL;

static struct okclient_stats stats; /* static struct initialized to 0 */

static char fn[3 + IP4_FMT];

static void load_cache()
{
  if (!initialized) {
    cache = cache_t_new(CACHESIZE);
    initialized = 1;
  }

  DIR  *d;
  struct dirent *dir;
  d = opendir("ip");
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (str_diff(dir->d_name,".") && str_diff(dir->d_name,"..")) {
        char result = 1;
        cache_t_set(cache,dir->d_name,str_len(dir->d_name),&result,sizeof(result),ttl);
      }
    }
    closedir(d);
  }
}

static char *get_cached(char ip[4],struct taia *start) {
  char *cached;
  unsigned int cachedlen;
  uint32 ttl;
  static char fmt[IP4_FMT];
  int i;
  struct tai now;

  fmt[ip4_fmt(fmt,ip)] = 0;

  for (;;) {
    cached = cache_t_get(cache,fmt,str_len(fmt),&cachedlen,&ttl,start ? &start->sec : 0);
    if (cached) {
      return cached;
    }
    i = str_rchr(fmt,'.');
    if (!fmt[i]) {
      return 0;
    }
    fmt[i] = 0;
  }
}

static int match_file(char ip[4])
{
  struct stat st;
  char result;
  int i;
  char rejected[IP4_FMT];

  fn[0] = 'i';
  fn[1] = 'p';
  fn[2] = '/';
  fn[3 + ip4_fmt(fn + 3,ip)] = 0;

  for (;;) {
    stats.stat_calls++;
    if (stat(fn,&st) == 0) {

      /* cache success */
      result = 1;
      cache_t_set(cache,fn+3,str_len(fn+3),&result,sizeof(result),ttl);

      return result;
    }
    
    /* treat temporary error as rejection */
    i = str_rchr(fn,'.');
    if (!fn[i]) {
      
      /* cache rejection */
      if (errno == ENOENT) { /* No such file or directory */
        result = 0;
        rejected[ip4_fmt(rejected,ip)] = 0;
        cache_t_set(cache,rejected,str_len(rejected),&result,sizeof(result),ttl);
      }

      return result;
    }
    fn[i] = 0;
  }
}

static void clear_stats()
{
  byte_zero(&stats,sizeof(struct okclient_stats));
}

void okclient_get_stats(struct okclient_stats *st)
{
  byte_copy(st,sizeof(struct okclient_stats),&stats);
}

void okclient_clear_stats()
{
  clear_stats();
}

void okclient_set_cache_ttl(uint32 newttl)
{
  ttl = newttl;
}

void okclient_init_cache(int load)
{
  if (!initialized) {
    cache = cache_t_new(CACHESIZE);
    initialized = 1;
  } else {
    cache_t_init(cache,CACHESIZE);
  }

  clear_stats();

  if (load) {
    load_cache();
  }
}

int okclient(char ip[4],struct taia *start)
{
  char *cached;

  if (!initialized) {
    cache = cache_t_new(CACHESIZE);
    initialized = 1;
  }

  cached = get_cached(ip,start);

  if (cached) {
    stats.cache_hits++;
    return *cached;
  }

  stats.cache_misses++;

  return match_file(ip);
}

