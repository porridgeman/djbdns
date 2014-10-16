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


}
