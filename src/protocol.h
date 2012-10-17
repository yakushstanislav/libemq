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
	EMQ_PROTOCOL_RES = 0x80
} protocol_binary_magic;

typedef enum protocol_command {
	/* system commands */
	EMQ_PROTOCOL_CMD_AUTH = 0x1,
	EMQ_PROTOCOL_CMD_PING = 0x2,
	EMQ_PROTOCOL_CMD_STAT = 0x3,
	EMQ_PROTOCOL_CMD_DISCONNECT = 0x4,

	/* user control commands */
	EMQ_PROTOCOL_CMD_USER_CREATE = 0x10,
	EMQ_PROTOCOL_CMD_USER_LIST = 0x11,
	EMQ_PROTOCOL_CMD_USER_RENAME = 0x12,
	EMQ_PROTOCOL_CMD_USER_SET_PERM = 0x13,
	EMQ_PROTOCOL_CMD_USER_DELETE = 0x14,

	/* queue commands */
	EMQ_PROTOCOL_CMD_QUEUE_CREATE = 0x20,
	EMQ_PROTOCOL_CMD_QUEUE_DECLARE = 0x21,
	EMQ_PROTOCOL_CMD_QUEUE_EXIST = 0x22,
	EMQ_PROTOCOL_CMD_QUEUE_LIST = 0x23,
	EMQ_PROTOCOL_CMD_QUEUE_SIZE = 0x24,
	EMQ_PROTOCOL_CMD_QUEUE_PUSH = 0x25,
	EMQ_PROTOCOL_CMD_QUEUE_GET = 0x26,
	EMQ_PROTOCOL_CMD_QUEUE_POP = 0x27,
	EMQ_PROTOCOL_CMD_QUEUE_SUBSCRIBE = 0x28,
	EMQ_PROTOCOL_CMD_QUEUE_UNSUBSCRIBE = 0x29,
	EMQ_PROTOCOL_CMD_QUEUE_PURGE = 0x30,
	EMQ_PROTOCOL_CMD_QUEUE_DELETE = 0x31
} protocol_command;

typedef enum protocol_response_status {
	EMQ_PROTOCOL_SUCCESS = 0x1,
	EMQ_PROTOCOL_SUCCESS_AUTH = 0x2,
	EMQ_PROTOCOL_SUCCESS_PING = 0x3,
	EMQ_PROTOCOL_SUCCESS_STAT = 0x4,
	EMQ_PROTOCOL_SUCCESS_USER_CREATE = 0x10,
	EMQ_PROTOCOL_SUCCESS_USER_LIST = 0x11,
	EMQ_PROTOCOL_SUCCESS_USER_RENAME = 0x12,
	EMQ_PROTOCOL_SUCCESS_USER_SET_PERM = 0x13,
	EMQ_PROTOCOL_SUCCESS_USER_DELETE = 0x14,
	EMQ_PROTOCOL_SUCCESS_QUEUE_CREATE = 0x20,
	EMQ_PROTOCOL_SUCCESS_QUEUE_DECLARE = 0x21,
	EMQ_PROTOCOL_SUCCESS_QUEUE_EXIST = 0x22,
	EMQ_PROTOCOL_SUCCESS_QUEUE_LIST = 0x23,
	EMQ_PROTOCOL_SUCCESS_QUEUE_SIZE = 0x24,
	EMQ_PROTOCOL_SUCCESS_QUEUE_PUSH = 0x25,
	EMQ_PROTOCOL_SUCCESS_QUEUE_GET = 0x26,
	EMQ_PROTOCOL_SUCCESS_QUEUE_POP = 0x27,
	EMQ_PROTOCOL_SUCCESS_QUEUE_SUBSCRIBE = 0x28,
	EMQ_PROTOCOL_SUCCESS_QUEUE_UNSUBSCRIBE = 0x29,
	EMQ_PROTOCOL_SUCCESS_QUEUE_PURGE = 0x30,
	EMQ_PROTOCOL_SUCCESS_QUEUE_DELETE = 0x31,
	EMQ_PROTOCOL_ERROR = 0x40,
	EMQ_PROTOCOL_ERROR_PACKET = 0x41,
	EMQ_PROTOCOL_ERROR_COMMAND = 0x42,
	EMQ_PROTOCOL_ERROR_ACCESS = 0x43,
	EMQ_PROTOCOL_ERROR_AUTH = 0x44,
	EMQ_PROTOCOL_ERROR_PING = 0x45,
	EMQ_PROTOCOL_ERROR_STAT = 0x46,
	EMQ_PROTOCOL_ERROR_USER_CREATE = 0x50,
	EMQ_PROTOCOL_ERROR_USER_LIST = 0x51,
	EMQ_PROTOCOL_ERROR_USER_RENAME = 0x52,
	EMQ_PROTOCOL_ERROR_USER_SET_PERM = 0x53,
	EMQ_PROTOCOL_ERROR_USER_DELETE = 0x54,
	EMQ_PROTOCOL_ERROR_QUEUE_CREATE = 0x60,
	EMQ_PROTOCOL_ERROR_QUEUE_DECLARE = 0x61,
	EMQ_PROTOCOL_ERROR_QUEUE_EXIST = 0x62,
	EMQ_PROTOCOL_ERROR_QUEUE_LIST = 0x63,
	EMQ_PROTOCOL_ERROR_QUEUE_SIZE = 0x64,
	EMQ_PROTOCOL_ERROR_QUEUE_PUSH = 0x65,
	EMQ_PROTOCOL_ERROR_QUEUE_GET = 0x66,
	EMQ_PROTOCOL_ERROR_QUEUE_POP = 0x67,
	EMQ_PROTOCOL_ERROR_QUEUE_SUBSCRIBE = 0x68,
	EMQ_PROTOCOL_ERROR_QUEUE_UNSUBSCRIBE = 0x69,
	EMQ_PROTOCOL_ERROR_QUEUE_PURGE = 0x70,
	EMQ_PROTOCOL_ERROR_QUEUE_DELETE = 0x71
} protocol_response_status;

#pragma pack(push, 1)

typedef struct protocol_request_header {
	uint16_t magic;
	uint8_t cmd;
	uint8_t reserved;
	uint32_t bodylen;
} protocol_request_header;

typedef struct protocol_response_header {
	uint16_t magic;
	uint8_t cmd;
	uint8_t status;
	uint32_t bodylen;
} protocol_response_header;

typedef struct protocol_request_auth {
	protocol_request_header header;
	struct {
		char name[32];
		char password[32];
	} body;
} protocol_request_auth;

typedef protocol_request_header protocol_request_ping;
typedef protocol_request_header protocol_request_stat;
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
	} body;
} protocol_request_queue_pop;

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
		uint32_t resv1;
		uint32_t resv2;
		uint32_t resv3;
		uint32_t resv4;
	} body;
} protocol_response_stat;

typedef struct protocol_response_queue_size {
	protocol_response_header header;
	struct {
		uint32_t size;
	} body;
} protocol_response_queue_size;

#pragma pack(pop)

#endif
