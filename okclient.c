#include <sys/types.h>
#include <sys/stat.h>
#include "str.h"
#include "ip4.h"
#include "okclient.h"
#include "cache.h"
#include "byte.h"
#include <stdio.h>

#define CACHESIZE 1000000
#define TTL 0
static int initialized = 0;
static cache_t cache;

static char fn[3 + IP4_FMT];

char *get_cached(char ip[4]) {
  char *cached;
  unsigned int cachedlen;
  uint32 ttl;
  static char fmt[IP4_FMT];
  int i;

  fmt[ip4_fmt(fmt,ip)] = 0;

  for (;;) {
    cached = cache_t_get(cache,fmt,str_len(fmt),&cachedlen,&ttl);
    if (cached) {
      //printf("get succeeded for %s\n", fmt);
      return cached;
    }
    i = str_rchr(fmt,'.');
    if (!fmt[i]) {
      //printf("get failed for %s\n", fmt);
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
    if (stat(fn,&st) == 0) {

      /* cache success */
      result = 1;
      cache_t_set(cache,fn+3,str_len(fn+3),&result,sizeof(result),TTL);  // TODO same ttl for success and rejection?
      //printf("  add success to cache %s\n",fn+3);fflush(stdout);
      return result;
    }
    
    /* treat temporary error as rejection */
    i = str_rchr(fn,'.');
    if (!fn[i]) {
      
      /* cache rejection */
      result = 0;
      rejected[ip4_fmt(rejected,ip)] = 0;
      cache_t_set(cache,rejected,str_len(rejected),&result,sizeof(result),TTL);
      //printf("  add rejection to cache %s\n",rejected);fflush(stdout);
      
      return result;
    }
    fn[i] = 0;
  }
}

int okclient(char ip[4])
{
  char *cached;

  if (!initialized) {
    cache = cache_t_new(CACHESIZE);
    initialized = 1;
  }

  cached = get_cached(ip);
  //if (cached) printf("  got from cache %d.%d.%d.%d   %d\n", ip[0],ip[1],ip[2],ip[3],*cached);fflush(stdout);
  return cached ? *cached : match_file(ip);
}
