#include "buffer.h"
#include "exit.h"
#include "cache.h"
#include "str.h"

static void string(const char *s)
{
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
  cache_set(key,str_len(key),data,str_len(data),86400);
}

static void delete(const char *key) {
  cache_delete(key,str_len(key));
}

static void get(const char *key, const char *expected) {
  char *data;
  uint32 datalen;
  uint32 ttl;

  data = cache_get(key,str_len(key),&datalen,&ttl);

  string("  cache_get"); space();
  string(key); space();
  string(data ? data : "0"); line();

  if (!expected && data || expected && !data || data && expected && str_diff(data,expected)) {
    string("*** failure ***   expected: "); string(expected ? expected : "0"); line(); 
  }
}

static void test_cache_delete() {
  if (!cache_init(20000)) _exit(111);

  string("simple test\n");
  set("yahoo.com", "206.190.36.45");
  get("yahoo.com", "206.190.36.45");
  delete("yahoo.com");
  get("yahoo.com", 0);

  string("delete multiple entries with same key\n");
  set("yahoo.com", "206.190.36.45");
  set("yahoo.com", "206.190.36.45");
  get("yahoo.com", "206.190.36.45");
  delete("yahoo.com");
  get("yahoo.com", 0);

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
