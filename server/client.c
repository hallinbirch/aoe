/* Copyright 2019 the Age of Empires Free Sofware Remake authors. See LEGAL for legal info */

/**
 * Age of Empires terminal client
 *
 * Provides a bare bones client to test various things.
 * NOTE this client does not support windows because it uses ncurses
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#include <xt/error.h>
#include <xt/os_macros.h>
#include <xt/thread.h>
#include <xt/string.h>
#include <xt/socket.h>

#if XT_IS_LINUX
#include <errno.h>
#endif

#include "common.h"

xtSocket sockfd = XT_SOCKET_INVALID_FD;
static struct xtSockaddr sa;
static struct xtThread t_event;

char host[40] = "127.0.0.1";
uint16_t port = DEFAULT_PORT;

#define INIT_XTSOCKET 0x01
#define INIT_WORKERS  0x02
#define INIT_NCURSES  0x04

#define CONNECT_TRIES 3
#define CONNECT_TIMEOUT 1000

#define ROW_MIN 25
#define COL_MIN 80

unsigned init = 0;

WINDOW *win = NULL;
int rows, cols;

int toosmall = 0;

char str_hdr[COL_MIN];

char line[COL_MIN];
unsigned linepos = 0;

void show_error(const char *str)
{
	// create border
	int y = rows / 2;

	for (int x = 0; x < cols; ++x) {
		mvaddch(y - 2, x, '-');
		mvaddch(y + 2, x, '-');
	}

	move(y - 1, 0); clrtoeol();
	move(y, 0); clrtoeol();
	move(y + 1, 0); clrtoeol();

	// print error message
	size_t n = strlen(str);

	if (n > (unsigned)cols)
		mvaddstr(y - 1, 0, str);
	else
		mvaddstr(y, (cols - n) / 2, str);

	refresh();
}

void tui_stop(void)
{
	if (win)
		delwin(win);

	endwin();

	if (toosmall)
		fprintf(stderr, "terminal size is %dx%d, but should be at least %dx%d\n", cols, rows, COL_MIN, ROW_MIN);
}

void fatal_error(const char *str, int delay, int code)
{
	show_error(str);
	napms(delay);

	tui_stop();

	if (code < 0)
		_exit(-code - 1);
	else
		exit(code);
}

static int net_pkg_process(struct net_pkg *pkg)
{
	switch (pkg->type) {
	case NT_TEXT:
		pkg->data.text.text[TEXT_BUFSZ - 1] = '\0';
		mvaddstr(2, 0, pkg->data.text.text);
		break;
	default:
		fatal_error("communication error", 1000, -1);
		return 1;
	}
	return 0;
}

void *event_loop(struct xtThread *t, void *arg)
{
	int err;
	char buf[256];
	struct net_pkg pkg;

	(void)t;
	(void)arg;

	while (1) {
		uint16_t in, n;

		// keep reading the packet header
		for (in = 0; in != NET_HEADER_SIZE; in += n)
			if ((err = xtSocketTCPRead(sockfd, &pkg, NET_HEADER_SIZE - in, &n)))
				goto read_error;


		uint16_t length = xtbe16toh(pkg.length);

		mvprintw(1, 0, "grab %hu bytes...\n", length);

		union pdata *data = &pkg.data;

		for (in = 0; in != length; in += n) {
			if ((err = xtSocketTCPRead(sockfd, data, length - in, &n))) {
read_error:
				if (err != XT_ESHUTDOWN) {
					snprintf(buf, sizeof buf, "event_loop: %s", xtGetErrorStr(err));
					fatal_error(buf, 1500, -2);
				}

				fatal_error("server stopped", 1500, -1);
				return NULL;
			}
		}

		net_pkg_ntoh(&pkg);
		net_pkg_process(&pkg);
	}

	return NULL;
}

int net_pkg_send(struct net_pkg *p)
{
	uint16_t dummy;
	uint16_t size = NET_HEADER_SIZE + p->length;

	net_pkg_hton(p);

	return tcp_write(sockfd, p, size, &dummy);
}

int cmd_serverctl(uint16_t opcode, uint16_t data)
{
	struct net_pkg p;

	net_pkg_init(&p, NT_SERVER_CONTROL, opcode, data);

	return net_pkg_send(&p);
}

int cmd_say(const char *msg)
{
	struct net_pkg p;

	net_pkg_init(&p, NT_TEXT, NET_TEXT_RECP_ALL, NET_TEXT_TYPE_USER, msg);

	return net_pkg_send(&p);
}

int cmd_op(const char *passwd)
{
	struct net_pkg p;

	net_pkg_init(&p, NT_OP, passwd);

	return net_pkg_send(&p);
}

int run_cmd(char *line, unsigned n)
{
	if ((n == 1 && line[0] == 'q') || (n == 4 && !strcmp(line, "quit")))
		return -1;

	if (n == 4 && !strcmp(line, "stop")) {
		return cmd_serverctl(SC_STOP, 0);
	} else if (n >= 3 && xtStringStartsWith(line, "say")) {
		return cmd_say(xtStringTrim(line + 3));
	} else if (n >= 3 && xtStringStartsWith(line, "op ")) {
		// password may contain spaces at begin or end. hence, do not trim.
		return cmd_op(line + 3);
	} else if (!strcmp(line, "crash")) {
		fatal_error("crash", 1000, 0);
	}

	return 0;
}

int mainloop(void)
{
	clear();
	mvaddstr(0, 0, str_hdr);

	while (1) {
		int ch, idle = 0;

		if ((ch = getch()) == KEY_RESIZE) {
			getmaxyx(win, rows, cols);

			if (rows < ROW_MIN || cols < COL_MIN) {
				toosmall = 1;
				return 1;
			}
			idle = 1;
		} else {
			switch (ch) {
			case '\b':
			case KEY_BACKSPACE:
				if (linepos)
					line[--linepos] = '\0';
				break;
			case '\r':
			case '\n':
			case KEY_ENTER:
				xtStringTrim(line);

				if (run_cmd(line, strlen(line)) < 0)
					return 0;

				line[linepos = 0] = '\0';
				break;
			default:
				if (ch <= 0xff && isprint(ch) && linepos < COL_MIN - 1) {
					line[linepos++] = ch;
					line[linepos] = '\0';
				} else {
					idle = 1;
				}
				break;
			}

			mvaddstr(rows - 1, 0, line);
			clrtoeol();
		}

		// only refresh the screen if the keyboard input buffer has been processed
		if (idle) {
			refresh();
			napms(50);
		}
	}

	return 0;
}

/* Spawn worker threads. */
static int start_workers(void)
{
	int err;

	if ((err = xtThreadCreate(&t_event, event_loop, NULL, 0, 0))) {
		xtPerror("spawn event_loop", err);
		return err;
	}

	return 0;
}

