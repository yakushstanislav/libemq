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

#ifndef _EMQ_PROTOCOL_H_
#define _EMQ_PROTOCOL_H_

#include <stdint.h>

typedef enum protocol_binary_magic {
	EMQ_PROTOCOL_REQ = 0x70,
	EMQ_PROTOCOL_RES = 0x80,
	EMQ_PROTOCOL_EVENT = 0x90
} protocol_binary_magic;

typedef enum protocol_command {
	/* system commands (10..15) */
	EMQ_PROTOCOL_CMD_AUTH = 0xA,
	EMQ_PROTOCOL_CMD_PING = 0xB,
	EMQ_PROTOCOL_CMD_STAT = 0xC,
	EMQ_PROTOCOL_CMD_SAVE = 0xD,
	EMQ_PROTOCOL_CMD_FLUSH = 0xE,
	EMQ_PROTOCOL_CMD_DISCONNECT = 0xF,

	/* user control commands (30..34) */
	EMQ_PROTOCOL_CMD_USER_CREATE = 0x1E,
	EMQ_PROTOCOL_CMD_USER_LIST = 0x1F,
	EMQ_PROTOCOL_CMD_USER_RENAME = 0x20,
	EMQ_PROTOCOL_CMD_USER_SET_PERM = 0x21,
	EMQ_PROTOCOL_CMD_USER_DELETE = 0x22,

	/* queue commands (35..48) */
	EMQ_PROTOCOL_CMD_QUEUE_CREATE = 0x23,
	EMQ_PROTOCOL_CMD_QUEUE_DECLARE = 0x24,
	EMQ_PROTOCOL_CMD_QUEUE_EXIST = 0x25,
	EMQ_PROTOCOL_CMD_QUEUE_LIST = 0x26,
	EMQ_PROTOCOL_CMD_QUEUE_RENAME = 0x27,
	EMQ_PROTOCOL_CMD_QUEUE_SIZE = 0x28,
	EMQ_PROTOCOL_CMD_QUEUE_PUSH = 0x29,
	EMQ_PROTOCOL_CMD_QUEUE_GET = 0x2A,
	EMQ_PROTOCOL_CMD_QUEUE_POP = 0x2B,
	EMQ_PROTOCOL_CMD_QUEUE_CONFIRM = 0x2C,
	EMQ_PROTOCOL_CMD_QUEUE_SUBSCRIBE = 0x2D,
	EMQ_PROTOCOL_CMD_QUEUE_UNSUBSCRIBE = 0x2E,
	EMQ_PROTOCOL_CMD_QUEUE_PURGE = 0x2F,
	EMQ_PROTOCOL_CMD_QUEUE_DELETE = 0x30,

	/* route commands (49..57) */
	EMQ_PROTOCOL_CMD_ROUTE_CREATE = 0x31,
	EMQ_PROTOCOL_CMD_ROUTE_EXIST = 0x32,
	EMQ_PROTOCOL_CMD_ROUTE_LIST = 0x33,
	EMQ_PROTOCOL_CMD_ROUTE_KEYS = 0x34,
	EMQ_PROTOCOL_CMD_ROUTE_RENAME = 0x35,
	EMQ_PROTOCOL_CMD_ROUTE_BIND = 0x36,
	EMQ_PROTOCOL_CMD_ROUTE_UNBIND = 0x37,
	EMQ_PROTOCOL_CMD_ROUTE_PUSH = 0x38,
	EMQ_PROTOCOL_CMD_ROUTE_DELETE = 0x39,

	/* channel commands (58..67) */
	EMQ_PROTOCOL_CMD_CHANNEL_CREATE = 0x3A,
	EMQ_PROTOCOL_CMD_CHANNEL_EXIST = 0x3B,
	EMQ_PROTOCOL_CMD_CHANNEL_LIST = 0x3C,
	EMQ_PROTOCOL_CMD_CHANNEL_RENAME = 0x3D,
	EMQ_PROTOCOL_CMD_CHANNEL_PUBLISH = 0x3E,
	EMQ_PROTOCOL_CMD_CHANNEL_SUBSCRIBE = 0x3F,
	EMQ_PROTOCOL_CMD_CHANNEL_PSUBSCRIBE = 0x40,
	EMQ_PROTOCOL_CMD_CHANNEL_UNSUBSCRIBE = 0x41,
	EMQ_PROTOCOL_CMD_CHANNEL_PUNSUBSCRIBE = 0x42,
	EMQ_PROTOCOL_CMD_CHANNEL_DELETE = 0x43
} protocol_command;

