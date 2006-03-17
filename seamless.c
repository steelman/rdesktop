/* -*- c-basic-offset: 8 -*-
   rdesktop: A Remote Desktop Protocol client.
   Seamless Windows support
   Copyright (C) Peter Astrand <astrand@cendio.se> 2005-2006
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "rdesktop.h"
#include <stdarg.h>

/* #define WITH_DEBUG_SEAMLESS */

#ifdef WITH_DEBUG_SEAMLESS
#define DEBUG_SEAMLESS(args) printf args;
#else
#define DEBUG_SEAMLESS(args)
#endif

extern BOOL g_seamless_rdp;
static VCHANNEL *seamless_channel;

static char *
seamless_get_token(char **s)
{
	char *comma, *head;
	head = *s;

	if (!head)
		return NULL;

	comma = strchr(head, ',');
	if (comma)
	{
		*comma = '\0';
		*s = comma + 1;
	}
	else
	{
		*s = NULL;
	}

	return head;
}


static BOOL
seamless_process_line(const char *line, void *data)
{
	char *p, *l;
	char *tok1, *tok2, *tok3, *tok4, *tok5, *tok6, *tok7, *tok8;
	unsigned long id, flags;
	char *endptr;

	l = xstrdup(line);
	p = l;

	DEBUG_SEAMLESS(("seamlessrdp got:%s\n", p));

	tok1 = seamless_get_token(&p);
	tok2 = seamless_get_token(&p);
	tok3 = seamless_get_token(&p);
	tok4 = seamless_get_token(&p);
	tok5 = seamless_get_token(&p);
	tok6 = seamless_get_token(&p);
	tok7 = seamless_get_token(&p);
	tok8 = seamless_get_token(&p);

	if (!strcmp("CREATE", tok1))
	{
		unsigned long parent;
		if (!tok4)
			return False;

		id = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		parent = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_create_window(id, parent, flags);
	}
	else if (!strcmp("DESTROY", tok1))
	{
		if (!tok3)
			return False;

		id = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_destroy_window(id, flags);

	}
	else if (!strcmp("SETICON", tok1))
	{
		unimpl("SeamlessRDP SETICON1\n");
	}
	else if (!strcmp("POSITION", tok1))
	{
		int x, y, width, height;

		if (!tok7)
			return False;

		id = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		x = strtol(tok3, &endptr, 0);
		if (*endptr)
			return False;
		y = strtol(tok4, &endptr, 0);
		if (*endptr)
			return False;

		width = strtol(tok5, &endptr, 0);
		if (*endptr)
			return False;
		height = strtol(tok6, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok7, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_move_window(id, x, y, width, height, flags);
	}
	else if (!strcmp("ZCHANGE", tok1))
	{
		unsigned long behind;

		id = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		behind = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_restack_window(id, behind, flags);
	}
	else if (!strcmp("TITLE", tok1))
	{
		if (!tok4)
			return False;

		id = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_settitle(id, tok3, flags);
	}
	else if (!strcmp("STATE", tok1))
	{
		unsigned int state;

		if (!tok4)
			return False;

		id = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		state = strtoul(tok3, &endptr, 0);
		if (*endptr)
			return False;

		flags = strtoul(tok4, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_setstate(id, state, flags);
	}
	else if (!strcmp("DEBUG", tok1))
	{
		printf("SeamlessRDP:%s\n", line);
	}
	else if (!strcmp("SYNCBEGIN", tok1))
	{
		if (!tok2)
			return False;

		flags = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_syncbegin(flags);
	}
	else if (!strcmp("SYNCEND", tok1))
	{
		if (!tok2)
			return False;

		flags = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		/* do nothing, currently */
	}
	else if (!strcmp("HELLO", tok1))
	{
		if (!tok2)
			return False;

		flags = strtoul(tok2, &endptr, 0);
		if (*endptr)
			return False;

		ui_seamless_begin();
	}


	xfree(l);
	return True;
}


static BOOL
seamless_line_handler(const char *line, void *data)
{
	if (!seamless_process_line(line, data))
	{
		warning("SeamlessRDP: Invalid request:%s\n", line);
	}
	return True;
}


static void
seamless_process(STREAM s)
{
	unsigned int pkglen;
	static char *rest = NULL;
	char *buf;

	pkglen = s->end - s->p;
	/* str_handle_lines requires null terminated strings */
	buf = xmalloc(pkglen + 1);
	STRNCPY(buf, (char *) s->p, pkglen + 1);
#if 0
	printf("seamless recv:\n");
	hexdump(s->p, pkglen);
#endif

	str_handle_lines(buf, &rest, seamless_line_handler, NULL);

	xfree(buf);
}


BOOL
seamless_init(void)
{
	if (!g_seamless_rdp)
		return False;

	seamless_channel =
		channel_register("seamrdp", CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP,
				 seamless_process);
	return (seamless_channel != NULL);
}


static void
seamless_send(const char *format, ...)
{
	STREAM s;
	size_t len;
	va_list argp;
	char buf[1024];

	va_start(argp, format);
	len = vsnprintf(buf, sizeof(buf), format, argp);
	va_end(argp);

	s = channel_init(seamless_channel, len);
	out_uint8p(s, buf, len) s_mark_end(s);

	DEBUG_SEAMLESS(("SeamlessRDP sending:%s", buf));

#if 0
	printf("seamless send:\n");
	hexdump(s->channel_hdr + 8, s->end - s->channel_hdr - 8);
#endif

	channel_send(s, seamless_channel);
}


void
seamless_send_sync()
{
	if (!g_seamless_rdp)
		return;

	seamless_send("SYNC\n");
}


void
seamless_send_state(unsigned long id, unsigned int state, unsigned long flags)
{
	if (!g_seamless_rdp)
		return;

	seamless_send("STATE,0x%08lx,0x%x,0x%lx\n", id, state, flags);
}


void
seamless_send_position(unsigned long id, int x, int y, int width, int height, unsigned long flags)
{
	seamless_send("POSITION,0x%08lx,%d,%d,%d,%d,0x%lx\n", id, x, y, width, height, flags);
}


/* Update select timeout */
void
seamless_select_timeout(struct timeval *tv)
{
	struct timeval ourtimeout = { 0, SEAMLESSRDP_POSITION_TIMER };

	if (g_seamless_rdp)
	{
		if (timercmp(&ourtimeout, tv, <))
		{
			tv->tv_sec = ourtimeout.tv_sec;
			tv->tv_usec = ourtimeout.tv_usec;
		}
	}
}


void
seamless_send_zchange(unsigned long id, unsigned long below, unsigned long flags)
{
	if (!g_seamless_rdp)
		return;

	seamless_send("ZCHANGE,0x%08lx,0x%08lx,0x%lx\n", id, below, flags);
}


void
seamless_send_focus(unsigned long id, unsigned long flags)
{
	if (!g_seamless_rdp)
		return;

	seamless_send("FOCUS,0x%08lx,0x%lx\n", id, flags);
}
