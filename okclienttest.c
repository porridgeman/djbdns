#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "okclient.h"
#include "ip4.h"
#include "tai.h"

static void checkip(const char *ipstr, int expected) {
	char ip[4];
	int ok;

	ip4_scan(ipstr,ip);
	ok = okclient(ip);

	printf("okclient(%s) = %d   %s\n", ipstr, ok, ok != expected ? "*" : "");
}

int main(int argc,char **argv) {

	char ip[4];

	checkip("127.0.0.1", 1);
	checkip("127.0.0.1", 1);
	checkip("127.0.0.2", 0);
	checkip("127.0.0.2", 0);

	{
		struct tai now;
		struct tai start;
		struct tai diff;
		int i;

		tai_now(&start);
		for (i = 0; i < 10000000; i++) {
			tai_now(&now);
		}
		tai_sub(&diff,&now,&start);

		printf("diff %lu\n", diff.x);
	}

	{
		struct stat st;

		struct tai now;
		struct tai start;
		struct tai diff;
		int i;

		tai_now(&start);
		for (i = 0; i < 10000000; i++) {
			stat("ip/127.0.0.2",&st);
		}
		tai_now(&now);

		tai_sub(&diff,&now,&start);

		printf("diff %lu\n", diff.x);
	}

	{
		struct tai expire;

		tai_uint(&expire,0);
		printf("expire %lu\n", expire.x);
	}

}