typedef enum protocol_response_status {
	EMQ_PROTOCOL_STATUS_SUCCESS = 0x1,
	EMQ_PROTOCOL_STATUS_ERROR = 0x2,
	EMQ_PROTOCOL_STATUS_ERROR_PACKET = 0x3,
	EMQ_PROTOCOL_STATUS_ERROR_COMMAND = 0x4,
	EMQ_PROTOCOL_STATUS_ERROR_ACCESS = 0x5,
	EMQ_PROTOCOL_STATUS_ERROR_MEMORY = 0x6,
	EMQ_PROTOCOL_STATUS_ERROR_VALUE = 0x7,
	EMQ_PROTOCOL_STATUS_ERROR_NOT_DECLARED = 0x8,
	EMQ_PROTOCOL_STATUS_ERROR_NOT_FOUND = 0x9,
	EMQ_PROTOCOL_STATUS_ERROR_NO_DATA = 0xA
} protocol_response_status;

typedef enum protocol_event_type {
	EMQ_PROTOCOL_EVENT_NOTIFY = 0x1,
	EMQ_PROTOCOL_EVENT_MESSAGE = 0x2
} protocol_event_type;

#pragma pack(push, 1)

typedef struct protocol_request_header {
	uint16_t magic;
	uint8_t cmd;
	uint8_t noack;
	uint32_t bodylen;
} protocol_request_header;

typedef struct protocol_response_header {
	uint16_t magic;
	uint8_t cmd;
	uint8_t status;
	uint32_t bodylen;
} protocol_response_header;

typedef struct protocol_event_header {
	uint16_t magic;
	uint8_t cmd;
	uint8_t type;
	uint32_t bodylen;
} protocol_event_header;

typedef struct protocol_request_auth {
	protocol_request_header header;
	struct {
		char name[32];
		char password[32];
	} body;
} protocol_request_auth;

typedef protocol_request_header protocol_request_ping;
typedef protocol_request_header protocol_request_stat;

typedef struct protocol_request_save {
	protocol_request_header header;
	struct {
		uint8_t async;
	} body;
} protocol_request_save;

typedef struct protocol_request_flush {
	protocol_request_header header;
	struct {
		uint32_t flags;
	} body;
} protocol_request_flush;

typedef protocol_request_header protocol_request_disconnect;

typedef struct protocol_request_user_create {
	protocol_request_header header;
	struct {
		char name[32];
		char password[32];
		uint64_t perm;
	} body;
} protocol_request_user_create;

typedef protocol_request_header protocol_request_user_list;

typedef struct protocol_request_user_rename {
	protocol_request_header header;
	struct {
		char from[32];
		char to[32];
	} body;
} protocol_request_user_rename;

typedef struct protocol_request_user_set_perm {
	protocol_request_header header;
	struct {
		char name[32];
		uint64_t perm;
	} body;
} protocol_request_user_set_perm;

typedef struct protocol_request_user_delete {
	protocol_request_header header;
	struct {
		char name[32];
	} body;
} protocol_request_user_delete;

typedef struct protocol_request_queue_create {
	protocol_request_header header;
	struct {
		char name[64];
		uint32_t max_msg;
		uint32_t max_msg_size;
		uint32_t flags;
	} body;
} protocol_request_queue_create;