/* Terminate worker threads. */
static int join_workers(void)
{
	int err;

#if XT_IS_LINUX
	if ((err = pthread_cancel(t_event.nativeThread))) {
		xtPerror("stop event_loop", err);
		goto fail;
	}

	if ((err = xtThreadJoin(&t_event, NULL))) {
		xtPerror("join event_loop", err);
		goto fail;
	}
#else
	/*
	 * windoze does not properly support thread cancellation...
	 * so we skip any proper thread joining and hope nothing weird happens...
	 */
#endif
	err = 0;
fail:
	return 0;
}

int main(int argc, char **argv)
{
	int err = 1;

	if (argc != 3) {
		fprintf(stderr, "usage: %s [server_ip] [port]\n", argc > 0 ? argv[0] : "client");
		goto fail;
	}

	if (argc > 1) {
		if (argc == 3)
			port = atoi(argv[2]);

		xtstrncpy(host, argv[1], sizeof host);
	}

	if (!(xtSocketInit())) {
		fputs("main: internal error\n", stderr);
		goto fail;
	}

	init |= INIT_XTSOCKET;

	if ((err = xtSocketCreate(&sockfd, XT_SOCKET_PROTO_TCP))) {
		xtPerror("sock create", err);
		goto fail;
	}

	if ((err = xtSocketSetSoReuseAddress(sockfd, true))) {
		xtPerror("sock reuse", err);
		goto fail;
	}

	if ((err = xtSocketSetSoKeepAlive(sockfd, true))) {
		xtPerror("sock keep alive", err);
		goto fail;
	}

	if ((err = !xtSockaddrFromString(&sa, host, port))) {
		xtPerror("sockaddr init", err);
		goto fail;
	}

	for (unsigned i = 0; i < CONNECT_TRIES; ++i) {
		puts("connecting...");

		if (!(err = xtSocketConnect(sockfd, &sa)))
			goto connected;

		xtSleepMS(1000);
	}

	xtPerror("could not connect", err);
	err = 2;
	goto fail;

connected:
	puts("connected");

	if ((err = start_workers()))
		goto fail;

	init |= INIT_WORKERS;

	if (!(win = initscr())) {
		perror("ncurses failed to start");
		goto fail;
	}

	init |= INIT_NCURSES;

	cbreak();
	keypad(win, true);
	nodelay(win, true);
	noecho();
	getmaxyx(win, rows, cols);

	snprintf(str_hdr, sizeof str_hdr, "AoE client v0 - connected to %s:%" PRIu16, host, port);

	if (rows < ROW_MIN || cols < COL_MIN) {
		toosmall = 1;
		goto fail;
	}

	err = mainloop();
fail:
	if (init & INIT_NCURSES)
		tui_stop();

	if (init & INIT_WORKERS)
		join_workers();

	if (init & INIT_XTSOCKET) {
		if (sockfd != XT_SOCKET_INVALID_FD)
			xtSocketClose(&sockfd);
		xtSocketDestruct();
	}

	return err;
}
