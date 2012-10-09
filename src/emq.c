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

#include "emq.h"
#include "version.h"
#include "network.h"
#include "protocol.h"
#include "packet.h"

#define EMQ_LIST_SET_FREE_METHOD(l, m) ((l)->free = (m))
#define EMQ_LIST_GET_FREE_METHOD(l) ((l)->free)

static const char *emq_error_array[] = {
	"",
	"Error allocate memory\n",
	"Error input data\n",
	"Error write request\n",
	"Error read request\n",
	"Error response\n"
};

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

	if (!client->request) {
		free(client);
		return NULL;
	}

	client->fd = 0;

	return client;
}

static void emq_client_release(emq_client *client)
{
	free(client->request);
	free(client);
}

static void emq_client_set_error(emq_client *client, int error)
{
	snprintf(client->error, sizeof(client->error), emq_error_array[error]);
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

	queue = (emq_queue*)malloc(sizeof(*queue));
	if (!queue) {
		return NULL;
	}

	memset(queue, 0, sizeof(*queue));

	return queue;
}

static void emq_queue_release(emq_queue *queue)
{
	free(queue);
}

static void emq_user_list_free_handler(void *value)
{
	emq_user *user = (emq_user*)value;

	emq_user_release(user);
}

static void emq_queue_list_free_handler(void *value)
{
	emq_queue *queue = (emq_queue*)value;

	emq_queue_release(queue);
}

emq_msg *emq_msg_create(void *data, size_t size, int zero_copy)
{
	emq_msg *msg;

	msg = (emq_msg*)malloc(sizeof(*msg));
	if (!msg) {
		return NULL;
	}

	msg->size = size;
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

	if (!new_msg->data) {
		free(new_msg);
		return NULL;
	}

	memcpy(new_msg->data, msg->data, msg->size);

	return new_msg;
}

void *emq_msg_data(emq_msg *msg)
{
	return msg->data;
}

size_t emq_msg_size(emq_msg *msg)
{
	return msg->size;
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_AUTH,
		EMQ_PROTOCOL_SUCCESS_AUTH, EMQ_PROTOCOL_ERROR_AUTH, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_AUTH) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_PING,
		EMQ_PROTOCOL_SUCCESS_PING, EMQ_PROTOCOL_ERROR_PING, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_PING) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_check_response_header(&response.header, EMQ_PROTOCOL_CMD_STAT,
		EMQ_PROTOCOL_SUCCESS_STAT, EMQ_PROTOCOL_ERROR_STAT, sizeof(response.body)) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&response.header, EMQ_PROTOCOL_SUCCESS_STAT) == EMQ_STATUS_ERR) {
		goto error;
	}

	memcpy(status, &response.body, sizeof(*status));

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_user_create(emq_client *client, const char *name, const char *password, uint64_t perm)
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_CREATE,
		EMQ_PROTOCOL_SUCCESS_USER_CREATE, EMQ_PROTOCOL_ERROR_USER_CREATE, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_USER_CREATE) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_USER_LIST,
		EMQ_PROTOCOL_SUCCESS_USER_LIST, EMQ_PROTOCOL_ERROR_USER_LIST) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_USER_LIST) == EMQ_STATUS_ERR) {
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_RENAME,
		EMQ_PROTOCOL_SUCCESS_USER_RENAME, EMQ_PROTOCOL_ERROR_USER_RENAME, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_USER_RENAME) == EMQ_STATUS_ERR) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_user_set_perm(emq_client *client, const char *name, uint64_t perm)
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_SET_PERM,
		EMQ_PROTOCOL_SUCCESS_USER_SET_PERM, EMQ_PROTOCOL_ERROR_USER_SET_PERM, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_USER_SET_PERM) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_USER_DELETE,
		EMQ_PROTOCOL_SUCCESS_USER_DELETE, EMQ_PROTOCOL_ERROR_USER_DELETE, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_USER_DELETE) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_CREATE,
		EMQ_PROTOCOL_SUCCESS_QUEUE_CREATE, EMQ_PROTOCOL_ERROR_QUEUE_CREATE, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_CREATE) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_DECLARE,
		EMQ_PROTOCOL_SUCCESS_QUEUE_DECLARE, EMQ_PROTOCOL_ERROR_QUEUE_DECLARE, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_DECLARE) == EMQ_STATUS_ERR) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_queue_exist(emq_client *client, const char *name)
{
	protocol_response_header header;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_exist_request(client, name) == EMQ_STATUS_ERR) {
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

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_EXIST,
		EMQ_PROTOCOL_SUCCESS_QUEUE_EXIST, EMQ_PROTOCOL_ERROR_QUEUE_EXIST, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_EXIST) == EMQ_STATUS_ERR) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
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

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_QUEUE_LIST,
		EMQ_PROTOCOL_SUCCESS_QUEUE_LIST, EMQ_PROTOCOL_ERROR_QUEUE_LIST) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_LIST) == EMQ_STATUS_ERR) {
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

		emq_list_add_value(list, queue);
	}

	free(buffer);

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return list;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

