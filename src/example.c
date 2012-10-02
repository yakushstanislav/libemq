#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "emq.h"

#define CHECK_STATUS(text, status) \
	if (status == EMQ_STATUS_OK) \
		printf("[\033[0;32mSuccess\033[0;0m] %s\n", text); \
	else \
		printf("[\033[0;31mError\033[0;0m] %s\n", text); 

#define ADDR "localhost"
#define PORT 7851

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
	printf("[\033[0;33mStatus\033[0;0m]\n");
	printf("Version\n");
	printf(" Major: %d\n", stat.version.major);
	printf(" Minor: %d\n", stat.version.minor);
	printf(" Patch: %d\n", stat.version.patch);
	printf("Uptime: %d\n", stat.uptime);
	printf("CPU sys: %f\n", stat.used_cpu_sys);
	printf("CPU user: %f\n", stat.used_cpu_user);
	printf("Memory: %d\n", stat.used_memory);
	printf("Memory RSS: %d\n", stat.used_memory_rss);
	printf("Fragmentation: %f\n", stat.fragmentation_ratio);
	printf("RX: %d\n", stat.rx);
	printf("TX: %d\n", stat.tx);
	printf("Clients: %d\n", stat.clients);
	printf("Users: %d\n", stat.users);
	printf("Queues: %d\n", stat.queues);
}

/* Example user management */
static void user_management(emq_client *client)
{
	emq_user_perm perm = 0; /* variable for contain user permissions */
	emq_list *users; /* users */
	emq_user *user; /* user */
	emq_list_iterator iter; /* iterator */
	emq_list_node *node; /* list node */
	int status;

	/* Create permissions for the new users */
	EMQ_SET_PERM(perm, EMQ_QUEUE_PERM_BIT); /* User can manage queues */

	/* Create new user with name "first.user" and password "password" */
	status = emq_user_create(client, "first.user", "password", perm);
	CHECK_STATUS("User create", status);

	/* Create new user with name "user_2" and password "password" */
	status = emq_user_create(client, "user_2", "password", perm);
	CHECK_STATUS("User create", status);

	/* Request the list of users */
	users = emq_user_list(client);
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
		printf("[Error] emq_user_list(%s)\n", EMQ_GET_ERROR(client));
	}

	/* Rename user with name "user_2" to "user" */
	status = emq_user_rename(client, "user_2", "user");
	CHECK_STATUS("User rename", status);

	/* Update permissions */
	EMQ_SET_PERM(perm, EMQ_ADMIN_PERM_BIT); /* User is administrator */

	/* Setting the new permissions */
	status = emq_user_set_perm(client, "user", perm);
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
	emq_list_iterator iter; /* iterator */
	emq_list_node *node; /* list node */
	emq_msg *msg; /* message for send to the server */
	emq_msg *queue_msg;
	int messages;
	size_t queue_size;
	int status;

	/* Create queue with name ".queue_1" */
	status = emq_queue_create(client, ".queue_1", EMQ_MAX_MSG, EMQ_MAX_MSG_SIZE, 0);
	CHECK_STATUS("Queue create", status);
	
	/* Create queue with name "queue-2" */
	status = emq_queue_create(client, "queue-2", EMQ_MAX_MSG/2, EMQ_MAX_MSG_SIZE/100, 0);
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
	CHECK_STATUS("Queue exist", status);

	/* Check on exist queue with name "queue-2" */
	status = emq_queue_exist(client, "queue-2");
	CHECK_STATUS("Queue exist", status);

	/* Check on exist queue with name "not-exist-queue" */
	status = emq_queue_exist(client, "not-exist-queue");
	CHECK_STATUS("Queue exist", !status);

	/* Request the queue list */
	queues = emq_queue_list(client);
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
			printf("-----------------------\n");
		}
		emq_list_release(queues);
	} else {
		printf("[Error] emq_queue_list(%s)\n", EMQ_GET_ERROR(client));
	}

	/* Create message for push to the queue */
	msg = emq_msg_create((void*)test_message, strlen(test_message) + 1);
	if (msg == NULL) {
		printf("[Error] emq_msg_create\n");
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
	printf("[\033[0;33mSuccess\033[0;0m] Queue \".queue_1\" size: %zu\n", queue_size);

	/* Get all messages from queue ".queue_1" */
	for (messages = 0; messages < queue_size; messages++) {
		queue_msg = emq_queue_get(client, ".queue_1");
		if (queue_msg != NULL)
		{
			printf("[\033[0;33mSuccess\033[0;0m] Get message: %s(%zu)\n",
				(char*)emq_msg_data(queue_msg), emq_msg_size(queue_msg));
			emq_msg_release(queue_msg);
		}
	}

	/* Get size queue with name ".queue_1" again */
	queue_size = emq_queue_size(client, ".queue_1");
	printf("[\033[0;33mSuccess\033[0;0m] Queue \".queue_1\" size: %zu\n", queue_size);

	/* Get size queue with name ".queue-2" */
	queue_size = emq_queue_size(client, "queue-2");
	printf("[\033[0;33mSuccess\033[0;0m] Queue \"queue-2\" size: %zu\n", queue_size);

	/* Pop all messages from queue "queue-2" */
	for (messages = 0; messages < queue_size; messages++) {
		queue_msg = emq_queue_pop(client, "queue-2");
		if (queue_msg != NULL)
		{
			printf("[\033[0;33mSuccess\033[0;0m] Pop message: %s(%zu)\n",
				(char*)emq_msg_data(queue_msg), emq_msg_size(queue_msg));
			emq_msg_release(queue_msg);
		}
	}

	/* Get size queue with name "queue-2" */
	queue_size = emq_queue_size(client, "queue-2");
	printf("[\033[0;33mSuccess\033[0;0m] Queue \"queue-2\" size: %zu\n", queue_size);

	/* Purge queue with name ".queue_1" */
	status = emq_queue_purge(client, ".queue_1");
	CHECK_STATUS("Queue purge", status);

	/* Get size queue with name ".queue_1" again */
	queue_size = emq_queue_size(client, ".queue_1");
	printf("[\033[0;33mSuccess\033[0;0m] Queue \".queue_1\" size: %zu\n", queue_size);

	/* Delete queue with name ".queue_1" */
	status = emq_queue_delete(client, ".queue_1");
	CHECK_STATUS("Queue delete", status);

	/* Delete queue with name "queue-2" */
	status = emq_queue_delete(client, "queue-2");
	CHECK_STATUS("Queue delete", status);

	/* Release message */
	emq_msg_release(msg);
}

int main(int argc, char *argv[])
{
	/* Connect to server */
	emq_client *client = emq_tcp_connect(ADDR, PORT);

	/* Connected? */
	if (client != NULL)
	{
		printf("Connected to %s:%d\n", ADDR, PORT);

		basic(client);
		user_management(client);
		queue_management(client);

		/* Disconnect from server */
		emq_disconnect(client);
	} else {
		printf("Error connect to %s:%d\n", ADDR, PORT);
	}

	return 0;
}
