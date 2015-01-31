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
} USBMbimState;

enum {
    MBIM_IFACE_COMM = 0,
    MBIM_IFACE_DATA,
    VENDOR_IFACE_2,
    VENDOR_IFACE_3,
    VENDOR_IFACE_4,
    VENDOR_IFACE_5,
    N_IFACES,
};

enum {
    MBIM_EP_DATA_BULK = 1,
    VENDOR_EP_2,
    VENDOR_EP_3,
    VENDOR_EP_4,
    VENDOR_EP_5,
    UNUSED_EP_1,
    VENDOR_EP_7,
    MBIM_EP_COMM_INT,
};

enum {
    STRING_IFACE_MBIM_COMM = 1,
    STRING_IFACE_MBIM_DATA,
    STRING_IFACE_2,
    STRING_UNUSED_1,
    STRING_IFACE_3,
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
    [STRING_IFACE_2]          = "COM(comm_if)",
    [STRING_UNUSED_1]         = "unused",
    [STRING_IFACE_3]          = "COM(data_if)",
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
        .bInterfaceNumber              = VENDOR_IFACE_2,
        .bNumEndpoints                 = 3,
        .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass            = 0x02,
        .bInterfaceProtocol            = 1,
        .iInterface                    = STRING_IFACE_2,
        .ndesc                         = 4,
        .descs = (USBDescOther[]) {
            {
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    0x24, 0x00, 0x10, 0x01,
                },
            }, {
                .data = (uint8_t[]) {
                    0x04,                       /*  u8    bLength */
                    0x24, 0x02, 0x0f,
                },
            }, {
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    0x24, 0x06, 0x02, 0x03,
                },
            }, {
                .data = (uint8_t[]) {
                    0x05,                       /*  u8    bLength */
                    0x24, 0x01, 0x03, 0x03,
                },
            },
        },
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | VENDOR_EP_7,
                .bmAttributes          = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize        = 64,
                .bInterval             = 3,
            }, {
                .bEndpointAddress      = USB_DIR_IN | VENDOR_EP_2,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | VENDOR_EP_2,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            },
        },
    }, {
        .bInterfaceNumber              = VENDOR_IFACE_3,
        .bNumEndpoints                 = 2,
        .bInterfaceClass               = USB_CLASS_VENDOR_SPEC,
        .bInterfaceSubClass            = 0,
        .bInterfaceProtocol            = 0,
        .iInterface                    = STRING_IFACE_3,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress      = USB_DIR_IN | VENDOR_EP_3,
                .bmAttributes          = USB_ENDPOINT_XFER_BULK,
                .wMaxPacketSize        = 512,
                .bInterval             = 0,
            }, {
                .bEndpointAddress      = USB_DIR_OUT | VENDOR_EP_3,
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
    int ret;

    ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }

    p->status = USB_RET_STALL;
}

static void usb_mbim_handle_data(USBDevice *dev, USBPacket *p)
{
    p->status = USB_RET_STALL;
}

static const VMStateDescription vmstate_usb_mbim = {
    .name         = "usb-mbim",
    .unmigratable = 1,
};

static void usb_mbim_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    dc->vmsd           = &vmstate_usb_mbim;
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
