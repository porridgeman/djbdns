#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "okclient.h"
#include "byte.h"
#include "ip4.h"
#include "taia.h"

static struct okclient_stats expected; /* static object is initialized to 0 */

static int failed = 0;

static void print_stat(char *stat, uint32 observed, uint32 expected)
{
	printf("  %s\t observed: %d\t expected: %d\t %s\n", stat, observed, expected,
		observed == expected ? "" : "*** failure ***");
}

static void print_stats()
{
	struct okclient_stats observed;

	okclient_get_stats(&observed);

	printf("\nstats:\n");
	print_stat("cache hits", observed.cache_hits, expected.cache_hits);
	print_stat("cache misses", observed.cache_misses, expected.cache_misses);
	print_stat("stat calls", observed.stat_calls, expected.stat_calls);
	printf("\n");
}

static void check_stat(char *stat, uint32 observed, uint32 expected)
{
	if (observed != expected) {
		printf("*** failure ***  %s\t observed: %d\t expected: %d\n", stat, observed, expected);
		failed = 1;
	}
}

static void check_stats()
{
	struct okclient_stats observed;
	okclient_get_stats(&observed);

	check_stat("cache hits", observed.cache_hits, expected.cache_hits);
	check_stat("cache misses", observed.cache_misses, expected.cache_misses);
	check_stat("stat calls", observed.stat_calls, expected.stat_calls);
}

static void checkip(const char *ipstr, int expected)
{
	char ip[4];
	struct taia stamp;
	int ok;

	taia_now(&stamp);

	ip4_scan(ipstr,ip);
	ok = okclient(ip,&stamp);

	printf("  okclient(%s) = %d   %s\n", ipstr, ok, ok != expected ? "*** failure ***" : "");
}

int main(int argc,char **argv)
{
	char ip[4];

	/*
	 * Create ip/ directory containing files:
	 * 
	 * 127.0.0.1
	 * 206
	 * 74.125.239
	 * 98.138
	 * 98.139.183.24
	 */

	printf("\ntest with loaded cache\n\n");

	okclient_init_cache(1);

	checkip("127.0.0.1", 1); expected.cache_hits++;
	checkip("127.0.0.1", 1); expected.cache_hits++; 
	checkip("127.0.0.1", 1); expected.cache_hits++;
	check_stats();
	checkip("127.0.0.2", 0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("127.0.0.2", 0); expected.cache_hits++;
	checkip("74.125.239.24", 1); expected.cache_hits++;
	checkip("74.125.239.15", 1); expected.cache_hits++;
	checkip("74.125.239.24", 1); expected.cache_hits++;
	checkip("74.125.239.24", 1); expected.cache_hits++;
	checkip("74.125.239.12", 1); expected.cache_hits++;
	checkip("74.125.239.35", 1); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.230", 0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("99.120.1.230", 0); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.231", 0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("99.120.1.231", 0); expected.cache_hits++;
	checkip("99.120.1.231", 0); expected.cache_hits++;
	checkip("99.120.1.231", 0); expected.cache_hits++;
	check_stats();

	print_stats();

	printf("test with unloaded cache\n\n"); /* cache will load lazily */
	
	okclient_init_cache(0);
	okclient_clear_stats();
	byte_zero(&expected,sizeof(struct okclient_stats));

	checkip("127.0.0.1", 1); expected.cache_misses++; expected.stat_calls += 1;
	check_stats();
	checkip("127.0.0.1", 1); expected.cache_hits++; 
	checkip("127.0.0.1", 1); expected.cache_hits++;
	check_stats();
	checkip("127.0.0.2", 0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("127.0.0.2", 0); expected.cache_hits++;
 	check_stats();
	checkip("74.125.239.24", 1); expected.cache_misses++; expected.stat_calls += 2;
	check_stats();
	checkip("74.125.239.15", 1); expected.cache_hits++;
	checkip("74.125.239.24", 1); expected.cache_hits++;
	checkip("74.125.239.24", 1); expected.cache_hits++;
	checkip("74.125.239.12", 1); expected.cache_hits++;
	checkip("74.125.239.35", 1); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.230", 0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("99.120.1.230", 0); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.231", 0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();check_stats();
	checkip("99.120.1.231", 0); expected.cache_hits++;
	checkip("99.120.1.231", 0); expected.cache_hits++;
	checkip("99.120.1.231", 0); expected.cache_hits++;
	check_stats();

	print_stats();

	printf("test TTL\n\n");

	okclient_set_cache_ttl(5); /* set TTL to 5 seconds */

	okclient_init_cache(0);
	okclient_clear_stats();
	byte_zero(&expected,sizeof(struct okclient_stats));

	checkip("127.0.0.1", 1); expected.cache_misses++; expected.stat_calls += 1;
	check_stats();
	checkip("127.0.0.1", 1); expected.cache_hits++; 
	check_stats();

	sleep(6); /* sleep 6 seconds, cache entry should be expired */

	checkip("127.0.0.1", 1); expected.cache_misses++; expected.stat_calls += 1;
	check_stats();
	checkip("127.0.0.1", 1); expected.cache_hits++; 
	check_stats();

	print_stats();

	printf("\ntests complete, %s\n", failed ? "*** failed ***" : "no failures");
}
