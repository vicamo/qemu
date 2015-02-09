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

#include "qemu-common.h"
#include "sysemu/char.h"

typedef struct {
    CharDriverState *chr;
} AtModemCharDriver;

static int atmodem_chr_write(CharDriverState *chr, const uint8_t *buf, int len)
{
    return len;
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
    chr->chr_write = atmodem_chr_write;
    chr->chr_close = atmodem_chr_close;

    return chr;
}
