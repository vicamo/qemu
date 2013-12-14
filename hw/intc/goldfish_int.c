/*
 * Goldfish Interrupt Controller
 *
 * Copyright (c) 2013 You-Sheng Yang
 * Copyright (C) 2007-2008 The Android Open Source Project
 *
 * This code is licensed under the GPL.
 */

#include "hw/sysbus.h"

#define TYPE_GOLDFISH_INT "goldfish_interrupt_controller"
#define GOLDFISH_INT(obj) OBJECT_CHECK(GoldfishIntState, (obj), TYPE_GOLDFISH_INT)

typedef struct GoldfishIntState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t level;
    uint32_t pending_count;
    uint32_t irq_enabled;
    uint32_t fiq_enabled;
    qemu_irq irq;
    qemu_irq fiq;
} GoldfishIntState;

enum {
    INTERRUPT_STATUS        = 0x00, // number of pending interrupts
    INTERRUPT_NUMBER        = 0x04,
    INTERRUPT_DISABLE_ALL   = 0x08,
    INTERRUPT_DISABLE       = 0x0c,
    INTERRUPT_ENABLE        = 0x10
};

static void goldfish_int_update(GoldfishIntState *s)
{
    uint32_t flags;

    flags = (s->level & s->irq_enabled);
    qemu_set_irq(s->irq, flags != 0);

    flags = (s->level & s->fiq_enabled);
    qemu_set_irq(s->fiq, flags != 0);
}

static void goldfish_int_set_irq(void *opaque, int irq, int level)
{
    GoldfishIntState *s = (GoldfishIntState *)opaque;
    uint32_t mask = (1U << irq);

    if (level) {
        if (!(s->level & mask)) {
            if (s->irq_enabled & mask)
                s->pending_count++;
            s->level |= mask;
        }
    } else {
        if (s->level & mask) {
            if (s->irq_enabled & mask)
                s->pending_count--;
            s->level &= ~mask;
        }
    }

    goldfish_int_update(s);
}

static uint64_t goldfish_int_read(void *opaque, hwaddr offset, unsigned size)
{
    GoldfishIntState *s = (GoldfishIntState *)opaque;

    switch (offset) {
    case INTERRUPT_STATUS: /* IRQ_STATUS */
        return s->pending_count;
    case INTERRUPT_NUMBER: {
        int i;
        uint32_t pending = s->level & s->irq_enabled;
        for (i = 0; i < 32; i++) {
            if (pending & (1U << i))
                return i;
        }
        return 0;
    }
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "goldfish_int_read: Bad offset %x\n", (int)offset);
        return 0;
    }
}

static void goldfish_int_write(void *opaque, hwaddr offset,
                               uint64_t value, unsigned size)
{
    GoldfishIntState *s = (GoldfishIntState *)opaque;
    uint32_t mask = (1U << value);

    switch (offset) {
        case INTERRUPT_DISABLE_ALL:
            s->pending_count = 0;
            s->level = 0;
            break;

        case INTERRUPT_DISABLE:
            if(s->irq_enabled & mask) {
                if(s->level & mask)
                    s->pending_count--;
                s->irq_enabled &= ~mask;
            }
            break;
        case INTERRUPT_ENABLE:
            if(!(s->irq_enabled & mask)) {
                s->irq_enabled |= mask;
                if(s->level & mask)
                    s->pending_count++;
            }
            break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                     "goldfish_int_write: Bad offset %x\n", (int)offset);
        return;
    }
    goldfish_int_update(s);
}

static const MemoryRegionOps goldfish_int_ops = {
    .read = goldfish_int_read,
    .write = goldfish_int_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int goldfish_int_init(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    GoldfishIntState *s = GOLDFISH_INT(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &goldfish_int_ops, s,
                          "goldfish_int", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    qdev_init_gpio_in(dev, goldfish_int_set_irq, 32);
    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_irq(sbd, &s->fiq);
    return 0;
}

static void goldfish_int_reset(DeviceState *d)
{
    GoldfishIntState *s = GOLDFISH_INT(d);

    s->level = 0;
    s->pending_count = 0;
    s->irq_enabled = 0;
    s->fiq_enabled = 0;
    goldfish_int_update(s);
}

static const VMStateDescription vmstate_goldfish_int = {
    .name = "goldfish_int",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(level, GoldfishIntState),
        VMSTATE_UINT32(pending_count, GoldfishIntState),
        VMSTATE_UINT32(irq_enabled, GoldfishIntState),
        VMSTATE_UINT32(fiq_enabled, GoldfishIntState),
        VMSTATE_END_OF_LIST()
    }
};

static void goldfish_int_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = goldfish_int_init;
    dc->no_user = 1;
    dc->reset = goldfish_int_reset;
    dc->vmsd = &vmstate_goldfish_int;
}

static const TypeInfo goldfish_int_info = {
    .name          = TYPE_GOLDFISH_INT,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(GoldfishIntState),
    .class_init    = goldfish_int_class_init,
};

static void goldfish_int_register_types(void)
{
    type_register_static(&goldfish_int_info);
}

type_init(goldfish_int_register_types)
