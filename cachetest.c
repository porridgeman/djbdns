#include "buffer.h"
#include "exit.h"
#include "cache.h"
#include "str.h"
#include "alloc.h"
#include "byte.h"
#include <stdio.h>

static int failure_count = 0;

static void set(const char *key, const char *data) {
  cache_set(key,str_len(key),data,str_len(data)+1,86400);
}

static void delete(const char *key) {
  cache_delete(key,str_len(key));
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

  printf("  cache_get %s %s\n",key,data ? data : "0");

  if ((!expected && data) || (expected && !data) || (data && expected && str_diff(data,expected))) {
    printf("*** failure ***   expected: %s\n",expected ? expected : "0");
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

static void cycle_test(int reverse) {
  int i;
  int j;
  int size;
  int *deleted;

  /* set initial entry in the cache */
  set(TEST_KEY,TEST_DATA);

  /* set entries in the cache until the initial entry is overwritten */
  for (size = 0; get(TEST_KEY); size++) set(test_key(size),TEST_DATA);

  /* array to track deleted entries */
  deleted = (int *)alloc((size + 1) * sizeof(int));
  if (!deleted) { printf("failed to alloc memory for deleted array\n"); exit(111);}
  byte_zero(deleted,sizeof(int));

  /* test that all other entries are still there */
  for (i = 0; i < size; i++) get_and_test(test_key(i),deleted[i] ? 0 : TEST_DATA);

  printf("\n");

  /* delete entries one by one, testing all entries on each pass */
  if (reverse) {
    for (j = size - 1; j >= 0; j--) {
      delete(test_key(j));
      deleted[j] = 1;
      for (i = 0; i < size; i++) get_and_test(test_key(i),deleted[i] ? 0 : TEST_DATA);
      printf("\n");
    }
  } else {
    for (j = 0; j < size; j++) {
      delete(test_key(j));
      deleted[j] = 1;
      for (i = 0; i < size; i++) get_and_test(test_key(i),deleted[i] ? 0 : TEST_DATA);
      printf("\n");
    }
  } 

  alloc_free(deleted);
}

static void test_cache_delete() {
  if (!cache_init(500)) _exit(111);

  printf("\nsimple test\n\n");
  set("yahoo.com","206.190.36.45");
  get_and_test("yahoo.com","206.190.36.45");
  delete("yahoo.com");
  get_and_test("yahoo.com",0);

  printf("\ndelete multiple entries with same key\n\n");
  set("yahoo.com","206.190.36.45");
  set("yahoo.com","206.190.36.45");
  get_and_test("yahoo.com","206.190.36.45");
  delete("yahoo.com");
  get_and_test("yahoo.com", 0);

  printf("\ndelete non existent entry\n");
  delete("www.google.com");

  printf("\ncycle cache and delete entries\n\n");
  cycle_test(0);

  printf("\ncycle cache and delete entries in reverse order\n\n");
  cycle_test(1);

  printf("test_cache_delete completed with %d error%s\n",failure_count,failure_count == 1 ? "" : "s");
}

int main(int argc,char **argv)
{
  int i;
  char *x;
  char *y;
  unsigned int u;
  uint32 ttl;

  if (!cache_init(200)) _exit(111);

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

  test_cache_delete();

  _exit(0);
}
