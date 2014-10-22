#include "buffer.h"
#include "exit.h"
#include "cache.h"
#include "str.h"
#include "alloc.h"
#include "byte.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int failure_count = 0;
static int resized = 0;
static unsigned int current_size = 0;

static void set(const char *key, const char *data,uint32 ttl) {
  cache_set(key,str_len(key),data,str_len(data)+1,ttl);
}

static void set_t(cache_t cache,const char *key, const char *data,uint32 ttl) {
  cache_t_set(cache,key,str_len(key),data,str_len(data)+1,ttl);
}

static char *get(const char *key) {
  char *data;
  uint32 datalen;
  uint32 ttl;

  return cache_get(key,str_len(key),&datalen,&ttl);
}

static char *get_t(cache_t cache,const char *key) {
  char *data;
  uint32 datalen;
  uint32 ttl;

  return cache_t_get(cache,key,str_len(key),&datalen,&ttl,0);
}

static void get_and_test(const char *key, const char *expected) {
  char *data;

  data = get(key);

  string("  cache_get "); string(key); space();
  string(data ? data : "0"); line();

  if ((!expected && data) || (expected && !data) || (data && expected && str_diff(data,expected))) {
    string("*** failure ***   expected: "); string(expected ? expected : "0"); line(); 
    failure_count++;
  }
}

#define TEST_DATA "206.190.36.45"
#define TEST_KEY "XXXXXXXXXX.test.com"
static char *test_key() {
  static char keybuf[sizeof(TEST_KEY)];
  static  unsigned int i = 0;
  sprintf(keybuf, "%010u.test.com", ++i);
  return keybuf;
}

static uint32 ttl(uint32 max_ttl)
{
  /* return a random ttl between max_ttl / 2 and max_ttl, inclusive */
  return (rand() % (max_ttl / 2 + 1)) + (max_ttl / 2);
}

static int get_cycle_size(unsigned int cachesize)
{
  int size;
  cache_t cache;

  cache = cache_t_new(cachesize, 0);

  if (!cache) {
    printf("failed creating cache of size %d\n",cachesize);
    _exit(111);
  }

  /* set initial entry in the cache */
  set_t(cache,TEST_KEY,TEST_DATA,86400);

  /* set entries in the cache until the initial entry is overwritten */
  for (size = 0; get_t(cache,TEST_KEY); size++) set_t(cache,test_key(),TEST_DATA,86400);

  cache_t_destroy(cache);

  return size;
}

static void resize_callback(double ratio,double cycle_time,uint32 oldsize,uint32 newsize,int success)
{
  printf("\n    resize!\n");
  printf("    ratio = %lf\n",ratio);
  printf("    oldsize = %u\n",oldsize);
  printf("    newsize = %u\n",newsize);
  printf("    target cycle time = %lf\n",ratio * cycle_time);
  printf("    %s\n",success ? "success" : "failure");

  current_size = newsize;
  resized = 1;
}

static void resize_test(cache_options_t *options,uint32 max_ttl)
{
  int i;
  int expected;
  int cycle_size;

  current_size = 1000;

  if (!cache_init(current_size, options)) _exit(111);

  printf("\n  add entries at 1000 per second (should grow by a lot)\n");
  expected = 1;
  for (i = 0; i < 10000 && !resized; i++) {
    usleep(1000); /* add 1000 per second */
    set(test_key(),TEST_DATA,ttl(max_ttl));
  }
  if (resized != expected) printf("\n    * was %sexpected to resize\n",expected ? " " : "not ");
  resized = 0;
  cycle_size = get_cycle_size(current_size);


  printf("\n  add 2 cycles at 1000 per second (shouldn't resize, at least not by much)\n");
  expected = 0;
  for (i = 0; i < cycle_size * 2 && !resized; i++) {
    usleep(1000); /* add 1000 per second */
    set(test_key(),TEST_DATA,ttl(max_ttl));
  }
  if (resized != expected) printf("\n    * was %sexpected to resize\n",expected ? " " : "not ");
  resized = 0;
  cycle_size = get_cycle_size(current_size);


  printf("\n  add 2 cycles at < 500 per second (should shrink by about half)\n");
  expected = 1;
  for (i = 0; i < cycle_size * 2 && !resized; i++) {
    usleep(2100); /* add less than 500 per second */
    set(test_key(),TEST_DATA,ttl(max_ttl));
  }
  if (resized != expected) printf("\n    * was %sexpected to resize\n",expected ? " " : "not ");
  resized = 0;
  cycle_size = get_cycle_size(current_size);


  printf("\n  add 2 cycles at 1000 per second (should grow to roughly double)\n");
  expected = 1;
  for (i = 0; i < cycle_size * 2 && !resized; i++) {
    usleep(1000); /* add 1000 per second */
    set(test_key(),TEST_DATA,ttl(max_ttl));
  }
  if (resized != expected) printf("\n    * was %sexpected to resize\n",expected ? " " : "not ");
  resized = 0;
  cycle_size = get_cycle_size(current_size);
}

static cache_options_t *get_options(resize_mode_t resize_mode,uint32 target_cycle_time,
  void (*resize_callback)(double,uint32,uint32,int))
{
  static cache_options_t options;

  options.resize_mode = resize_mode;
  options.target_cycle_time = target_cycle_time;
  options.resize_callback = resize_callback;

  return &options;
}

static void test_resize()
{
  printf("\n\ntesting with resize mode TARGET_CYCLE_TIME, target cycle time should be 5\n");
  resize_test(get_options(TARGET_CYCLE_TIME,5,resize_callback),86400);

  printf("\n\ntesting with resize mode MAX_TTL, target cycle time should be 6\n");
  resize_test(get_options(MAX_TTL,0,resize_callback),6);

  printf("\nresize tests complete\n");
}

int main(int argc,char **argv)
{
  int i;
  char *x;
  char *y;
  unsigned int u;
  uint32 ttl;

  srand(time(NULL));

  if (!cache_init(200,0)) _exit(111);

  if (*argv) ++argv;

  while (x = *argv++) {
    i = str_chr(x,':');
    if (x[i])
      cache_set(x,i,x + i + 1,str_len(x) - i - 1,86400);
    else {
      y = cache_get(x,i,&u,&ttl);
      if (y)
        buffer_put(buffer_1,y,u);
      buffer_puts(buffer_1,"\n");
    }
  }

  buffer_flush(buffer_1);

  test_resize();

  _exit(0);
}
