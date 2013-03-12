#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

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

/* Basic methods */
static void basic(emq_client *client)
{
	emq_status stat; /* variable for contain server statistics */
	int status;

	/* Warning: don't forget authenticate in the server */
	/* Authentication */
	status = emq_auth(client, "eagle", "eagle");
	CHECK_STATUS("Auth", status);

	/* Ping server */
	status = emq_ping(client);
	CHECK_STATUS("Ping", status);

	/* Get statistics from the server */
	status = emq_stat(client, &stat);
	CHECK_STATUS("Stat", status);

	/* Print server statistics */
	printf(YELLOW("[Status]\n"));
	printf("Version\n");
	printf(" Major: %d\n", stat.version.major);
	printf(" Minor: %d\n", stat.version.minor);
	printf(" Patch: %d\n", stat.version.patch);
	printf("Uptime: %d\n", stat.uptime);
	printf("CPU sys: %f\n", stat.used_cpu_sys);
	printf("CPU user: %f\n", stat.used_cpu_user);
	printf("Memory: %d\n", stat.used_memory);
	printf("Memory RSS: %d\n", stat.used_memory_rss);
	printf("Memory fragmentation: %f\n", stat.fragmentation_ratio);
	printf("Clients: %d\n", stat.clients);
	printf("Users: %d\n", stat.users);
	printf("Queues: %d\n", stat.queues);
}

/* Example user management */
static void user_management(emq_client *client)
{
	emq_list *users; /* users */
	emq_user *user; /* user */
	emq_list_iterator iter; /* list iterator */
	emq_list_node *node; /* list node */
	int status;

	/* Create new user with name "first.user" and password "password" */
	status = emq_user_create(client, "first.user", "password", EMQ_QUEUE_PERM);
	CHECK_STATUS("User create", status);

	/* Create new user with name "user_2" and password "password" */
	status = emq_user_create(client, "user_2", "password", EMQ_QUEUE_PERM);
	CHECK_STATUS("User create", status);

	/* Request the list of users */
	users = emq_user_list(client);
	printf(YELLOW("[User list]\n"));
	if (users != NULL) {
		/* Enumeration all users */
		emq_list_rewind(users, &iter);
		while ((node = emq_list_next(&iter)) != NULL)
		{
			user = EMQ_LIST_VALUE(node);
			printf("Username: %s\n", user->name);
			printf("Password: %s\n", user->password);
			printf("Permissions: %" PRIu64 "\n", user->perm);
			printf("-----------------------\n");
		}
		emq_list_release(users);
	} else {
		printf(RED("[Error]") " emq_user_list(%s)\n", EMQ_GET_ERROR(client));
	}

	/* Rename user with name "user_2" to "user" */
	status = emq_user_rename(client, "user_2", "user");
	CHECK_STATUS("User rename", status);

	/* Setting the new permissions */
	status = emq_user_set_perm(client, "user", EMQ_QUEUE_PERM | EMQ_ADMIN_PERM);
	CHECK_STATUS("User set perm", status);

	/* Delete user with name "first.user" */
	status = emq_user_delete(client, "first.user");
	CHECK_STATUS("User delete", status);

	/* Delete user with name "user" */
	status = emq_user_delete(client, "user");
	CHECK_STATUS("User delete", status);
}

