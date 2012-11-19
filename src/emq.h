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
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS3
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _EMQ_H_
#define _EMQ_H_

#include <stdio.h>
#include <stdint.h>

#define EMQ_VERSION_MAJOR 0
#define EMQ_VERSION_MINOR 0 /* 0..9 */

#define EMQ_VERSION(major, minor) (major * 10 + minor)

#define EMQ_STATUS_OK 0
#define EMQ_STATUS_ERR -1

#define EMQ_ERROR_BUF_SIZE 256
#define EMQ_DEFAULT_REQUEST_SIZE 4096
#define EMQ_MAX_REQUEST_SIZE 2147483647

#define EMQ_GET_STATUS(client) (client->status)
#define EMQ_SET_STATUS(client, value) (client->status = value)

#define EMQ_LIST_LENGTH(l) ((l)->length)
#define EMQ_LIST_VALUE(n) ((n)->value)

#define EMQ_QUEUE_PERM 1
#define EMQ_ADMIN_PERM 32
#define EMQ_NOT_CHANGE_PERM 64
#define EMQ_QUEUE_CREATE_PERM 1024
#define EMQ_QUEUE_DECLARE_PERM 2048
#define EMQ_QUEUE_EXIST_PERM 4096
#define EMQ_QUEUE_LIST_PERM 8192
#define EMQ_QUEUE_SIZE_PERM 16384
#define EMQ_QUEUE_PUSH_PERM 32768
#define EMQ_QUEUE_GET_PERM 65536
#define EMQ_QUEUE_POP_PERM 131072
#define EMQ_QUEUE_SUBSCRIBE_PERM 262144
#define EMQ_QUEUE_UNSUBSCRIBE_PERM 524288
#define EMQ_QUEUE_PURGE_PERM 1048576
#define EMQ_QUEUE_DELETE_PERM 2097152

#define EMQ_QUEUE_NONE 0
#define EMQ_QUEUE_AUTODELETE 1
#define EMQ_QUEUE_FORCE_PUSH 2

#define EMQ_QUEUE_SUBSCRIBE_MSG 0
#define EMQ_QUEUE_SUBSCRIBE_NOTIFY 1

#define EMQ_MAX_MSG 4294967295
#define EMQ_MAX_MSG_SIZE 2147483647

#define EMQ_ERROR_NONE 0
#define EMQ_ERROR_ALLOC 1
#define EMQ_ERROR_DATA 2
#define EMQ_ERROR_WRITE 3
#define EMQ_ERROR_READ 4
#define EMQ_ERROR_RESPONSE 5

#define EMQ_GET_ERROR(client) (client->error)
#define EMQ_ISSET_ERROR(client) (client->error[0] != '\0')
#define EMQ_CLEAR_ERROR(client) (client->error[0] = '\0')

#define EMQ_ZEROCOPY_ON 1
#define EMQ_ZEROCOPY_OFF 0

typedef struct emq_list_node {
	struct emq_list_node *prev;
	struct emq_list_node *next;
	void *value;
} emq_list_node;

typedef struct emq_list_iterator {
	emq_list_node *next;
} emq_list_iterator;

typedef struct emq_list {
	emq_list_node *head;
	emq_list_node *tail;
	size_t length;
	void (*free)(void *value);
} emq_list;

typedef struct emq_client {
	int status;
	char error[EMQ_ERROR_BUF_SIZE];
	char *request;
	size_t size;
	size_t pos;
	int fd;
	emq_list *subscriptions;
} emq_client;

typedef uint64_t emq_perm;

typedef struct emq_user {
	char name[32];
	char password[32];
	emq_perm perm;
} emq_user;

typedef struct emq_queue {
	char name[64];
	uint32_t max_msg;
	uint32_t max_msg_size;
	uint32_t flags;
	uint32_t size;
	uint32_t declared_clients;
	uint32_t subscribed_clients;
} emq_queue;

typedef struct emq_msg {
	void *data;
	size_t size;
	int zero_copy;
} emq_msg;

typedef int emq_msg_callback(emq_client *client, const char *name, emq_msg *msg);

#pragma pack(push, 1)

typedef struct emq_status {
	struct {
		uint8_t major;
		uint8_t minor;
		uint8_t patch;
	} version;
	uint32_t uptime;
	float used_cpu_sys;
	float used_cpu_user;
	uint32_t used_memory;
	uint32_t used_memory_rss;
	float fragmentation_ratio;
	uint32_t clients;
	uint32_t users;
	uint32_t queues;
	uint32_t resv1;
	uint32_t resv2;
	uint32_t resv3;
	uint32_t resv4;
} emq_status;

#pragma pack(pop)

emq_msg *emq_msg_create(void *data, size_t size, int zero_copy);
emq_msg *emq_msg_copy(emq_msg *msg);
void *emq_msg_data(emq_msg *msg);
size_t emq_msg_size(emq_msg *msg);
void emq_msg_release(emq_msg *msg);

emq_client *emq_tcp_connect(const char *addr, int port);
emq_client *emq_unix_connect(const char *path);
void emq_disconnect(emq_client *client);

int emq_auth(emq_client *client, const char *name, const char *password);
int emq_ping(emq_client *client);
int emq_stat(emq_client *client, emq_status *status);

int emq_user_create(emq_client *client, const char *name, const char *password, emq_perm perm);
emq_list *emq_user_list(emq_client *client);
int emq_user_rename(emq_client *client, const char *from, const char *to);
int emq_user_set_perm(emq_client *client, const char *name, emq_perm perm);
int emq_user_delete(emq_client *client, const char *name);

int emq_queue_create(emq_client *client, const char *name, uint32_t max_msg, uint32_t max_msg_size, uint32_t flags);
int emq_queue_declare(emq_client *client, const char *name);
int emq_queue_exist(emq_client *client, const char *name);
emq_list *emq_queue_list(emq_client *client);
size_t emq_queue_size(emq_client *client, const char *name);
int emq_queue_push(emq_client *client, const char *name, emq_msg *msg);
emq_msg *emq_queue_get(emq_client *client, const char *name);
emq_msg *emq_queue_pop(emq_client *client, const char *name);
int emq_queue_subscribe(emq_client *client, const char *name, uint32_t flags, emq_msg_callback *callback);
int emq_queue_unsubscribe(emq_client *client, const char *name);
int emq_queue_purge(emq_client *client, const char *name);
int emq_queue_delete(emq_client *client, const char *name);

int emq_process(emq_client *client);

char *emq_last_error(emq_client *client);
int emq_version(void);

void emq_list_rewind(emq_list *list, emq_list_iterator *iter);
emq_list_node *emq_list_next(emq_list_iterator *iter);
void emq_list_release(emq_list *list);

#endif
