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

#ifndef _EMQ_PACKET_H_
#define _EMQ_PACKET_H_

#include <stdint.h>

#include "emq.h"
#include "protocol.h"

int emq_check_response_header(protocol_response_header *header, uint8_t cmd, uint8_t status, uint32_t bodylen);

int emq_auth_request(emq_client *client, const char *name, const char *password);
int emq_ping_request(emq_client *client);
int emq_stat_request(emq_client *client);

int emq_user_create_request(emq_client *client, const char *name, const char *password, uint64_t perm);
int emq_user_list_request(emq_client *client);
int emq_user_rename_request(emq_client *client, const char *from, const char *to);
int emq_user_set_perm_request(emq_client *client, const char *name, uint64_t perm);
int emq_user_delete_request(emq_client *client, const char *name);

int emq_queue_create_request(emq_client *client, const char *name, uint32_t max_msg, uint32_t max_msg_size, uint32_t flags);
int emq_queue_declare_request(emq_client *client, const char *name);
int emq_queue_exist_request(emq_client *client, const char *name);
int emq_queue_list_request(emq_client *client);
int emq_queue_size_request(emq_client *client, const char *name);
int emq_queue_push_request(emq_client *client, const char *name, uint32_t msg_size);
int emq_queue_get_request(emq_client *client, const char *name);
int emq_queue_pop_request(emq_client *client, const char *name);
int emq_queue_purge_request(emq_client *client, const char *name);
int emq_queue_delete_request(emq_client *client, const char *name);

#endif
