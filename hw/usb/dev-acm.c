/*
 * QEMU USB ACM device
 *
 * Copyright (C) 2015 You-Sheng Yang <vicamo@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu-common.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "hw/hw.h"
#include "sysemu/char.h"

#define ACM_BUFSIZE  512

typedef struct {
    /* qemu interfaces */
    USBDevice dev;

    uint8_t recv_buf[ACM_BUFSIZE];
    uint8_t *recv_ptr;

    /* properties */
    uint32_t debug;
    CharDriverState *cs;
} USBAcmState;

#define D(fmt, ...)  \
    if (s->debug) { \
        fprintf(stderr, "usb-acm: " fmt "\n", ##__VA_ARGS__); \
    }

enum {
    ACM_IFACE_CTRL = 0,
    ACM_IFACE_DATA,
    N_IFACES,
};

enum {
    ACM_EP_CTRL = 1,
    ACM_EP_DATA_IN,
    ACM_EP_DATA_OUT,
    N_EPS,
};

enum {
    STRING_MANUFACTURER = 1,
    STRING_PRODUCT,
    STRING_SERIALNUMBER,
    STRING_CONFIGURATION,
    STRING_IFACE_ACM_CTRL,
    STRING_IFACE_ACM_DATA,
};

static const USBDescStrings usb_acm_stringtable = {
    [STRING_MANUFACTURER]    = "QEMU",
    [STRING_PRODUCT]         = "QEMU ACM device",
    [STRING_SERIALNUMBER]    = "1",
    [STRING_CONFIGURATION]   = "1",
    [STRING_IFACE_ACM_CTRL]  = "COM(comm_if)",
    [STRING_IFACE_ACM_DATA]  = "COM(data_if)",
};

static const USBDescIfaceAssoc desc_iface_groups[] = {
    {
        .bFirstInterface    = ACM_IFACE_CTRL,
        .bInterfaceCount    = N_IFACES,
        .bFunctionClass     = USB_CLASS_COMM,
        .bFunctionSubClass  = USB_CDC_SUBCLASS_ACM,
        .bFunctionProtocol  = USB_CDC_ACM_PROTO_AT_V25TER,
        .iFunction          = STRING_IFACE_ACM_CTRL,
    },
};

#define U16(x) ((x) & 0xff), (((x) >> 8) & 0xff)

static const USBDescIface desc_iface[] = {
    {
        .bInterfaceNumber              = ACM_IFACE_CTRL,
        .bNumEndpoints                 = 1,
        .bInterfaceClass               = USB_CLASS_COMM,
        .bInterfaceSubClass            = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol            = USB_CDC_ACM_PROTO_AT_V25TER,
        .iInterface                    = STRING_IFACE_ACM_CTRL,
        .ndesc                         = 4,
        .descs = (USBDescOther[]) {
            {
                /* Header Descriptor */
                .data = (uint8_t[]) {
                    0x05,                          /* u8    bLength */
                    USB_DT_CS_INTERFACE,           /* u8    bDescriptorType */
                    USB_CDC_HEADER_TYPE,           /* u8    bDescriptorSubType */
                    U16(0x0120),                   /* le16  bcdCDC */
                },
            }, {
                /* ACM Descriptor */
                .data = (uint8_t[]) {
                    0x04,                          /* u8    bLength */
                    USB_DT_CS_INTERFACE,           /* u8    bDescriptorType */
                    USB_CDC_ACM_TYPE,              /* u8    bDescriptorSubType */
                    0x00,                          /* u8    bmCapabilities */
                },
            }, {
                /* Union Descriptor */
                .data = (uint8_t[]) {
                    0x05,                          /* u8    bLength */
                    USB_DT_CS_INTERFACE,           /* u8    bDescriptorType */
                    USB_CDC_UNION_TYPE,            /* u8    bDescriptorSubType */
                    ACM_IFACE_CTRL,                /* u8    bControlInterface */
                    ACM_IFACE_DATA,                /* u8    bSubordinateInterface0 */
                },
            }, {
                /* Call Management Descriptor */
                .data = (uint8_t[]) {
                    0x05,                          /* u8    bLength */
                    USB_DT_CS_INTERFACE,           /* u8    bDescriptorType */
                    USB_CDC_CALL_MANAGEMENT_TYPE,  /* u8    bDescriptorSubType */
                    0x00,                          /* u8    bmCapabilities */
                    ACM_IFACE_DATA,                /* u8    bDataInterface */
                },
            },
        },
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | ACM_EP_CTRL,
                .bmAttributes          = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize        = 64,
                .bInterval             = 4, /* 2 ^ (4 - 1) * 125us = 1ms */
            },
        },
    }, {
        .bInterfaceNumber              = ACM_IFACE_DATA,
        .bNumEndpoints                 = 2,
        .bInterfaceClass               = USB_CLASS_CDC_DATA,
        .bInterfaceSubClass            = USB_SUBCLASS_UNDEFINED,
        .bInterfaceProtocol            = USB_CDC_PROTO_NONE,
        .iInterface                    = STRING_IFACE_ACM_DATA,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | ACM_EP_DATA_IN,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = ACM_BUFSIZE,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | ACM_EP_DATA_OUT,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = ACM_BUFSIZE,
            },
        },
    },
};

