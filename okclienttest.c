#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "okclient.h"
#include "byte.h"
#include "ip4.h"
#include "taia.h"
#include "exit.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>

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

	failed |= ok != expected;

	printf("  okclient(%s) = %d   %s\n", ipstr, ok, ok != expected ? "*** failure ***" : "");
}

static void print_create_error_and_exit()
{
	printf("\nplease make sure the following files exist:\n");
	printf("ip/98.139.183.24\n");
	printf("ip/74.125.239\n");
	printf("ip/98.138\n");
	printf("ip/206\n");
	printf("and the following file doesn't exist:\n");
	printf("ip/97.130.180.20\n");
	_exit(1);
}

static void print_message_and_exit()
{
	printf("\nplease make sure the following files exist:\n");
	printf("ip/98.139.183.24\n");
	printf("ip/74.125.239\n");
	printf("ip/98.138\n");
	printf("ip/206\n");
	printf("and the following file doesn't exist:\n");
	printf("ip/97.130.180.20\n");
	_exit(1);
}

static void create_file(char *fn)
{
	FILE *fp;

	fp = fopen(fn, "w+");
	if (fp) {
		fclose(fp);
	} else {
		printf("unable to create or open file %s\n%s\n",fn,strerror(errno));
		print_message_and_exit();
	}
}

static void remove_file(char *fn)
{
	if (remove(fn) && errno != ENOENT) {
		printf("unable to remove file %s\n%s\n",fn,strerror(errno));
		print_message_and_exit();
	}
}

static void create_ip_dir()
{
	if (mkdir("ip", 0755) && errno != EEXIST) {
		printf("unable to create directory ip\n%s\n",strerror(errno));
		print_message_and_exit();
	}
	
	create_file("ip/98.139.183.24");
	create_file("ip/74.125.239");
	create_file("ip/98.138");
	create_file("ip/206");
	remove_file("ip/97.130.180.20"); /* in case it didn't gety removed during a previous test */
}

int main(int argc,char **argv)
{
	char ip[4];

	create_ip_dir();

	printf("\ntest with loaded cache\n\n");

	okclient_init_cache(1);

	checkip("98.139.183.24",1); expected.cache_hits++;
	checkip("98.139.183.24",1); expected.cache_hits++; 
	checkip("98.139.183.24",1); expected.cache_hits++;
	check_stats();
	checkip("98.139.183.25",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("98.139.183.25",0); expected.cache_hits++;
	checkip("98.139.183.24",1); expected.cache_hits++;
	checkip("74.125.239.24",1); expected.cache_hits++;
	checkip("74.125.239.15",1); expected.cache_hits++;
	checkip("74.125.239.24",1); expected.cache_hits++;
	checkip("74.125.239.24",1); expected.cache_hits++;
	checkip("74.125.239.12",1); expected.cache_hits++;
	checkip("74.125.239.35",1); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.230",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("99.120.1.230",0); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.231",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("99.120.1.231",0); expected.cache_hits++;
	checkip("99.120.1.231",0); expected.cache_hits++;
	checkip("99.120.1.231",0); expected.cache_hits++;
	check_stats();
	checkip("98.138.10.10",1); expected.cache_hits++;
	checkip("98.138.10.10",1); expected.cache_hits++;
	check_stats();
	checkip("206.10.10.10",1); expected.cache_hits++;
	checkip("206.10.10.10",1); expected.cache_hits++;	
	check_stats();	

	print_stats();

	printf("test with unloaded cache\n\n"); /* cache will load lazily */

	okclient_init_cache(0);
	okclient_clear_stats();
	byte_zero(&expected,sizeof(struct okclient_stats));

	checkip("98.139.183.24",1); expected.cache_misses++; expected.stat_calls += 1;
	check_stats();
	checkip("98.139.183.24",1); expected.cache_hits++; 
	checkip("98.139.183.24",1); expected.cache_hits++;
	check_stats();
	checkip("98.139.183.25",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("98.139.183.25",0); expected.cache_hits++;
	checkip("98.139.183.24",1); expected.cache_hits++;
 	check_stats();
	checkip("74.125.239.24",1); expected.cache_misses++; expected.stat_calls += 2;
	check_stats();
	checkip("74.125.239.15",1); expected.cache_hits++;
	checkip("74.125.239.24",1); expected.cache_hits++;
	checkip("74.125.239.24",1); expected.cache_hits++;
	checkip("74.125.239.12",1); expected.cache_hits++;
	checkip("74.125.239.35",1); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.230",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("99.120.1.230",0); expected.cache_hits++;
	check_stats();
	checkip("99.120.1.231",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();check_stats();
	checkip("99.120.1.231",0); expected.cache_hits++;
	checkip("99.120.1.231",0); expected.cache_hits++;
	checkip("99.120.1.231",0); expected.cache_hits++;
	check_stats();	
	checkip("98.138.10.10",1); expected.cache_misses++; expected.stat_calls += 3;
	checkip("98.138.10.10",1); expected.cache_hits++;
	check_stats();	
	checkip("206.10.10.10",1); expected.cache_misses++; expected.stat_calls += 4;
	checkip("206.10.10.10",1); expected.cache_hits++;	
	check_stats();

	print_stats();


	printf("test TTL\n\n");

	okclient_set_cache_ttl(2); /* set TTL to 2 seconds */

	okclient_init_cache(0);
	okclient_clear_stats();
	byte_zero(&expected,sizeof(struct okclient_stats));

	checkip("98.139.183.24",1); expected.cache_misses++; expected.stat_calls += 1;
	check_stats();
	checkip("98.139.183.24",1); expected.cache_hits++; 
	check_stats();

	sleep(3); /* sleep 3 seconds, cache entry should be expired */

	checkip("98.139.183.24",1); expected.cache_misses++; expected.stat_calls += 1;
	check_stats();
	checkip("98.139.183.24",1); expected.cache_hits++; 
	check_stats();

	print_stats();


	printf("test config change\n\n");

	okclient_set_cache_ttl(2); /* set TTL to 2 seconds */

	okclient_init_cache(0);
	okclient_clear_stats();
	byte_zero(&expected,sizeof(struct okclient_stats));

	checkip("97.130.180.20",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("97.130.180.20",0); expected.cache_hits++; 
	check_stats();

	create_file("ip/97.130.180.20");

	checkip("97.130.180.20",0); expected.cache_hits++; 
	check_stats();

	sleep(3); /* sleep 3 seconds, cache entry should be expired */

	checkip("97.130.180.20",1); expected.cache_misses++; expected.stat_calls += 1;
	check_stats();
	checkip("97.130.180.20",1); expected.cache_hits++;
	check_stats();

	remove_file("ip/97.130.180.20");

	checkip("97.130.180.20",1); expected.cache_hits++;
	check_stats();

	sleep(3); /* sleep 3 seconds, cache entry should be expired */

	checkip("97.130.180.20",0); expected.cache_misses++; expected.stat_calls += 4;
	check_stats();
	checkip("97.130.180.20",0); expected.cache_hits++;
	check_stats();


	print_stats();

	printf("\ntests complete, %s\n", failed ? "*** failed ***" : "no failures");
}
