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
