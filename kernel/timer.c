/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/
#include "kernel.h"
#include "clock_tick.h"
#include "timer.h"
#include "postcode.h"
#include "trace.h"
#include "init.h"

/**
 * Local unique postcode.
 */
#define PC_EOR PC_IER(PC_OS_CMPT_TIMER_8)

/**
 * Data structure for location timer
 */
typedef struct {
    /* The last load count value */
    u64_t system_us;

    /* The clock time total count value */
    u32_t remaining_us;

    list_t tt_wait_list;

    list_t tt_pend_list;

    list_t tt_idle_list;

    list_t callback_list;
} _timer_resource_t;

/**
 * Local timer resource
 */
_timer_resource_t g_timer_rsc = {0u};

/**
 * @brief Check if the timer unique id if is's invalid.
 *
 * @param id The provided unique id.
 *
 * @return The true is invalid, otherwise is valid.
 */
static b_t _timer_context_isInvalid(timer_context_t *pCurTimer)
{
    u32_t start, end;
    INIT_SECTION_FIRST(INIT_SECTION_OS_TIMER_LIST, start);
    INIT_SECTION_LAST(INIT_SECTION_OS_TIMER_LIST, end);

    return ((u32_t)pCurTimer < start || (u32_t)pCurTimer >= end) ? true : false;
}

/**
 * @brief Check if the timer object if is's initialized.
 *
 * @param id The provided unique id.
 *
 * @return The true is initialized, otherwise is uninitialized.
 */
static b_t _timer_context_isInit(timer_context_t *pCurTimer)
{
    return (pCurTimer) ? (((pCurTimer->head.cs) ? (true) : (false))) : false;
}

/**
 * @brief Compare the customization value between the current and extract thread.
 *
 * @param pCurNode The pointer of the current thread node.
 * @param pExtractNode The pointer of the extract thread node.
 *
 * @return The false value indicates it is right position and it can kiil the loop routine,
 *         otherwise is not a correct position.
 */
static b_t _timeout_node_order_compare_condition(list_node_t *pCurNode, list_node_t *pExtractNode)
{
    struct expired_time *pCurTime = (struct expired_time *)pCurNode;
    struct expired_time *pExtractTime = (struct expired_time *)pExtractNode;

    if ((!pCurTime) || (!pExtractTime)) {
        /* no available timer */
        return false;
    }

    if (pCurTime->duration_us >= pExtractTime->duration_us) {
        pCurTime->duration_us -= pExtractTime->duration_us;
        return true;
    } else {
        pExtractTime->duration_us -= pCurTime->duration_us;
        return false;
    }
}

/**
 * @brief Push one timer context into waiting list,
 *        but it has to compared the existed duration time, and calculate the divided time.
 *
 * @param pCurHead The pointer of the timer linker head.
 */
static void _timeout_transfer_toWaitList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToTimeoutList = (list_t *)&g_timer_rsc.tt_wait_list;
    linker_list_transaction_specific(pLinker, pToTimeoutList, _timeout_node_order_compare_condition);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Push one timer context into waiting list,
 *        but it has to compared the existed duration time, and calculate the divided time.
 *
 * @param pCurHead The pointer of the timer linker head.
 */
