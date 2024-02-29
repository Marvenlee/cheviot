/*
 * Copyright 2023  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef MACHINE_BOARD_RASPBERRY_PI_4_INTERRUPT_H
#define MACHINE_BOARD_RASPBERRY_PI_4_INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>


/* GIC distributor register map */
struct bcm2711_gic_dist_registers
{
    uint32_t enable;                /* 0x000 */
    uint32_t ic_type;               /* 0x004 */
    uint32_t dist_ident;            /* 0x008 */
    uint32_t res1[29];              /* [0x00C, 0x080) */

    uint32_t group[32];             /* [0x080, 0x100) */

    uint32_t enable_set[32];        /* [0x100, 0x180) */
    uint32_t enable_clr[32];        /* [0x180, 0x200) */
    uint32_t pending_set[32];       /* [0x200, 0x280) */
    uint32_t pending_clr[32];       /* [0x280, 0x300) */
    uint32_t active[32];            /* [0x300, 0x380) */
    uint32_t res2[32];              /* [0x380, 0x400) */

    uint32_t priority[255];         /* [0x400, 0x7FC) */
    uint32_t res3;                  /* 0x7FC */

    uint32_t targets[255];            /* [0x800, 0xBFC) */
    uint32_t res4;                  /* 0xBFC */

    uint32_t config[64];             /* [0xC00, 0xD00) */

    uint32_t spi[32];               /* [0xD00, 0xD80) */
    uint32_t res5[20];              /* [0xD80, 0xDD0) */
    uint32_t res6;                  /* 0xDD0 */
    uint32_t legacy_int;            /* 0xDD4 */
    uint32_t res7[2];               /* [0xDD8, 0xDE0) */
    uint32_t match_d;               /* 0xDE0 */
    uint32_t enable_d;              /* 0xDE4 */
    uint32_t res8[70];               /* [0xDE8, 0xF00) */

    uint32_t sgi_control;           /* 0xF00 */
    uint32_t res9[3];               /* [0xF04, 0xF10) */
    uint32_t sgi_pending_clr[4];    /* [0xF10, 0xF20) */
    uint32_t res10[40];             /* [0xF20, 0xFC0) */

    uint32_t periph_id[12];         /* [0xFC0, 0xFF0) */
    uint32_t component_id[4];       /* [0xFF0, 0xFFF] */
};


/* GIC CPU Interface register map */
struct bcm2711_gic_cpu_iface_registers
{
    uint32_t icontrol;              /*  0x000         */
    uint32_t pri_msk_c;             /*  0x004         */
    uint32_t pb_c;                  /*  0x008         */
    uint32_t int_ack;               /*  0x00C         */
    uint32_t eoi;                   /*  0x010         */
    uint32_t run_priority;          /*  0x014         */
    uint32_t hi_pend;               /*  0x018         */
    uint32_t ns_alias_bp_c;         /*  0x01C         */
    uint32_t ns_alias_ack;          /*  0x020  */
    uint32_t ns_alias_eoi;          /*  0x024  */
    uint32_t ns_alias_hi_pend;      /*  0x028  */

    uint32_t res1[41];               // 0x02C - 0x0CC
    
    uint32_t active_priority[4];    /* 0x0D0 - 0xDC] */
    uint32_t ns_active_priority[4]; /* 0xE0,0xEC] */
    uint32_t res2[3];

    uint32_t cpu_if_ident;          /*  0x0FC         */   // Bottom bits should be 0x43B for ARM
    uint32_t res3[960];             /* [0x100. 0xFC0) */
};


/*
 * Total number of interrupts on bcm2711 used in Raspberry Pi 4
 */
#define NIRQ (192)


/*
 * GICv2 GIC400 register flags and masks
 */
#define GICD_CTL_ENABLE (1 << 0)
#define GICC_CTL_ENABLE (1 << 0)

#define GICD_TYPE_CPUS  0x0E0
#define GICD_TYPE_LINES 0x01F


/*
 * Default interrupt priorities
 */
#define GIC_PRI_LOWEST     240
#define GIC_PRI_IRQ        160
#define GIC_PRI_IPI        144
#define GIC_PRI_HIGHEST    128 


/*
 * Type of interrupt's configuration 
 *
 * Used by set_irq_config
 *
 * TODO: Move interrupt configuration to user-mode alloc_interrupt flag
 */
#define IRQ_CFG_LEVEL         0
#define IRQ_CFG_RISING_EDGE   2


/*
 * Interrupt assignments
 */
#define IRQ_TIMER0 (96)
#define IRQ_TIMER1 (97)
#define IRQ_TIMER2 (98)
#define IRQ_TIMER3 (99)
#define IRQ_SPURIOUS (1023)

/*
 * Prototypes
 */ 
void init_gicv2_distributor(void);
void init_gicv2_cpu_iface(void);
void set_irq_affinity(int irq, int cpu);
void set_irq_priority(int irq, int priority);
void set_irq_config(int irq, int config);


#endif


