#include "buffer.h"
#include "exit.h"
#include "cache.h"
#include "str.h"
#include "alloc.h"
#include "byte.h"
#include <stdio.h>

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

static void wrap_test(int reverse) {
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
  if (!deleted) { string("failed to alloc memory for deleted array"); line(); exit(111);}
  byte_zero(deleted,sizeof(int));

  /* test that all other entries are still there */
  for (i = 0; i < size; i++) get_and_test(test_key(i),deleted[i] ? 0 : TEST_DATA);

  line();

  /* delete entries one by one, testing all entries on each pass */
  if (reverse) {
    for (j = size - 1; j >= 0; j--) {
      delete(test_key(j));
      deleted[j] = 1;
      for (i = 0; i < size; i++) get_and_test(test_key(i),deleted[i] ? 0 : TEST_DATA);
      line();
    }
  } else {
    for (j = 0; j < size; j++) {
      delete(test_key(j));
      deleted[j] = 1;
      for (i = 0; i < size; i++) get_and_test(test_key(i),deleted[i] ? 0 : TEST_DATA);
      line();
    }
  } 

  alloc_free(deleted);
}

static void test_cache_delete() {
  if (!cache_init(500)) _exit(111);

  string("\nsimple test\n");
  set("yahoo.com","206.190.36.45");
  get_and_test("yahoo.com","206.190.36.45");
  delete("yahoo.com");
  get_and_test("yahoo.com",0);

  string("\ndelete multiple entries with same key\n");
  set("yahoo.com","206.190.36.45");
  set("yahoo.com","206.190.36.45");
  get_and_test("yahoo.com","206.190.36.45");
  delete("yahoo.com");
  get_and_test("yahoo.com", 0);

  string("\ndelete non existent entry\n");
  delete("www.google.com");

  string("\nwrap entries and delete\n");
  wrap_test(0);

  string("\nwrap entries and delete in reverse order\n");
  wrap_test(1);

  string("test_cache_delete completed with "); integer(failure_count);
  string(" error"); string(failure_count == 1 ? "" : "s"); line();
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
