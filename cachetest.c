#include "buffer.h"
#include "exit.h"
#include "cache.h"
#include "str.h"
#include "alloc.h"
#include "byte.h"
#include <stdio.h>
#include <unistd.h>

static int failure_count = 0;

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

static char *get(const char *key) {
  char *data;
  uint32 datalen;
  uint32 ttl;

  return cache_get(key,str_len(key),&datalen,&ttl);
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

static void resize_callback(double ratio,uint32 oldsize,uint32 newsize,int resized)
{
  printf("ratio = %lf  oldsize = %u  newsize = %u  %s\n", 
    ratio,oldsize,newsize,resized ? "success" : "failure");
}

static void test_resize()
{
  int i;
  int total;
  cache_options options = {
    TARGET_CYCLE_TIME, /* allow resize, use target_cycle_time */
    10,                /* target_cycle_time */
    resize_callback
  };

  if (!cache_init(1000, &options)) _exit(111);

  total = 0;
  for (i = 0; i < 10000; i++,total++) {
    usleep(5000); /* add 200 per second */
    set(test_key(total),TEST_DATA,86400);
  }

  for (i = 0; i < 10000; i++,total++) {
    usleep(11000); /* add less than 100 per second */
    set(test_key(total),TEST_DATA,86400);
  }

  for (i = 0; i < 10000; i++,total++) {
    usleep(5000); /* add 200 per second */
    set(test_key(total),TEST_DATA,86400);
  }
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
