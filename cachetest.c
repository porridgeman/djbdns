#include "buffer.h"
#include "exit.h"
#include "cache.h"
#include "str.h"
#include "alloc.h"
#include "byte.h"
#include <stdio.h>
#include <unistd.h>

static int failure_count = 0;
static int resized = 0;
static unsigned int current_size = 0;

static void string(const char *s)
{
  buffer_puts(buffer_1,s);
}

static void integer(const int i)
{
  char s[20];
  sprintf(s,"%d",i);
  buffer_puts(buffer_1,s);
}

static void line(void)
{
  string("\n");
  buffer_flush(buffer_1);
}

static void space(void)
{
  string(" ");
}

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
static char *test_key(int i) {
  static char keybuf[sizeof(TEST_KEY)];
  sprintf(keybuf, "%010d.test.com", i);
  return keybuf;
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
  for (size = 0; get_t(cache,TEST_KEY); size++) set_t(cache,test_key(size),TEST_DATA,86400);

  cache_t_destroy(cache);

  return size;
}

static void resize_callback(double ratio,uint32 oldsize,uint32 newsize,int success)
{
  printf("  ratio = %lf  oldsize = %u  newsize = %u  %s\n", 
    ratio,oldsize,newsize,success ? "success" : "failure");

  current_size = newsize;
  resized = 1;
}

static void test_resize()
{
  int i;
  int total;
  int cycle_size;
  cache_options options = {
    TARGET_CYCLE_TIME, /* allow resize, use target_cycle_time */
    5,                /* target_cycle_time */
    resize_callback
  };

  //cycle_size = get_cycle_size(1000);

  current_size = 1000;

  if (!cache_init(current_size, &options)) _exit(111);

  total = 0;

  printf("\nadd entries at 1000 per second (should grow by quite a bit)\n");
  for (i = 0; i < 10000 && !resized; i++,total++) {
    usleep(1000); /* add 1000 per second */
    set(test_key(total),TEST_DATA,86400);
  }
  printf("resized = %d\n",resized);
  resized = 0;
  cycle_size = get_cycle_size(current_size);
  printf("cycle size for current size %d is %d\n",current_size,cycle_size);


  printf("\nadd 2 cycles at 1000 per second (shouldn't resize)\n");
  for (i = 0; i < cycle_size * 2 && !resized; i++,total++) {
    usleep(1000); /* add 1000 per second */
    set(test_key(total),TEST_DATA,86400);
  }
  printf("resized = %d\n",resized);
  resized = 0;
  cycle_size = get_cycle_size(current_size);
  printf("cycle size for current size %d is %d\n",current_size,cycle_size);


  printf("\nadd 2 cycles at < 500 per second (should shrink to about half)\n");
  for (i = 0; i < cycle_size * 2 && !resized; i++,total++) {
    usleep(2100); /* add less than 500 per second */
    set(test_key(total),TEST_DATA,86400);
  }
  printf("resized = %d\n",resized);
  resized = 0;
  cycle_size = get_cycle_size(current_size);
  printf("cycle size for current size %d is %d\n",current_size,cycle_size);

  printf("\nadd 2 cycles at 1000 per second (should roughly double)\n");
  for (i = 0; i < cycle_size * 2 && !resized; i++,total++) {
    usleep(1000); /* add 1000 per second */
    set(test_key(total),TEST_DATA,86400);
  }
  printf("resized = %d\n",resized);
  resized = 0;
  cycle_size = get_cycle_size(current_size);
  printf("cycle size for current size %d is %d\n",current_size,cycle_size);
}

int main(int argc,char **argv)
{
  int i;
  char *x;
  char *y;
  unsigned int u;
  uint32 ttl;

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
