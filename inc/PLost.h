#ifndef PLOSTDCLIENT_H
#define PLOSTDCLIENT_H

enum {
	REQUEST_INITIATION	= 'I' << 8 | '?',
	REQUEST_INITIATED	= 'I' << 8 | 'Y',
	REQUEST_UNKNOWN		= '?' << 8 | '?'
};

#endif
