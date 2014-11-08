/*
 * PLost - Determines packet loss in both upstream and downstream.
 * Copyright (C) 2002 Anthony DeRobertis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/

#include "PLost.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

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

	int zero = 0;
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero))) {
		perror("setsockopt IPV6_V6ONLY false");
		return 1;
	}

	sockaddr_in6 addr;
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(1223);
	addr.sin6_addr = in6addr_any;
	if (bind(sock, (sockaddr*)&addr, sizeof(addr))) {
		perror("bind IPv6");
		std::clog << "What? No IPv6 support?! You must be stuck in 1998"
			      << " or something...\n"
				  << "Trying IPv4.\n";
		close(sock);
		sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (!sock) {
			perror("socket");
			std::clog << "In-ter-net, what's dat?\n";
			return 1;
		}
		sockaddr_in addr = { AF_INET, htons(1223), INADDR_ANY };
		if (bind(sock, (sockaddr*)&addr, sizeof(addr))) {
			perror("bind IPv4");
			std::clog << "In-ter-net, what's dat?\n";
			return 1;
		}
	}

	connection_loop(sock);
}


void connection_loop(const int sock) {
	const int buffer_size = 1500, addr_buffer_size = 128,
	          service_buffer_size = 32, addr_size = 64;

	char *const buffer = new char[buffer_size];
	char *const addr_buffer = new char[addr_buffer_size];
	char *const service_buffer = new char[service_buffer_size];
	sockaddr *const remote_addr = (sockaddr *)new char[addr_size];

	socklen_t remote_addr_len;
	for (;;) { // only a signal will take this mess down...
		remote_addr_len = addr_size;
		int size = recvfrom(sock, buffer, buffer_size, MSG_NOSIGNAL,
				remote_addr, &remote_addr_len);
		if (size == -1) {
			perror("recv");
		}
		getnameinfo(remote_addr, remote_addr_len, addr_buffer,
				addr_buffer_size, service_buffer, service_buffer_size,
				NI_NUMERICHOST|NI_NUMERICSERV|NI_DGRAM);
		std::clog << "Message from " << addr_buffer
			      << " service " << service_buffer << ": ";
		process_request(buffer, size, remote_addr,
				remote_addr_len, sock);
		transaction_gc();
	}
	
	delete[] addr_buffer;
	delete[] service_buffer;
	delete[] remote_addr;
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
