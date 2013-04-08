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
#include <stdint.h>
#include <string.h>

#include "emq.h"
#include "packet.h"
#include "protocol.h"

#define strlenz(str) (strlen(str) + 1)

static void emq_set_header_request(protocol_request_header *header, uint8_t cmd,
	uint8_t noack, uint32_t bodylen)
{
	header->magic = EMQ_PROTOCOL_REQ;
	header->cmd = cmd;
	header->noack = noack;
	header->bodylen = bodylen;
}

static void emq_set_client_request(emq_client *client, void *request, size_t size)
{
	memcpy(client->request, request, size);
	client->pos = size;
}

static int emq_check_realloc_client_request(emq_client *client, size_t size)
{
	if (client->size < size) {
		if (size > EMQ_MAX_REQUEST_SIZE) {
			return EMQ_STATUS_ERR;
		}

		client->request = (char*)realloc(client->request, size);
		client->size = size;

		if (!client->request) {
			return EMQ_STATUS_ERR;
		}
	}

	return EMQ_STATUS_OK;
}

int emq_check_response_header(protocol_response_header *header, uint8_t cmd,
	uint8_t status_success, uint8_t status_error, uint32_t bodylen)
{
	if (header->magic != EMQ_PROTOCOL_RES || header->cmd != cmd ||
		(header->status != status_success && header->status != status_error) ||
		header->bodylen != bodylen) {
		return EMQ_STATUS_ERR;
	}

	return EMQ_STATUS_OK;
}

int emq_check_response_header_mini(protocol_response_header *header, uint8_t cmd,
	uint8_t status_success, uint8_t status_error)
{
	if (header->magic != EMQ_PROTOCOL_RES || header->cmd != cmd ||
		(header->status != status_success && header->status != status_error)) {
		return EMQ_STATUS_ERR;
	}

	return EMQ_STATUS_OK;
}

int emq_check_event_header(protocol_event_header *header, uint8_t cmd, uint8_t type1, uint8_t type2)
{
	if (header->magic != EMQ_PROTOCOL_EVENT || header->cmd != cmd ||
		(header->type != type1 && header->type != type2)) {
		return EMQ_STATUS_ERR;
	}

	return EMQ_STATUS_OK;
}

int emq_check_status(protocol_response_header *header, uint8_t status)
{
	return header->status == status ? EMQ_STATUS_OK : EMQ_STATUS_ERR;
}

