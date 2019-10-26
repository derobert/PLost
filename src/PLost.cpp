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
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <iostream>

bool run_test(int sock);
bool initiate(int sock, uint64_t &);
bool measure(int sock, uint64_t);
unsigned long long microtime();

int main(int argc, char *argv[]) {
	// Getopt?! Nah...
	const char *service = NULL;
	short port = 0;
	if (argc == 0) {
		std::clog << "argv is fubar. No cookie for you!.\n";
		return 1;
	} else if (argc < 2 || argc > 3) {
		std::clog << "Usage: " << argv[0] << " remote-end [port]\n";
		return 1;
	} else if (argc == 3)
		service = argv[2];
	else if (argc == 2)
		service = "1223";
	else {
		std::cerr << "Hope you know your machine is broken. HAND!\n";
		abort();
	}

	// Find who to connect to, in a protocol-independant manner. Oooh,
	// yeah, IPv4 support. Now, if only the server supported IPv4...
	addrinfo hints;
	hints.ai_flags = 0; 
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM; 
	hints.ai_protocol = 0;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;
						
	addrinfo *info;
	int err = getaddrinfo(argv[1], service, &hints, &info);
	if (err) {
		std::clog << gai_strerror(err) << std::endl;
		return 1;
	}
	
	// I wanna sock (my old one is holy)... I mean a socket!
	int sock = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (!sock) {
		perror("socket");
		return 1;
	}

	// Who are we talking to?
	sockaddr *who = info->ai_addr;
	socklen_t wholen = info->ai_addrlen;
	char rev_host[128], rev_service[16];
	if (getnameinfo(who, wholen, rev_host, sizeof(rev_host), rev_service,
				sizeof(rev_service), NI_DGRAM))
		perror("getnameinfo (not fatal; ignoring)");
	else
		std::clog << "Contacting " << rev_host << " on service "
			      << rev_service << std::endl;

	// Connect!
	if (connect(sock, who, wholen)) {
		perror("connect");
		return 1;
	}
	
	if (!run_test(sock))
		return 1;
	
	// Clean up
	freeaddrinfo(info);
}

bool run_test(int sock) {
	uint64_t transid;
	if (!initiate(sock, transid))
		return false;
	if (!measure(sock, transid))
		return false;

	return true;
}


bool initiate(int sock, uint64_t &transid) {
	// Generate the transid
	int fd = open("/dev/urandom", O_RDONLY);
	if (!fd) {
		perror("open /dev/urandom");
		return false;
	}
	ssize_t read_amount = read(fd, &transid, sizeof(transid));
	if (read_amount == -1) {
		perror("read");
		return false;
	} else if (read_amount != sizeof(transid)) {
		std::clog << "Sorry, don't feel like dealing with short reads. POSIX says\n"
			      << "I have to, so feel free to submit a patch...\n";
		return false;
	}
	close(fd);

	// Request a connection
	bool haveConn = false;
	int retry_time = 1000;
	pollfd pollme = {
		fd: sock,
		events: POLLIN
	};
	while (retry_time <= 128000 && !haveConn) {
		char buf[PROTOCOL_BASE_SIZE];
		PLOST_VERSION(buf) = htons(PROTOCOL_VERSION_ZERO);
		PLOST_REQUEST(buf) = htons(REQUEST_INITIATION);
		PLOST_TRANSID(buf) = transid;
		if (-1 == send(sock, buf, PROTOCOL_BASE_SIZE, MSG_NOSIGNAL))
			perror("send (will retry)");
		std::clog << "Sent request: ";
		int pollres = poll(&pollme, 1, retry_time);
		if (pollres == -1) {
			perror("poll");
			return false;
		} else if (pollres == 0)
			std::clog << "Timed out after " << (retry_time / 1000)
				      << " seconds.\n";
		else if (pollres == 1)
			if (pollme.revents == POLLIN) {
				int ramount = recv(sock, buf, PROTOCOL_BASE_SIZE, 0);
				if (ramount == -1)
					perror("recv");
				else if (ramount != PROTOCOL_BASE_SIZE)
					std::clog << "Response is wrong size.\n";
				else if (ntohs(PLOST_VERSION(buf)) != PROTOCOL_VERSION_ZERO)
					std::clog << "Response is wrong version.\n";
				else if (ntohs(PLOST_REQUEST(buf)) != REQUEST_INITIATED)
					std::clog << "Response is wrong type\n";
				else if (PLOST_TRANSID(buf) != transid)
					std::clog << "Response has wrong transaction id.\n";
				else {
					std::clog << "Got response.\n";
					haveConn = true;
				}
			} else
				std::clog << "Got unexpected revents: " << pollme.revents
					      << std::endl;
		else {
			std::clog << "Poll returned " << pollres
				      << " which is quite humorous.\n";
			return false;
		}
		
		retry_time <<= 1;
	}
	if (!haveConn) {
		std::clog << "Could not establish connection; out of retries. Bailing out.\n";
		return false;
	}

	return true;
}

