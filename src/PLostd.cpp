#include "PLost.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>

void connection_loop(const int sock);
void process_request(const char *const buffer, const int size,
		const sockaddr *who, const socklen_t wholen, const int sock);

int main() {
	int sock = socket(PF_INET6, SOCK_DGRAM, 0);
	if (!sock) {
		perror("socket");
		return 1;
	}

	sockaddr_in6 addr;
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(1223);
	addr.sin6_addr = in6addr_any;
	if (bind(sock, (sockaddr*)&addr, sizeof(addr))) {
		perror("bind");
		return 1;
	}

	connection_loop(sock);
}


void connection_loop(const int sock) {
	const int buffer_size = 1500, addr_buffer_size = 64;
	char *const buffer = new char[buffer_size];
	char *const addr_buffer = new char[addr_buffer_size];

	sockaddr_in6 remote_addr;
	socklen_t remote_addr_len;
	for (;;) { // only a signal will take this mess down...
		int size = recvfrom(sock, buffer, buffer_size, MSG_NOSIGNAL,
				(sockaddr*)&remote_addr, &remote_addr_len);
		inet_ntop(AF_INET6, &remote_addr.sin6_addr, addr_buffer,
				addr_buffer_size);
		if (remote_addr_len != sizeof(remote_addr)) {
			std::cerr << "Addr size is " << remote_addr_len
				      << ", not " << sizeof(remote_addr) << "!\n";
			continue;
		}
		std::clog << "Message from " << addr_buffer << ": ";
		process_request(buffer, size, (sockaddr *)&remote_addr,
				remote_addr_len, sock);
	}
	
	delete[] addr_buffer;
	delete[] buffer;
}

void process_request(const char *const buffer, const int size,
		const sockaddr *who, const socklen_t wholen, const int sock) {
	if (size < 10) {
		std::clog << "too small!\n";
		return;
	}

	const uint16_t request = ntohs(*((uint16_t *)(buffer)));
	const uint64_t transid = *((uint64_t *)(buffer + 2));

	char *const response_buffer = new char[1500];
	int response_length = 0;

	switch (request) {
		case REQUEST_INITIATION:
			std::clog << "Initiate, id = " << transid << std::endl;
			*(uint16_t *)(response_buffer+0) = htons(REQUEST_INITIATED);
			*(uint64_t *)(response_buffer+2) = transid;
			response_length = 10;
			break;
		default:
			std::clog << "Unknown request, id = " << transid << std::endl;
			*(uint16_t *)(response_buffer+0) = htons(REQUEST_UNKNOWN);
			*(uint64_t *)(response_buffer+2) = transid;
			response_length = 10;
			break;
	}
	if (response_length)
		if (-1 == sendto(sock, response_buffer, response_length,
					MSG_DONTWAIT|MSG_NOSIGNAL, who, wholen))
			perror("sendto");


	delete[] response_buffer;
}
