#include "PLost.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include <iostream>
#include <map>

void connection_loop(const int sock);
void process_request(const char *const buffer, const int size,
		const sockaddr *who, const socklen_t wholen, const int sock);
void transaction_gc();

struct transactionInfo {
	transactionInfo() : count(0), last(time(NULL)) {}
	unsigned int count;
	time_t last;
};
typedef std::map<uint64_t, transactionInfo> transactionMap;
transactionMap gTransactions;

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
		transaction_gc();
	}
	
	delete[] addr_buffer;
	delete[] buffer;
}

void process_request(const char *const buffer, const int size,
		const sockaddr *who, const socklen_t wholen, const int sock) {
	if (size < 2) {
		std::clog << "too small for even the version field!\n";
		return;
	}
	const uint16_t version = ntohs(PLOST_VERSION(buffer));
	if (version != PROTOCOL_VERSION_ZERO) {
		std::clog << "wrong version...\n";
		return;
	}
	if (size < PROTOCOL_BASE_SIZE) {
		std::clog << "too small for this version!\n";
		return;
	}

	const uint16_t request = ntohs(PLOST_REQUEST(buffer));
	const uint64_t transid = PLOST_TRANSID(buffer);

	char *const response_buffer = new char[1500];
	int response_length = 0;

	switch (request) {
		case REQUEST_INITIATION:
			std::clog << "Initiate, id = " << transid << std::endl;
			PLOST_VERSION(response_buffer) = htons(version);
			PLOST_REQUEST(response_buffer) = htons(REQUEST_INITIATED);
			PLOST_TRANSID(response_buffer) = transid;
			response_length = PROTOCOL_BASE_SIZE;
			gTransactions[transid] = transactionInfo();
			break;
		case REQUEST_PING:
			std::clog << "Ping, id = " << transid;
			PLOST_VERSION(response_buffer) = htons(version);
			PLOST_TRANSID(response_buffer) = transid;
			response_length = PROTOCOL_BASE_SIZE;

			if (!gTransactions.count(transid)) {
				// unknown transaction. Server restart, timeout, etc.
				PLOST_REQUEST(response_buffer) = htons(REQUEST_PONG_WHO);
				std::clog << " DNE\n";
			} else {
				transactionInfo &tinfo = gTransactions[transid];
				tinfo.last = time(NULL);
				PLOST_REQUEST(response_buffer) = htons(REQUEST_PONG);
				PLOST_RXCOUNT(response_buffer) = htons(++tinfo.count);
				std::clog << ", count = " << tinfo.count << std::endl;
			}
			break;
		case REQUEST_CLEANUP:
			std::clog << "Clean up, id = " << transid;
			PLOST_VERSION(response_buffer) = htons(version);
			PLOST_REQUEST(response_buffer) = htons(REQUEST_CLEANED);
			PLOST_TRANSID(response_buffer) = transid;
			response_length = PROTOCOL_BASE_SIZE;
			if (!gTransactions.erase(transid))
				std::clog << " DNE";
			std::clog << std::endl;
			break;
		default:
			std::clog << "Unknown request, id = " << transid << std::endl;
			PLOST_VERSION(response_buffer) = htons(version);
			PLOST_REQUEST(response_buffer) = htons(REQUEST_UNKNOWN);
			PLOST_TRANSID(response_buffer) = transid;
			response_length = PROTOCOL_BASE_SIZE;
			break;
	}
	if (response_length)
		if (-1 == sendto(sock, response_buffer, response_length,
					MSG_DONTWAIT|MSG_NOSIGNAL, who, wholen))
			perror("sendto");


	delete[] response_buffer;
}

void transaction_gc() {
	const int max_transaction_lifetime = 60;
	const int eol = time(NULL) - max_transaction_lifetime;
	for(transactionMap::iterator i = gTransactions.begin();
			i != gTransactions.end(); ++i)
		if (i->second.last <= eol) {
			std::clog << "Cleaning up id = " << i->first << std::endl;
			gTransactions.erase(i);
		}
}
