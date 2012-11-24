#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "emq.h"

#define GREEN(text) "\033[0;32m" text "\033[0;0m"
#define RED(text) "\033[0;31m" text "\033[0;0m"
#define YELLOW(text) "\033[0;33m" text "\033[0;0m"
#define MAGENTA(text) "\033[0;35m" text "\033[0;0m"

#define CHECK_STATUS(text, status) \
	if (status == EMQ_STATUS_OK) \
		printf(GREEN("[Success]") " %s\n", text); \
	else \
		printf(RED("[Error]") " %s\n", text);

#define ADDR "localhost"

#define TEST_MESSAGE "Hello EagleMQ"
#define MESSAGES 10

static int message_counter = 0;

pthread_t thread;

void *worker(void *data)
{
	emq_client *client = emq_tcp_connect(ADDR, EMQ_DEFAULT_PORT);
	emq_msg *msg = emq_msg_create(TEST_MESSAGE, strlen(TEST_MESSAGE) + 1, EMQ_ZEROCOPY_ON);
	int status;
	int i;

	if (client != NULL) {
		printf(YELLOW("[Success]") " [Worker] Connected to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);

		status = emq_auth(client, "eagle", "eagle");
		CHECK_STATUS("[Worker] Auth", status);

		status = emq_queue_declare(client, ".queue-test");
		CHECK_STATUS("[Worker] Queue declare", status);

		for (i = 0; i < MESSAGES; i++) {
			status = emq_queue_push(client, ".queue-test", msg);
			CHECK_STATUS("[Worker] Queue push", status);
			sleep(1);
		}

		emq_disconnect(client);
	} else {
		printf(RED("[Error]") " Error connect to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);
	}

	emq_msg_release(msg);

	pthread_exit(NULL);
}

int queue_message_callback(emq_client *client, const char *name, emq_msg *msg)
{
	int status;

	printf(YELLOW("[Success]") " [Event] Message \'%s in queue %s\n", (char*)emq_msg_data(msg), name);

	if (++message_counter >= MESSAGES) {
		status = emq_queue_unsubscribe(client, ".queue-test");
		CHECK_STATUS("[Event] Queue unsubscribe", status);
		emq_msg_release(msg);
		return 1;
	}

	emq_msg_release(msg);
	return 0;
}

void init_worker(void)
{
	if (pthread_create(&thread, NULL, worker, NULL)) {
		printf(RED("Error create thread\n"));
		exit(-1);
	}
}

int main(int argc, char *argv[])
{
	emq_client *client = emq_tcp_connect(ADDR, EMQ_DEFAULT_PORT);
	int status;

	printf(MAGENTA("This is a simple example of using libemq\n"));
	printf(MAGENTA("libemq version: %d.%d\n"), EMQ_VERSION_MAJOR, EMQ_VERSION_MINOR);

	if (client != NULL)
	{
		printf(YELLOW("[Success]") " Connected to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);

		status = emq_auth(client, "eagle", "eagle");
		CHECK_STATUS("Auth", status);

		status = emq_queue_create(client, ".queue-test", 10, 100, EMQ_QUEUE_FORCE_PUSH | EMQ_QUEUE_AUTODELETE);
		CHECK_STATUS("Queue create", status);

		status = emq_queue_declare(client, ".queue-test");
		CHECK_STATUS("Queue declare", status);

		status = emq_queue_subscribe(client, ".queue-test", EMQ_QUEUE_SUBSCRIBE_MSG, queue_message_callback);
		CHECK_STATUS("Queue subscribe", status);

		init_worker();

		status = emq_process(client);
		CHECK_STATUS("Queue process", status);

		emq_disconnect(client);
	} else {
		printf(RED("[Error]") " Error connect to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);
	}

	return 0;
}
