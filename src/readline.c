#include "../include/crux/readline.h"
#include "../include/crux/hub.h"
#include "../include/crux/vec.h"
#include "../include/crux/err.h"

#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <termios.h>
#include <sys/ioctl.h>

struct str {
	XVEC(char);
};

XVEC_STATIC_EXT(str, struct str, char, 1)

#define PENDING 0
#define COMPLETE 1
#define CANCELD 2
#define END 3

struct xreadline {
	struct termios restore;
	struct str line;
	size_t pos;
	FILE *out;
	const char *prompt;
	int fd;
	int state;
	bool redraw;
	bool tty;
};

static int
enable_raw(int fd, struct termios *restore)
{
	if (!isatty(fd)) {
		errno = ENOTTY;
		return -1;
	}

	if (tcgetattr(fd, restore) < 0) {
		return -1;
	}

	struct termios raw;

	raw = *restore;  // modify the original mode
	// input modes: no break, no CR to NL, no parity check, no strip char,
	//              no start/stop output control
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	// output modes: disable post processing
	raw.c_oflag &= ~(OPOST);
	// control modes: set 8 bit chars
	raw.c_cflag |= (CS8);
	// local modes: echo off, canonical off, no extended functions,
	//              no signal chars (^Z,^C)
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	// control chars: set return condition to min number of bytes and timer
	raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; // 1 byte, no timer

	// put terminal in raw mode after flushing
	return tcsetattr(fd, TCSAFLUSH, &raw);
}

static int
disable_raw(int fd, struct termios *restore)
{
	if (!isatty(fd)) {
		errno = ENOTTY;
		return -1;
	}
    return tcsetattr(fd, TCSAFLUSH, restore);
}

static size_t
get_cols(int fd)
{
	struct winsize ws;
    return ioctl(fd, TIOCGWINSZ, &ws) == 0 ? ws.ws_col : 80;
}

static void
redraw(struct xreadline *rl)
{
    size_t plen = strlen(rl->prompt);
	const char *buf = rl->line.arr;
    size_t len = rl->line.count;
	size_t cols = get_cols(rl->fd);
	size_t pos = rl->pos;

    while (plen+pos >= cols) {
        buf++;
        len--;
        pos--;
    }
    while (plen+len > cols) {
        len--;
    }

	fprintf(rl->out,
			"\x1b[0G"           // cursor to left edge
			"%.*s"              // prompt
			"%.*s"              // line contents
			"\x1b[0K"           // erase to right
			"\x1b[0G\x1b[%zuC", // move cursor
			(int)plen, rl->prompt,
			(int)len, buf,
			pos + plen);
	fflush(rl->out);
}

static size_t
max_pos(struct xreadline *rl)
{
	size_t max = rl->line.count;
	//if (max > 0 && rl->vi) { max--; }
	return max;
}

static bool
is_space(int c, bool full)
{
	return full ? isspace (c) : !isalnum (c);
}

static bool
is_word(int c, bool full)
{
	return full ? !isspace (c) : isalnum (c);
}

static size_t
prev_word(struct xreadline *rl, bool full)
{
	size_t pos = rl->pos;
	if (pos > 0) {
		const char *buf = rl->line.arr;
		while (pos > 0 && is_space(buf[pos-1], full)) {
			pos--;
		}
		while (pos > 0 && is_word(buf[pos-1], full)) {
			pos--;
		}
	}
	return pos;
}

#if 0
static size_t
next_word(struct xreadline *rl, bool full)
{
	size_t pos = rl->pos;
	size_t max = max_pos(rl);
	if (pos < max) {
		const char *buf = rl->line.arr;
		while (pos < max && is_space(buf[pos-1], full)) {
			pos++;
		}
		while (pos < max && is_word(buf[pos-1], full)) {
			pos++;
		}
	}
	return pos > max ? max : pos;
}
#endif

static int
clear_screen(struct xreadline *rl)
{
	static const char msg[] = "\x1b[H\x1b[2J";
	fwrite(msg, 1, sizeof(msg)-1, rl->out);
	fflush(rl->out);
	rl->redraw = true;
	return 1;
}

static int
edit_insert(struct xreadline *rl, char c)
{
	int rc = str_push(&rl->line, c);
	if (rc < 0) { return rc; }
	rl->pos++;
	rl->redraw = true;
	return 1;
}

static int
edit_backspace(struct xreadline *rl)
{
	if (rl->pos > 0) {
		int rc = str_remove(&rl->line, rl->pos-1, 1);
		if (rc < 0) { return rc; }
		rl->pos--;
		rl->redraw = true;
	}
	return 1;
}

static int
edit_delete(struct xreadline *rl)
{
    if (rl->pos < rl->line.count) {
		int rc = str_remove(&rl->line, rl->pos, 1);
		if (rc < 0) { return rc; }
		size_t max = max_pos(rl);
		if (rl->pos > max) { rl->pos = max; }
		rl->redraw = true;
	}
	return 1;
}

static int
edit_delete_end(struct xreadline *rl)
{
    if (rl->pos <= rl->line.count) {
		int rc = str_remove(&rl->line, rl->pos, rl->line.count - rl->pos);
		if (rc < 0) { return rc; }
		rl->redraw = true;
	}
	return 1;
}

static int
edit_delete_left_word(struct xreadline *rl, bool full)
{
	size_t pos = prev_word(rl, full);
	if (pos < rl->pos) {
		int rc = str_remove(&rl->line, pos, rl->pos - pos);
		if (rc < 0) { return rc; }
		rl->pos = pos;
		rl->redraw = true;
	}
	return 1;
}

