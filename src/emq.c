/*
   Copyright (c) 2012, Stanislav Yakush(st.yakush@yandex.ru)
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the libemq nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#include "emq.h"
#include "network.h"
#include "protocol.h"
#include "packet.h"

#define strlenz(str) (strlen(str) + 1)

#define EMQ_LIST_SET_FREE_METHOD(l, m) ((l)->free = (m))
#define EMQ_LIST_GET_FREE_METHOD(l) ((l)->free)

static const char *emq_error_array[] = {
	"",
	"Error allocate memory",
	"Error input data",
	"Error write request",
	"Error read request",
	"Response error",
	"Packet error",
	"Command error",
	"Access error",
	"Memory error",
	"Value not declared",
	"Value not found",
	"No data"
};

typedef struct emq_queue_subscription {
	char name[64];
	emq_msg_callback *callback;
} emq_queue_subscription;

typedef struct emq_channel_subscription {
	char name[64];
	char channel[32];
	emq_msg_callback *callback;
} emq_channel_subscription;

static emq_list *emq_list_init(void);
static int emq_list_add_value(emq_list *list, void *value);
static void emq_queue_subscription_list_free_handler(void *value);
static void emq_channel_subscription_list_free_handler(void *value);

static emq_client *emq_client_init(void)
{
	emq_client *client;

	client = (emq_client*)malloc(sizeof(*client));

	if (!client) {
		return NULL;
	}

	memset(client, 0, sizeof(*client));

	EMQ_CLEAR_ERROR(client);

	client->status = EMQ_STATUS_OK;
	client->request = (char*)malloc(EMQ_DEFAULT_REQUEST_SIZE);
	client->size = EMQ_DEFAULT_REQUEST_SIZE;
	client->pos = 0;
	client->noack = 0;
	client->fd = 0;
	client->queue_subscriptions = emq_list_init();
	client->channel_subscriptions = emq_list_init();

	if (!client->request) {
		free(client);
		return NULL;
	}

	if (!client->queue_subscriptions) {
		free(client->request);
		free(client);
	}

	if (!client->channel_subscriptions) {
		emq_list_release(client->queue_subscriptions);
		free(client->request);
		free(client);
	}

	EMQ_LIST_SET_FREE_METHOD(client->queue_subscriptions, emq_queue_subscription_list_free_handler);
	EMQ_LIST_SET_FREE_METHOD(client->channel_subscriptions, emq_channel_subscription_list_free_handler);

	return client;
}

static void emq_client_release(emq_client *client)
{
	emq_list_release(client->queue_subscriptions);
	emq_list_release(client->channel_subscriptions);
	free(client->request);
	free(client);
}

static void emq_client_set_error(emq_client *client, int error)
{
	snprintf(client->error, sizeof(client->error), "%s", emq_error_array[error]);
}

static emq_list *emq_list_init(void)
{
	emq_list *list;

	list = (emq_list*)malloc(sizeof(*list));
	if (!list) {
		return NULL;
	}

	list->head = list->tail = NULL;
	list->length = 0;
	list->free = NULL;

	return list;
}

static int emq_list_add_value(emq_list *list, void *value)
{
	emq_list_node *node;

	node = (emq_list_node*)malloc(sizeof(*node));
	if (!node) {
		return EMQ_STATUS_ERR;
	}

	node->value = value;

	if (list->length == 0) {
		list->head = list->tail = node;
		node->prev = node->next = NULL;
	} else {
		node->prev = NULL;
		node->next = list->head;
		list->head->prev = node;
		list->head = node;
	}

	list->length++;

	return EMQ_STATUS_OK;
}

void emq_list_delete_node(emq_list *list, emq_list_node *node)
{
	if (node->prev) {
		node->prev->next = node->next;
	} else {
		list->head = node->next;
	}

	if (node->next) {
		node->next->prev = node->prev;
	} else {
		list->tail = node->prev;
	}

	if (list->free) {
		list->free(node->value);
	}

	free(node);
	list->length--;
}

void emq_list_release(emq_list *list)
{
	emq_list_node *current, *next;
	size_t length;

	current = list->head;
	length = list->length;

	while (length--) {
		next = current->next;

		if (list->free) {
			list->free(current->value);
		}

		free(current);
		current = next;
	}

	free(list);
}

void emq_list_rewind(emq_list *list, emq_list_iterator *iter)
{
	iter->next = list->head;
}

emq_list_node *emq_list_next(emq_list_iterator *iter)
{
	emq_list_node *current = iter->next;

	if (current != NULL) {
		iter->next = current->next;
	}

	return current;
}

static emq_user *emq_user_init(void)
{
	emq_user *user;

	user = (emq_user*)malloc(sizeof(*user));
	if (!user) {
		return NULL;
	}

	memset(user, 0, sizeof(*user));

	return user;
}

static void emq_user_release(emq_user *user)
{
	free(user);
}

static emq_queue *emq_queue_init(void)
{
	emq_queue *queue;

	queue = (emq_queue*)calloc(1, sizeof(*queue));
	if (!queue) {
		return NULL;
	}

	return queue;
}

static void emq_queue_release(emq_queue *queue)
{
	free(queue);
}

static emq_route *emq_route_init(void)
{
	emq_route *route;

	route = (emq_route*)calloc(1, sizeof(*route));
	if (!route) {
		return NULL;
	}

	return route;
}

static void emq_route_release(emq_route *route)
{
	free(route);
}

static emq_channel *emq_channel_init(void)
{
	emq_channel *channel;

	channel = (emq_channel*)calloc(1, sizeof(*channel));
	if (!channel) {
		return NULL;
	}

	return channel;
}

static void emq_channel_release(emq_channel *channel)
{
	free(channel);
}

static emq_route_key *emq_route_key_init(void)
{
	emq_route_key *route_key;

	route_key = (emq_route_key*)calloc(1, sizeof(*route_key));
	if (!route_key) {
		return NULL;
	}

	return route_key;
}

static void emq_route_key_release(emq_route_key *route_key)
{
	free(route_key);
}

static emq_queue_subscription *emq_queue_subscription_create(const char *name, emq_msg_callback *callback)
{
	emq_queue_subscription *subscription;

	subscription = (emq_queue_subscription*)malloc(sizeof(*subscription));
	if (!subscription) {
		return NULL;
	}

	memset(subscription, 0, sizeof(*subscription));

	memcpy(subscription->name, name, strlenz(name));
	subscription->callback = callback;

	return subscription;
}

static void emq_queue_subscription_release(emq_queue_subscription *subscription)
{
	free(subscription);
}

static emq_channel_subscription *emq_channel_subscription_create(const char *name, const char *data, emq_msg_callback *callback)
{
	emq_channel_subscription *subscription;

	subscription = (emq_channel_subscription*)malloc(sizeof(*subscription));
	if (!subscription) {
		return NULL;
	}

	memset(subscription, 0, sizeof(*subscription));

	memcpy(subscription->name, name, strlenz(name));
	memcpy(subscription->channel, data, strlenz(data));
	subscription->callback = callback;

	return subscription;
}

static void emq_channel_subscription_release(emq_channel_subscription *subscription)
{
	free(subscription);
}

static void emq_user_list_free_handler(void *value)
{
	emq_user_release(value);
}

static void emq_queue_list_free_handler(void *value)
{
	emq_queue_release(value);
}

static void emq_route_list_free_handler(void *value)
{
	emq_route_release(value);
}

static void emq_channel_list_free_handler(void *value)
{
	emq_channel_release(value);
}

static void emq_route_key_list_free_handler(void *value)
{
	emq_route_key_release(value);
}

static void emq_queue_subscription_list_free_handler(void *value)
{
	emq_queue_subscription_release(value);
}

static void emq_channel_subscription_list_free_handler(void *value)
{
	emq_channel_subscription_release(value);
}

emq_msg *emq_msg_create(void *data, size_t size, int zero_copy)
{
	emq_msg *msg;

	msg = (emq_msg*)malloc(sizeof(*msg));
	if (!msg) {
		return NULL;
	}

	msg->size = size;
	msg->tag = 0;
	msg->expire = 0;
	msg->zero_copy = zero_copy;

	if (!zero_copy) {
		msg->data = malloc(size);
		if (!msg->data) {
			free(msg);
			return NULL;
		}
		memcpy(msg->data, data, size);
	} else {
		msg->data = data;
	}

	return msg;
}

emq_msg *emq_msg_copy(emq_msg *msg)
{
	emq_msg *new_msg;

	new_msg = (emq_msg*)malloc(sizeof(*new_msg));
	if (!new_msg) {
		return NULL;
	}

	new_msg->data = malloc(msg->size);
	new_msg->size = msg->size;
	new_msg->tag = msg->tag;
	new_msg->expire = msg->expire;

	if (!new_msg->data) {
		free(new_msg);
		return NULL;
	}

	memcpy(new_msg->data, msg->data, msg->size);

	return new_msg;
}

void emq_msg_expire(emq_msg *msg, emq_time time)
{
	msg->expire = time;
}

void *emq_msg_data(emq_msg *msg)
{
	return msg->data;
}

size_t emq_msg_size(emq_msg *msg)
{
	return msg->size;
}

emq_tag emq_msg_tag(emq_msg *msg)
{
	return msg->tag;
}

void emq_msg_release(emq_msg *msg)
{
	if (!msg->zero_copy) {
		free(msg->data);
	}

	free(msg);
}

emq_client *emq_tcp_connect(const char *addr, int port)
{
	emq_client *client = emq_client_init();

	if (!client) {
		return NULL;
	}

	EMQ_CLEAR_ERROR(client);

	if ((emq_client_tcp_connect(client, addr, port)) == EMQ_NET_ERR) {
		emq_client_release(client);
		return NULL;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return client;
}

emq_client *emq_unix_connect(const char *path)
{
	emq_client *client = emq_client_init();

	if (!client) {
		return NULL;
	}

	EMQ_CLEAR_ERROR(client);

	if ((emq_client_unix_connect(client, path)) == EMQ_NET_ERR) {
		emq_client_release(client);
		return NULL;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return client;
}

void emq_disconnect(emq_client *client)
{
	if (client != NULL) {
		emq_client_disconnect(client);
		emq_client_release(client);
	}
}

int emq_auth(emq_client *client, const char *name, const char *password)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_auth_request(client, name, password) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_AUTH, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_ping(emq_client *client)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_ping_request(client) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_PING, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_stat(emq_client *client, emq_status *status)
{
	protocol_response_stat response;

	EMQ_CLEAR_ERROR(client);

	if (emq_stat_request(client) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&response, sizeof(response)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&response.header, EMQ_PROTOCOL_CMD_STAT, sizeof(response.body)) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&response.header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&response.header));
		goto error;
	}

	memcpy(status, &response.body, sizeof(*status));

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_save(emq_client *client, uint8_t async)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_save_request(client, async) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_SAVE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_flush(emq_client *client, uint32_t flags)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_flush_request(client, flags) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_FLUSH, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_user_create(emq_client *client, const char *name, const char *password, emq_perm perm)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_user_create_request(client, name, password, perm) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_CREATE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

emq_list *emq_user_list(emq_client *client)
{
	protocol_response_header header;
	emq_user *user;
	emq_list *list;
	char *buffer;
	uint32_t i;

	EMQ_CLEAR_ERROR(client);

	if (emq_user_list_request(client) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_USER_LIST) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	buffer = (char*)malloc(header.bodylen);
	if (!buffer) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	if (emq_client_read(client, buffer, header.bodylen) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		free(buffer);
		goto error;
	}

	list = emq_list_init();
	if (!list) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		free(buffer);
		goto error;
	}

	EMQ_LIST_SET_FREE_METHOD(list, emq_user_list_free_handler);

	for (i = 0; i < header.bodylen;)
	{
		user = emq_user_init();
		if (!user) {
			emq_client_set_error(client, EMQ_ERROR_ALLOC);
			emq_list_release(list);
			free(buffer);
			goto error;
		}

		memcpy(user->name, buffer + i, 32);
		memcpy(user->password, buffer + i + 32, 32);
		memcpy(&user->perm, buffer + i + 64, sizeof(uint64_t));
		i += 64 + sizeof(uint64_t);

		emq_list_add_value(list, user);
	}

	free(buffer);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return list;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

int emq_user_rename(emq_client *client, const char *from, const char *to)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_user_rename_request(client, from, to) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_RENAME, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_user_set_perm(emq_client *client, const char *name, emq_perm perm)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_user_set_perm_request(client, name, perm) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_SET_PERM, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_user_delete(emq_client *client, const char *name)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_user_delete_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_DELETE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_create(emq_client *client, const char *name, uint32_t max_msg, uint32_t max_msg_size, uint32_t flags)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_create_request(client, name, max_msg, max_msg_size, flags) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_CREATE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_declare(emq_client *client, const char *name)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_declare_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_DECLARE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_exist(emq_client *client, const char *name)
{
	protocol_response_queue_exist response;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_exist_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&response.header, sizeof(response.header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&response.header, EMQ_PROTOCOL_CMD_QUEUE_EXIST,
			sizeof(response.body)) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&response.header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&response.header));
		goto error;
	}

	if (emq_client_read(client, (char*)&response.body, sizeof(response.body)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return response.body.status;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return 0;
}

emq_list *emq_queue_list(emq_client *client)
{
	protocol_response_header header;
	emq_queue *queue;
	emq_list *list;
	char *buffer;
	uint32_t i;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_list_request(client) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_QUEUE_LIST) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	buffer = (char*)malloc(header.bodylen);
	if (!buffer) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	if (emq_client_read(client, buffer, header.bodylen) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		free(buffer);
		goto error;
	}

	list = emq_list_init();
	if (!list) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		free(buffer);
		goto error;
	}

	EMQ_LIST_SET_FREE_METHOD(list, emq_queue_list_free_handler);

	for (i = 0; i < header.bodylen;)
	{
		queue = emq_queue_init();
		if (!queue) {
			emq_client_set_error(client, EMQ_ERROR_ALLOC);
			emq_list_release(list);
			free(buffer);
			goto error;
		}

		memcpy(queue->name, buffer + i, 64);
		i += 64;
		memcpy(&queue->max_msg, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&queue->max_msg_size, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&queue->flags, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&queue->size, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&queue->declared_clients, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&queue->subscribed_clients, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);

		emq_list_add_value(list, queue);
	}

	free(buffer);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return list;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

int emq_queue_rename(emq_client *client, const char *from, const char *to)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_rename_request(client, from, to) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_RENAME, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_size(emq_client *client, const char *name)
{
	protocol_response_queue_size response;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_size_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&response.header, sizeof(response.header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&response.header, EMQ_PROTOCOL_CMD_QUEUE_SIZE,
			sizeof(response.body)) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&response.header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&response.header));
		goto error;
	}

	if (emq_client_read(client, (char*)&response.body, sizeof(response.body)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return response.body.size;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return -1;
}

int emq_queue_push(emq_client *client, const char *name, emq_msg *msg)
{
	protocol_response_header header;
	struct iovec data[3];

	EMQ_CLEAR_ERROR(client);

	if (msg->size < 1) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_queue_push_request(client, name, sizeof(msg->expire) + msg->size) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	data[0].iov_base = client->request;
	data[0].iov_len = client->pos;
	data[1].iov_base = &msg->expire;
	data[1].iov_len = sizeof(msg->expire);
	data[2].iov_base = msg->data;
	data[2].iov_len = msg->size;

	if (emq_client_writev(client, data, 3) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_PUSH, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

static emq_msg *emq_read_message(emq_client *client, size_t size)
{
	emq_msg *msg;

	msg = (emq_msg*)malloc(sizeof(*msg));
	if (!msg) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		return NULL;
	}

	msg->data = malloc(size);
	msg->size = size;
	msg->tag = 0;
	msg->expire = 0;
	msg->zero_copy = EMQ_ZEROCOPY_OFF;

	if (!msg->data) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		free(msg);
		return NULL;
	}

	if (emq_client_read(client, (char*)msg->data, msg->size) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		free(msg->data);
		free(msg);
		return NULL;
	}

	return msg;
}

static emq_msg *emq_read_message_tag(emq_client *client, size_t size)
{
	emq_msg *msg;
	uint64_t tag;

	if (emq_client_read(client, (char*)&tag, sizeof(tag)) == -1) {
		return NULL;
	}

	msg = emq_read_message(client, size - sizeof(tag));

	msg->tag = tag;

	return msg;
}

emq_msg *emq_queue_get(emq_client *client, const char *name)
{
	protocol_response_header header;
	emq_msg *msg;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_get_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_QUEUE_GET) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	if ((msg = emq_read_message_tag(client, header.bodylen)) == NULL) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return msg;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

emq_msg *emq_queue_pop(emq_client *client, const char *name, uint32_t timeout)
{
	protocol_response_header header;
	emq_msg *msg;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_pop_request(client, name, timeout) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_QUEUE_POP) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	if ((msg = emq_read_message_tag(client, header.bodylen)) == NULL) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return msg;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

int emq_queue_confirm(emq_client *client, const char *name, emq_tag tag)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_confirm_request(client, name, tag) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_QUEUE_CONFIRM) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_subscribe(emq_client *client, const char *name, uint32_t flags, emq_msg_callback *callback)
{
	protocol_response_header header;
	emq_queue_subscription *subscription;

	EMQ_CLEAR_ERROR(client);

	if (!callback) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_queue_subscribe_request(client, name, flags) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_SUBSCRIBE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	subscription = emq_queue_subscription_create(name, callback);
	if (!subscription) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	emq_list_add_value(client->queue_subscriptions, subscription);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_unsubscribe(emq_client *client, const char *name)
{
	protocol_response_header header;
	emq_queue_subscription *subscription;
	emq_list_iterator iter;
	emq_list_node *node;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_unsubscribe_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_UNSUBSCRIBE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	emq_list_rewind(client->queue_subscriptions, &iter);
	while ((node = emq_list_next(&iter)) != NULL)
	{
		subscription = EMQ_LIST_VALUE(node);
		if (!strcmp(subscription->name, name)) {
			emq_list_delete_node(client->queue_subscriptions, node);
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_purge(emq_client *client, const char *name)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_purge_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_PURGE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_delete(emq_client *client, const char *name)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_delete_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_DELETE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_route_create(emq_client *client, const char *name, uint32_t flags)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_create_request(client, name, flags) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_ROUTE_CREATE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_route_exist(emq_client *client, const char *name)
{
	protocol_response_route_exist response;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_exist_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&response.header, sizeof(response.header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&response.header, EMQ_PROTOCOL_CMD_ROUTE_EXIST,
			sizeof(response.body)) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&response.header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&response.header));
		goto error;
	}

	if (emq_client_read(client, (char*)&response.body, sizeof(response.body)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return response.body.status;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return 0;
}

emq_list *emq_route_list(emq_client *client)
{
	protocol_response_header header;
	emq_route *route;
	emq_list *list;
	char *buffer;
	uint32_t i;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_list_request(client) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_ROUTE_LIST) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	buffer = (char*)malloc(header.bodylen);
	if (!buffer) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	if (emq_client_read(client, buffer, header.bodylen) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		free(buffer);
		goto error;
	}

	list = emq_list_init();
	if (!list) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		free(buffer);
		goto error;
	}

	EMQ_LIST_SET_FREE_METHOD(list, emq_route_list_free_handler);

	for (i = 0; i < header.bodylen;)
	{
		route = emq_route_init();
		if (!route) {
			emq_client_set_error(client, EMQ_ERROR_ALLOC);
			emq_list_release(list);
			free(buffer);
			goto error;
		}

		memcpy(route->name, buffer + i, 64);
		i += 64;
		memcpy(&route->flags, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&route->keys, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);

		emq_list_add_value(list, route);
	}

	free(buffer);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return list;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

emq_list *emq_route_keys(emq_client *client, const char *name)
{
	protocol_response_header header;
	emq_route_key *route_key;
	emq_list *list;
	char *buffer;
	uint32_t i;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_keys_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_ROUTE_KEYS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	buffer = (char*)malloc(header.bodylen);
	if (!buffer) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	if (emq_client_read(client, buffer, header.bodylen) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		free(buffer);
		goto error;
	}

	list = emq_list_init();
	if (!list) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		free(buffer);
		goto error;
	}

	EMQ_LIST_SET_FREE_METHOD(list, emq_route_key_list_free_handler);

	for (i = 0; i < header.bodylen;)
	{
		route_key = emq_route_key_init();
		if (!route_key) {
			emq_client_set_error(client, EMQ_ERROR_ALLOC);
			emq_list_release(list);
			free(buffer);
			goto error;
		}

		memcpy(route_key->key, buffer + i, 32);
		i += 32;
		memcpy(route_key->queue, buffer + i, 64);
		i += 64;

		emq_list_add_value(list, route_key);
	}

	free(buffer);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return list;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

int emq_route_rename(emq_client *client, const char *from, const char *to)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_rename_request(client, from, to) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_ROUTE_RENAME, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_route_bind(emq_client *client, const char *name, const char *queue, const char *key)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_bind_request(client, name, queue, key) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_ROUTE_BIND, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_route_unbind(emq_client *client, const char *name, const char *queue, const char *key)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_unbind_request(client, name, queue, key) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_ROUTE_UNBIND, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_route_push(emq_client *client, const char *name, const char *key, emq_msg *msg)
{
	protocol_response_header header;
	struct iovec data[3];

	EMQ_CLEAR_ERROR(client);

	if (msg->size < 1) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_route_push_request(client, name, key, sizeof(msg->expire) + msg->size) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	data[0].iov_base = client->request;
	data[0].iov_len = client->pos;
	data[1].iov_base = &msg->expire;
	data[1].iov_len = sizeof(msg->expire);
	data[2].iov_base = msg->data;
	data[2].iov_len = msg->size;

	if (emq_client_writev(client, data, 3) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_ROUTE_PUSH, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_route_delete(emq_client *client, const char *name)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_route_delete_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_ROUTE_DELETE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_create(emq_client *client, const char *name, uint32_t flags)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_channel_create_request(client, name, flags) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_CREATE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_exist(emq_client *client, const char *name)
{
	protocol_response_channel_exist response;

	EMQ_CLEAR_ERROR(client);

	if (emq_channel_exist_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&response.header, sizeof(response.header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&response.header, EMQ_PROTOCOL_CMD_CHANNEL_EXIST,
			sizeof(response.body)) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&response.header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&response.header));
		goto error;
	}

	if (emq_client_read(client, (char*)&response.body, sizeof(response.body)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return response.body.status;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return 0;
}

emq_list *emq_channel_list(emq_client *client)
{
	protocol_response_header header;
	emq_channel *channel;
	emq_list *list;
	char *buffer;
	uint32_t i;

	EMQ_CLEAR_ERROR(client);

	if (emq_channel_list_request(client) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_CHANNEL_LIST) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, emq_get_error(&header));
		goto error;
	}

	buffer = (char*)malloc(header.bodylen);
	if (!buffer) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	if (emq_client_read(client, buffer, header.bodylen) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		free(buffer);
		goto error;
	}

	list = emq_list_init();
	if (!list) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		free(buffer);
		goto error;
	}

	EMQ_LIST_SET_FREE_METHOD(list, emq_channel_list_free_handler);

	for (i = 0; i < header.bodylen;)
	{
		channel = emq_channel_init();
		if (!channel) {
			emq_client_set_error(client, EMQ_ERROR_ALLOC);
			emq_list_release(list);
			free(buffer);
			goto error;
		}

		memcpy(channel->name, buffer + i, 64);
		i += 64;
		memcpy(&channel->flags, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&channel->topics, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);
		memcpy(&channel->patterns, buffer + i, sizeof(uint32_t));
		i += sizeof(uint32_t);

		emq_list_add_value(list, channel);
	}

	free(buffer);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return list;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;

}

int emq_channel_rename(emq_client *client, const char *from, const char *to)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_channel_rename_request(client, from, to) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_RENAME, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_publish(emq_client *client, const char *name, const char *topic, emq_msg *msg)
{
	protocol_response_header header;
	struct iovec data[2];

	EMQ_CLEAR_ERROR(client);

	if (msg->size < 1) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_channel_publish_request(client, name, topic, msg->size) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	data[0].iov_base = client->request;
	data[0].iov_len = client->pos;
	data[1].iov_base = msg->data;
	data[1].iov_len = msg->size;

	if (emq_client_writev(client, data, 2) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_PUBLISH, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_subscribe(emq_client *client, const char *name, const char *topic, emq_msg_callback *callback)
{
	protocol_response_header header;
	emq_channel_subscription *subscription;

	EMQ_CLEAR_ERROR(client);

	if (!callback) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_channel_subscribe_request(client, name, topic) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_SUBSCRIBE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	subscription = emq_channel_subscription_create(name, topic, callback);
	if (!subscription) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	emq_list_add_value(client->channel_subscriptions, subscription);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_psubscribe(emq_client *client, const char *name, const char *pattern, emq_msg_callback *callback)
{
	protocol_response_header header;
	emq_channel_subscription *subscription;

	EMQ_CLEAR_ERROR(client);

	if (!callback) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_channel_psubscribe_request(client, name, pattern) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_PSUBSCRIBE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	subscription = emq_channel_subscription_create(name, pattern, callback);
	if (!subscription) {
		emq_client_set_error(client, EMQ_ERROR_ALLOC);
		goto error;
	}

	emq_list_add_value(client->channel_subscriptions, subscription);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_unsubscribe(emq_client *client, const char *name, const char *topic)
{
	protocol_response_header header;
	emq_channel_subscription *subscription;
	emq_list_iterator iter;
	emq_list_node *node;

	EMQ_CLEAR_ERROR(client);

	if (emq_channel_unsubscribe_request(client, name, topic) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_UNSUBSCRIBE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	emq_list_rewind(client->channel_subscriptions, &iter);
	while ((node = emq_list_next(&iter)) != NULL)
	{
		subscription = EMQ_LIST_VALUE(node);
		if (!strcmp(subscription->name, name)) {
			emq_list_delete_node(client->channel_subscriptions, node);
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_punsubscribe(emq_client *client, const char *name, const char *pattern)
{
	protocol_response_header header;
	emq_channel_subscription *subscription;
	emq_list_iterator iter;
	emq_list_node *node;

	EMQ_CLEAR_ERROR(client);

	if (emq_channel_punsubscribe_request(client, name, pattern) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_UNSUBSCRIBE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	emq_list_rewind(client->channel_subscriptions, &iter);
	while ((node = emq_list_next(&iter)) != NULL)
	{
		subscription = EMQ_LIST_VALUE(node);
		if (!strcmp(subscription->name, name)) {
			emq_list_delete_node(client->channel_subscriptions, node);
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_channel_delete(emq_client *client, const char *name)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_channel_delete_request(client, name) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (!client->noack)
	{
		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_CHANNEL_DELETE, 0) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_RESPONSE);
			goto error;
		}

		if (emq_check_status(&header, EMQ_PROTOCOL_STATUS_SUCCESS) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, emq_get_error(&header));
			goto error;
		}
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

void emq_noack_enable(emq_client *client)
{
	client->noack = 1;
}

void emq_noack_disable(emq_client *client)
{
	client->noack = 0;
}

static int emq_queue_process(emq_client *client, protocol_event_header *header)
{
	emq_queue_subscription *subscription;
	emq_list_iterator iter;
	emq_list_node *node;
	emq_msg *msg = NULL;
	char name[64];
	int found = 0;

	if (emq_client_read(client, name, sizeof(name)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		return -1;
	}

	emq_list_rewind(client->queue_subscriptions, &iter);
	while ((node = emq_list_next(&iter)) != NULL)
	{
		subscription = EMQ_LIST_VALUE(node);
		if (!strcmp(subscription->name, name)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		return -1;
	}

	if (header->type == EMQ_PROTOCOL_EVENT_MESSAGE) {
		if ((msg = emq_read_message(client, header->bodylen - 64)) == NULL) {
			return -1;
		}
	}

	if (subscription->callback(client, EMQ_CALLBACK_QUEUE, subscription->name, NULL, NULL, msg) &&
		client->queue_subscriptions->length == 0) {
		return 1;
	}

	return 0;
}

int emq_channel_process(emq_client *client, protocol_event_header *header, int extended)
{
	emq_channel_subscription *subscription;
	emq_list_iterator iter;
	emq_list_node *node;
	emq_msg *msg = NULL;
	char name[64];
	char topic[32];
	char pattern[32];
	int found = 0;

	if (emq_client_read(client, name, sizeof(name)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		return -1;
	}

	if (emq_client_read(client, topic, sizeof(topic)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		return -1;
	}

	if (extended)
	{
		if (emq_client_read(client, pattern, sizeof(pattern)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			return -1;
		}
	}

	emq_list_rewind(client->channel_subscriptions, &iter);
	while ((node = emq_list_next(&iter)) != NULL)
	{
		subscription = EMQ_LIST_VALUE(node);
		if (!strcmp(subscription->name, name)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		return -1;
	}

	if ((msg = emq_read_message(client, header->bodylen - (extended ? 128 : 96))) == NULL) {
		return -1;
	}

	if (subscription->callback(client, EMQ_CALLBACK_CHANNEL, subscription->name, topic,
		(extended ? pattern : NULL), msg) && client->queue_subscriptions->length == 0) {
		return 1;
	}

	return 0;
}

int emq_process(emq_client *client)
{
	protocol_event_header header;
	int status = 0;

	EMQ_CLEAR_ERROR(client);

	for (;;)
	{
		if (!EMQ_LIST_LENGTH(client->queue_subscriptions) &&
			!EMQ_LIST_LENGTH(client->channel_subscriptions))
			break;

		if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (emq_check_event_header(&header, EMQ_PROTOCOL_EVENT_NOTIFY, EMQ_PROTOCOL_EVENT_MESSAGE) == EMQ_STATUS_ERR) {
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		if (header.cmd != EMQ_PROTOCOL_CMD_QUEUE_SUBSCRIBE &&
			header.cmd != EMQ_PROTOCOL_CMD_CHANNEL_SUBSCRIBE &&
			header.cmd != EMQ_PROTOCOL_CMD_CHANNEL_PSUBSCRIBE)
		{
			emq_client_set_error(client, EMQ_ERROR_READ);
			goto error;
		}

		switch (header.cmd)
		{
			case EMQ_PROTOCOL_CMD_QUEUE_SUBSCRIBE:
				status = emq_queue_process(client, &header);
				break;
			case EMQ_PROTOCOL_CMD_CHANNEL_SUBSCRIBE:
				status = emq_channel_process(client, &header, 0);
				break;
			case EMQ_PROTOCOL_CMD_CHANNEL_PSUBSCRIBE:
				status = emq_channel_process(client, &header, 1);
				break;
		}

		switch (status)
		{
			case 0: break;
			case 1: goto stop;
			case -1: goto error;
		}

		continue;
		stop: break;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

char *emq_last_error(emq_client *client)
{
	return EMQ_GET_ERROR(client);
}

int emq_version(void)
{
	return EMQ_VERSION(EMQ_VERSION_MAJOR, EMQ_VERSION_MINOR);
}
