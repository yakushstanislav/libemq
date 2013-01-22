# LIBEMQ
libemq - is an open source client library for the EagleMQ written in C.

It has been designed to be light on memory usage, high performance, and provide full access to server side methods.

# Methods

## Common methods

### emq\_client *emq\_tcp\_connect(const char *addr, int port);
Connect to the EagleMQ server via TCP protocol.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>addr</td>
		<td>the server IP</td>
	</tr>
	<tr>
		<td>2</td>
		<td>port</td>
		<td>the server port</td>
	</tr>
</table>

Return: emq\_client on success, NULL on error.

### emq\_client *emq\_unix\_connect(const char *path);
Connect to the EagleMQ server via unix domain socket.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>path</td>
		<td>the path to unix domain socket</td>
	</tr>
</table>

Return: emq\_client on success, NULL on error.

### void emq\_disconnect(emq\_client *client);
Disconnects the client from the server and removes the connection context.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

### void emq\_noack\_enable(emq\_client *client);
Enable noack mode.

When enabled, the server does not respond to commands status messages and the result is always EMQ\_STATUS\_OK.
Using this option is useful for performance.
Also, it should be used to send commands if you are subscribed to the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

### void emq\_noack\_disable(emq\_client *client);
Disable noack mode.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

### int emq\_process(emq\_client *client);
Processing of all server events.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### void emq\_list\_rewind(emq\_list *list, emq\_list\_iterator *iter);
Initialize the list iterator.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Direction</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>list</td>
		<td>in</td>
		<td>the list</td>
	</tr>
		<tr>
		<td>2</td>
		<td>iter</td>
		<td>out</td>
		<td>the list iterator</td>
	</tr>
</table>

### emq\_list\_node *emq\_list\_next(emq\_list\_iterator *iter);
Gets the next value list.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>iter</td>
		<td>the list iterator</td>
	</tr>
</table>

Return: emq\_list\_node.

### void emq\_list\_release(emq\_list *list);
Delete list.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>list</td>
		<td>the list</td>
	</tr>
</table>

### char *emq\_last\_error(emq\_client *client);
Description of the last error client.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

Return: description of the error.

### int emq\_version(void);
Returns the version libemq.

Return: version libemq.

## Messages methods

### emq\_msg *emq\_msg\_create(void *data, size\_t size, int zero\_copy);
Create a new message.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>data</td>
		<td>the message data</td>
	</tr>
	<tr>
		<td>2</td>
		<td>size</td>
		<td>the message size</td>
	</tr>
		<tr>
		<td>3</td>
		<td>zero_copy</td>
		<td>flag to use zero copy (EMQ_ZEROCOPY_ON, EMQ_ZEROCOPY_OFF)</td>
	</tr>
</table>

Return: new message.

### emq\_msg *emq\_msg\_copy(emq\_msg *msg);
Copy the message.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>msg</td>
		<td>the message</td>
	</tr>
</table>

Return: copy of the message.

### void *emq\_msg\_data(emq\_msg *msg);
Get the message data.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>msg</td>
		<td>the message</td>
	</tr>
</table>

Return: message data.

### size\_t emq\_msg\_size(emq\_msg *msg);
Get the message size.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>msg</td>
		<td>the message</td>
	</tr>
</table>

Return: size of the message.

### void emq\_msg\_release(emq\_msg *msg)
Delete the message.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>msg</td>
		<td>the message</td>
	</tr>
</table>

## Basic methods

### int emq\_auth(emq\_client *client, const char *name, const char *password);
Client authentication on the server.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the user name</td>
	</tr>
	<tr>
		<td>3</td>
		<td>password</td>
		<td>the user password</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_ping(emq\_client *client);
Ping server. Used to check the server status.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_stat(emq\_client *client, emq\_status *status);
Get server statistics.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Direction</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>in</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>status</td>
		<td>out</td>
		<td>the server statistics</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq_save(emq_client *client, uint8_t async);
Saving data to disk.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>async</td>
		<td>Saving mode (0 - synchronously save, 1 - asynchronously save)</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_flush(emq\_client *client, uint32\_t flags);
