/*
 * QEMU USB MBIM device
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

typedef struct {
    /* qemu interfaces */
    USBDevice dev;

    /* properties */
    uint32_t debug;
} USBMbimState;

#define D(fmt, ...)  \
    if (s->debug) { \
        fprintf(stderr, "usb-mbim: " fmt "\n", ##__VA_ARGS__); \
    }

enum {
    MBIM_IFACE_COMM = 0,
    MBIM_IFACE_DATA,
    ACM_IFACE_COMM,
    ACM_IFACE_DATA,
    VENDOR_IFACE_4,
    VENDOR_IFACE_5,
    N_IFACES,
};

enum {
    MBIM_EP_DATA_BULK = 1,
    ACM_EP_COMM_BULK,
    ACM_EP_DATA_BULK,
    VENDOR_EP_4,
    VENDOR_EP_5,
    UNUSED_EP_1,
    ACM_EP_COMM_INT,
    MBIM_EP_COMM_INT,
};

enum {
    STRING_IFACE_MBIM_COMM = 1,
    STRING_IFACE_MBIM_DATA,
    STRING_IFACE_ACM_COMM,
    STRING_UNUSED_1,
    STRING_IFACE_ACM_DATA,
    STRING_IFACE_4,
    STRING_IFACE_5,
    STRING_UNUSED_2,
    STRING_MANUFACTURER,
    STRING_PRODUCT,
    STRING_SERIALNUMBER,
};

static const USBDescStrings usb_mbim_stringtable = {
    [STRING_IFACE_MBIM_COMM]  = "COM(comm_if)",
    [STRING_IFACE_MBIM_DATA]  = "COM(data_if)",
    [STRING_IFACE_ACM_COMM]   = "COM(comm_if)",
    [STRING_UNUSED_1]         = "unused",
    [STRING_IFACE_ACM_DATA]   = "COM(data_if)",
    [STRING_IFACE_4]          = "COM(data_if)",
    [STRING_IFACE_5]          = "COM(data_if)",
    [STRING_UNUSED_2]         = "unused",
    [STRING_MANUFACTURER]     = "D-Link,Inc",
    [STRING_PRODUCT]          = "D-Link DWM-157",
    [STRING_SERIALNUMBER]     = "1",
};

static const USBDescIfaceAssoc desc_iface_groups[] = {
    {
        .bFirstInterface    = MBIM_IFACE_COMM,
        .bInterfaceCount    = 2,
        .bFunctionClass     = USB_CLASS_COMM,
        .bFunctionSubClass  = USB_CDC_SUBCLASS_MBIM,
        .bFunctionProtocol  = USB_CDC_PROTO_NONE,
        .iFunction          = STRING_IFACE_MBIM_COMM,
    },
};

static const USBDescIface desc_iface[] = {
    {
        .bInterfaceNumber              = MBIM_IFACE_COMM,
        .bNumEndpoints                 = 1,
        .bInterfaceClass               = USB_CLASS_COMM,
        .bInterfaceSubClass            = USB_CDC_SUBCLASS_MBIM,
        .bInterfaceProtocol            = USB_CDC_PROTO_NONE,
        .iInterface                    = STRING_IFACE_MBIM_COMM,
        .ndesc                         = 3,
        .descs = (USBDescOther[]) {
            {
                /* Header Descriptor */
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    USB_DT_CS_INTERFACE,        /*  u8    bDescriptorType */
                    USB_CDC_HEADER_TYPE,        /*  u8    bDescriptorSubType */
                    0x10, 0x01,                 /*  le16  bcdCDC */
                },
            }, {
                /* Union Descriptor */
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    USB_DT_CS_INTERFACE,        /*  u8    bDescriptorType */
                    USB_CDC_UNION_TYPE,         /*  u8    bDescriptorSubType */
                    MBIM_IFACE_COMM,            /*  u8    bMasterInterface0 */
                    MBIM_IFACE_DATA,            /*  u8    bSlaveInterface0 */
                },
            }, {
                /* MBIM Descriptor */
                .data = (uint8_t[]) {
                    0x0c,                       /*  u8    bLength */
                    USB_DT_CS_INTERFACE,        /*  u8    bDescriptorType */
                    USB_CDC_MBIM_TYPE,          /*  u8    bDescriptorSubType */
                    0x00, 0x01,                 /*  le16  bcdMBIMVersion */
                    0x00, 0x02,                 /*  u16   wMaxControlMessage */
                    0x10,                       /*  u8    bNumberFilters */
                    0x40,                       /*  u8    bMaxFilterSize */
                    0xdc, 0x05,                 /*  u16   wMaxSegmentSize */
                    0x20,                       /*  u8    bmNetworkCapabilities */
                },
            },
        },
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | MBIM_EP_COMM_INT,
                .bmAttributes          = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize        = 64,
                .bInterval             = 1,
            },
        },
    }, {
        .bInterfaceNumber              = MBIM_IFACE_DATA,
        .bAlternateSetting             = 0,
        .bNumEndpoints                 = 0,
        .bInterfaceClass               = USB_CLASS_CDC_DATA,
        .bInterfaceSubClass            = USB_SUBCLASS_UNDEFINED,
        .bInterfaceProtocol            = USB_CDC_MBIM_PROTO_NTB,
        .iInterface                    = STRING_IFACE_MBIM_DATA,
    }, {
        .bInterfaceNumber              = MBIM_IFACE_DATA,
        .bAlternateSetting             = 1,
        .bNumEndpoints                 = 2,
        .bInterfaceClass               = USB_CLASS_CDC_DATA,
        .bInterfaceSubClass            = USB_SUBCLASS_UNDEFINED,
        .bInterfaceProtocol            = USB_CDC_MBIM_PROTO_NTB,
        .iInterface                    = 0,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | MBIM_EP_DATA_BULK,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | MBIM_EP_DATA_BULK,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            },
        },
    }, {
        .bInterfaceNumber              = ACM_IFACE_COMM,
        .bNumEndpoints                 = 3,
        .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass            = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol            = USB_CDC_ACM_PROTO_AT_V25TER,
        .iInterface                    = STRING_IFACE_ACM_COMM,
        .ndesc                         = 4,
        .descs = (USBDescOther[]) {
            {
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    USB_DT_CS_INTERFACE,        /*  u8    bDescriptorType */
                    USB_CDC_HEADER_TYPE,        /*  u8    bDescriptorSubType */
                    0x10, 0x01,                 /*  le16  bcdCDC */
                },
            }, {
                .data = (uint8_t[]) {
                    0x04,                       /*  u8    bLength */
                    USB_DT_CS_INTERFACE,        /*  u8    bDescriptorType */
                    USB_CDC_ACM_TYPE,           /*  u8    bDescriptorSubType */
                    0x0f,                       /*  u8    bmCapabilities */
                },
            }, {
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    USB_DT_CS_INTERFACE,        /*  u8    bDescriptorType */
                    USB_CDC_UNION_TYPE,         /*  u8    bDescriptorSubType */
                    ACM_IFACE_COMM,             /*  u8    bMasterInterface0 */
                    ACM_IFACE_DATA,             /*  u8    bSlaveInterface0 */
                },
            }, {
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    USB_DT_CS_INTERFACE,        /*  u8    bDescriptorType */
                    USB_CDC_CALL_MANAGEMENT_TYPE, /*  u8    bDescriptorSubType */
                    0x03,                       /*  u8    bmCapabilities */
                    ACM_IFACE_DATA,             /*  u8    bDataInterface */
                },
            },
        },
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | ACM_EP_COMM_INT,
                .bmAttributes          = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize        = 64,
                .bInterval             = 3,
            }, {
                .bEndpointAddress      = USB_DIR_IN | ACM_EP_COMM_BULK,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | ACM_EP_COMM_BULK,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            },
        },
    }, {
        .bInterfaceNumber              = ACM_IFACE_DATA,
        .bNumEndpoints                 = 2,
        .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass            = 0,
        .bInterfaceProtocol            = 0,
        .iInterface                    = STRING_IFACE_ACM_DATA,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | ACM_EP_DATA_BULK,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | ACM_EP_DATA_BULK,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            },
        },
    }, {
        .bInterfaceNumber              = VENDOR_IFACE_4,
        .bNumEndpoints                 = 2,
        .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass            = 0,
        .bInterfaceProtocol            = 0,
        .iInterface                    = STRING_IFACE_4,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | VENDOR_EP_4,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | VENDOR_EP_4,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            },
        },
    }, {
        .bInterfaceNumber              = VENDOR_IFACE_5,
        .bNumEndpoints                 = 2,
        .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass            = 0,
        .bInterfaceProtocol            = 0,
        .iInterface                    = STRING_IFACE_5,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | VENDOR_EP_5,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | VENDOR_EP_5,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
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
            .iConfiguration        = 0,
            .bmAttributes          = USB_CFG_ATT_ONE, /* Bus Powered */
            .bMaxPower             = 0xfa, /* 500 mA */
            .nif_groups            = ARRAY_SIZE(desc_iface_groups),
            .if_groups             = desc_iface_groups,
            .nif                   = ARRAY_SIZE(desc_iface),
            .ifs                   = desc_iface,
        },
    },
};

