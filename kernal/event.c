/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 **/

#include "kernal.h"
#include "timer.h"
#include "event.h"
#include "postcode.h"
#include "trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Local unique postcode.
 */
#define _PC_CMPT_FAILED PC_FAILED(PC_CMPT_EVENT_6)

/**
 * The local function lists for current file internal use.
 */
static u32_t _event_init_privilege_routine(arguments_t *pArgs);
static u32_t _event_set_privilege_routine(arguments_t *pArgs);
static u32_t _event_wait_privilege_routine(arguments_t *pArgs);

static void _event_schedule(os_id_t id);

/**
 * @brief Get the event context based on provided unique id.
 *
 * @param id The event unique id.
 *
 * @return The pointer of the current unique id event context.
 */
static event_context_t *_event_object_contextGet(os_id_t id)
{
    return (event_context_t *)(_impl_kernal_member_unified_id_toContainerAddress(id));
}

/**
 * @brief Get the init event list head.
 *
 * @return The value of the init list head.
 */
static list_t *_event_list_initHeadGet(void)
{
    return (list_t *)_impl_kernal_member_list_get(KERNAL_MEMBER_EVENT, KERNAL_MEMBER_LIST_EVENT_INIT);
}

/**
 * @brief Get the active event list head.
 *
 * @return The value of the active list head.
 */
static list_t *_event_list_activeHeadGet(void)
{
    return (list_t *)_impl_kernal_member_list_get(KERNAL_MEMBER_EVENT, KERNAL_MEMBER_LIST_EVENT_ACTIVE);
}

/**
 * @brief Pick up a highest priority thread that blocking by the event pending list.
 *
 * @param The event unique id.
 *
 * @return The highest blocking thread head.
 */
static list_t *_event_list_blockingHeadGet(os_id_t id)
{
    event_context_t *pCurEvent = _event_object_contextGet(id);

    return (list_t *)((pCurEvent) ? (&pCurEvent->blockingThreadHead) : (NULL));
}

/**
 * @brief Push one event context into init list.
 *
 * @param pCurHead The pointer of the timer linker head.
 */
static void _event_list_transfer_toInit(linker_head_t *pCurHead)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToInitList = (list_t *)_event_list_initHeadGet();
    linker_list_transaction_common(&pCurHead->linker, pToInitList, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Push one event context into active list.
 *
 * @param pCurHead The pointer of the timer linker head.
 */
static void _event_list_transfer_toActive(linker_head_t *pCurHead)
{
    ENTER_CRITICAL_SECTION();

    list_t *pToActiveList = (list_t *)_event_list_activeHeadGet();
    linker_list_transaction_common(&pCurHead->linker, pToActiveList, LIST_TAIL);

    EXIT_CRITICAL_SECTION();
}

/**
 * @brief Check if the event unique id if is's invalid.
 *
 * @param id The provided unique id.
 *
 * @return The true is invalid, otherwise is valid.
 */
static b_t _event_id_isInvalid(u32_t id)
{
    return _impl_kernal_member_unified_id_isInvalid(KERNAL_MEMBER_EVENT, id);
}

/**
 * @brief Check if the event object if is's initialized.
 *
 * @param id The provided unique id.
 *
 * @return The true is initialized, otherwise is uninitialized.
 */
static b_t _event_object_isInit(u32_t id)
{
    event_context_t *pCurEvent = _event_object_contextGet(id);

    return ((pCurEvent) ? (((pCurEvent->head.linker.pList) ? (TRUE) : (FALSE))) : FALSE);
}

/**
 * @brief The event timeout callback fucntion.
 *
 * @param id The event unique id.
 */
static void _event_callback_fromTimeOut(os_id_t id)
{
    _impl_kernal_thread_entry_trigger(_impl_kernal_member_unified_id_timerToThread(id), id, PC_SC_TIMEOUT, _event_schedule);
}

/**
 * @brief Convert the internal os id to kernal member number.
 *
 * @param id The provided unique id.
 *
 * @return The value of member number.
 */
u32_t _impl_event_os_id_to_number(os_id_t id)
{
    if (_event_id_isInvalid(id)) {
        return 0u;
    }

    return (u32_t)((id - _impl_kernal_member_id_toUnifiedIdStart(KERNAL_MEMBER_EVENT)) / sizeof(event_context_t));
}

/**
 * @brief Initialize a new event.
 *
 * @param edgeMask specific the event desired condition of edge or level.
 * @param clearMask automatically clear the set events.
 * @param pName The event name.
 *
 * @return The event unique id.
 */
os_id_t _impl_event_init(u32_t edgeMask, u32_t clrDisMask, const char_t *pName)
{
    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)edgeMask},
        [1] = {.u32_val = (u32_t)clrDisMask},
        [2] = {.pch_val = (const char_t *)pName},
    };

    return _impl_kernal_privilege_invoke((const void *)_event_init_privilege_routine, arguments);
}

