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

static void emq_set_header_request(protocol_request_header *header, uint8_t cmd, uint32_t bodylen)
{
	header->magic = EMQ_PROTOCOL_REQ;
	header->cmd = cmd;
	header->reserved = 0;
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

int emq_check_status(protocol_response_header *header, uint8_t status)
{
	return header->status == status ? EMQ_STATUS_OK : EMQ_STATUS_ERR;
}

int emq_auth_request(emq_client *client, const char *name, const char *password)
{
	protocol_request_auth req;

	if (strlen(name) > 32 || strlen(password) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_AUTH, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));
	memcpy(req.body.password, password, strlen(password));

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

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_PING, 0);

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

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_STAT, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_create_request(emq_client *client, const char *name, const char *password, uint64_t perm)
{
	protocol_request_user_create req;

	if (strlen(name) > 32 || strlen(password) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_CREATE, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));
	memcpy(req.body.password, password, strlen(password));
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

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_USER_LIST, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_rename_request(emq_client *client, const char *from, const char *to)
{
	protocol_request_user_rename req;

	if (strlen(from) > 32 || strlen(to) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_RENAME, sizeof(req.body));

	memcpy(req.body.from, from, strlen(from));
	memcpy(req.body.to, to, strlen(to));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_user_set_perm_request(emq_client *client, const char *name, uint64_t perm)
{
	protocol_request_user_set_perm req;

	if (strlen(name) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_SET_PERM, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));
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

	if (strlen(name) > 32) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_USER_DELETE, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_create_request(emq_client *client, const char *name, uint32_t max_msg, uint32_t max_msg_size, uint32_t flags)
{
	protocol_request_queue_create req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_CREATE, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));
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

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_DECLARE, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_exist_request(emq_client *client, const char *name)
{
	protocol_request_queue_exist req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_EXIST, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

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

	emq_set_header_request(&req, EMQ_PROTOCOL_CMD_QUEUE_LIST, 0);

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_size_request(emq_client *client, const char *name)
{
	protocol_request_queue_size req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_SIZE, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_push_request(emq_client *client, const char *name, uint32_t msg_size)
{
	protocol_request_queue_push req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_PUSH, 64 + msg_size);

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_get_request(emq_client *client, const char *name)
{
	protocol_request_queue_get req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_GET, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_pop_request(emq_client *client, const char *name)
{
	protocol_request_queue_pop req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_POP, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_purge_request(emq_client *client, const char *name)
{
	protocol_request_queue_purge req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_PURGE, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}

int emq_queue_delete_request(emq_client *client, const char *name)
{
	protocol_request_queue_delete req;

	if (strlen(name) > 64) {
		return EMQ_STATUS_ERR;
	}

	memset(&req, 0, sizeof(req));

	emq_set_header_request(&req.header, EMQ_PROTOCOL_CMD_QUEUE_DELETE, sizeof(req.body));

	memcpy(req.body.name, name, strlen(name));

	if (emq_check_realloc_client_request(client, sizeof(req)) == EMQ_STATUS_ERR) {
		return EMQ_STATUS_ERR;
	}

	emq_set_client_request(client, &req, sizeof(req));

	return EMQ_STATUS_OK;
}
