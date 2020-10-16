/**
 * @file ipc.cc
 * @author yuwangliang (wotsen@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2020-10-03
 * 
 * @copyright Copyright (c) 2020 yuwangliang
 * 
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "co/posix_ipc.h"

using namespace ipc;

int main(void) {
	Pipe p;

	char buf[512] = "hello world";

	// printf("read : %d\n", p.read(buf, 512, 4000));

	printf("write len %d\n", p.write(buf, strlen(buf), 3000));
	printf("write len %d\n", p.write(buf, strlen(buf)));

	memset(buf, 0, sizeof(buf));

	p.read(buf, 512, 4000);

	printf("read : %s\n", buf);

	PosixMsgQueue mq("/ipc_test", 'd');

	printf("mq send : %d\n", mq.write("hello world", strlen("hello world")));
	memset(buf, 0, sizeof(buf));
	printf("mq read : %d\n", mq.read(buf, 11));

	printf("read : %s, %d %s\n", buf, errno, strerror(errno));

	mq.destroy();

	return 0;
}