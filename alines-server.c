#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>

#include "shared.c"

#define TMPFILE_TEMPLATE "/tmp/alines-XXXXXX"

typedef enum { FR_OK = 0, FR_UI, FR_MENUER } fail_reason;

// Globals
extern char **environ;
char *G_programArgv[1024] = { NULL };
char *G_password = "";

void usage()
{
	fprintf(stderr,
		"alines-server [options] <program/script> [program args...]\n"
		"Options:\n"
		"\t-h help\n"
		"\t-p <port (64937)>\n"
		"\t-P <password (none)>\n"
	);
	exit(1);
}

void fillTempName(char filename[sizeof(TMPFILE_TEMPLATE)])
{
	memcpy(filename, TMPFILE_TEMPLATE, sizeof(TMPFILE_TEMPLATE));
	const int fd = mkstemp(filename);
	if (fd < 0) {
		panic("mkstemp fail");
	}
	close(fd);
	unlink(filename);
}

void uiDisconnect(int uifd, char *msg)
{
	log_info(msg);
	fdWriteU8(uifd, PKID_TO_UI_DISCONNECT); // packet id
	fdWriteStr(uifd, msg);
	close(uifd);
}

void uiCloseMenu(int uifd)
{
	fdWriteU8(uifd, PKID_TO_UI_CLOSE_MENU); // packet id
}

void menuerCloseDisconnect(int menuerfd)
{
	fdWriteU8(menuerfd, PKID_FROM_UI_NO_SELECTION); // packet id
	close(menuerfd);
}

fail_reason copyNewMenu(int menuerfd, int uifd)
{
	char strCpyBuf[u16_MAX + 1];

	// Read header
	u8 flags;
	u16 entryCount, selectedEntry;
	if (!fdReadU8(menuerfd, &flags)
			|| !fdReadU16(menuerfd, &entryCount)
			|| !fdReadU16(menuerfd, &selectedEntry)
			|| fdReadStrBuf(menuerfd, strCpyBuf) < 0) {
		return FR_MENUER;
	}

	// Write header
	if (!fdWriteU8(uifd, PKID_TO_UI_OPEN_MENU)
			|| !fdWriteU8(uifd, flags)
			|| !fdWriteU16(uifd, entryCount)
			|| !fdWriteU16(uifd, selectedEntry)
			|| !fdWriteStr(uifd, strCpyBuf)) {
		return FR_MENUER;
	}

	// Copy menu entries
	for (u16 i = 0; i < entryCount; i++) {
		if (fdReadStrBuf(menuerfd, strCpyBuf) < 0) return FR_MENUER;
		if (!fdWriteStr(uifd, strCpyBuf)) return FR_UI;
	}
	return FR_OK;
}

fail_reason copyUiResponse(int uifd, int menuerfd)
{
	u8 pkId;
	if (!fdReadU8(uifd, &pkId)) return FR_UI;

	if (pkId == PKID_FROM_UI_NO_SELECTION) {
		log_info("ui sent close");
		if (!fdWriteU8(menuerfd, pkId)) return FR_MENUER;
	} else if (pkId == PKID_FROM_UI_SINGLE_SELECTION) {
		log_info("ui sent single");
		u16 sel;
		if (!fdReadU16(uifd, &sel)) return FR_UI;
		if (!fdWriteU8(menuerfd, pkId) || !fdWriteU16(menuerfd, sel)) return FR_MENUER;
	} else if (pkId == PKID_FROM_UI_MULTI_SELECTION) {
		// TODO: verify that UI was allowed to send this response
		log_info("ui sent multi");
		u16 len, idx;
		if (!fdReadU16(uifd, &len)) return FR_UI;
		if (!fdWriteU8(menuerfd, pkId) || !fdWriteU16(menuerfd, len)) return FR_MENUER;
		for (u16 i = 0; i < len; i++) {
			if (!fdReadU16(uifd, &idx)) return FR_UI;
			if (!fdWriteU16(menuerfd, idx)) return FR_MENUER;
		}
	} else if (pkId == PKID_FROM_UI_CUSTOM_ENTRY) {
		// TODO: verify that UI was allowed to send this response
		log_info("ui sent custom");
		char entryStr[u16_MAX + 1];
		if (fdReadStrBuf(uifd, entryStr) < 0) return FR_UI;
		if (!fdWriteU8(menuerfd, pkId) || !fdWriteStr(menuerfd, entryStr)) return FR_MENUER;
	} else {
		return FR_UI;
	}
	return FR_OK;
}

void accept_menuer(int menuerServerFd, int uifd)
{
	const int menuerfd = accept(menuerServerFd, NULL, NULL);
	if (menuerfd < 0) {
		panic("menuer accept failed");
	}

	log_info("new menuer");
	fail_reason fr = copyNewMenu(menuerfd, uifd);
	if (fr == FR_MENUER) {
		uiDisconnect(uifd, "menuer not responding with menu");
		close(menuerfd);
		return;
	} else if (fr == FR_UI) {
		log_info("failed to send menu to ui");
		close(uifd);
		menuerCloseDisconnect(menuerfd);
		return;
	}
	log_info("menu sent to ui");

	// Poll on both UI and Menuer to see if Menuer disconnects before UI sends a response
	struct pollfd pollFds[2] = {
		{
			.fd = uifd,
			.events = POLLIN,
		},
		{
			.fd = menuerfd,
		},
	};
	while (true) {
		if (poll(pollFds, 2, 100) < 0) {
			continue;
		}

		// If UI has data, copy response to Menuer
		if (pollFds[0].revents & POLLIN) {
			fr = copyUiResponse(uifd, menuerfd);
			if (fr == FR_MENUER) {
				log_info("menuer not accepting response");
				uiCloseMenu(uifd);
				close(menuerfd);
				return;
			} else if (fr == FR_UI) {
				log_info("failed to get response from UI");
				close(uifd);
				menuerCloseDisconnect(menuerfd);
				return;
			}
			close(menuerfd);
			return;
		}

		// If Menuer disconnected, close menu
		if (pollFds[1].revents & (POLLERR | POLLHUP)) {
			log_info("menuer disconnected");
			uiCloseMenu(uifd);
			close(menuerfd);
			return;
		}
	}
}