static void _timeout_transfer_toPendList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToTimeoutList = (list_t *)&g_timer_rsc.tt_pend_list;
    linker_list_transaction_common(pLinker, pToTimeoutList, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Push one timer context into waiting list,
 *        but it has to compared the existed duration time, and calculate the divided time.
 *
 * @param pCurHead The pointer of the timer linker head.
 */
static void _timeout_transfer_toIdleList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToTimeoutList = (list_t *)&g_timer_rsc.tt_idle_list;
    linker_list_transaction_common(pLinker, pToTimeoutList, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Push one timer context into uninitialized status.
 *
 * Push one timer context into uninitialized status.
 *
 * @param pCurHead The pointer of the timer linker head.
 *
 * @retval NONE .
 */
static void _timeout_transfer_toNoInitList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    linker_list_transaction_common(pLinker, NULL, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Remove a list node from the waiting list and push the duration time into the next node.
 *
 * @param The node linker head pointer.
 */
static void _timeout_remove_fromWaitList(linker_t *pLinker)
{
    ENTER_CRITICAL_SECTION();

    struct expired_time *pCurTime = (struct expired_time *)&pLinker->node;
    struct expired_time *pNext = (struct expired_time *)pCurTime->linker.node.pNext;

    if (pNext) {
        pNext->duration_us += pCurTime->duration_us;
    }
    pCurTime->duration_us = 0u;

    EXIT_CRITICAL_SECTION();
}

static void _timeout_schedule(void)
{
    ENTER_CRITICAL_SECTION();

    struct expired_time *pCurExpired = (struct expired_time *)g_timer_rsc.tt_wait_list.pHead;
    if (pCurExpired) {
        clock_time_interval_set(pCurExpired->duration_us);
    } else {
        clock_time_interval_set(OS_TIME_FOREVER_VAL);
    }

    EXIT_CRITICAL_SECTION();
}

void timer_callback_fromTimeOut(void *pNode)
{
    timer_context_t *pCurTimer = (timer_context_t *)CONTAINEROF(pNode, timer_context_t, expire);
    struct expired_time *pExpired = (struct expired_time *)&pCurTimer->expire;

    if (pCurTimer->control == TIMER_CTRL_CYCLE_VAL) {
        u64_t timeout_us = pCurTimer->timeout_ms * 1000u;
        u64_t elapsed_us = g_timer_rsc.system_us - pExpired->duration_us;

        while (elapsed_us >= timeout_us) {
            elapsed_us -= timeout_us;
        }
        pExpired->duration_us = timeout_us - elapsed_us;
        _timeout_transfer_toWaitList((linker_t *)&pExpired->linker);
    } else if (pCurTimer->control == TIMER_CTRL_ONCE_VAL) {
        _timeout_transfer_toIdleList((linker_t *)&pExpired->linker);
    } else if (pCurTimer->control == TIMER_CTRL_TEMPORARY_VAL) {
        _timeout_transfer_toNoInitList((linker_t *)&pExpired->linker);
        os_memset((u8_t *)pCurTimer, 0u, sizeof(timer_context_t));
    }

    list_t *pCallback_list = (list_t *)&g_timer_rsc.callback_list;
    if (!list_node_isExisted(pCallback_list, &pCurTimer->call.node)) {
        list_node_push((list_t *)&g_timer_rsc.callback_list, &pCurTimer->call.node, LIST_HEAD);
    }
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static i32p_t _timer_schedule_request_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    _timeout_schedule();

    EXIT_CRITICAL_SECTION();
    return 0;
}

/**
 * @brief kernel RTOS request to update new schedule.
 *
 * @return The result of timer schedule request.
 */
static i32p_t _timer_schedule(void)
{
    return kernel_privilege_invoke((const void *)_timer_schedule_request_privilege_routine, NULL);
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _timer_init_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    pTimer_callbackFunc_t pCallFun = (pTimer_callbackFunc_t)(pArgs[0].ptr_val);
    const char_t *pName = (const char_t *)pArgs[1].pch_val;

    INIT_SECTION_FOREACH(INIT_SECTION_OS_TIMER_LIST, timer_context_t, pCurTimer)
    {
        if (_timer_context_isInvalid(pCurTimer)) {
            break;
        }

        if (_timer_context_isInit(pCurTimer)) {
            continue;
        }

        os_memset((char_t *)pCurTimer, 0x0u, sizeof(timer_context_t));
        pCurTimer->head.cs = CS_INITED;
        pCurTimer->head.pName = pName;
        pCurTimer->call.pTimerCallEntry = pCallFun;
        timeout_init(&pCurTimer->expire, timer_callback_fromTimeOut);

        EXIT_CRITICAL_SECTION();
        return (u32_t)pCurTimer;
    }

    EXIT_CRITICAL_SECTION();
    return 0u;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _timer_automatic_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    pTimer_callbackFunc_t pCallFun = (pTimer_callbackFunc_t)(pArgs[0].ptr_val);
    const char_t *pName = (const char_t *)pArgs[1].pch_val;

    INIT_SECTION_FOREACH(INIT_SECTION_OS_TIMER_LIST, timer_context_t, pCurTimer)
    {
        if (_timer_context_isInvalid(pCurTimer)) {
            break;
        }

        if (_timer_context_isInit(pCurTimer)) {
            continue;
        }

        os_memset((char_t *)pCurTimer, 0x0u, sizeof(timer_context_t));
        pCurTimer->head.cs = CS_INITED;
        pCurTimer->head.pName = pName;
        pCurTimer->control = TIMER_CTRL_TEMPORARY_VAL;
        pCurTimer->call.pTimerCallEntry = pCallFun;
        timeout_init(&pCurTimer->expire, timer_callback_fromTimeOut);

        EXIT_CRITICAL_SECTION();
        return (u32_t)pCurTimer;
    }

    EXIT_CRITICAL_SECTION();
    return 0u;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _timer_start_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    timer_context_t *pCurTimer = (timer_context_t *)pArgs[0].u32_val;
    u8_t ctrl = (u8_t)pArgs[1].u8_val;
    u32_t timeout_ms = (u32_t)pArgs[2].u32_val;

    pCurTimer->timeout_ms = timeout_ms;
    pCurTimer->control = ctrl;
    timeout_set(&pCurTimer->expire, timeout_ms, true);

    EXIT_CRITICAL_SECTION();
    return 0;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _timer_stop_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    timer_context_t *pCurTimer = (timer_context_t *)pArgs[0].u32_val;
    timeout_remove(&pCurTimer->expire, true);

    EXIT_CRITICAL_SECTION();
    return 0;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _timer_total_system_ms_get_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    UNUSED_MSG(pArgs);

    u64_t us = (!g_timer_rsc.remaining_us) ? (clock_time_elapsed_get()) : (0u);

    us += g_timer_rsc.system_us;

    u32_t ms = ((us / 1000u) & 0xFFFFFFFFu);

    EXIT_CRITICAL_SECTION();
    return ms;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _timer_total_system_us_get_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    UNUSED_MSG(pArgs);

    u32_t us = (u32_t)((!g_timer_rsc.remaining_us) ? (clock_time_elapsed_get()) : (0u));

    us += g_timer_rsc.system_us;

    EXIT_CRITICAL_SECTION();
    return us;
}

/**
 * @brief Initialize a new timer, or allocate a temporary timer to run.
 *
 * @param pCallFun The timer entry function pointer.
 * @param pName The timer's name, it supported NULL pointer.
 *
 * @return The value of the timer unique id.
 */
u32_t _impl_timer_init(pTimer_callbackFunc_t pCallFun, const char_t *pName)
{
    arguments_t arguments[] = {
        [0] = {.ptr_val = (const void *)pCallFun},
        [1] = {.pch_val = (const void *)pName},
    };

    return kernel_privilege_invoke((const void *)_timer_init_privilege_routine, arguments);
}

/**
 * @brief Allocate a temporary timer to run.
 *
 * @param pCallFun The timer entry function pointer.
 * @param pName The timer's name, it supported NULL pointer.
 *
 * @return The value of the timer unique id.
 */
u32_t _impl_timer_automatic(pTimer_callbackFunc_t pCallFun, const char_t *pName)
{
    arguments_t arguments[] = {
        [0] = {.ptr_val = (const void *)pCallFun},
        [1] = {.pch_val = (const void *)pName},
    };

    return kernel_privilege_invoke((const void *)_timer_automatic_privilege_routine, arguments);
}

/**
 * @brief Timer starts operation, be careful if the timer's last time isn't expired or be handled,
 *        the newer start will override it.
 *
 * @param ctx The timer unique id.
 * @param control It defines the timer running mode.
 * @param timeout_ms The timer expired time.
 *
 * @return The result of timer start operation.
 */
i32p_t _impl_timer_start(u32_t ctx, u8_t control, u32_t timeout_ms)
{
    timer_context_t *pCtx = (timer_context_t *)ctx;
    if (_timer_context_isInvalid(pCtx)) {
        return PC_EOR;
    }

    if (!_timer_context_isInit(pCtx)) {
        return PC_EOR;
    }

    if (!timeout_ms) {
        return PC_EOR;
    }

    if ((control != TIMER_CTRL_ONCE_VAL) && (control != TIMER_CTRL_CYCLE_VAL)) {
        return PC_EOR;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)ctx},
        [1] = {.u8_val = (u8_t)control},
        [2] = {.u32_val = (u32_t)timeout_ms},
    };

    return kernel_privilege_invoke((const void *)_timer_start_privilege_routine, arguments);
}

/**
 * @brief timer stops operation.
 *
 * @param ctx The timer unique id.
 *
 * @return The result of timer stop operation.
 */
i32p_t _impl_timer_stop(u32_t ctx)
{
    timer_context_t *pCtx = (timer_context_t *)ctx;
    if (_timer_context_isInvalid(pCtx)) {
        return PC_EOR;
    }

    if (!_timer_context_isInit(pCtx)) {
        return PC_EOR;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)ctx},
    };

    return kernel_privilege_invoke((const void *)_timer_stop_privilege_routine, arguments);
}