Deleting the data on the server.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>flags</td>
		<td>flags to delete the data from the server (EMQ_FLUSH_ALL, EMQ_FLUSH_USER, EMQ_FLUSH_QUEUE)</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

## User methods

### int emq\_user\_create(emq\_client *client, const char *name, const char *password, emq\_perm perm);
Create a new user.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the user name</td>
	</tr>
	<tr>
		<td>3</td>
		<td>password</td>
		<td>the user password</td>
	</tr>
	<tr>
		<td>4</td>
		<td>perm</td>
		<td>the user permissions (see Table 1)</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### emq\_list *emq\_user\_list(emq\_client *client);
Get a list of all users.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

Return: list of users.

### int emq\_user\_rename(emq\_client *client, const char *from, const char *to);
Rename the user.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>from</td>
		<td>the old user name</td>
	</tr>
	<tr>
		<td>3</td>
		<td>to</td>
		<td>the new user name</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_user\_set\_perm(emq\_client *client, const char *name, emq\_perm perm);
Set permissions to the user.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the user name</td>
	</tr>
	<tr>
		<td>3</td>
		<td>perm</td>
		<td>the user permissions (see Table 1)</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_user\_delete(emq\_client *client, const char *name);
Delete the user.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the user name</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

## Table 1. Description of user permissions

<table border="1">
    <tr>
        <td><b>№</b></td>
        <td><b>Name</b></td>
        <td><b>Command</b></td>
        <td><b>Description</b></td>
    </tr>
    <tr>
        <td>1</td>
        <td>EMQ_QUEUE_PERM</td>
        <td>
            <i>.queue_create</i><br/>
            <i>.queue_declare</i><br/>
            <i>.queue_exist</i><br/>
            <i>.queue_list</i><br/>
            <i>.queue_size</i><br/>
            <i>.queue_push</i><br/>
            <i>.queue_get</i><br/>
            <i>.queue_pop</i><br/>
            <i>.queue_subscribe</i><br/>
            <i>.queue_unsubscribe</i><br/>
            <i>.queue_purge</i><br/>
            <i>.queue_delete</i>
		</td>
        <td>The ability to perform all the commands associated with queues</td>
    </tr>
    <tr>
        <td>2</td>
        <td>EMQ_ADMIN_PERM</td>
        <td>-</td>
        <td>The ability to perform all the commands on the server (including the commands for working with users)</td>
    </tr>
    <tr>
        <td>3</td>
        <td>EMQ_NOT_CHANGE_PERM</td>
        <td>-</td>
        <td>If a user is created with the flag, then it can not be removed</td>
    </tr>
    <tr>
        <td>4</td>
        <td>EMQ_QUEUE_CREATE_PERM</td>
        <td>.queue_create</td>
        <td>Permission to create queues</td>
    </tr>
    <tr>
        <td>5</td>
        <td>EMQ_QUEUE_DECLARE_PERM</td>
        <td>.queue_declare</td>
        <td>Permission to declare the queue</td>
    </tr>
    <tr>
        <td>6</td>
        <td>EMQ_QUEUE_EXIST_PERM</td>
        <td>.queue_exist</td>
        <td>Permission to check the existence of the queue</td>
    </tr>
    <tr>
        <td>7</td>
        <td>EMQ_QUEUE_LIST_PERM</td>
        <td>.queue_list</td>
        <td>Permission to get a list of queues</td>
    </tr>
    <tr>
        <td>8</td>
        <td>EMQ_QUEUE_SIZE_PERM</td>
        <td>.queue_size</td>
        <td>Permission to get the size of the queue</td>
    </tr>
    <tr>
        <td>9</td>
        <td>EMQ_QUEUE_PUSH_PERM</td>
        <td>.queue_push</td>
        <td>Permission to push messages to the queue</td>
    </tr>
    <tr>
        <td>10</td>
        <td>EMQ_QUEUE_GET_PERM</td>
        <td>.queue_get</td>
        <td>Permission to get messages from the queue</td>
    </tr>
    <tr>
        <td>11</td>
        <td>EMQ_QUEUE_POP_PERM</td>
        <td>.queue_pop</td>
        <td>Permission to pop messages from the queue</td>
    </tr>
    <tr>
        <td>12</td>
        <td>EMQ_QUEUE_SUBSCRIBE_PERM</td>
        <td>.queue_subscribe</td>
        <td>Permission to subscribe to the queue</td>
    </tr>
    <tr>
        <td>13</td>
        <td>EMQ_QUEUE_UNSUBSCRIBE_PERM</td>
        <td>.queue_unsubscribe</td>
        <td>Permission to unsubscribe to the queue</td>
    </tr>
    <tr>
        <td>14</td>
        <td>EMQ_QUEUE_PURGE_PERM</td>
        <td>.queue_purge</td>
        <td>Permission to delete all messages from the queue</td>
    </tr>
    <tr>
        <td>15</td>
        <td>EMQ_QUEUE_DELETE_PERM</td>
        <td>.queue_delete</td>
        <td>Permission to delete queue</td>
    </tr>