typedef struct protocol_request_queue_declare {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_declare;

typedef struct protocol_request_queue_exist {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_exist;

typedef protocol_request_header protocol_request_queue_list;

typedef struct protocol_request_queue_rename {
	protocol_request_header header;
	struct {
		char from[64];
		char to[64];
	} body;
} protocol_request_queue_rename;

typedef struct protocol_request_queue_size {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_size;

typedef struct protocol_request_queue_push {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_push;

typedef struct protocol_request_queue_get {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_get;

typedef struct protocol_request_queue_pop {
	protocol_request_header header;
	struct {
		char name[64];
		uint32_t timeout;
	} body;
} protocol_request_queue_pop;

typedef struct protocol_request_queue_confirm {
	protocol_request_header header;
	struct {
		char name[64];
		uint64_t tag;
	} body;
} protocol_request_queue_confirm;

typedef struct protocol_request_queue_subscribe {
	protocol_request_header header;
	struct {
		char name[64];
		uint32_t flags;
	} body;
} protocol_request_queue_subscribe;

typedef struct protocol_request_queue_unsubscribe {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_unsubscribe;

typedef struct protocol_request_queue_purge {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_purge;

typedef struct protocol_request_queue_delete {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_queue_delete;

typedef struct protocol_request_route_create {
	protocol_request_header header;
	struct {
		char name[64];
		uint32_t flags;
	} body;
} protocol_request_route_create;

typedef struct protocol_request_route_exist {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_route_exist;

typedef struct protocol_request_header protocol_request_route_list;

typedef struct protocol_request_route_keys {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_route_keys;

typedef struct protocol_request_route_rename {
	protocol_request_header header;
	struct {
		char from[64];
		char to[64];
	} body;
} protocol_request_route_rename;

typedef struct protocol_request_route_bind {
	protocol_request_header header;
	struct {
		char name[64];
		char queue[64];
		char key[32];
	} body;
} protocol_request_route_bind;

typedef struct protocol_request_route_unbind {
	protocol_request_header header;
	struct {
		char name[64];
		char queue[64];
		char key[32];
	} body;
} protocol_request_route_unbind;

typedef struct protocol_request_route_push {
	protocol_request_header header;
	struct {
		char name[64];
		char key[32];
	} body;
} protocol_request_route_push;

typedef struct protocol_request_route_delete {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_route_delete;

typedef struct protocol_request_channel_create {
	protocol_request_header header;
	struct {
		char name[64];
		uint32_t flags;
	} body;
} protocol_request_channel_create;

typedef struct protocol_request_channel_exist {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_channel_exist;

typedef struct protocol_request_header protocol_request_channel_list;

typedef struct protocol_request_channel_rename {
	protocol_request_header header;
	struct {
		char from[64];
		char to[64];
	} body;
} protocol_request_channel_rename;

typedef struct protocol_request_channel_publish {
	protocol_request_header header;
	struct {
		char name[64];
		char topic[32];
	} body;
} protocol_request_channel_publish;

typedef struct protocol_request_channel_subscribe {
	protocol_request_header header;
	struct {
		char name[64];
		char topic[32];
	} body;
} protocol_request_channel_subscribe;

typedef struct protocol_request_channel_psubscribe {
	protocol_request_header header;
	struct {
		char name[64];
		char pattern[32];
	} body;
} protocol_request_channel_psubscribe;

typedef struct protocol_request_channel_unsubscribe {
	protocol_request_header header;
	struct {
		char name[64];
		char topic[32];
	} body;
} protocol_request_channel_unsubscribe;

typedef struct protocol_request_channel_punsubscribe {
	protocol_request_header header;
	struct {
		char name[64];
		char pattern[32];
	} body;
} protocol_request_channel_punsubscribe;

typedef struct protocol_request_channel_delete {
	protocol_request_header header;
	struct {
		char name[64];
	} body;
} protocol_request_channel_delete;

typedef struct protocol_response_stat {
	protocol_response_header header;
	struct {
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
		uint32_t routes;
		uint32_t channels;
		uint32_t resv3;
		uint32_t resv4;
	} body;
} protocol_response_stat;

typedef struct protocol_response_queue_exist {
	protocol_response_header header;
	struct {
		uint32_t status;
	} body;
} protocol_response_queue_exist;

typedef struct protocol_response_queue_size {
	protocol_response_header header;
	struct {
		uint32_t size;
	} body;
} protocol_response_queue_size;

typedef struct protocol_response_route_exist {
	protocol_response_header header;
	struct {
		uint32_t status;
	} body;
} protocol_response_route_exist;

typedef struct protocol_response_channel_exist {
	protocol_response_header header;
	struct {
		uint32_t status;
	} body;
} protocol_response_channel_exist;

#pragma pack(pop)

#endif