/**
 * @brief Check the timer to confirm if it's already scheduled in the waiting list.
 *
 * @param ctx The timer unique id.
 *
 * @return The true result indicates time busy, otherwise is free status.
 */
b_t _impl_timer_busy(u32_t ctx)
{
    timer_context_t *pCtx = (timer_context_t *)ctx;
    if (_timer_context_isInvalid(pCtx)) {
        return false;
    }

    if (!_timer_context_isInit(pCtx)) {
        return false;
    }

    ENTER_CRITICAL_SECTION();
    timer_context_t *pCurTimer = (timer_context_t *)ctx;
    b_t isBusy = (pCurTimer->expire.linker.pList == (list_t *)&g_timer_rsc.tt_wait_list) ? true : false;

    EXIT_CRITICAL_SECTION();
    return isBusy;
}

/**
 * @brief Get the kernel RTOS system time (ms).
 *
 * @return The value of the total system time (ms).
 */
u32_t _impl_timer_total_system_ms_get(void)
{
    return kernel_privilege_invoke((const void *)_timer_total_system_ms_get_privilege_routine, NULL);
}

/**
 * @brief Get the kernel RTOS system time (us).
 *
 * @return The value of the total system time (us).
 */
u32_t _impl_timer_total_system_us_get(void)
{
    return kernel_privilege_invoke((const void *)_timer_total_system_us_get_privilege_routine, NULL);
}

