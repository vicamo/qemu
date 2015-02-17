/*
 * QEMU atmodem chardev
 *
 * Copyright (c) 2015 You-Sheng Yang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <glib.h>

#include "qemu-common.h"
#include "sysemu/char.h"

#define DEBUG_ATMODEM 1

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef struct {
    CharDriverState *chr;

    char incoming[128];
    int in_pos;
    char outgoing[1024];
    int out_pos;

    char last_cmd[128];

    char s3; /* V250, 6.2.1 "Command line termination character" */
    char s4; /* V250, 6.2.2 "Response formatting character" */
} AtModemCharDriver;

typedef int (*AtCommandHandler) (AtModemCharDriver *drv,
                char *cmd, char *content);

static void atmodem_handle_incoming(AtModemCharDriver *drv);

#if DEBUG_ATMODEM

#define D(fmt, ...) \
    fprintf(stderr, "atmodem: " fmt "\n", ##__VA_ARGS__)

static const char *quote(uint8_t *buf, int len)
{
    static char temp[1024];
    char *p = temp;
    uint8_t c, *end = buf + len;
    int available, n;

    available = sizeof(temp) - (p - temp);
    while ((buf != end) && available) {
        c = *buf++;

        if (isprint(c)) {
            *p++ = c;
            --available;
            continue;
        }

        if (c == '\r') {
            n = snprintf(p, available, "<CR>");
        } else if (c == '\n') {
            n = snprintf(p, available, "<LF>");
        } else {
            n = snprintf(p, available, "\\x%02X", c);
        }

        if (n <= available) {
            p += n;
            available -= n;
        } else {
            break;
        }
    }

    if (available) {
        *p = '\0';
    } else {
        temp[sizeof(temp) - 1] = '\0';
    }

    return temp;
}

#else
#define D(fmt, ...)
#endif

static int atmodem_chr_be_try_write_once(AtModemCharDriver *drv,
                uint8_t *buf, int len)
{
    int n;

    n = qemu_chr_be_can_write(drv->chr);
    if (n) {
        n = min(n, len);
        D("< %s", quote(buf, n));
        qemu_chr_be_write(drv->chr, buf, n);
    }

    return n;
}

static int atmodem_chr_be_try_write(AtModemCharDriver *drv,
                uint8_t *buf, int len)
{
    int n, written = 0;

    do {
        n = atmodem_chr_be_try_write_once(drv, buf, len);
        buf += n;
        len -= n;
        written += n;
    } while (n && len);

    return written;
}

static void atmodem_chr_accept_input(CharDriverState *chr)
{
    AtModemCharDriver *drv = (AtModemCharDriver *)chr->opaque;
    int n;

    if (!drv->out_pos) {
        return;
    }

    n = atmodem_chr_be_try_write(drv, (uint8_t *)drv->outgoing, drv->out_pos);
    drv->out_pos -= n;
    if (drv->out_pos) {
        memmove(drv->outgoing, drv->outgoing + n, drv->out_pos);
    }
}

static int atmodem_chr_write(CharDriverState *chr, const uint8_t *buf, int len)
{
    AtModemCharDriver *drv = (AtModemCharDriver *)chr->opaque;
    int n, written = 0;

#if 1
    qemu_hexdump((char *)buf, stderr, "[>", len);
#endif

    do {
        n = sizeof(drv->incoming) - drv->in_pos;
        if (!n) {
            break;
        }

        n = min(n, len);
        memcpy(drv->incoming + drv->in_pos, buf, n);
        buf += n;
        len -= n;
        drv->in_pos += n;
        written += n;

        atmodem_handle_incoming(drv);
    } while (len);

    return written;
}

static void atmodem_chr_close(struct CharDriverState *chr)
{
    AtModemCharDriver *drv = chr->opaque;

    g_free(drv);
}

CharDriverState *qemu_chr_open_atmodem(void)
{
    AtModemCharDriver *drv;
    CharDriverState *chr;

    drv = g_malloc0(sizeof(AtModemCharDriver));

    drv->chr = chr = g_malloc0(sizeof(CharDriverState));
    chr->opaque = drv;
    chr->chr_accept_input = atmodem_chr_accept_input;
    chr->chr_write = atmodem_chr_write;
    chr->chr_close = atmodem_chr_close;

    drv->s3 = '\r';
    drv->s4 = '\n';

    return chr;
}

static int atmodem_begin_line(AtModemCharDriver *drv)
{
    if ((drv->out_pos + 2) <= sizeof(drv->outgoing)) {
        drv->outgoing[drv->out_pos++] = drv->s3;
        drv->outgoing[drv->out_pos++] = drv->s4;
    }

    return 2;
}

static int atmodem_append_line_v(AtModemCharDriver *drv, const char *fmt, va_list ap)
{
    int n, available;

    available = sizeof(drv->outgoing) - drv->out_pos - 2;
    n = vsnprintf(drv->outgoing + drv->out_pos, available, fmt, ap);

    if (n >= 0 && n <= available) {
        drv->out_pos += n;
        drv->outgoing[drv->out_pos++] = drv->s3;
        drv->outgoing[drv->out_pos++] = drv->s4;
        n += 2;
    }

    return n;
}

static void atmodem_end_line(AtModemCharDriver *drv)
{
    atmodem_chr_accept_input(drv->chr);
}

static void atmodem_respond(AtModemCharDriver *drv, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    atmodem_begin_line(drv);
    atmodem_append_line_v(drv, fmt, ap);
    atmodem_end_line(drv);

    va_end(ap);
}

static int handle_at(AtModemCharDriver *drv, char *cmd, char *content)
{
    D("%s", __PRETTY_FUNCTION__);
    atmodem_respond(drv, "OK");
    return 0;
}

static int handle_dial(AtModemCharDriver *drv, char *cmd, char *content)
{
    D("%s", __PRETTY_FUNCTION__);
    atmodem_respond(drv, "OK");
    return 0;
}

static const struct {
    const char *cmd;
    AtCommandHandler handler;
} cmds_table[] = {
    { "^D", handle_dial },
    { "", handle_at },

    { NULL }
};

static int atmodem_process_line(AtModemCharDriver *drv, char *line)
{
    const char *cmd;
    char *content;
    int i;

    D("> %s", quote((uint8_t *)line, strlen(line)));

    content = strchr(line, ':');
    if (content) {
        *content++ = '\0';
    }

    for (i = 0; cmds_table[i].cmd; ++i) {
        cmd = cmds_table[i].cmd;

        if ('^' == cmd[0]) {
            ++cmd;
            if (g_ascii_strncasecmp(line, cmd, strlen(cmd))) {
                continue;
            }
        } else if (g_ascii_strcasecmp(line, cmd)) {
            continue;
        }

        return cmds_table[i].handler(drv, line, content);
    }

    atmodem_respond(drv, "ERROR");

    return 0;
}

static int atmodem_handle_incoming_loop(AtModemCharDriver *drv)
{
    char *p, *term, *remain, *end;

    p = remain = drv->incoming;
    end = drv->incoming + drv->in_pos;

    while (p != end) {
        if ('a' != tolower(*p)) {
            remain = ++p;
            continue;
        }

        ++p;
        if (p == end) {
            break;
        }

        /* V250, 5.2.1, "A/" or "a/" to repeat last command. */
        if ('/' == *p) {
            char tmp[sizeof(drv->last_cmd)];
            strncpy(tmp, drv->last_cmd, sizeof tmp);

            remain = ++p;
            if (atmodem_process_line(drv, tmp)) {
                break;
            }
            continue;
        }

        if ('t' != tolower(*p)) {
            /* Could be 'a', so don't increase p. */
            continue;
        }

        ++p;
        term = memchr(p, drv->s3, end - p);
        if (NULL == term) {
            break;
        }

        *term = '\0';
        remain = ++term;

        /* Save current command in drv->last_cmd before processing it. */
        strncpy(drv->last_cmd, p, sizeof(drv->last_cmd));

        if (atmodem_process_line(drv, p)) {
            break;
        }

        p = remain;
    }

    return end - remain;
}

static void atmodem_handle_incoming(AtModemCharDriver *drv)
{
    int remain;

    remain = atmodem_handle_incoming_loop(drv);;
    if (remain != drv->in_pos) {
        memmove(drv->incoming, (drv->incoming + drv->in_pos - remain),
            remain);
    }

    drv->in_pos = remain;
}