static const USBDescDevice desc_device = {
    .bcdUSB                        = 0x0200,
    .bDeviceClass                  = USB_CLASS_MISC,
    .bDeviceSubClass               = 0x02,
    .bDeviceProtocol               = 0x01, /* Interface Association */
    .bMaxPacketSize0               = 64,
    .bNumConfigurations            = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = N_IFACES,
            .bConfigurationValue   = 1,
            .iConfiguration        = STRING_CONFIGURATION,
            .bmAttributes          = USB_CFG_ATT_ONE, /* Bus Powered */
            .bMaxPower             = 0xfa, /* 500 mA */
            .nif_groups            = ARRAY_SIZE(desc_iface_groups),
            .if_groups             = desc_iface_groups,
            .nif                   = ARRAY_SIZE(desc_iface),
            .ifs                   = desc_iface,
        },
    },
};

static const USBDesc desc_acm = {
    .id = {
        .idVendor       = 0x46f4, /* CRC16() of "QEMU" */
        .idProduct      = 0x0005,
        .bcdDevice      = 0,
        .iManufacturer  = STRING_MANUFACTURER,
        .iProduct       = STRING_PRODUCT,
        .iSerialNumber  = STRING_SERIALNUMBER,
    },
    .high  = &desc_device,
    .str   = usb_acm_stringtable,
};

static void usb_acm_handle_reset(USBDevice *dev);

static int usb_acm_cs_can_read(void *opaque)
{
    USBAcmState *s = opaque;
    int ret;

    if (!s->dev.attached) {
        ret = 0;
    } else {
        ret = ACM_BUFSIZE - (s->recv_ptr - s->recv_buf);
    }

    D("%s: %d", __FUNCTION__, ret);
    return ret;
}

static void usb_acm_cs_read(void *opaque, const uint8_t *buf, int size)
{
    USBAcmState *s = opaque;

    D("%s: %d", __FUNCTION__, size);
    memcpy(s->recv_ptr, buf, size);
    s->recv_ptr += size;
}

static void usb_acm_cs_event(void *opaque, int event)
{
    USBAcmState *s = opaque;

    switch (event) {
    case CHR_EVENT_OPENED:
        if (!s->dev.attached) {
            usb_device_attach(&s->dev, &error_abort);
        }
        break;

    case CHR_EVENT_CLOSED:
        if (s->dev.attached) {
            usb_device_detach(&s->dev);
        }
        break;
    }
}

static void usb_acm_realize(USBDevice *dev, Error **errp)
{
    USBAcmState *s = DO_UPCAST(USBAcmState, dev, dev);
    Error *local_err = NULL;

    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->dev.opaque = s;

    if (!s->cs) {
        error_setg(errp, "Property chardev is required");
        return;
    }

    usb_check_attach(dev, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    usb_acm_handle_reset(dev);

    qemu_chr_add_handlers(s->cs, usb_acm_cs_can_read, usb_acm_cs_read,
                          usb_acm_cs_event, s);
    /*
     * qemu_chr_add_handlers may open backend chardev and cause current
     * device being attached, so we set auto_attach to 0 if already opened.
     */
    if (dev->attached) {
        dev->auto_attach = 0;
    }
}

static void usb_acm_handle_attach(USBDevice *dev)
{
    USBAcmState *s = DO_UPCAST(USBAcmState, dev, dev);

    usb_desc_attach(dev);

    if (!s->cs->be_open) {
        qemu_chr_fe_set_open(s->cs, 1);
    }
}

static void usb_acm_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
    USBAcmState *s = DO_UPCAST(USBAcmState, dev, dev);
    int ret;

    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }

    D("failed control transaction: "
        "request 0x%04x value 0x%04x index 0x%04x length 0x%04x",
        request, value, index, length);

    p->status = USB_RET_STALL;
}

static void usb_acm_handle_data(USBDevice *dev, USBPacket *p)
{
    USBAcmState *s = DO_UPCAST(USBAcmState, dev, dev);

    D("failed data transaction: pid 0x%x ep 0x%x len 0x%zx",
        p->pid, p->ep->nr, p->iov.size);

    p->status = USB_RET_STALL;
}

static void usb_acm_handle_reset(USBDevice *dev)
{
    USBAcmState *s = DO_UPCAST(USBAcmState, dev, dev);

    s->recv_ptr = s->recv_buf;
}

static const VMStateDescription vmstate_usb_acm = {
    .name         = "usb-acm",
    .unmigratable = 1,
};

static Property usb_acm_props[] = {
    DEFINE_PROP_UINT32("debug", USBAcmState, debug, 0),
    DEFINE_PROP_CHR("chardev", USBAcmState, cs),
    DEFINE_PROP_END_OF_LIST(),
};

static void usb_acm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    dc->vmsd           = &vmstate_usb_acm;
    dc->props          = usb_acm_props;
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);

    uc->product_desc   = "QEMU USB ACM Interface";
    uc->usb_desc       = &desc_acm;
    uc->realize        = usb_acm_realize;
    uc->handle_attach  = usb_acm_handle_attach;
    uc->handle_control = usb_acm_handle_control;
    uc->handle_data    = usb_acm_handle_data;
    uc->handle_reset   = usb_acm_handle_reset;
}

static const TypeInfo usb_acm_info = {
    .name          = "usb-acm",
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBAcmState),
    .class_init    = usb_acm_class_init,
};

static void usb_acm_register_types(void)
{
    type_register_static(&usb_acm_info);
}

type_init(usb_acm_register_types)
