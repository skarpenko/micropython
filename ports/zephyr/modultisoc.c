/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 The UltiSoC Project. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpconfig.h"
#if MICROPY_PY_ULTISOC

#include <stdio.h>
#include <zephyr.h>

#include "py/runtime.h"


/* Control device */
#define USOC_CTRL_IOBASE  0x80100000                 /* Control device I/O base */
#define USOC_CTRL_SOCVER  (USOC_CTRL_IOBASE + 0x000) /* SoC version (R/O) */
#define USOC_CTRL_RAMBASE (USOC_CTRL_IOBASE + 0x004) /* RAM base address (R/O) */
#define USOC_CTRL_RAMSIZE (USOC_CTRL_IOBASE + 0x008) /* RAM size (R/O) */
#define USOC_CTRL_ROMSIZE (USOC_CTRL_IOBASE + 0x00C) /* ROM size (R/O) */
#define USOC_CTRL_SYSFREQ (USOC_CTRL_IOBASE + 0x010) /* System frequency (R/O) */
#define USOC_CTRL_LED     (USOC_CTRL_IOBASE + 0x100) /* LEDs control register (R/W) */


/* Read word */
static inline unsigned readl(unsigned long addr)
{
    return *((volatile unsigned long*)addr);
}


/* Write word */
static inline void writel(unsigned v, unsigned long addr)
{
    *((volatile unsigned long*)addr) = v;
}


/* Enable interrupts */
static inline void interrupts_enable(void)
{
    __asm__ __volatile__ (
        ".set push        ;"
        ".set noreorder   ;"
        "mfc0 $t0, $12    ;"
        "ori $t0, $t0, 1  ;"
        "mtc0 $t0, $12    ;"
        "nop              ;"
        "nop              ;"
        ".set pop         ;"
        :
        :
        : "$t0"
    );
}


/* Disable interrupts */
static inline unsigned long interrupts_disable(void)
{
    unsigned v;
    __asm__ __volatile__ (
        ".set push         ;"
        ".set noreorder    ;"
        "mfc0 %0, $12      ;"
        "li $t0, ~1        ;"
        "and $t0, %0, $t0  ;"
        "mtc0 $t0, $12     ;"
        "nop               ;"
        ".set pop          ;"
        : "=r" (v)
        :
        : "$t0"
    );

    return v & 0x01;
}


/* Restore interrupts */
static inline void interrupts_restore(unsigned long state)
{
    __asm__ __volatile__ (
        ".set push             ;"
        ".set noreorder        ;"
        "mfc0 $t0, $12         ;"
        "li $t1, ~1            ;"
        "and $t0, $t0, $t1     ;"
        "or $t0, $t0, %0       ;"
        "mtc0 $t0, $12         ;"
        "nop                   ;"
        "nop                   ;"
        ".set pop              ;"
        :
        : "r" (state)
        : "$t0", "$t1"
    );
}


/* Read lower half of timestamp counter */
static inline unsigned cpu_rdtsc_lo(void)
{
    unsigned v;
    __asm__ __volatile__ (
        ".set push       ;"
        ".set noreorder  ;"
        "mfc0 %0, $8     ;"
        ".set pop        ;"
        : "=r" (v)
        :
        :
    );

    return v;
}


/* Read upper half of timestamp counter */
static inline unsigned cpu_rdtsc_hi(void)
{
    unsigned v;
    __asm__ __volatile__ (
        ".set push       ;"
        ".set noreorder  ;"
        "mfc0 %0, $9     ;"
        ".set pop        ;"
        : "=r" (v)
        :
        :
    );

    return v;
}


/* Read timestamp counter */
static inline unsigned long long cpu_rdtsc(void)
{
    unsigned lo = cpu_rdtsc_lo(); /* Read lower half first to latch upper half */
    unsigned hi = cpu_rdtsc_hi();
    return ((unsigned long long)hi << 32) | lo;
}