</table>

## Queue methods

### int emq\_queue\_create(emq\_client *client, const char *name, uint32\_t max\_msg, uint32\_t max\_msg\_size, uint32\_t flags);
Create the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
	<tr>
		<td>3</td>
		<td>max_msg</td>
		<td>the maximum number of messages</td>
	</tr>
	<tr>
		<td>4</td>
		<td>max_msg_size</td>
		<td>the maximum message size</td>
	</tr>
	<tr>
		<td>5</td>
		<td>flags</td>
		<td>flags to create a queue (EMQ_QUEUE_NONE, EMQ_QUEUE_AUTODELETE, EMQ_QUEUE_FORCE_PUSH, EMQ_QUEUE_ROUND_ROBIN, EMQ_QUEUE_DURABLE)</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_queue\_declare(emq\_client *client, const char *name);
Declare the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_queue\_exist(emq\_client *client, const char *name);
Check the queue of existence.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: Value greater than 0 if the queue exists.

### emq\_list *emq\_queue\_list(emq\_client *client);
Get a list of queues.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
</table>

Return: list of queues.

### size\_t emq\_queue\_size(emq\_client *client, const char *name);
Get the number of elements in the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: number of elements in the queue.

### int emq\_queue\_push(emq\_client *client, const char *name, emq\_msg *msg);
Push a message to the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
	<tr>
		<td>3</td>
		<td>msg</td>
		<td>the message</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### emq\_msg *emq\_queue\_get(emq\_client *client, const char *name);
Get a message from the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: message on success, NULL on error.

### emq\_msg *emq\_queue\_pop(emq\_client *client, const char *name);
Pop a message from the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: message on success, NULL on error.

### int emq\_queue\_subscribe(emq\_client *client, const char *name, uint32\_t flags, emq\_msg\_callback *callback);
Subscribe to the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
	<tr>
		<td>3</td>
		<td>flags</td>
		<td>flags subscription (EMQ_QUEUE_SUBSCRIBE_MSG, EMQ_QUEUE_SUBSCRIBE_NOTIFY)</td>
	</tr>
	<tr>
		<td>4</td>
		<td>callback</td>
		<td>callback to the event subscription</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_queue\_unsubscribe(emq\_client *client, const char *name);
Unsubscribe from the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_queue\_purge(emq\_client *client, const char *name);
Purge the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

### int emq\_queue\_delete(emq\_client *client, const char *name);
Delete the queue.

<table border="1">
	<tr>
		<td><b>№</b></td>
		<td><b>Name</b></td>
		<td><b>Description</b></td>
	</tr>
	<tr>
		<td>1</td>
		<td>client</td>
		<td>the context of a client connection</td>
	</tr>
	<tr>
		<td>2</td>
		<td>name</td>
		<td>the queue name</td>
	</tr>
</table>

Return: EMQ\_STATUS\_OK on success, EMQ\_STATUS\_ERR on error.

# Author
libemq has written by Stanislav Yakush(st.yakush@yandex.ru) and is released under the BSD license.