/**
 * @brief Get the kernel RTOS system time (ms).
 *
 * @return The value of the total system time (ms).
 */
u32_t timer_total_system_ms_get(void)
{
    return _impl_timer_total_system_ms_get();
}

/**
 * @brief Get the kernel RTOS system time (us).
 *
 * @return The value of the total system time (us).
 */
u32_t timer_total_system_us_get(void)
{
    return _impl_timer_total_system_us_get();
}

/**
 * @brief kernel RTOS request to update new schedule.
 *
 * @return The result of timer schedule request.
 */
i32p_t timer_schedule(void)
{
    return _timer_schedule();
}

/**
 * @brief Timer callback function handle in the kernel thread.
 */
void timer_reamining_elapsed_handler(void)
{
    list_t *pListRunning = (list_t *)&g_timer_rsc.callback_list;

    ENTER_CRITICAL_SECTION();
    struct timer_callback *pCallFunEntry = (struct timer_callback *)list_node_pop(pListRunning, LIST_TAIL);
    EXIT_CRITICAL_SECTION();

    while (pCallFunEntry) {
        if (pCallFunEntry->pTimerCallEntry) {
            pCallFunEntry->pTimerCallEntry();
        }

        ENTER_CRITICAL_SECTION();
        pCallFunEntry = (struct timer_callback *)list_node_pop(pListRunning, LIST_TAIL);
        EXIT_CRITICAL_SECTION();
    }
}

