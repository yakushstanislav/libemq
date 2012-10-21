#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "emq.h"

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT 7851
#define DEFAULT_USER_NAME "eagle"
#define DEFAULT_USER_PASSWORD "eagle"

#define DEFAULT_CLIENTS 50
#define DEFAULT_MESSAGES 10000
#define DEFAULT_MSG_SIZE 10

#define QUEUE_NAME ".queue-benchmark"

pthread_t *threads;
char *message_data;

static struct config {
	const char *host;
	int port;
	const char *unix_socket;
	const char *user_name;
	const char *user_password;
	int clients;
	int messages;
	int msg_size;
} config;

static long long mstime(void)
{
	struct timeval tv;
	long long mst;

	gettimeofday(&tv, NULL);
	mst = ((long)tv.tv_sec)*1000;
	mst += tv.tv_usec/1000;

	return mst;
}

static void generate_string(char *str, int len)
{
	int i, rand_char;

	srand(time(NULL));

	for (i = 0; i < len; ++i)
	{
		rand_char = rand() % (26 + 26 + 10);
		if (rand_char < 26) {
			str[i] = 'a' + rand_char;
		} else if (rand_char < 26 + 26) {
			str[i] = 'A' + rand_char - 26;
		} else {
			str[i] = '0' + rand_char - 26 - 26;
		}
	}

	str[len] = '\0';
}

static void *worker(void *data)
{
	emq_client *client;
	emq_msg *message;
	int msg = config.messages / config.clients;
	int status;
	int i;

	if (!config.unix_socket) {
		client = emq_tcp_connect(config.host, config.port);
	} else {
		client = emq_unix_connect(config.unix_socket);
	}

	if (client != NULL)
	{
		emq_auth(client, config.user_name, config.user_password);

		emq_queue_declare(client, QUEUE_NAME);

		message = emq_msg_create(message_data, config.msg_size, EMQ_ZEROCOPY_ON);

		for (i = 0; i < msg; i++) {
			emq_queue_push(client, QUEUE_NAME, message);
		}

		emq_msg_release(message);

		emq_disconnect(client);
	} else {
		printf("Error connect to server...\n");
	}

	pthread_exit(NULL);
}

static void setup_server(void)
{
	emq_client *client;
	int status;

	if (!config.unix_socket) {
		client = emq_tcp_connect(config.host, config.port);
	} else {
		client = emq_unix_connect(config.unix_socket);
	}

	if (client != NULL)
	{
		status = emq_auth(client, config.user_name, config.user_password);
		if (status != EMQ_STATUS_OK) {
			printf("Authorization error (%s/%s)\n", config.user_name, config.user_password);
			emq_disconnect(client);
			exit(-1);
		}

		status = emq_queue_exist(client, QUEUE_NAME);
		if (status == EMQ_STATUS_OK) {
			emq_queue_delete(client, QUEUE_NAME);
		}

		status = emq_queue_create(client, QUEUE_NAME, EMQ_MAX_MSG, EMQ_MAX_MSG_SIZE, 0);
		if (status != EMQ_STATUS_OK) {
			printf("Error create queue \'" QUEUE_NAME "\'\n");
			emq_disconnect(client);
			exit(-1);
		}

		emq_disconnect(client);
	} else {
		printf("Error connect to server...\n");
		exit(-1);
	}

}

static void cleanup_server(void)
{
	emq_client *client;
	int status;

	if (!config.unix_socket) {
		client = emq_tcp_connect(config.host, config.port);
	} else {
		client = emq_unix_connect(config.unix_socket);
	}

	if (client != NULL)
	{
		status = emq_auth(client, config.user_name, config.user_password);

		status = emq_queue_exist(client, QUEUE_NAME);
		if (status == EMQ_STATUS_OK) {
			emq_queue_delete(client, QUEUE_NAME);
		}

		emq_disconnect(client);
	} else {
		printf("Error connect to server...\n");
		exit(-1);
	}
}

static void init_threads(void)
{
	int i;

	threads = (pthread_t*)malloc(sizeof(pthread_t) * config.clients);

	if (!threads) {
		printf("Error allocate memory\n");
		exit(-1);
	}

	for (i = 0; i < config.clients; i++) {
		if (pthread_create(&threads[i], NULL, worker, NULL)) {
			printf("Error create thread %d\n", i);
			exit(-1);
		}
	}
}