static const USBDesc desc_mbim = {
    .id = {
        .idVendor       = 0x2001, /* D-Link Corp. */
        .idProduct      = 0x7d02, /* D-Link DWM-157 */
        .bcdDevice      = 0x0300,
        .iManufacturer  = STRING_MANUFACTURER,
        .iProduct       = STRING_PRODUCT,
        .iSerialNumber  = STRING_SERIALNUMBER,
    },
    .high  = &desc_device,
    .str   = usb_mbim_stringtable,
};

static void usb_mbim_realize(USBDevice *dev, Error **errp)
{
    USBMbimState *s = DO_UPCAST(USBMbimState, dev, dev);

    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->dev.opaque = s;
}

static void usb_mbim_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
    USBMbimState *s = DO_UPCAST(USBMbimState, dev, dev);
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

static void usb_mbim_handle_data(USBDevice *dev, USBPacket *p)
{
    USBMbimState *s = DO_UPCAST(USBMbimState, dev, dev);

    D("failed data transaction: pid 0x%x ep 0x%x len 0x%zx",
        p->pid, p->ep->nr, p->iov.size);

    p->status = USB_RET_STALL;
}

static const VMStateDescription vmstate_usb_mbim = {
    .name         = "usb-mbim",
    .unmigratable = 1,
};

static Property usb_mbim_props[] = {
    DEFINE_PROP_UINT32("debug", USBMbimState, debug, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void usb_mbim_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    dc->vmsd           = &vmstate_usb_mbim;
    dc->props          = usb_mbim_props;
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);

    uc->product_desc   = "QEMU USB MBIM Interface";
    uc->usb_desc       = &desc_mbim;
    uc->realize        = usb_mbim_realize;
    uc->handle_attach  = usb_desc_attach;
    uc->handle_control = usb_mbim_handle_control;
    uc->handle_data    = usb_mbim_handle_data;
}

static const TypeInfo usb_mbim_info = {
    .name          = "usb-mbim",
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBMbimState),
    .class_init    = usb_mbim_class_init,
};

static void usb_mbim_register_types(void)
{
    type_register_static(&usb_mbim_info);
}

type_init(usb_mbim_register_types)
