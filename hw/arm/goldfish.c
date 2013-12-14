/*
 * Android Goldfish board emulation.
 *
 * Copyright (c) 2013 You-Sheng Yang
 *
 * This code is licensed under the GPL.
 */

#include "hw/boards.h"

static struct arm_boot_info goldfish_binfo;

static void goldfish_init(QEMUMachineInitArgs *args)
{
    ARMCPU *cpu;
    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    CPUARMState *env;
    DeviceState *dev, *sysctl;
    //qemu_irq cpu_irq;
    qemu_irq pic[32];

    if (!args->cpu_model) {
        args->cpu_model = "arm926";
    }

    cpu = cpu_arm_init(args->cpu_model);
    if (!cpu) {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    memory_region_init_ram(ram, NULL, "goldfish.ram", args->ram_size);
    vmstate_register_ram_global(ram);
    memory_region_add_subregion(sysmem, 0, ram);

    sysctl = qdev_create(NULL, "realview_sysctl");
    qdev_prop_set_uint32(sysctl, "sys_id", 0x0190F400);
    qdev_prop_set_uint32(sysctl, "proc_id", 0x02000000);
    qdev_init_nofail(sysctl);
    sysbus_mmio_map(SYS_BUS_DEVICE(sysctl), 0, 0x10000000);

    dev = sysbus_create_varargs("goldfish_interrupt_controller", 0xff000000,
                                qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ),
                                qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_FIQ),
                                NULL);
    for (n = 0; n < ARRAY_SIZE(pic); n++) {
        pic[n] = qdev_get_gpio_in(dev, n);
    }

    //cpu_irq = qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ);
    //env = &cpu->env;

    goldfish_binfo.ram_size = args->ram_size;
    goldfish_binfo.kernel_filename = args->kernel_filename;
    goldfish_binfo.kernel_cmdline = args->kernel_cmdline;
    goldfish_binfo.initrd_filename = args->initrd_filename;
    goldfish_binfo.nb_cpus = 1;
    goldfish_binfo.board_id = 1441;
    arm_load_kernel(cpu, &goldfish_binfo);
}

static QEMUMachine goldfish_machine = {
    .name = "android_arm",
    .desc = "ARM Android Emulator",
    .init = goldfish_init,
    .max_cpus = 1,
};

static void goldfish_machine_init(void)
{
    qemu_register_machine(&goldfish_machine);
}

machine_init(goldfish_machine_init);