static void process_threads(void)
{
	int i;

	for (i = 0; i < config.clients; i++) {
		if (pthread_join(threads[i], NULL)) {
			printf("Error process thread %d\n", i);
		}
	}
}

static void destroy_threads(void)
{
	free(threads);
}

static void init_message(void)
{
	int i;
	message_data = (char*)malloc(config.msg_size + 1);

	if (!message_data) {
		printf("Error allocate memory\n");
		exit(-1);
	}

	generate_string(message_data, config.msg_size);
}

static void destroy_message(void)
{
	free(message_data);
}

static void print_statistics(long long start, long long end)
{
	long long ms = end - start;
	float sec = (end - start)/1000.0;

	printf("===== Information =====\n");
	printf("Clients: %d\n", config.clients);
	printf("Total messages: %d\n", config.messages);
	printf("Message size: %d\n", config.msg_size);
	printf("===== Results =====\n");
	printf("%d requests completed in %lld miliseconds(%.2f seconds)\n", config.messages, ms, sec);
	printf("%.2f requests per second\n", config.messages/sec);
}

static int init_config(void)
{
	config.host = DEFAULT_HOST;
	config.port = DEFAULT_PORT;
	config.unix_socket = NULL;
	config.user_name = DEFAULT_USER_NAME;
	config.user_password = DEFAULT_USER_PASSWORD;
	config.clients = DEFAULT_CLIENTS;
	config.messages = DEFAULT_MESSAGES;
	config.msg_size = DEFAULT_MSG_SIZE;
}

static void usage(void)
{
	printf(
			"libemq simple benchmark\n"
			"Usage: benchmark [options]\n"
			"-h <hostname> - server IP (default: %s)\n"
			"-p <port> - server port (default: %d)\n"
			"-u <unix socket> - server socket\n"
			"-name <name> - user name (default: %s)\n"
			"-password <password> - user password (default: %s)\n"
			"-c <clients> - number of parallel connections (default: %d)\n"
			"-m <messages> - number of messages for all connections (default: %d)\n"
			"-s <message size> - size of one message (default: %d)\n"
			"-h - show this message and exit\n",
				DEFAULT_HOST, DEFAULT_PORT, DEFAULT_USER_NAME, DEFAULT_USER_PASSWORD,
				DEFAULT_CLIENTS, DEFAULT_MESSAGES, DEFAULT_MSG_SIZE);
}

static void parse_args(int argc, char *argv[])
{
	int i, last_arg;

	for (i = 1; i < argc; i++)
	{
		last_arg = i == argc - 1;

		if (!strcmp(argv[i], "-h") && !last_arg) {
			config.host = argv[i + 1];
		} else if (!strcmp(argv[i], "-p") && !last_arg) {
			config.port = atoi(argv[i + 1]);
		} else if (!strcmp(argv[i], "-u") && !last_arg) {
			config.unix_socket = argv[i + 1];
		} else if (!strcmp(argv[i], "-name") && !last_arg) {
			config.user_name = argv[i + 1];
		} else if (!strcmp(argv[i], "-password") && !last_arg) {
			config.user_password = argv[i + 1];
		} else if (!strcmp(argv[i], "-c") && !last_arg) {
			config.clients = atoi(argv[i + 1]);
		} else if (!strcmp(argv[i], "-m") && !last_arg) {
			config.messages = atoi(argv[i + 1]);
		} else if (!strcmp(argv[i], "-s") && !last_arg) {
			config.msg_size = atoi(argv[i + 1]);
		} else if (!strcmp(argv[i], "-h")) {
			usage();
			exit(0);
		}
	}
}

int main(int argc, char *argv[])
{
	long long start, end;

	init_config();
	parse_args(argc, argv);

	printf("Starting benchmarking...\n");
	start = mstime();

	setup_server();
	init_message();
	init_threads();

	process_threads();
	end = mstime();

	print_statistics(start, end);

	destroy_message();
	destroy_threads();
	cleanup_server();

	return 0;
}