bool measure(int sock, uint64_t transid) {
	// Methodology here is to send REQUEST_PING until we get 100
	// REQUEST_PONGs, or 1000 REQUEST_PINGs at most. We send one out
	// every 1,200,000 microseconds, so the process will take 2 minutes
	// without any packet loss, possibly up to 20 minutes if every
	// packet vanishes.
	const int MAX_PING = 1000, MAX_PONG = 100;
	const long IPG = 1200000;
	
	int ping_count = 0, pong_count = 0, max_remote_count = 0;
	unsigned long long next = microtime();
	pollfd pollme = {
		fd: sock,
		events: POLLIN
	};
	char buf[PROTOCOL_BASE_SIZE];
	while (pong_count < MAX_PONG && ping_count < MAX_PING) {
		if (next <= microtime()) {
			// Send
			PLOST_VERSION(buf) = htons(PROTOCOL_VERSION_ZERO);
			PLOST_REQUEST(buf) = htons(REQUEST_PING);
			PLOST_TRANSID(buf) = transid;
			if (-1 == send(sock, buf, PROTOCOL_BASE_SIZE, 0))
				perror("send (ping not counted)");
			else
				++ping_count;
			next += IPG;
		}

		// Wait for reply
		unsigned long long cur_time = microtime();
		unsigned long long delay;
		if (cur_time > next) {
			delay = 10;
			std::clog << "Underflow; forcing delay to 10ms minimum instead of -"
				      << (cur_time-next) << "us.\n";
		} else
			delay = (next-cur_time)/1000;
		int pollres = poll(&pollme, 1, delay);
		if (pollres == -1) {
			perror("poll");
			return false;
		} else if (pollres == 0)
			(void)0;	// nothing received; will send another packet...
		else if (pollres == 1)
			if (pollme.revents == POLLIN) {
				int ramount = recv(sock, buf, PROTOCOL_BASE_SIZE, 0);
				if (ramount == -1)
					perror("recv");
				else if (ramount != PROTOCOL_BASE_SIZE)
					std::clog << "Response is wrong size.\n";
				else if (ntohs(PLOST_VERSION(buf)) != PROTOCOL_VERSION_ZERO)
					std::clog << "Response is wrong version.\n";
				else if (ntohs(PLOST_REQUEST(buf)) == REQUEST_PONG_WHO) {
					std::clog << "Server forgot about us! Aborting.\n";
					return false;
				} else if (ntohs(PLOST_REQUEST(buf)) != REQUEST_PONG)
					std::clog << "Response is wrong type\n";
				else if (PLOST_TRANSID(buf) != transid)
					std::clog << "Response has wrong transaction id.\n";
				else {
					++pong_count;
					int remote_count = ntohs(PLOST_RXCOUNT(buf));
					if (remote_count > max_remote_count) {
						std::clog << "Pong, pings = " << ping_count 
							      << " pongs = " << pong_count
								  << " remote = " << remote_count
								  << std::endl;
						max_remote_count = remote_count;
					} else {
						std::clog << "Pong, pings = " << ping_count 
							      << " pongs = " << pong_count
								  << " remote = " << remote_count
								  << " OoO = " << remote_count - max_remote_count
								  << std::endl;
					}
				}
			} else
				std::clog << "Got unexpected revents: " << pollme.revents
					      << std::endl;
		else {
			std::clog << "Poll returned " << pollres
				      << " which is quite humorous.\n";
			return false;
		}
	}
	
	// OK: We've finished now. Wait an additional 5,000 milliseconds for
	// packets to come in. If a packet comes in, we'll restart the
	// timer.
	const int END_WAIT_TIME = 5000;

	std::clog << "Finished sending. Waiting for any stray replies...\n";
	bool gotPacket;
	do {
		gotPacket = false;
		int pollres = poll(&pollme, 1, END_WAIT_TIME);
		if (pollres == -1) {
			perror("poll");
			return false;
		} else if (pollres == 0)
			(void)0; // do nothing
		else if (pollres == 1)
			if (pollme.revents == POLLIN) {
				gotPacket = true;
				int ramount = recv(sock, buf, PROTOCOL_BASE_SIZE, 0);
				if (ramount == -1)
					perror("recv");
				else if (ramount != PROTOCOL_BASE_SIZE)
					std::clog << "Response is wrong size.\n";
				else if (ntohs(PLOST_VERSION(buf)) != PROTOCOL_VERSION_ZERO)
					std::clog << "Response is wrong version.\n";
				else if (ntohs(PLOST_REQUEST(buf)) == REQUEST_PONG_WHO) {
					std::clog << "Server forgot about us! Aborting.\n";
					return false;
				} else if (ntohs(PLOST_REQUEST(buf)) != REQUEST_PONG)
					std::clog << "Response is wrong type\n";
				else if (PLOST_TRANSID(buf) != transid)
					std::clog << "Response has wrong transaction id.\n";
				else {
					++pong_count;
					int remote_count = ntohs(PLOST_RXCOUNT(buf));
					if (remote_count > max_remote_count) {
						std::clog << "Pong, pings = " << ping_count 
							      << " pongs = " << pong_count
								  << " remote = " << remote_count
								  << std::endl;
						max_remote_count = remote_count;
					} else {
						std::clog << "Pong, pings = " << ping_count 
							      << " pongs = " << pong_count
								  << " remote = " << remote_count
								  << " OoO = " << remote_count - max_remote_count
								  << std::endl;
					}
				}
			} else
				std::clog << "Got unexpected revents: " << pollme.revents
					      << std::endl;
		else {
			std::clog << "Poll returned " << pollres
				      << " which is quite humorous.\n";
			return false;
		}
	} while (gotPacket);

	std::cout << "     Local Sent: " << ping_count << " packets\n"
		      << "Remote Received: " << max_remote_count << " packets\n"
			  << " Local Received: " << pong_count << " packets\n"
			  << std::endl;
	if (pong_count < MAX_PONG)
		std::cout << "Warning: Did not receive the wanted number of pongs\n"
				  << "         so accuracy may be reduced.\n\n";
	std::cout << "  Upstream Loss: "
		      << int(100-(double(100*max_remote_count)/ping_count)+.5)
		      << "%\nDownstream Loss: "
			  << int(100-(double(100*pong_count)/max_remote_count)+.5)
			  << "%\n";
	
	return true;
}

unsigned long long microtime() {
	timeval tm;
	gettimeofday(&tm, NULL);
	return ((unsigned long long)tm.tv_sec) * 1000000
		+ tm.tv_usec;
}