/* Read timestamp counter (interrupt safe) */
static inline unsigned long long cpu_rdtsc_safe(void)
{
    unsigned long intstate = interrupts_disable();
    unsigned lo = cpu_rdtsc_lo();
    unsigned hi = cpu_rdtsc_hi();
    interrupts_restore(intstate);
    return ((unsigned long long)hi << 32) | lo;
}


/* Python: soft reset */
STATIC mp_obj_t ultisoc_reset(void) {
    __asm__ __volatile__ (
        ".set push       ;"
        ".set noreorder  ;"
        "jr $zero        ;"
        "nop             ;"
        ".set pop        ;"
    );
    // Won't get here...
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ultisoc_reset_obj, ultisoc_reset);


/* Python: CPU Id value */
STATIC mp_obj_t ultisoc_cpuid(void) {
    unsigned int v;

    /* Read CPU Id value */
    __asm__ __volatile__ (
        ".set push         ;"
        ".set noreorder    ;"
        "mfc0 %0, $15      ;"
        ".set pop          ;"
        : "=r" (v)
        :
        :
    );

    return mp_obj_new_int_from_uint((mp_uint_t)v);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ultisoc_cpuid_obj, ultisoc_cpuid);


/* Python: SoC version */
STATIC mp_obj_t ultisoc_socid(void) {
    unsigned int soc_id = readl(USOC_CTRL_SOCVER);
    return mp_obj_new_int_from_uint((mp_uint_t)soc_id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ultisoc_socid_obj, ultisoc_socid);


/* Python: system frequency */
STATIC mp_obj_t ultisoc_sys_freq(void) {
    unsigned int sys_freq = readl(USOC_CTRL_SYSFREQ);
    return mp_obj_new_int_from_uint((mp_uint_t)sys_freq);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ultisoc_sys_freq_obj, ultisoc_sys_freq);


/* Python: CPU timestamp counter */
STATIC mp_obj_t ultisoc_cpu_tsc(void) {
    unsigned long long tsc = cpu_rdtsc_safe();
    return mp_obj_new_int_from_ull(tsc);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ultisoc_cpu_tsc_obj, ultisoc_cpu_tsc);


/* Python: CPU timestamp counter (low 32 bits) */
STATIC mp_obj_t ultisoc_cpu_tsc32(void) {
    unsigned int tsc = cpu_rdtsc_lo();
    return mp_obj_new_int_from_uint(tsc);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ultisoc_cpu_tsc32_obj, ultisoc_cpu_tsc32);


/* Python: LEDs control */
STATIC mp_obj_t ultisoc_leds(size_t n_args, const mp_obj_t *args) {
    unsigned led_status = readl(USOC_CTRL_LED);
    if(n_args > 0) {
        unsigned new_status = mp_obj_get_int(args[0]);
        writel(new_status, USOC_CTRL_LED);
    }
    return mp_obj_new_int_from_uint((mp_uint_t)led_status);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ultisoc_leds_obj, 0, 1, ultisoc_leds);


STATIC const mp_rom_map_elem_t mp_module_ultisoc_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ultisoc) },
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&ultisoc_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_cpuid), MP_ROM_PTR(&ultisoc_cpuid_obj) },
    { MP_ROM_QSTR(MP_QSTR_socid), MP_ROM_PTR(&ultisoc_socid_obj) },
    { MP_ROM_QSTR(MP_QSTR_sys_freq), MP_ROM_PTR(&ultisoc_sys_freq_obj) },
    { MP_ROM_QSTR(MP_QSTR_cpu_tsc), MP_ROM_PTR(&ultisoc_cpu_tsc_obj) },
    { MP_ROM_QSTR(MP_QSTR_cpu_tsc32), MP_ROM_PTR(&ultisoc_cpu_tsc32_obj) },
    { MP_ROM_QSTR(MP_QSTR_leds), MP_ROM_PTR(&ultisoc_leds_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_ultisoc_globals, mp_module_ultisoc_globals_table);


const mp_obj_module_t mp_module_ultisoc = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_ultisoc_globals,
};


#endif // MICROPY_PY_ULTISOC
