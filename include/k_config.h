/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/
#ifndef _K_CONFIG_H_
#define _K_CONFIG_H_

#include "os_config.h"
#include "../build_version.h"

#ifndef MALLOC_HEAP_SIZE_SUPPORTED
#define MALLOC_HEAP_SIZE_SUPPORTED (4)
#endif

#ifndef THREAD_RUNTIME_NUMBER_SUPPORTED
#define THREAD_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef SEMAPHORE_RUNTIME_NUMBER_SUPPORTED
#define SEMAPHORE_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef EVENT_RUNTIME_NUMBER_SUPPORTED
#define EVENT_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef MUTEX_RUNTIME_NUMBER_SUPPORTED
#define MUTEX_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef QUEUE_RUNTIME_NUMBER_SUPPORTED
#define QUEUE_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef TIMER_RUNTIME_NUMBER_SUPPORTED
#define TIMER_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef POOL_RUNTIME_NUMBER_SUPPORTED
#define POOL_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef PUBLISH_RUNTIME_NUMBER_SUPPORTED
#define PUBLISH_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef SUBSCRIBE_RUNTIME_NUMBER_SUPPORTED
#define SUBSCRIBE_RUNTIME_NUMBER_SUPPORTED (1u)
#endif

#ifndef PORTAL_SYSTEM_CORE_CLOCK_MHZ
#define PORTAL_SYSTEM_CORE_CLOCK_MHZ (120u)
#endif

#ifndef PORTAL_SYSTEM_CLOCK_INTERVAL_MIN_US
#define PORTAL_SYSTEM_CLOCK_INTERVAL_MIN_US (50u)
#endif

#ifndef STACK_ALIGN
#define STACK_ALIGN (8u)
#endif

#ifndef STACK_SIZE_MINIMUM
#define STACK_SIZE_MINIMUM (128u)
#endif

#ifndef STACK_SIZE_MAXIMUM
#define STACK_SIZE_MAXIMUM (0xFFFFFFu)
#endif

#ifndef THREAD_PSP_WITH_PRIVILEGED
#define THREAD_PSP_WITH_PRIVILEGED (0u)
#endif

#ifndef IDLE_THREAD_STACK_SIZE
#define IDLE_THREAD_STACK_SIZE (512u)
#endif

#ifndef KERNEL_THREAD_STACK_SIZE
#define KERNEL_THREAD_STACK_SIZE (1024u)
#endif

/* It defined the AtOS extern symbol for convenience use, but it has extra memory consumption */
#ifndef OS_API_DISABLED
#define OS_API_ENABLED (1)
#endif

/* It defined the AtOS extern symbol typedef for convenience use, but it has extra memory consumption */
#ifndef OS_TYPEDEF_DISABLED
#define OS_TYPEDEF_ENABLED (1)
#endif

#ifndef OS_ID_ENHANCEMENT_DISABLED
#define OS_ID_ENHANCEMENT_ENABLED (1)
#endif

/* Configuration of the Cortex-M Processor and Core Peripherals.
 * You should check the chip header file or datasheet to check the following declaration symbol that support ARM Cortex-M Processor and Core
 * Peripherals, and put it here. It looks like this.
 *
 * #define __SAUREGION_PRESENT                          (1u) // SAU regions present
 * #define __MPU_PRESENT                                (1u) // MPU present
 * #define __VTOR_PRESENT                               (1u) // VTOR present
 * #define __NVIC_PRIO_BITS                             (4u) // Number of Bits used for Priority Levels
 * #define __Vendor_SysTickConfig                       (0u) // Set to 1 if different SysTick Config is used
 * #define __FPU_PRESENT                                (1u) // FPU present
 * #define __DSP_PRESENT                                (1u) // DSP extension present
 *
 * Or you can manually defined it according to your using ARM Cortex M seiral chip here with the following declaration symbol.
 *
 * #define ARCH_SAUREGION_PRESENT                        (1u) // SAU regions present
 * #define ARCH_MPU_PRESENT                              (1u) // MPU present
 * #define ARCH_VTOR_PRESENT                             (1u) // VTOR present
 * #define ARCH_NVIC_PRIO_BITS                           (4u) // Number of Bits used for Priority Levels
 * #define ARCH_Vendor_SysTickConfig                     (0u) // Set to 1 if different SysTick Config is used
 * #define ARCH_FPU_PRESENT                              (1u) // FPU present
 * #define ARCH_DSP_PRESENT                              (1u) // DSP extension present
 *
 * There is no default setting, which must be set in the atos_configuration.h file.
 */

#if !defined __CC_ARM && !defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050) && !defined __ICCARM__ && !defined __GUNC__ &&          \
    !defined __TMS470__ && !defined __TASKING__ && !defined ARCH_NATIVE_GCC

#warning Not supported compiler type
#endif

#endif /* _K_CONFIG_H_ */