void timeout_init(struct expired_time *pExpire, pTimeout_callbackFunc_t fun)
{
    pExpire->duration_us = 0u;
    pExpire->fn = fun;
    _timeout_transfer_toIdleList((linker_t *)pExpire);
}

void timeout_set(struct expired_time *pExpire, u32_t timeout_ms, b_t immediately)
{
    ENTER_CRITICAL_SECTION();
    b_t need = false;
    if (pExpire->linker.pList == &g_timer_rsc.tt_wait_list) {
        _timeout_remove_fromWaitList((linker_t *)&pExpire->linker);
        need = true;
    }

    if ((timeout_ms == OS_TIME_FOREVER_VAL) || (timeout_ms == 0)) {
        if (pExpire->linker.pList != &g_timer_rsc.tt_idle_list) {
            _timeout_transfer_toIdleList((linker_t *)&pExpire->linker);
        }
    } else {
        pExpire->duration_us = timeout_ms * 1000u;
        _timeout_transfer_toWaitList((linker_t *)&pExpire->linker);
        need = true;
    }

    if (need && immediately) {
        _timer_schedule();
    }
    EXIT_CRITICAL_SECTION();
}

void timeout_remove(struct expired_time *pExpire, b_t immediately)
{
    ENTER_CRITICAL_SECTION();

    b_t need = false;
    if (pExpire->linker.pList == &g_timer_rsc.tt_wait_list) {
        _timeout_remove_fromWaitList((linker_t *)&pExpire->linker);
        need = true;
    }
    _timeout_transfer_toIdleList((linker_t *)&pExpire->linker);

    if (need && immediately) {
        _timer_schedule();
    }

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief kernel RTOS handle the clock time.
 *
 * @param elapsed_us Clock time reported elapsed time.
 */
void timeout_handler(u32_t elapsed_us)
{
    ENTER_CRITICAL_SECTION();

    struct expired_time *pCurExpired = NULL;
    g_timer_rsc.remaining_us = elapsed_us;

    list_iterator_t it = {0u};
    list_t *pListWaiting = (list_t *)&g_timer_rsc.tt_wait_list;
    list_iterator_init(&it, pListWaiting);
    while (list_iterator_next_condition(&it, (void *)&pCurExpired)) {
        if (g_timer_rsc.remaining_us >= pCurExpired->duration_us) {
            g_timer_rsc.remaining_us -= pCurExpired->duration_us;
            g_timer_rsc.system_us += pCurExpired->duration_us;
            pCurExpired->duration_us = 0u;

            if (pCurExpired->fn != timer_callback_fromTimeOut) {
                pCurExpired->fn((void *)&pCurExpired->linker.node);

                _timeout_transfer_toIdleList((linker_t *)&pCurExpired->linker);
            } else {
                pCurExpired->duration_us = g_timer_rsc.system_us;
                _timeout_transfer_toPendList((linker_t *)&pCurExpired->linker);
            }
        } else {
            pCurExpired->duration_us -= g_timer_rsc.remaining_us;
            break;
        }
    }
    g_timer_rsc.system_us += g_timer_rsc.remaining_us;
    g_timer_rsc.remaining_us = 0u;

    b_t need = false;
    list_t *pListPending = (list_t *)&g_timer_rsc.tt_pend_list;
    list_iterator_init(&it, pListPending);
    while (list_iterator_next_condition(&it, (void *)&pCurExpired)) {
        if (pCurExpired->fn != NULL) {
            pCurExpired->fn((void *)&pCurExpired->linker.node);
            need = true;
        }
    }

    if (need) {
        kernel_message_notification();
    }
    _timer_schedule();

    EXIT_CRITICAL_SECTION();
}