/* Example queue management */
static void queue_management(emq_client *client)
{
	const char *test_message = "test message"; /* test message data */
	emq_list *queues; /* queues */
	emq_queue *queue; /* queue */
	emq_list_iterator iter; /* list iterator */
	emq_list_node *node; /* list node */
	emq_msg *msg; /* message for send to the server */
	emq_msg *queue_msg;
	int messages;
	int queue_size;
	int status;

	/* Create queue with name ".queue_1" */
	status = emq_queue_create(client, ".queue_1", EMQ_MAX_MSG, EMQ_MAX_MSG_SIZE, EMQ_QUEUE_NONE);
	CHECK_STATUS("Queue create", status);

	/* Create queue with name "queue-2" */
	status = emq_queue_create(client, "queue-2", EMQ_MAX_MSG/2, EMQ_MAX_MSG_SIZE/100, EMQ_QUEUE_NONE);
	CHECK_STATUS("Queue create", status);

	/* Warning: don't forget declare queue */
	/* Declare queue with name ".queue_1" */
	status = emq_queue_declare(client, ".queue_1");
	CHECK_STATUS("Queue declare", status);

	/* Declare queue with name "queue-2" */
	status = emq_queue_declare(client, "queue-2");
	CHECK_STATUS("Queue declare", status);

	/* Check on exist queue with name ".queue_1" */
	status = emq_queue_exist(client, ".queue_1");
	printf(YELLOW("[Success]") " Queue \".queue_1\" exist: %d\n", status);

	/* Check on exist queue with name "queue-2" */
	status = emq_queue_exist(client, "queue-2");
	printf(YELLOW("[Success]") " Queue \"queue-2\" exist: %d\n", status);

	/* Check on exist queue with name "not-exist-queue" */
	status = emq_queue_exist(client, "not-exist-queue");
	printf(YELLOW("[Success]") " Queue \"not-exist-queue\" exist: %d\n", status);

	/* Request the queue list */
	queues = emq_queue_list(client);
	printf(YELLOW("[Queue list]\n"));
	if (queues != NULL) {
		/* Enumeration of all queues */
		emq_list_rewind(queues, &iter);
		while ((node = emq_list_next(&iter)) != NULL)
		{
			queue = EMQ_LIST_VALUE(node);
			printf("Name: %s\n", queue->name);
			printf("Max msg: %u\n", queue->max_msg);
			printf("Max msg size: %d\n", queue->max_msg_size);
			printf("Flags: %d\n", queue->flags);
			printf("Size: %d\n", queue->size);
			printf("Declared clients %d\n", queue->declared_clients);
			printf("Subscribed clients %d\n", queue->subscribed_clients);
			printf("-----------------------\n");
		}
		emq_list_release(queues);
	} else {
		printf(RED("[Error]") " emq_queue_list(%s)\n", EMQ_GET_ERROR(client));
	}

	/* Create message for push to the queue */
	msg = emq_msg_create((void*)test_message, strlen(test_message) + 1, EMQ_ZEROCOPY_ON);
	if (msg == NULL) {
		printf(RED("[Error]") " emq_msg_create\n");
		exit(1);
	}

	/* Push five messages to queues with name ".queue_1" and "queue-2" */
	for (messages = 0; messages < 5; messages++) {
		status = emq_queue_push(client, ".queue_1", msg);
		CHECK_STATUS("Queue push", status);

		status = emq_queue_push(client, "queue-2", msg);
		CHECK_STATUS("Queue push", status);
		printf("-----------------------\n");
	}

	/* Get size queue with name ".queue_1" */
	queue_size = emq_queue_size(client, ".queue_1");
	printf(YELLOW("[Success]") " Queue \".queue_1\" size: %d\n", queue_size);

	/* Get all messages from queue ".queue_1" */
	for (messages = 0; messages < queue_size; messages++) {
		queue_msg = emq_queue_get(client, ".queue_1");
		if (queue_msg != NULL)
		{
			printf(YELLOW("[Success]") " Get message: %s(%zu)\n",
				(char*)emq_msg_data(queue_msg), emq_msg_size(queue_msg));
			emq_msg_release(queue_msg);
		}
	}

	/* Get size queue with name ".queue_1" again */
	queue_size = emq_queue_size(client, ".queue_1");
	printf(YELLOW("[Success]") " Queue \".queue_1\" size: %d\n", queue_size);

	/* Get size queue with name ".queue-2" */
	queue_size = emq_queue_size(client, "queue-2");
	printf(YELLOW("[Success]") " Queue \"queue-2\" size: %d\n", queue_size);

	/* Pop all messages from queue "queue-2" */
	for (messages = 0; messages < queue_size; messages++) {
		queue_msg = emq_queue_pop(client, "queue-2");
		if (queue_msg != NULL)
		{
			printf(YELLOW("[Success]") " Pop message: %s(%zu)\n",
				(char*)emq_msg_data(queue_msg), emq_msg_size(queue_msg));
			emq_msg_release(queue_msg);
		}
	}

	/* Get size queue with name "queue-2" */
	queue_size = emq_queue_size(client, "queue-2");
	printf(YELLOW("[Success]") " Queue \"queue-2\" size: %d\n", queue_size);

	/* Purge queue with name ".queue_1" */
	status = emq_queue_purge(client, ".queue_1");
	CHECK_STATUS("Queue purge", status);

	/* Get size queue with name ".queue_1" again */
	queue_size = emq_queue_size(client, ".queue_1");
	printf(YELLOW("[Success]") " Queue \".queue_1\" size: %d\n", queue_size);

	/* Delete queue with name ".queue_1" */
	status = emq_queue_delete(client, ".queue_1");
	CHECK_STATUS("Queue delete", status);

	/* Delete queue with name "queue-2" */
	status = emq_queue_delete(client, "queue-2");
	CHECK_STATUS("Queue delete", status);

	/* Release message */
	emq_msg_release(msg);
}