u32p_t _impl_event_wait_callfunc_register(pEvent_callbackFunc_t pCallFun)
{
    return 0u;
}

/**
 * @brief Set/clear/toggle a event bits.
 *
 * @param id The event unique id.
 * @param set The event value bits set.
 * @param clear The event value bits clear.
 * @param toggle The event value bits toggle.
 *
 * @return The result of the operation.
 */
u32p_t _impl_event_set(os_id_t id, u32_t set, u32_t clear, u32_t toggle)
{
    if (_event_id_isInvalid(id)) {
        return _PC_CMPT_FAILED;
    }

    if (!_event_object_isInit(id)) {
        return _PC_CMPT_FAILED;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)id},
        [1] = {.u32_val = (u32_t)set},
        [2] = {.u32_val = (u32_t)clear},
        [3] = {.u32_val = (u32_t)toggle},
    };

    return _impl_kernal_privilege_invoke((const void *)_event_set_privilege_routine, arguments);
}

/**
 * @brief Wait a target event.
 *
 * @param id The event unique id.
 * @param pEvtData The pointer of event value.
 * @param desired_val If the desired is not zero, All changed bits seen can wake up the thread to handle event.
 * @param listen_mask Current thread listen which bits in the event.
 * @param group_mask To define a group event.
 * @param timeout_ms The event wait timeout setting.
 *
 * @return The result of the operation.
 */
