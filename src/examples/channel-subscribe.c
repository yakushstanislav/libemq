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

pthread_t thread1, thread2;

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

		for (i = 0; i < MESSAGES / 2; i++) {
			status = emq_channel_publish(client, ".channel-test", "world.russia.moscow", msg);
			CHECK_STATUS("[Worker] Channel publish", status);
			status = emq_channel_publish(client, ".channel-test", "world.belarus.minsk", msg);
			CHECK_STATUS("[Worker] Channel publish", status);
			sleep(1);
		}

		emq_disconnect(client);
	} else {
		printf(RED("[Error]") " Error connect to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);
	}

	emq_msg_release(msg);

	pthread_exit(NULL);
}

int channel_message_callback(emq_client *client, int type, const char *name,
	const char *topic, const char *pattern, emq_msg *msg)
{
	printf(YELLOW("[Success]") " [Event] Message \'%s\' (channel: %s, pattern: %s, topic: %s) \n",
		(char*)emq_msg_data(msg), name, pattern, topic);

	if (++message_counter >= MESSAGES) {
		emq_noack_enable(client);
		printf(YELLOW("[Success]") " [Event] Channel punsubscribe\n");
		emq_channel_punsubscribe(client, ".channel-test", "world.belarus.*");
		emq_noack_disable(client);
		emq_msg_release(msg);
		return 1;
	}

	emq_msg_release(msg);

	return 0;
}

void init_worker(pthread_t *thread)
{
	if (pthread_create(thread, NULL, worker, NULL)) {
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

		status = emq_channel_create(client, ".channel-test", EMQ_CHANNEL_AUTODELETE | EMQ_CHANNEL_ROUND_ROBIN);
		CHECK_STATUS("Channel create", status);

		status = emq_channel_psubscribe(client, ".channel-test", "world.belarus.*", channel_message_callback);
		CHECK_STATUS("Channel psubscribe", status);

		init_worker(&thread1);
		init_worker(&thread2);

		status = emq_process(client);
		CHECK_STATUS("Channel process", status);

		emq_disconnect(client);
	} else {
		printf(RED("[Error]") " Error connect to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);
	}

	return 0;
}