/* Example route management */
static void route_management(emq_client *client)
{
	const char *test_message = "test message"; /* test message data */
	emq_list *routes; /* routes */
	emq_route *route; /* route */
	emq_list *route_keys; /* route keys */
	emq_route_key *route_key; /* route key */
	emq_list_iterator iter; /* list iterator */
	emq_list_node *node; /* list node */
	emq_msg *msg; /* message for send to the server */
	int messages;
	int queue_size;
	int status;

	/* Create route with name ".route_1" */
	status = emq_route_create(client, ".route_1", EMQ_ROUTE_NONE);
	CHECK_STATUS("Route create", status);

	/* Create queue with name ".queue_1" */
	status = emq_queue_create(client, ".queue_1", EMQ_MAX_MSG, EMQ_MAX_MSG_SIZE, EMQ_QUEUE_NONE);
	CHECK_STATUS("Queue create", status);

	/* Create queue with name ".queue_2" */
	status = emq_queue_create(client, ".queue_2", EMQ_MAX_MSG, EMQ_MAX_MSG_SIZE, EMQ_QUEUE_NONE);
	CHECK_STATUS("Queue create", status);

	/* Check on exist route with name ".route_1" */
	status = emq_route_exist(client, ".route_1");
	printf(YELLOW("[Success]") " Route \".route_1\" exist: %d\n", status);

	/* Check on exist route with name "not-exist-route" */
	status = emq_route_exist(client, "not-exist-route");
	printf(YELLOW("[Success]") " Route \"not-exist-route\" exist: %d\n", status);

	/* Request the route list */
	routes = emq_route_list(client);
	printf(YELLOW("[Route list]\n"));
	if (routes != NULL) {
		/* Enumeration of all routes */
		emq_list_rewind(routes, &iter);
		while ((node = emq_list_next(&iter)) != NULL)
		{
			route = EMQ_LIST_VALUE(node);
			printf("Name: %s\n", route->name);
			printf("Flags: %d\n", route->flags);
			printf("Keys: %d\n", route->keys);
			printf("-----------------------\n");
		}
		emq_list_release(routes);
	} else {
		printf(RED("[Error]") " emq_queue_list(%s)\n", EMQ_GET_ERROR(client));
	}

	/* Bind queue with name ".queue_1" to route with name ".route_1" and key "key1" */
	status = emq_route_bind(client, ".route_1", ".queue_1", "key1");
	CHECK_STATUS("Route bind", status);

	/* Bind queue with name ".queue_2" to route with name ".route_1" and key "key1" */
	status = emq_route_bind(client, ".route_1", ".queue_2", "key1");
	CHECK_STATUS("Route bind", status);

	/* Bind queue with name ".queue_2" to route with name ".route_1" and key "key2" */
	status = emq_route_bind(client, ".route_1", ".queue_2", "key2");
	CHECK_STATUS("Route bind", status);

	/* Request the keys from route with name ".route_1" */
	route_keys = emq_route_keys(client, ".route_1");
	printf(YELLOW("[Route keys]\n"));
	if (route_keys != NULL) {
		/* Enumeration of all route keys */
		emq_list_rewind(route_keys, &iter);
		while ((node = emq_list_next(&iter)) != NULL)
		{
			route_key = EMQ_LIST_VALUE(node);
			printf("Key: %s\n", route_key->key);
			printf("Queue: %s\n", route_key->queue);
			printf("-----------------------\n");
		}
		emq_list_release(route_keys);
	} else {
		printf(RED("[Error]") " emq_queue_list(%s)\n", EMQ_GET_ERROR(client));
	}

	/* Create message for push to the route */
	msg = emq_msg_create((void*)test_message, strlen(test_message) + 1, EMQ_ZEROCOPY_ON);
	if (msg == NULL) {
		printf(RED("[Error]") " emq_msg_create\n");
		exit(1);
	}

	/* Push five messages to route with name ".route_1" */
	for (messages = 0; messages < 5; messages++) {
		status = emq_route_push(client, ".route_1", "key1", msg);
		CHECK_STATUS("Route push", status);
		status = emq_route_push(client, ".route_1", "key2", msg);
		CHECK_STATUS("Route push", status);
		printf("-----------------------\n");
	}

	/* Release message */
	emq_msg_release(msg);

	/* Get size queue with name ".queue_1" */
	queue_size = emq_queue_size(client, ".queue_1");
	printf(YELLOW("[Success]") " Queue \".queue_1\" size: %d\n", queue_size);

	/* Get size queue with name ".queue_2" */
	queue_size = emq_queue_size(client, ".queue_2");
	printf(YELLOW("[Success]") " Queue \".queue_2\" size: %d\n", queue_size);

	/* Unbind queue with name ".queue_1" from route with name ".route_1" and key "key1" */
	status = emq_route_unbind(client, ".route_1", ".queue_1", "key1");
	CHECK_STATUS("Route unbind", status);

	/* Unbind queue with name ".queue_1" from route with name ".route_1" and key "key1" */
	status = emq_route_unbind(client, ".route_1", ".queue_2", "key1");
	CHECK_STATUS("Route unbind", status);

	/* Unbind queue with name ".queue_1" from route with name ".route_1" and key "key2" */
	status = emq_route_unbind(client, ".route_1", ".queue_2", "key2");
	CHECK_STATUS("Route unbind", status);

	/* Delete route with name ".route_1" */
	status = emq_route_delete(client, ".route_1");
	CHECK_STATUS("Route delete", status);

	/* Delete queue with name ".queue_1" */
	status = emq_queue_delete(client, ".queue_1");
	CHECK_STATUS("Queue delete", status);

	/* Delete queue with name ".queue_2" */
	status = emq_queue_delete(client, ".queue_2");
	CHECK_STATUS("Queue delete", status);
}

int main(int argc, char *argv[])
{
	/* Connect to server */
	emq_client *client = emq_tcp_connect(ADDR, EMQ_DEFAULT_PORT);
	int status;

	printf(MAGENTA("This is a simple example of using libemq\n"));
	printf(MAGENTA("libemq version: %d.%d\n"), EMQ_VERSION_MAJOR, EMQ_VERSION_MINOR);

	/* Connected? */
	if (client != NULL)
	{
		printf(YELLOW("[Success]") " Connected to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);

		basic(client);
		user_management(client);
		queue_management(client);
		route_management(client);

		/* Delete all users and queues (you can use EMQ_FLUSH_ALL) */
		status = emq_flush(client, EMQ_FLUSH_USER | EMQ_FLUSH_QUEUE | EMQ_FLUSH_ROUTE);
		CHECK_STATUS("Flush all data", status);

		/* Disconnect from server */
		emq_disconnect(client);
	} else {
		printf(RED("[Error]") " Error connect to %s:%d\n", ADDR, EMQ_DEFAULT_PORT);
	}

	return 0;
}