u32p_t _impl_event_wait(os_id_t id, os_evt_val_t *pEvtData, u32_t desired_val, u32_t listen_mask, u32_t group_mask, u32_t timeout_ms)
{
    if (_event_id_isInvalid(id)) {
        return _PC_CMPT_FAILED;
    }

    if (!_event_object_isInit(id)) {
        return _PC_CMPT_FAILED;
    }

    if (!pEvtData) {
        return _PC_CMPT_FAILED;
    }

    if (!timeout_ms) {
        return _PC_CMPT_FAILED;
    }

    if (!_impl_kernal_isInThreadMode()) {
        return _PC_CMPT_FAILED;
    }

    arguments_t arguments[] = {
        [0] = {.u32_val = (u32_t)id},          [1] = {.pv_val = (void *)pEvtData},   [2] = {.u32_val = (u32_t)desired_val},
        [3] = {.u32_val = (u32_t)listen_mask}, [4] = {.u32_val = (u32_t)group_mask}, [5] = {.u32_val = (u32_t)timeout_ms},
    };

    u32p_t postcode = _impl_kernal_privilege_invoke((const void *)_event_wait_privilege_routine, arguments);

    ENTER_CRITICAL_SECTION();

    if (PC_IOK(postcode)) {
        thread_context_t *pCurThread = (thread_context_t *)_impl_kernal_thread_runContextGet();
        postcode = (u32p_t)_impl_kernal_schedule_entry_result_take((action_schedule_t *)&pCurThread->schedule);
    }

    if (PC_IOK(postcode) && (postcode != PC_SC_TIMEOUT)) {
        postcode = PC_SC_SUCCESS;
    }

    EXIT_CRITICAL_SECTION();
    return postcode;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _event_init_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    u32_t edgeMask = (u32_t)(pArgs[0].u32_val);
    u32_t clrDisMask = (u32_t)(pArgs[1].u32_val);
    const char_t *pName = (const char_t *)(pArgs[2].pch_val);
    u32_t endAddr = 0u;
    event_context_t *pCurEvent = NULL;

    pCurEvent = (event_context_t *)_impl_kernal_member_id_toContainerStartAddress(KERNAL_MEMBER_EVENT);
    endAddr = (u32_t)_impl_kernal_member_id_toContainerEndAddress(KERNAL_MEMBER_EVENT);
    do {
        os_id_t id = _impl_kernal_member_containerAddress_toUnifiedid((u32_t)pCurEvent);
        if (_event_id_isInvalid(id)) {
            break;
        }

        if (_event_object_isInit(id)) {
            continue;
        }

        _memset((char_t *)pCurEvent, 0x0u, sizeof(event_context_t));
        pCurEvent->head.id = id;
        pCurEvent->head.pName = pName;

        pCurEvent->value = 0u;
        pCurEvent->edgeMask = edgeMask;
        pCurEvent->clearMask = ~clrDisMask;
        pCurEvent->call.pCallbackFunc = NULL;

        _event_list_transfer_toInit((linker_head_t *)&pCurEvent->head);

        EXIT_CRITICAL_SECTION();
        return id;

    } while ((u32_t)++pCurEvent < endAddr);

    EXIT_CRITICAL_SECTION();
    return OS_INVALID_ID;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _event_set_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    os_id_t id = (os_id_t)pArgs[0].u32_val;
    u32_t set = (u32_t)pArgs[1].u32_val;
    u32_t clear = (u32_t)pArgs[2].u32_val;
    u32_t toggle = (u32_t)pArgs[3].u32_val;

    u32p_t postcode = PC_SC_SUCCESS;
    event_context_t *pCurEvent = NULL;
    u32_t value = 0u;
    u32_t defer = 0u;
    u32_t report = 0u;
    u32_t reported = 0u;

    pCurEvent = _event_object_contextGet(id);

    value = pCurEvent->value;
    /* clear bits */
    value &= ~clear;
    /* set bits */
    value |= set;
    /* toggle bits */
    value ^= toggle;

    /* Calculate defer bits */
    defer = pCurEvent->defer;
    defer |= (u32_t)(value ^ pCurEvent->value);

    /* Calculate triggered report bits */
    report = (u32_t)(defer & pCurEvent->edgeMask);     // Edge trigger
    report |= (u32_t)(value & (~pCurEvent->edgeMask)); // Level trigger

    list_iterator_t it = {0u};
    list_iterator_init(&it, _event_list_blockingHeadGet(id));
    thread_context_t *pCurThread = (thread_context_t *)list_iterator_next(&it);
    while (pCurThread) {
        u32_t unreported = ~(report ^ pCurThread->event.desired) & pCurThread->event.listen;

        if (unreported) {
            pCurThread->event.pEvtVal->value |= unreported;
            reported |= unreported;

            if (pCurThread->event.group) { // Group event
                if (pCurThread->event.group == (pCurThread->event.pEvtVal->value & pCurThread->event.group)) {
                    _impl_kernal_thread_entry_trigger(pCurThread->head.id, id, PC_SC_SUCCESS, _event_schedule);
                }
            } else {
                if (pCurThread->event.pEvtVal->value) { // Single event
                    _impl_kernal_thread_entry_trigger(pCurThread->head.id, id, PC_SC_SUCCESS, _event_schedule);
                }
            }
        }

        if (PC_IER(postcode)) {
            break;
        }
        pCurThread = (thread_context_t *)list_iterator_next(&it);
    }

    pCurEvent->value = value;
    pCurEvent->value &= ~(reported & pCurEvent->clearMask); // Clear reported value

    pCurEvent->defer = defer;
    pCurEvent->defer &= ~reported; // Clear reported defer

    EXIT_CRITICAL_SECTION();
    return postcode;
}

/**
 * @brief It's sub-routine running at privilege mode.
 *
 * @param pArgs The function argument packages.
 *
 * @return The result of privilege routine.
 */
static u32_t _event_wait_privilege_routine(arguments_t *pArgs)
{
    ENTER_CRITICAL_SECTION();

    os_id_t id = (os_id_t)pArgs[0].u32_val;
    os_evt_val_t *pEvtData = (os_evt_val_t *)pArgs[1].pv_val;
    u32_t desired = (u32_t)pArgs[2].u32_val;
    u32_t listen = (u32_t)pArgs[3].u32_val;
    u32_t group = (u32_t)pArgs[4].u32_val;
    u32_t timeout_ms = (u32_t)pArgs[5].u32_val;
    thread_context_t *pCurThread = NULL;
    event_context_t *pCurEvent = NULL;
    u32_t report = 0u;
    u32_t reported = 0u;
    u32p_t postcode = PC_SC_SUCCESS;

    pCurEvent = _event_object_contextGet(id);
    pCurThread = _impl_kernal_thread_runContextGet();
    pCurThread->event.listen = listen;
    pCurThread->event.desired = desired;
    pCurThread->event.group = group;
    pCurThread->event.pEvtVal = pEvtData;

    report = (u32_t)(pCurEvent->defer & pCurEvent->edgeMask);     // Edge trigger
    report |= (u32_t)(pCurEvent->value & (~pCurEvent->edgeMask)); // Level trigger

    reported = ~(report ^ desired) & listen;
    if (reported) {
        pCurThread->event.pEvtVal->value = reported;
        pCurEvent->defer &= ~reported;                          // Clear reported defer
        pCurEvent->value &= ~(reported & pCurEvent->clearMask); // Clear reported value

        if (!group) {
            EXIT_CRITICAL_SECTION();
            return postcode;
        }

        if (group == (reported & group)) {
            EXIT_CRITICAL_SECTION();
            return postcode;
        }
    }

    postcode =
        _impl_kernal_thread_exit_trigger(pCurThread->head.id, id, _event_list_blockingHeadGet(id), timeout_ms, _event_callback_fromTimeOut);

    EXIT_CRITICAL_SECTION();
    return postcode;
}