size_t emq_queue_size(emq_client *client, const char *name)
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
		EMQ_PROTOCOL_SUCCESS_QUEUE_SIZE, EMQ_PROTOCOL_ERROR_QUEUE_SIZE, sizeof(response.body)) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&response.header, EMQ_PROTOCOL_SUCCESS_QUEUE_SIZE) == EMQ_STATUS_ERR) {
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

	EMQ_CLEAR_ERROR(client);

	if (msg->size < 1) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_queue_push_request(client, name, msg->size) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_DATA);
		goto error;
	}

	if (emq_client_write(client, client->request, client->pos) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_write(client, msg->data, msg->size) == -1) {
		emq_client_set_error(client, EMQ_ERROR_WRITE);
		goto error;
	}

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_PUSH,
		EMQ_PROTOCOL_SUCCESS_QUEUE_PUSH, EMQ_PROTOCOL_ERROR_QUEUE_PUSH, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_PUSH) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_QUEUE_GET,
		EMQ_PROTOCOL_SUCCESS_QUEUE_GET, EMQ_PROTOCOL_ERROR_QUEUE_GET) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_GET) == EMQ_STATUS_ERR) {
		goto error;
	}

	if ((msg = emq_read_message(client, header.bodylen)) == NULL) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return msg;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
}

emq_msg *emq_queue_pop(emq_client *client, const char *name)
{
	protocol_response_header header;
	emq_msg *msg;

	EMQ_CLEAR_ERROR(client);

	if (emq_queue_pop_request(client, name) == EMQ_STATUS_ERR) {
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

	if (emq_check_response_header_mini(&header, EMQ_PROTOCOL_CMD_QUEUE_POP,
		EMQ_PROTOCOL_SUCCESS_QUEUE_POP, EMQ_PROTOCOL_ERROR_QUEUE_POP) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_POP) == EMQ_STATUS_ERR) {
		goto error;
	}

	if ((msg = emq_read_message(client, header.bodylen)) == NULL) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return msg;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return NULL;
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_PURGE,
		EMQ_PROTOCOL_SUCCESS_QUEUE_PURGE, EMQ_PROTOCOL_ERROR_QUEUE_PURGE, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_PURGE) == EMQ_STATUS_ERR) {
		goto error;
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

	if (emq_client_read(client, (char*)&header, sizeof(header)) == -1) {
		emq_client_set_error(client, EMQ_ERROR_READ);
		goto error;
	}

	if (emq_check_response_header(&header, EMQ_PROTOCOL_CMD_QUEUE_DELETE,
		EMQ_PROTOCOL_SUCCESS_QUEUE_DELETE, EMQ_PROTOCOL_ERROR_QUEUE_DELETE, 0) == EMQ_STATUS_ERR) {
		emq_client_set_error(client, EMQ_ERROR_RESPONSE);
		goto error;
	}

	if (emq_check_status(&header, EMQ_PROTOCOL_SUCCESS_QUEUE_DELETE) == EMQ_STATUS_ERR) {
		goto error;
	}

	EMQ_SET_STATUS(client, EMQ_STATUS_OK);
	return EMQ_STATUS_OK;

error:
	EMQ_SET_STATUS(client, EMQ_STATUS_ERR);
	return EMQ_STATUS_ERR;
}

int emq_version(void)
{
	return EMQ_VERSION;
}
