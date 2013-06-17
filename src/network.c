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

#include "fmacros.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "emq.h"
#include "network.h"

static void net_set_error(char *err, const char *fmt,...)
{
	va_list list;

	if (!err) return;

	va_start(list, fmt);
	vsnprintf(err, EMQ_ERROR_BUF_SIZE, fmt, list);
	va_end(list);
}

static int net_create_socket(char *err, int domain)
{
	int sock, on = 1;

	if ((sock = socket(domain, SOCK_STREAM, 0)) == -1) {
		net_set_error(err, "socket: %s", strerror(errno));
		return EMQ_NET_ERR;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		net_set_error(err, "setsockopt: %s", strerror(errno));
		return EMQ_NET_ERR;
	}

	return sock;
}

static int net_tcp_nodelay(char *err, int fd)
{
	int yes = 1;

	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) {
		net_set_error(err, "setsockopt: %s", strerror(errno));
		return EMQ_NET_ERR;
	}

	return EMQ_NET_OK;
}

int emq_client_tcp_connect(emq_client *client, const char *addr, int port)
{
	struct sockaddr_in sa;
	struct hostent *he;

	if ((client->fd = net_create_socket(client->error, AF_INET)) == EMQ_NET_ERR) {
		return EMQ_NET_ERR;
	}

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	if (inet_aton(addr, &sa.sin_addr) == 0) {
		he = gethostbyname(addr);
		if (he == NULL) {
			net_set_error(client->error, "can't resolve: %s", addr);
			close(client->fd);
			return EMQ_NET_ERR;
		}
		memcpy(&sa.sin_addr, he->h_addr, sizeof(struct in_addr));
	}

	if (connect(client->fd, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
		net_set_error(client->error, "connect: %s", strerror(errno));
		close(client->fd);
		return EMQ_NET_ERR;
	}

	if (net_tcp_nodelay(client->error, client->fd) == EMQ_NET_ERR) {
		close(client->fd);
		return EMQ_NET_ERR;
	}

	return EMQ_NET_OK;
}

int emq_client_unix_connect(emq_client *client, const char *path)
{
	struct sockaddr_un sa;

	if ((client->fd = net_create_socket(client->error, AF_LOCAL)) == EMQ_NET_ERR) {
		return EMQ_NET_ERR;
	}

	sa.sun_family = AF_LOCAL;
	strncpy(sa.sun_path, path, sizeof(sa.sun_path)-1);

	if (connect(client->fd, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
		net_set_error(client->error, "connect: %s", strerror(errno));
		close(client->fd);
		return EMQ_NET_ERR;
	}

	return EMQ_NET_OK;
}

int emq_client_read(emq_client *client, char *buf, int count)
{
	int nread, totlen = 0;

	while (totlen != count)
	{
		nread = read(client->fd, buf, count-totlen);

		if (nread == -1 || nread == 0) return -1;

		totlen += nread;
		buf += nread;
	}

	return totlen;
}

int emq_client_write(emq_client *client, char *buf, int count)
{
	int nwritten, totlen = 0;

	while (totlen != count)
	{
		nwritten = write(client->fd, buf, count-totlen);

		if (nwritten == 0) return totlen;
		if (nwritten == -1) return -1;

		totlen += nwritten;
		buf += nwritten;
	}

	return totlen;
}

int emq_client_writev(emq_client *client, struct iovec *iov, int iovcnt)
{
	int ret;

	while ((ret = writev(client->fd, iov, iovcnt)) == -1 && errno == EINTR);

	return ret;
}

void emq_client_disconnect(emq_client *client)
{
	close(client->fd);
}