int emq_auth_request(emq_client *client, const char *name, const char *password)
{
	protocol_request_auth req;

	if (strlenz(name) > 32 || strlenz(password) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_AUTH, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	memcpy(req.body.password, password, strlenz(password));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_ping_request(emq_client *client)
{
	protocol_request_ping req;

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_PING, client->noack, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_stat_request(emq_client *client)
{
	protocol_request_stat req;

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_STAT, client->noack, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_save_request(emq_client *client, uint8_t async)
{
	protocol_request_save req;

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_SAVE, client->noack, sizeof(req.body));

	req.body.async = async;

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_flush_request(emq_client *client, uint32_t flags)
{
	protocol_request_flush req;

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_FLUSH, client->noack, sizeof(req.body));

	req.body.flags = flags;

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_create_request(emq_client *client, const char *name, const char *password, uint64_t perm)
{
	protocol_request_user_create req;

	if (strlenz(name) > 32 || strlenz(password) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_CREATE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	memcpy(req.body.password, password, strlenz(password));
	req.body.perm = perm;

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_list_request(emq_client *client)
{
	protocol_request_user_list req;

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_USER_LIST, client->noack, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_rename_request(emq_client *client, const char *from, const char *to)
{
	protocol_request_user_rename req;

	if (strlenz(from) > 32 || strlenz(to) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_RENAME, client->noack, sizeof(req.body));

	memcpy(req.body.from, from, strlenz(from));
	memcpy(req.body.to, to, strlenz(to));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_set_perm_request(emq_client *client, const char *name, uint64_t perm)
{
	protocol_request_user_set_perm req;

	if (strlenz(name) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_SET_PERM, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	req.body.perm = perm;

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_delete_request(emq_client *client, const char *name)
{
	protocol_request_user_delete req;

	if (strlenz(name) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_DELETE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_create_request(emq_client *client, const char *name, uint32_t max_msg, uint32_t max_msg_size, uint32_t flags)
{
	protocol_request_queue_create req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_CREATE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	req.body.max_msg = max_msg;
	req.body.max_msg_size = max_msg_size;
	req.body.flags = flags;

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_declare_request(emq_client *client, const char *name)
{
	protocol_request_queue_declare req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_DECLARE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_exist_request(emq_client *client, const char *name)
{
	protocol_request_queue_exist req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_EXIST, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_list_request(emq_client *client)
{
	protocol_request_queue_list req;

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_QUEUE_LIST, client->noack, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_rename_request(emq_client *client, const char *from, const char *to)
{
	protocol_request_queue_rename req;

	if (strlenz(from) > 64 || strlenz(to) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_RENAME, client->noack, sizeof(req.body));

	memcpy(req.body.from, from, strlenz(from));
	memcpy(req.body.to, to, strlenz(to));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_size_request(emq_client *client, const char *name)
{
	protocol_request_queue_size req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_SIZE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_push_request(emq_client *client, const char *name, uint32_t msg_size)
{
	protocol_request_queue_push req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_PUSH, client->noack, 64 + msg_size);

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_get_request(emq_client *client, const char *name)
{
	protocol_request_queue_get req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_GET, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_pop_request(emq_client *client, const char *name)
{
	protocol_request_queue_pop req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_POP, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_subscribe_request(emq_client *client, const char *name, uint32_t flags)
{
	protocol_request_queue_subscribe req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_SUBSCRIBE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	req.body.flags = flags;

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_unsubscribe_request(emq_client *client, const char *name)
{
	protocol_request_queue_unsubscribe req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_UNSUBSCRIBE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_purge_request(emq_client *client, const char *name)
{
	protocol_request_queue_purge req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_PURGE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_delete_request(emq_client *client, const char *name)
{
	protocol_request_queue_delete req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_DELETE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_create_request(emq_client *client, const char *name, uint32_t flags)
{
	protocol_request_route_create req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_CREATE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	req.body.flags = flags;

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_exist_request(emq_client *client, const char *name)
{
	protocol_request_route_exist req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_EXIST, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_list_request(emq_client *client)
{
	protocol_request_route_list req;

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_ROUTE_LIST, client->noack, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_keys_request(emq_client *client, const char *name)
{
	protocol_request_route_keys req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_KEYS, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_rename_request(emq_client *client, const char *from, const char *to)
{
	protocol_request_route_rename req;

	if (strlenz(from) > 64 || strlenz(to) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_RENAME, client->noack, sizeof(req.body));

	memcpy(req.body.from, from, strlenz(from));
	memcpy(req.body.to, to, strlenz(to));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_bind_request(emq_client *client, const char *name, const char *queue, const char *key)
{
	protocol_request_route_bind req;

	if (strlenz(name) > 64 || strlenz(queue) > 64 || strlenz(key) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_BIND, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	memcpy(req.body.queue, queue, strlenz(queue));
	memcpy(req.body.key, key, strlenz(key));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_unbind_request(emq_client *client, const char *name, const char *queue, const char *key)
{
	protocol_request_route_unbind req;

	if (strlenz(name) > 64 || strlenz(queue) > 64 || strlenz(key) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_UNBIND, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));
	memcpy(req.body.queue, queue, strlenz(queue));
	memcpy(req.body.key, key, strlenz(key));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_push_request(emq_client *client, const char *name, const char *key, uint32_t msg_size)
{
	protocol_request_route_push req;

	if (strlenz(name) > 64 || strlenz(key) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_PUSH, client->noack, 96 + msg_size);

	memcpy(req.body.name, name, strlenz(name));
	memcpy(req.body.key, key, strlenz(key));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_route_delete_request(emq_client *client, const char *name)
{
	protocol_request_route_delete req;

	if (strlenz(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_ROUTE_DELETE, client->noack, sizeof(req.body));

	memcpy(req.body.name, name, strlenz(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}