/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/
#include "type_def.h"
#include "arch.h"

/**
 * @brief ARM core trigger the svc call interrupt.
 */
i32p_t kernel_svc_call(u32_t args_0, u32_t args_1, u32_t args_2, u32_t args_3)
{
    /* TODO */
}

/**
 * @brief ARM core systick interrupt handle function.
 */
void SysTick_Handler(void)
{
    /* TODO */
}

/**
 * @brief ARM core fault interrupt handle function.
 */
void HardFault_Handler(void)
{
    /* TODO */
}

/**
 * @brief To check if it's in interrupt content.
 */
b_t port_isInInterruptContent(void)
{
    /* TODO */

    return FALSE;
}

/**
 * @brief To check if it's in kernel thread content.
 */
b_t port_isInThreadMode(void)
{
    /* TODO */
    return TRUE;
}

/**
 * @brief ARM core trigger the pendsv interrupt.
 */
void port_setPendSV(void)
{
    /* TODO */
}

/**
 * @brief ARM core config kernel thread interrupt priority.
 */
void port_interrupt_init(void)
{
    /* TODO */
}

/**
 * @brief ARM core SVC interrupt handle function.
 */
void SVC_Handler(void)
{
    /* TODO */
}

/**
 * @brief ARM core PendSV interrupt handle function.
 */
void PendSV_Handler(void)
{
    /* TODO */
}

/**
 * @brief ARM core trigger the first thread to run.
 */
void port_run_theFirstThread(u32_t sp)
{
    /* TODO */
}

/**
 * @brief Initialize a thread stack frame.
 *
 * @param pEntryFunction The entry function pointer.
 * @param pAddress The stack address.
 * @param size The stack size.
 *
 * @return The PSP stack address.
 */
u32_t port_stack_frame_init(void (*pEntryFunction)(void), u32_t *pAddress, u32_t size)
{
    /* TODO */
}
