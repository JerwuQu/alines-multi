#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <assert.h>

#include "shared.c"

void usage()
{
	fprintf(stderr,
		"alines-menu [options]\n"
		"Options:\n"
		"\t-h help\n"
		"\t-t <menu title>\n"
		"\t-i output selected index rather than string\n"
		"\t-m enable mutli-select\n"
		"\t-c enable custom entry\n"
	);
	exit(1);
}

int main(int argc, char **argv)
{
	bool outputIndex = false;
	char *title = "menu";
	u8 flags = 0;

	// TODO: add spec support for "selected index"

	int argi = 1;
	for (; argi < argc; argi++) {
		if (!strcmp(argv[argi], "-h")) {
			usage();
		} else if (!strcmp(argv[argi], "-t") && argi + 1 < argc) {
			title = argv[++argi];
		} else if (!strcmp(argv[argi], "-i")) {
			outputIndex = true;
		} else if (!strcmp(argv[argi], "-m")) {
			flags |= MENU_FLAG_ALLOW_MULTI;
		} else if (!strcmp(argv[argi], "-c")) {
			flags |= MENU_FLAG_ALLOW_CUSTOM;
		} else {
			usage();
		}
	}

	if (outputIndex && (flags & MENU_FLAG_ALLOW_CUSTOM)) {
		panic("output index (-i) and custom entry (-c) cannot be used at the same time");
	}

	// Get socket path
	const char *socketPath = getenv(ENV_ALINES_SOCKET);
	if (!socketPath) {
		panic("missing socket env var. not launched from server?");
	}

	// Read stdin
	size_t inDataCap = 4096;
	char *inData = xmalloc(inDataCap);
	size_t inDataLen = 0;
	{
		size_t readLen;
		char buf[4096];
		while ((readLen = fread(buf, 1, sizeof(buf), stdin))) {
			if (inDataLen + readLen + 1 >= inDataCap) {
				inDataCap <<= 1;
				inData = xrealloc(inData, inDataCap);
			}
			memcpy(inData + inDataLen, buf, readLen);
			inDataLen += readLen;
		}
		if (!inData) {
			panic("no data");
		}
	}

	// Count entries
	size_t entryCount = inDataLen == 0 || inData[inDataLen - 1] == '\n' ? 0 : 1;
	for (size_t i = 0; i < inDataLen; i++) {
		if (inData[i] == '\n') {
			entryCount++;
		}
	}
	if (entryCount > u16_MAX) {
		panic("too many entries");
	}

	// Read entries
	char **entries = xmalloc(entryCount * sizeof(char*));
	size_t dataStart = 0;
	size_t entryI = 0;
	for (size_t i = 0; i < inDataLen; i++) {
		if (inData[i] == '\n') {
			inData[i] = 0;
			entries[entryI++] = &inData[dataStart];
			dataStart = i + 1;
		}
	}
	if (entryI < entryCount) {
		inData[inDataLen] = 0;
		entries[entryI] = &inData[dataStart];
	}

	// Connect
	struct sockaddr_un sockAddr;
	sockAddr.sun_family = AF_UNIX;
	strcpy(sockAddr.sun_path, socketPath);
	const int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (connect(sockfd, (struct sockaddr*)(&sockAddr), sizeof(sockAddr))) {
		panic("domain socket connect failed");
	}

	// Send menu
	assert(fdWriteU8(sockfd, flags));
	assert(fdWriteU16(sockfd, entryCount));
	assert(fdWriteStr(sockfd, title));
	for (u16 i = 0; i < entryCount; i++) {
		assert(fdWriteStr(sockfd, entries[i]));
	}

	// Receive response
	u8 pkId;
	assert(fdReadU8(sockfd, &pkId));
	if (pkId == PKID_FROM_UI_CLOSE) {
		return 1;
	} else if (pkId == PKID_FROM_UI_SINGLE_SELECTION) {
		u16 idx;
		assert(fdReadU16(sockfd, &idx));
		if (idx > entryCount) {
			panic("invalid selection index");
		}
		if (outputIndex) {
			printf("%d\n", idx);
		} else {
			printf("%s\n", entries[idx]);
		}
	} else if (pkId == PKID_FROM_UI_MULTI_SELECTION) {
		u16 len, idx;
		assert(fdReadU16(sockfd, &len));
		for (u16 i = 0; i < len; i++) {
			assert(fdReadU16(sockfd, &idx));
			if (idx > entryCount) {
				panic("invalid selection index");
			}
			if (outputIndex) {
				printf("%d\n", idx);
			} else {
				printf("%s\n", entries[idx]);
			}
		}
	} else if (pkId == PKID_FROM_UI_CUSTOM_ENTRY) {
		char entryStr[u16_MAX + 1];
		assert(fdReadStrBuf(sockfd, entryStr) >= 0);
		printf("%s\n", entryStr);
	} else {
		panic("invalid response packet");
		return 1;
	}

	close(sockfd);
	return 0;
}