void *ui_thread(void *data)
{
	const int uifd = (int)(size_t)data;
	log_info("new UI client");

	// Handshake
	{
		log_info("awaiting handshake");
		char clientPassword[u16_MAX + 1];
		if (fdReadStrBuf(uifd, clientPassword) < 0) {
			uiDisconnect(uifd, "expected password");
			return NULL;
		}
		const bool passMatch = !strcmp(clientPassword, G_password);
		if (!passMatch) {
			uiDisconnect(uifd, "incorrect password");
			return NULL;
		}
		log_info("handshake ok!");
	}

	// Inherit env from parent
	size_t parentEnvCount = 0;
	{
		char **e = environ;
		while (*(e++)) parentEnvCount++;
	}
	char **programEnv = xmalloc((parentEnvCount + 2) * sizeof(char*));
	programEnv[parentEnvCount + 1] = NULL;
	for (size_t i = 0; i < parentEnvCount; i++) {
		programEnv[i] = environ[i];
	}

	// Create env var for domain socket
	char socketPathEnv[] = ENV_ALINES_SOCKET "=" TMPFILE_TEMPLATE;
	char *socketPath = &socketPathEnv[sizeof(ENV_ALINES_SOCKET)];
	fillTempName(socketPath);
	programEnv[parentEnvCount] = socketPathEnv;

	// Exec
	const int forkPid = fork();
	if (forkPid < 0) {
		panic("fork failed");
	} else if (!forkPid) {
		execve(G_programArgv[0], G_programArgv, programEnv);
		panic("program exec failed");
	}

	// Domain socket bind & listen
	struct sockaddr_un sockAddr;
	sockAddr.sun_family = AF_UNIX;
	strcpy(sockAddr.sun_path, socketPath);
	const int menuerServerFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (bind(menuerServerFd, (struct sockaddr*)(&sockAddr), sizeof(sockAddr))) {
		panic("menuer domain socket server bind failed");
	}
	if (listen(menuerServerFd, 100)) {
		panic("menuer domain socket server listen failed");
	}
	log_info("listening for menuers...");

	// Socket accept select
	struct pollfd pollFd = {
		.fd = menuerServerFd,
		.events = POLLIN,
	};

	// Accept menuers
	while (true) {
		if (kill(forkPid, 0)) {
			// TODO: report success/error based on exit code
			uiDisconnect(uifd, "program exited");
			break;
		}

		if (poll(&pollFd, 1, 100) < 0) {
			continue;
		}

		if (pollFd.revents & POLLIN) {
			log_info("accepting menuer...");
			accept_menuer(menuerServerFd, uifd);
		}
	}

	close(menuerServerFd);
	free(programEnv);
	return NULL;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
	}

	// Parse args
	u16 port = 64937;
	int argi = 1;
	for (; argi < argc; argi++) {
		if (!strcmp(argv[argi], "-h")) {
			usage();
		} else if (!strcmp(argv[argi], "-p") && argi + 1 < argc) {
			port = atoi(argv[++argi]);
			if (!port) {
				panic("invalid port");
			}
		} else if (!strcmp(argv[argi], "-P") && argi + 1 < argc) {
			G_password = argv[++argi];
		} else if (argv[argi][0] == '-') {
			usage();
		} else {
			break;
		}
	}
	if (argi == argc) {
		usage();
	}

	// Prepare G_programArgv in the form `execve` expects
	{
		const size_t progArgCount = argc - argi;
		if (progArgCount >= sizeof(G_programArgv) / sizeof(G_programArgv[0])) {
			panic("too many args");
		}
		for (size_t i = 0; i < progArgCount; i++) {
			G_programArgv[i] = argv[argi + i];
		}
		G_programArgv[progArgCount] = NULL;
	}

	// Automatically kill defunct child processes
	signal(SIGCHLD, SIG_IGN);

	// Listen
	const int menuerServerFd = socket(AF_INET, SOCK_STREAM, 0);
	const struct sockaddr_in serverAddr = {
		.sin_family = AF_INET,
		.sin_addr = {
			.s_addr = htonl(INADDR_ANY),
		},
		.sin_port = htons(port),
	};
	setsockopt(menuerServerFd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if (bind(menuerServerFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) {
		panic("socket bind failed");
	}
	if (listen(menuerServerFd, 10)) {
		panic("socket listen failed");
	}

	log_info("server running");
	while (true) {
		// Accept
		struct sockaddr_in uiAddr;
		const int uifd = accept(menuerServerFd, (struct sockaddr*)&uiAddr, &(socklen_t){sizeof(uiAddr)});
		if (uifd < 0) {
			panic("UI client accept failed");
		}

		// Start UI thread
		pthread_t thread;
		if (pthread_create(&thread, NULL, ui_thread, (void*)(size_t)uifd)) {
			panic("thread creation failed");
		}
		pthread_detach(thread);
	}

	return 0;
}