#if 0
static void
edit_delete_right_word(struct xreadline *rl, bool full)
{
	size_t pos = next_word(rl, full);
	if (pos > rl->pos) {
		int rc = str_remove(&rl->line, rl->pos, pos);
		if (rc < 0) { return rc; }
		rl->pos = pos;
		rl->redraw = true;
	}
}
#endif

static int
edit_swap(struct xreadline *rl)
{
	if (rl->pos > 0) {
		size_t pos = rl->pos;
		if (rl->pos == rl->line.count) { pos--; }
		char tmp = rl->line.arr[pos-1];
		rl->line.arr[pos-1] = rl->line.arr[pos];
		rl->line.arr[pos] = tmp;
		if (rl->pos < rl->line.count) { rl->pos++; }
		rl->redraw = true;
	}
	return 1;
}

static int
edit_clear(struct xreadline *rl)
{
	if (rl->line.count > 0) {
		rl->line.count = 0;
		rl->pos = 0;
		rl->redraw = true;
	}
	return 1;
}

static int
edit_move_left(struct xreadline *rl)
{
    if (rl->pos > 0) {
        rl->pos--;
		rl->redraw = true;
    }
	return 1;
}

static int
edit_move_right(struct xreadline *rl)
{
    if (rl->pos < max_pos(rl)) {
        rl->pos++;
		rl->redraw = true;
    }
	return 1;
}

static int
edit_move_start(struct xreadline *rl)
{
    if (rl->pos > 0) {
        rl->pos = 0;
		rl->redraw = true;
    }
	return 1;
}

static int
edit_move_end(struct xreadline *rl)
{
    size_t max = max_pos(rl);
    if (rl->pos < max) {
        rl->pos = max;
		rl->redraw = true;
    }
	return 1;
}

static int
advance(struct xreadline *rl, const uint8_t *buf, size_t len)
{
	switch (buf[0]) {
	case 0x0d:    // enter
		rl->state = COMPLETE;
	default:
		return edit_insert(rl, buf[0]);
	case 0x03:    // ctrl-c
		rl->state = CANCELD;
		return 1;
	case 0x09:    // tab
		return 1;
	case 0x7f:    // backspace
	case 0x08:    // ctrl-h
		return edit_backspace(rl);
	case 0x04:    // ctrl-d, remove right-hand char, EOF if line is empty
		if (rl->line.count > 0) {
			return edit_delete(rl);
		}
		rl->state = END;
		return 1;
	case 0x14:    // ctrl-t, swaps current character with previous
		return edit_swap(rl);
	case 0x02:    // ctrl-b, move cursor left
		return edit_move_left(rl);
	case 0x06:    // ctrl-f, move cursor right
		return edit_move_right(rl);
	case 0x10:    // ctrl-p, previous history
		//edit_history(rl, -1);
		return 1;
	case 0x0e:    // ctrl-n, next history
		//edit_history(rl, 1);
		return 1;
	case 0x15:    // ctrl+u, delete the whole line
		return edit_clear(rl);
	case 0x0b:    // ctrl+k, delete from current to end of line
		return edit_delete_end(rl);
	case 0x01:    // ctrl+a, go to the start of the line
		return edit_move_start(rl);
	case 0x05:    // ctrl+e, go to the end of the line
		return edit_move_end(rl);
	case 0x17:    // ctrl+w, delete previous word
		return edit_delete_left_word(rl, false);
	case 0x0c:    // ctrl+l, clear screen
		return clear_screen(rl);
	case 0x1b:    // escape
		if (len == 1 ) { return 0; }
		if (buf[1] == 0x5b) {
			if (len < 3) {
				return 0;
			}
			switch (buf[2]) {
				case 0x44:
					edit_move_left(rl);
					return 3;
				case 0x43:
					edit_move_right(rl);
					return 3;
				case 0x41:
					//edit_history(rl, -1);
					return 3;
				case 0x42:
					//edit_history(rl, 1);
					return 3;
			}
			if (buf[2] > 0x30 && buf[2] < 0x37) {
				// extended escape sequence, ends at byte 0x40-0x7e
				int end = 2;
				while (true) {
					if (buf[end] >= 0x40 && buf[end] <= 0x7e) {
						break;
					}
					if (++end == (int)len) {
						return 0;
					}
				}
				if (end == 3 && buf[2] == 0x33 && buf[3] == 0x7e) { // forward delete
					edit_delete(rl);
				}
				return end + 1;
			}
		}
		return 1;
	}
}

char *
xreadline(const char *prompt)
{
	struct xreadline rl = {
		.line = XVEC_INIT,
		.out = stderr,
		.prompt = prompt,
		.fd = STDIN_FILENO,
		.state = PENDING,
		.redraw = true,
	};

	int err = 0;
	uint8_t buf[1024], *p = buf;
	char *line = NULL;

	if (enable_raw(rl.fd, &rl.restore) < 0) {
		err = xerrno;
		goto done;
	}

	do {
		if (rl.redraw) {
			redraw(&rl);
			rl.redraw = false;
		}
		ssize_t n = xread(rl.fd, buf, sizeof(buf) - (p - buf), -1);
		if (n < 0) { err = (int)n; goto disable; }
		p += n;
again:
		err = advance(&rl, buf, p - buf);
		if (err < 0) { break; }
		if (err == 0) { continue; }
		memmove(buf, buf + err, p - buf - err);
		p -= err;
		if (p > buf) {
			goto again;
		}
	} while(rl.state == PENDING);

	if (rl.state == COMPLETE) {
		line = rl.line.arr;
		rl.line.arr = NULL;
		fwrite("\r\n", 1, 2, rl.out);
		fflush(rl.out);
	}

disable:
	disable_raw(rl.fd, &rl.restore);
done:
	if (err < 0) {
		fprintf(rl.out, "\nerror: %s\n", xerr_str(err));
	}
	str_final(&rl.line);
	return line;
}

