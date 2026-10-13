//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

//
// Forwarder example based on https://github.com/C-o-r-E/nng_pubsub_proxy
//
// This example shows how to use raw sockets to set up a forwarder or proxy for
// pub/sub using IPC (Inter-Process Communication) transport.
//
// IPC SETUP:
// - Front endpoint (ipc:///tmp/pubsub_proxy_front.sock): Publishers connect here
// - Back endpoint (ipc:///tmp/pubsub_proxy_back.sock): Subscribers connect here
// - The forwarder relays all messages from front to back in raw mode (no filtering)
//
// An example setup for running this example would involve the following:
//
//  - Run this example binary (in the background or a terminal, etc)
//  - In a new terminal, connect a subscriber:
//      `nngcat --sub --dial "ipc:///tmp/pubsub_proxy_back.sock" --quoted`
//  - In a second terminal, connect another subscriber:
//      `nngcat --sub --dial "ipc:///tmp/pubsub_proxy_back.sock" --quoted`
//  - In a third terminal, publish messages:
//      `for n in $(seq 0 99);`
//        `do nngcat --pub --dial "ipc:///tmp/pubsub_proxy_front.sock" --data "$n";`
//      `done`
//
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <nng/nng.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/pubsub0/pub.h>

#include "NngIpcPubsubDefined.h"

#ifndef NNGIPC_DIR_PATH
#define NNGIPC_DIR_PATH "/tmp/nngipc/"
#endif
// Each use normalizes NNGIPC_DIR_PATH because external builds can override the
// default configuration without preserving its trailing '/'.
static int normalize_ipc_directory_path(char *path, size_t path_size)
{
    int length = snprintf(path, path_size, "%s", NNGIPC_DIR_PATH);
    if (length <= 0 || (size_t)length >= path_size) {
        fprintf(stderr, "Invalid NNGIPC_DIR_PATH\\n");
        return -1;
    }
    if (path[length - 1] != '/') {
        if ((size_t)length + 1 >= path_size) {
            fprintf(stderr, "NNGIPC_DIR_PATH is too long\\n");
            return -1;
        }
        path[length++] = '/';
        path[length] = '\0';
    }
    return 0;
}

bool utils_runCmd(const char *argv[])
{
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], (char* const*)argv);
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            return false;
        }
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    return false;
}

void
panic_on_error(int should_panic, const char *format, ...)
{
	if (should_panic) {
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		exit(EXIT_FAILURE);
	}
}

int
main(int argc, char **argv)
{
	nng_socket sock_front_end = NNG_SOCKET_INITIALIZER;
	nng_socket sock_back_end  = NNG_SOCKET_INITIALIZER;
	int        ret            = 0;
	int        socket_mode    = 0755; // IPC socket file permissions

	(void) argc;
	(void) argv;

	char ipc_dir_path[256];
	char front_end_url[256];
	char back_end_url[256];
	if (normalize_ipc_directory_path(ipc_dir_path, sizeof(ipc_dir_path)) != 0 ||
	    snprintf(front_end_url, sizeof(front_end_url), "ipc://%s%s",
		    ipc_dir_path, NNGIPC_PUBLISH_NAME) >= (int)sizeof(front_end_url) ||
	    snprintf(back_end_url, sizeof(back_end_url), "ipc://%s%s",
		    ipc_dir_path, NNGIPC_SUBSCRIBE_NAME) >= (int)sizeof(back_end_url)) {
		fprintf(stderr, "Invalid NNG IPC endpoint URL\\n");
		return EXIT_FAILURE;
	}

	const char *cmd[] = {"mkdir", "-p", ipc_dir_path, NULL};
	utils_runCmd(cmd);

	//
	//  First we need some nng sockets. Not to be confused with network
	//  sockets
	//
#if 0
	ret = nng_init(NULL);
	panic_on_error(ret, "Failed to init library\n");
#endif

	// Create raw mode sockets for pub/sub forwarding
	// Raw mode means no protocol-level filtering; all messages are relayed
	ret = nng_sub0_open_raw(&sock_front_end);
	panic_on_error(ret, "Failed to open front end socket\n");

	ret = nng_pub0_open_raw(&sock_back_end);
	panic_on_error(ret, "Failed to open back end socket\n");

#if 0
	// Configure socket options before creating listeners
	// This sets the receive buffer size for the front-end socket
	size_t recv_buf = 1 * 1024 * 1024; // 1MB receive buffer
	ret = nng_socket_set_size(sock_front_end, NNG_OPT_RECVBUF, recv_buf);
	if (ret != 0) {
		fprintf(stderr, "Warning: Failed to set receive buffer: %s\n",
		    nng_strerror(ret));
	}

	// Set the send buffer size for the back-end socket
	size_t send_buf = 1 * 1024 * 1024; // 1MB send buffer
	ret = nng_socket_set_size(sock_back_end, NNG_OPT_SENDBUF, send_buf);
	if (ret != 0) {
		fprintf(stderr, "Warning: Failed to set send buffer: %s\n",
		    nng_strerror(ret));
	}
#endif

	//
	//  Now we need to set up a listener for each socket so that they have
	//  addresses
	//

	nng_listener front_ls = NNG_LISTENER_INITIALIZER;
	nng_listener back_ls  = NNG_LISTENER_INITIALIZER;

	// Create listeners for IPC endpoints
	printf("Creating front listener with URL: %s\n", front_end_url);
	ret = nng_listener_create(&front_ls, sock_front_end, front_end_url);
	panic_on_error(ret, "Failed to create front listener: %s (error code: %d)\n", nng_strerror(ret), ret);

	printf("Creating front listener with URL: %s\n", back_end_url);
	ret = nng_listener_create(&back_ls, sock_back_end, back_end_url);
	panic_on_error(ret, "Failed to create back listener: %s (error code: %d)\n", nng_strerror(ret), ret);

	// Configure IPC socket permissions before starting listeners
	// This makes the sockets accessible by the user
	ret = nng_listener_set_int(front_ls, NNG_OPT_IPC_PERMISSIONS, socket_mode);
	if (ret != 0) {
		fprintf(stderr, "Warning: Failed to set front listener permissions: %s\n",
		    nng_strerror(ret));
	}

	ret = nng_listener_set_int(back_ls, NNG_OPT_IPC_PERMISSIONS, socket_mode);
	if (ret != 0) {
		fprintf(stderr, "Warning: Failed to set back listener permissions: %s\n",
		    nng_strerror(ret));
	}

	// Start the listeners
	ret = nng_listener_start(front_ls, 0);
	panic_on_error(ret, "Failed to start front listener: %s (error code: %d)\n", nng_strerror(ret), ret);

	printf("Front listener started at %s\n", front_end_url);

	ret = nng_listener_start(back_ls, 0);
	panic_on_error(ret, "Failed to start back listener: %s (error code: %d)\n", nng_strerror(ret), ret);

	printf("Back listener started at %s\n", back_end_url);

	//
	//  Finally let nng do the forwarding/proxying
	//  This blocks and continuously forwards messages between the two sockets
	//

	printf("Pub/Sub forwarder running. Press Ctrl+C to exit.\n");
	ret = nng_device(sock_front_end, sock_back_end);
	panic_on_error(
	    ret, "nng_device returned %d: %s\n", ret, nng_strerror(ret));

	printf("done");
	nng_fini();
	return 0;
}
