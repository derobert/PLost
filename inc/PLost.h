#ifndef PLOSTDCLIENT_H
#define PLOSTDCLIENT_H

enum {
	PROTOCOL_VERSION_ZERO = 'V' << 8 | '0',
	PROTOCOL_BASE_SIZE = 14
};

enum {
	REQUEST_INITIATION	= 'I' << 8 | '?',
	REQUEST_INITIATED	= 'I' << 8 | 'Y',
	REQUEST_CLEANUP		= 'F' << 8 | 'I',
	REQUEST_CLEANED		= 'F' << 8 | 'I',
	REQUEST_PING		= 'P' << 8 | 'I',
	REQUEST_PONG_WHO	= 'P' << 8 | '?',
	REQUEST_PONG		= 'P' << 8 | '!',
	REQUEST_UNKNOWN		= '?' << 8 | '?'
};

#define PLOST_VERSION(buf) (*(uint16_t *)(buf + 0))
#define PLOST_REQUEST(buf) (*(uint16_t *)(buf + 2))
#define PLOST_TRANSID(buf) (*(uint64_t *)(buf + 4))
#define PLOST_RXCOUNT(buf) (*(uint16_t *)(buf + 12))

#endif
