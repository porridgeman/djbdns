#include <sys/types.h>
#include <sys/stat.h>
#include "str.h"
#include "ip4.h"
#include "okclient.h"
#include "cache.h"

#define CACHESIZE 1000000
#define TTL 1800 // TODO is this in seconds?
static int initialized = 0;
static cache_t cache;

static char fn[3 + IP4_FMT];

int match_file(char ip[4])
{
  struct stat st;
  int i;

  fn[0] = 'i';
  fn[1] = 'p';
  fn[2] = '/';
  fn[3 + ip4_fmt(fn + 3,ip)] = 0;

  for (;;) {
    if (stat(fn,&st) == 0) return 1;
    /* treat temporary error as rejection */
    i = str_rchr(fn,'.');
    if (!fn[i]) return 0;
    fn[i] = 0;
  }
}

int okclient(char ip[4])
{
  char *cached;
  unsigned int cachedlen;
  uint32 ttl;
  int result;
  char data;

  if (!initialized) {
    cache = cache_t_new(CACHESIZE);
    initialized = 1;
  }

  cached = cache_t_get(cache,ip,4,&cachedlen,&ttl);
  if (cached) {
    result = *cached;
  } else {
    result = match_file(ip);
    data = result;
    cache_t_set(cache,ip,4,&data,sizeof(data),TTL);
  }

  return result;
}