/**
 * @brief The event schedule routine execute the the pendsv context.
 *
 * @param id The unique id of the entry thread.
 */
static void _event_schedule(os_id_t id)
{
    thread_context_t *pEntryThread = (thread_context_t *)(_impl_kernal_member_unified_id_toContainerAddress(id));
    thread_entry_t *pEntry = NULL;
    b_t isAvail = FALSE;

    if (_impl_kernal_member_unified_id_toId(pEntryThread->schedule.hold) != KERNAL_MEMBER_EVENT) {
        pEntryThread->schedule.entry.result = _PC_CMPT_FAILED;
        return;
    }

    if ((pEntryThread->schedule.entry.result != PC_SC_SUCCESS) && (pEntryThread->schedule.entry.result != PC_SC_TIMEOUT)) {
        return;
    }

    pEntry = &pEntryThread->schedule.entry;
    if (!_impl_timer_status_isBusy(_impl_kernal_member_unified_id_threadToTimer(pEntryThread->head.id))) {
        if (_impl_kernal_member_unified_id_toId(pEntry->release) == KERNAL_MEMBER_TIMER_INTERNAL) {
            pEntry->result = PC_SC_TIMEOUT;
        } else {
            isAvail = true;
        }
    } else if (_impl_kernal_member_unified_id_toId(pEntry->release) == KERNAL_MEMBER_EVENT) {
        _impl_timer_stop(_impl_kernal_member_unified_id_threadToTimer(pEntryThread->head.id));
        isAvail = true;
    } else {
        pEntry->result = _PC_CMPT_FAILED;
    }

    /* Auto clear user configuration */
    pEntryThread->event.listen = 0u;
    pEntryThread->event.desired = 0u;

    if (isAvail) {
        pEntry->result = PC_SC_SUCCESS;
    }
}

/**
 * @brief Get event snapshot informations.
 *
 * @param instance The event instance number.
 * @param pMsgs The kernal snapshot information pointer.
 *
 * @return TRUE: Operation pass, FALSE: Operation failed.
 */
b_t _impl_trace_event_snapshot(u32_t instance, kernal_snapshot_t *pMsgs)
{
#if defined KTRACE
    event_context_t *pCurEvent = NULL;
    u32_t offset = 0u;
    os_id_t id = OS_INVALID_ID;

    ENTER_CRITICAL_SECTION();

    offset = sizeof(event_context_t) * instance;
    pCurEvent = (event_context_t *)(_impl_kernal_member_id_toContainerStartAddress(KERNAL_MEMBER_EVENT) + offset);
    id = _impl_kernal_member_containerAddress_toUnifiedid((u32_t)pCurEvent);
    _memset((u8_t *)pMsgs, 0x0u, sizeof(kernal_snapshot_t));

    if (_event_id_isInvalid(id)) {
        EXIT_CRITICAL_SECTION();
        return FALSE;
    }

    if (pCurEvent->head.linker.pList == _event_list_activeHeadGet()) {
        pMsgs->pState = "init";
    } else if (pCurEvent->head.linker.pList) {
        pMsgs->pState = "*";
    } else {
        pMsgs->pState = "unused";

        EXIT_CRITICAL_SECTION();
        return FALSE;
    }

    pMsgs->id = pCurEvent->head.id;
    pMsgs->pName = pCurEvent->head.pName;

    pMsgs->event.set = pCurEvent->value;
    pMsgs->event.edge = pCurEvent->edgeMask;
    pMsgs->event.wait_list = pCurEvent->blockingThreadHead;

    EXIT_CRITICAL_SECTION();
    return TRUE;
#else
    return FALSE;
#endif
}

#ifdef __cplusplus
}
#endif
