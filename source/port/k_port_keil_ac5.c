/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/
#include "./port/k_port.h"

#if defined ( __FPU_PRESENT )
#define FPU_ENABLED  __FPU_PRESENT
#else
#define FPU_ENABLED  0
#endif

extern void kernel_privilege_call_inSVC_c(_u32_t *svc_args);
extern void kernel_scheduler_inPendSV_c(_u32_t **ppCurPsp, _u32_t **ppNextPSP);

/**
 * @brief ARM core SVC interrupt handle function.
 */
__asm void SVC_Handler(void)
{
    /* Before call this the [R0-R3, R12, LR, PC, PSR] store into PSP */
    TST   LR, #0x04                                             /* call from which stack pointer base on the bit 2 of EXC_RETURN (LR) */
    ITE   EQ
    MRSEQ R0, MSP                                               /* Set R0 = MSP */
    MRSNE R0, PSP                                               /* Set R0 = PSP */

    B     __cpp(kernel_privilege_call_inSVC_c)            /* call kernel_privilege_call_inSVC_c */

    /**
     * return from exception, restoring {R0-R3, R12, LR, PC, PSR}
     */
    ALIGN 4
}

/**
 * @brief ARM core PendSV interrupt handle function.
 */
__asm void PendSV_Handler(void)
{
    /**
     * Save current context
     * Before call this the {R0-R3, R12, LR, PC, PSR} store into PSP
    */
    CPSID    I                                                   /* Disable interrupts */
    ISB


    /**
     * Call kernel_scheduler_inPendSV_c
     */
    PUSH     {R0, R1, R12, LR}
    MOV      R0, SP                                              /* R0 points to the argument ppCurPsp  */
    ADD      R1, SP, #4                                          /* R1 points to the argument ppNextPSP */
    BL       __cpp(kernel_scheduler_inPendSV_c)                  /* Call kernel_scheduler_inPendSV_c */
    POP      {R0, R1, R12, LR}                                   /* R0 = ppCurPsp, R1 = ppNextPSP */

    CMP      R0, R1                                              /* if R0 = R1 */
    BEQ      Exit                                                /* Skip context switch */

    MRS      R2, PSP                                             /* Get current process stack pointer value */

#if ( FPU_ENABLED )                                              /* If the Cortex-M is not supported, the ASM instruction will not support VSTMDBEQ */
    TST      LR, #0x10                                           /* Test bit 4 of EXC_RETURN (0: FPU active on exception entry, 1: FPU not active) */
    IT       EQ                                                  /* if (LR[4] == 0) */
    VSTMDBEQ R2!, {S16 - S31}                                    /* Save floating point registers, EQ suffix will save FPU registers {s16 - S31} */
                                                                 /* if bit of LR was zero (S0-S15, FPSCR alread saved by MCU) */
    MRS      R3, CONTROL                                         /* Save CONTROL register in R3 to be pushed on stack - bit 2 (FPCA) indicates floating-point is active */

    STMDB    R2!, {R3 - R11}                                     /* Save CONTROL, {R4 - R11} */
    STMDB    R2!, {LR}                                           /* Save EXC_RETURN in saved context */
#else
    STMDB    R2!, {R4 - R11}                                     /* Save {R4 - R11} */
#endif

    /**
     * Context switching code
    */
    STR      R2, [R0]                                            /* *ppCurPSP = CurPSP */
    LDR      R2, [R1]                                            /* NextPSP = *pNextPSP */

#if ( FPU_ENABLED )
    LDMIA    R2!, {LR}                                           /* restore LR */
    LDMIA    R2!, {R3 - R11}                                     /* restore {R3 - R11} */

    MSR      CONTROL, R3                                         /* restore CONTROL register */
    ISB

    TST      LR, #0x10                                           /* Test bit 4. If zero, need to unstack floating point regs */
    IT       EQ
    VLDMIAEQ R2!, {S16 - S31}                                    /* Load floating point registers */
#else
    LDMIA    R2!, {R4 - R11}                                     /* no FPU present - context is {R4 - R11} */
#endif

    MSR      PSP, R2                                             /* Set PSP to next thread */

    /**
     * End of Context switching code
     */
Exit
    CPSIE       I                                                /* Enable interrupts */
    ISB

    BX       LR                                                  /* return from exception, restoring {R0 - R3, R12, LR, PC, PSR} */

    ALIGN 4
}

/**
 * @brief ARM core trigger the first thread to run.
 */
__asm void port_run_theFirstThread(_u32_t sp)
{
    /**
     * initialize R4-R11 from context frame using passed SP
     */
#if ( FPU_ENABLED )
    LDMIA   R0!, {R2 - R11}                                     /* Context includes EXC_RETURN and CONTROL {R4 - R11} */
#else
    LDMIA   R0!, {R4 - R11}                                     /* Context {R4 - R11} */
#endif

    /* MOV     R2, #0xFFFFFFFD */

    MSR     PSP, R0                                             /* load PSP with what is now the current SP value */

    MOV     R1, #3                                              /* set the CONTROL[SPSEL] bit to start using PSP (no FPU active, unpriviledged) */
    MSR     CONTROL, R1
    ISB

    /* STR     R2, [LR] */                                      /* Set EXC_RETURN */
    MOV     LR, #0xFFFFFFFD
    BX      LR                                                  /* return to caller */

    ALIGN 4
}

