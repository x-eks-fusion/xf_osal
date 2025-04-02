/**
 * @file xf_osal_thread.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-09-10
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "xf_osal_internal.h"

#if XF_OSAL_THREAD_IS_ENABLE

/* ==================== [Defines] =========================================== */

#ifndef uxSemaphoreGetCountFromISR
#define uxSemaphoreGetCountFromISR( xSemaphore ) uxQueueMessagesWaitingFromISR( ( QueueHandle_t ) ( xSemaphore ) )
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

#define _MAP_VALUE(oldValue, a, b, c, d) ({ \
    ((b) != (a) && (d) != (c)) ? \
        ( (((oldValue) == (a)||(oldValue) > (a)) && (oldValue) <= (b)) ? \
            ( ((oldValue) - (a)) * ((d) - (c)) / ((b) - (a)) + (c) ) : \
            oldValue ) : \
        oldValue ; \
})

#define UNMAP_PRIORITY(oldValue) (_MAP_VALUE((oldValue), Thread_Priority_Lowest, Thread_Priority_Highest, XF_OSAL_PRIORITY_LOW, XF_OSAL_PRIORITY_ISR - 1))
#define MAP_PRIORITY(oldValue) (_MAP_VALUE((oldValue), XF_OSAL_PRIORITY_LOW, XF_OSAL_PRIORITY_ISR - 1, Thread_Priority_Lowest, Thread_Priority_Highest))

/* ==================== [Global Functions] ================================== */

xf_osal_thread_t xf_osal_thread_create(xf_osal_thread_func_t func, void *argument, const xf_osal_thread_attr_t *attr)
{
    const char *name;
    uint32_t stack;
    TaskHandle_t hTask;
    UBaseType_t prio;
    int32_t mem;

    hTask = NULL;

    if ((IRQ_Context() == 0U) && (func != NULL)) {
        stack = configMINIMAL_STACK_SIZE;
        prio  = (UBaseType_t)XF_OSAL_PRIORITY_NORMOL;

        name = NULL;
        mem  = -1;

        if (attr != NULL) {
            if (attr->name != NULL) {
                name = attr->name;
            }
            if (attr->priority != XF_OSAL_PRIORITY_NONE) {
                prio = (UBaseType_t)attr->priority;
            }

            if ((prio < XF_OSAL_PRIORITY_IDLE) || (prio > XF_OSAL_PRIORITY_ISR)
                    || ((attr->attr_bits & XF_OSAL_JOINABLE) == XF_OSAL_JOINABLE)) {
                /* Invalid priority or unsupported XF_OSAL_JOINABLE attribute used */
                return (NULL);
            }

            if (attr->stack_size > 0U) {
                /* In FreeRTOS stack is not in bytes, but in sizeof(StackType_t) which is 4 on ARM ports.       */
                /* Stack size should be therefore 4 byte aligned in order to avoid division caused side effects */
                stack = attr->stack_size / sizeof(StackType_t);
            }

            if ((attr->cb_mem    != NULL) && (attr->cb_size    >= sizeof(StaticTask_t)) &&
                    (attr->stack_mem != NULL) && (attr->stack_size >  0U)) {
                /* The memory for control block and stack is provided, use static object */
                mem = 1;
            } else {
                if ((attr->cb_mem == NULL) && (attr->cb_size == 0U) && (attr->stack_mem == NULL)) {
                    /* Control block and stack memory will be allocated from the dynamic pool */
                    mem = 0;
                }
            }
        } else {
            mem = 0;
        }
        prio = MAP_PRIORITY(prio);
        if (mem == 1) {
#if (configSUPPORT_STATIC_ALLOCATION == 1)
            hTask = xTaskCreateStatic((TaskFunction_t)func, name, stack, argument, prio, (StackType_t *)attr->stack_mem,
                                      (StaticTask_t *)attr->cb_mem);
#endif
        } else {
            if (mem == 0) {
#if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
                if (xTaskCreate((TaskFunction_t)func, name, (configSTACK_DEPTH_TYPE)stack, argument, prio, &hTask) != pdPASS) {
                    hTask = NULL;
                }
#endif
            }
        }
    }
    /* Return thread ID */
    return ((xf_osal_thread_t)hTask);
}

const char *xf_osal_thread_get_name(xf_osal_thread_t thread)
{
    xf_osal_thread_t hTask = (xf_osal_thread_t)thread;
    const char *name;

    if (hTask == NULL) {
        name = NULL;
    } else {
        name = pcTaskGetName(hTask);
    }

    /* Return name as null-terminated string */
    return (name);
}

xf_osal_thread_t xf_osal_thread_get_current(void)
{
    xf_osal_thread_t thread;

    thread = (xf_osal_thread_t)xTaskGetCurrentTaskHandle();

    /* Return thread ID */
    return (thread);
}

xf_osal_state_t xf_osal_thread_get_state(xf_osal_thread_t thread)
{
    xf_osal_thread_t hTask = (xf_osal_thread_t)thread;
    xf_osal_state_t state;

    if ((IRQ_Context() != 0U) || (hTask == NULL)) {
        state = XF_OSAL_ERROR;
    } else {
        switch (eTaskGetState(hTask)) {
        case eRunning:   state = XF_OSAL_RUNNING;    break;
        case eReady:     state = XF_OSAL_READY;      break;
        case eBlocked:
        case eSuspended: state = XF_OSAL_BLOCKED;    break;
        case eDeleted:
        case eInvalid:
        default:         state = XF_OSAL_ERROR;      break;
        }
    }
    /* Return current thread state */
    return (state);
}

uint32_t xf_osal_thread_get_stack_space(xf_osal_thread_t thread)
{
    TaskHandle_t hTask = (TaskHandle_t)thread;
    uint32_t sz;

    if ((IRQ_Context() != 0U) || (hTask == NULL)) {
        sz = 0U;
    } else {
        sz = (uint32_t)(uxTaskGetStackHighWaterMark(hTask) * sizeof(StackType_t));
    }

    /* Return remaining stack space in bytes */
    return (sz);
}

xf_err_t xf_osal_thread_set_priority(xf_osal_thread_t thread, xf_osal_priority_t priority)
{
    TaskHandle_t hTask = (TaskHandle_t)thread;
    xf_err_t stat;

    if (IRQ_Context() != 0U) {
        stat = XF_ERR_ISR;
    } else if ((hTask == NULL) || (priority < XF_OSAL_PRIORITY_IDLE) || (priority > XF_OSAL_PRIORITY_ISR)) {
        stat = XF_ERR_INVALID_ARG;
    } else {
        stat = XF_OK;
        priority = (xf_osal_priority_t)MAP_PRIORITY(priority);
        vTaskPrioritySet(hTask, (UBaseType_t)priority);
    }

    /* Return execution status */
    return (stat);
}

xf_osal_priority_t xf_osal_thread_get_priority(xf_osal_thread_t thread)
{
    TaskHandle_t hTask = (TaskHandle_t)thread;
    xf_osal_priority_t prio;

    if ((IRQ_Context() != 0U) || (hTask == NULL)) {
        prio = XF_OSAL_PRIORITY_ERROR;
    } else {
        prio = (xf_osal_priority_t)UNMAP_PRIORITY(((int32_t)uxTaskPriorityGet(hTask)));
    }

    /* Return current thread priority */
    return (prio);
}

xf_err_t xf_osal_thread_yield(void)
{
    xf_err_t stat;

    if (IRQ_Context() != 0U) {
        stat = XF_ERR_ISR;
    } else {
        stat = XF_OK;
        taskYIELD();
    }

    /* Return execution status */
    return (stat);
}

xf_err_t xf_osal_thread_suspend(xf_osal_thread_t thread)
{
    TaskHandle_t hTask = (TaskHandle_t)thread;
    xf_err_t stat;

    if (IRQ_Context() != 0U) {
        stat = XF_ERR_ISR;
    } else if (hTask == NULL) {
        stat = XF_ERR_INVALID_ARG;
    } else {
        stat = XF_OK;
        vTaskSuspend(hTask);
    }

    /* Return execution status */
    return (stat);
}

xf_err_t xf_osal_thread_resume(xf_osal_thread_t thread)
{
    TaskHandle_t hTask = (TaskHandle_t)thread;
    xf_err_t stat;
    eTaskState tstate;

    if (IRQ_Context() != 0U) {
        stat = XF_ERR_ISR;
    } else if (hTask == NULL) {
        stat = XF_ERR_INVALID_ARG;
    } else {
        tstate = eTaskGetState(hTask);

        if (tstate == eSuspended) {
            /* Thread is suspended */
            stat = XF_OK;
            vTaskResume(hTask);
        } else {
            /* Not suspended, might be blocked */
            if (xTaskAbortDelay(hTask) == pdPASS) {
                /* Thread was unblocked */
                stat = XF_OK;
            } else {
                /* Thread was not blocked */
                stat = XF_ERR_RESOURCE;
            }
        }
    }

    /* Return execution status */
    return (stat);
}

xf_err_t xf_osal_thread_delete(xf_osal_thread_t thread)
{
    TaskHandle_t hTask = (TaskHandle_t)thread;
    xf_err_t stat;
#ifndef USE_FreeRTOS_HEAP_1
    eTaskState tstate;

    if (IRQ_Context() != 0U) {
        stat = XF_ERR_ISR;
    } else if (hTask == NULL) {
        stat = XF_OK;
        vTaskDelete(hTask);
    } else {
        tstate = eTaskGetState(hTask);

        if (tstate != eDeleted) {
            stat = XF_OK;
            vTaskDelete(hTask);
        } else {
            stat = XF_ERR_RESOURCE;
        }
    }
#else
    stat = XF_FAIL;
#endif

    /* Return execution status */
    return (stat);
}

uint32_t xf_osal_thread_get_count(void)
{
    uint32_t count;

    if (IRQ_Context() != 0U) {
        count = 0U;
    } else {
        count = uxTaskGetNumberOfTasks();
    }

    /* Return number of active threads */
    return (count);
}

uint32_t xf_osal_thread_enumerate(xf_osal_thread_t *thread_array, uint32_t array_items)
{
    if (NULL == thread_array) {
        return xf_osal_thread_get_count();
    }

    uint32_t i, count;
    TaskStatus_t *task;

    if ((IRQ_Context() != 0U) || (thread_array == NULL) || (array_items == 0U)) {
        count = 0U;
    } else {
        vTaskSuspendAll();

        /* Allocate memory on heap to temporarily store TaskStatus_t information */
        count = uxTaskGetNumberOfTasks();
        task  = pvPortMalloc(count * sizeof(TaskStatus_t));

        if (task != NULL) {
            /* Retrieve task status information */
#if configUSE_TRACE_FACILITY
            count = uxTaskGetSystemState(task, count, NULL);
#endif

            /* Copy handles from task status array into provided thread array */
            for (i = 0U; (i < count) && (i < array_items); i++) {
                thread_array[i] = (xf_osal_thread_t)task[i].xHandle;
            }
            count = i;
        }
        (void)xTaskResumeAll();

        vPortFree(task);
    }

    /* Return number of enumerated threads */
    return (count);
}

xf_err_t xf_osal_thread_notify_set(xf_osal_thread_t thread, uint32_t flags)
{
    TaskHandle_t hTask = (TaskHandle_t)thread;
    xf_err_t err;
    BaseType_t yield;

    if ((hTask == NULL) || ((flags & THREAD_FLAGS_INVALID_BITS) != 0U)) {
        err = XF_ERR_INVALID_ARG;
    } else {
        err = XF_OK;

        if (IRQ_Context() != 0U) {
            yield = pdFALSE;

            (void)xTaskNotifyFromISR(hTask, flags, eSetBits, &yield);

            portYIELD_FROM_ISR(yield);
        } else {
            (void)xTaskNotify(hTask, flags, eSetBits);
        }
    }
    /* Return flags after setting */
    return (err);
}

xf_err_t xf_osal_thread_notify_clear(uint32_t flags)
{
    TaskHandle_t hTask;
    xf_err_t err = XF_OK;
    uint32_t rflags;

    if (IRQ_Context() != 0U) {
        err = XF_ERR_ISR;
    } else if ((flags & THREAD_FLAGS_INVALID_BITS) != 0U) {
        err = XF_ERR_INVALID_ARG;
    } else {
        hTask = xTaskGetCurrentTaskHandle();

        if (xTaskNotifyAndQuery(hTask, 0, eNoAction, &rflags) == pdPASS) {
            flags &= ~rflags;
            if (xTaskNotify(hTask, flags, eSetValueWithOverwrite) != pdPASS) {
                err = XF_FAIL;
            }
        } else {
            err = XF_FAIL;
        }
    }

    return (err);
}

uint32_t xf_osal_thread_notify_get(void)
{
    TaskHandle_t hTask;
    uint32_t flag = 0U;

    if (IRQ_Context() != 0U) {
        flag = 0U;
    } else {
        hTask = xTaskGetCurrentTaskHandle();

        if (xTaskNotifyAndQuery(hTask, 0, eNoAction, &flag) != pdPASS) {
            flag = 0U;
        }
    }

    /* Return current flags */
    return (flag);
}

xf_err_t xf_osal_thread_notify_wait(uint32_t flags, uint32_t options, uint32_t timeout)
{
    uint32_t ulNotificationValue = 0;
    uint32_t ulBitsToClearOnExit = 0;
    TickType_t xStartTime, xElapsedTime = 0;
    TickType_t xRemainingTime;
    BaseType_t xResult;

    // 参数检查
    if (flags == 0 || (options & ~(XF_OSAL_NO_CLEAR | XF_OSAL_WAIT_ANY | XF_OSAL_WAIT_ALL))) {
        return XF_ERR_INVALID_ARG;
    }

    // 如果不使用 XF_OSAL_NO_CLEAR，收到通知后应清除 flags 中的位
    if (!(options & XF_OSAL_NO_CLEAR)) {
        ulBitsToClearOnExit = flags;
    }

    // 获取当前时间用于超时计算
    xStartTime = xTaskGetTickCount();
    xRemainingTime = (timeout == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);

    // 循环等待，直到条件满足或超时
    while (xRemainingTime > 0) {
        // 等待通知
        xResult = xTaskNotifyWait(0x00, ulBitsToClearOnExit, &ulNotificationValue, xRemainingTime);

        if (xResult == pdFALSE) {
            return XF_ERR_TIMEOUT;  // 超时返回
        }
        // 根据 options 参数处理
        if (options & XF_OSAL_WAIT_ALL) {
            // 如果已经满足所有 flags
            if ((ulNotificationValue & flags) == flags) {
                return XF_OK;
            }
        } else {
            // 等待任何一个 flag 被设置
            if (ulNotificationValue & flags) {
                return XF_OK;
            }
        }

        // 计算已经过去的时间
        xElapsedTime = xTaskGetTickCount() - xStartTime;

        // 计算剩余时间
        if (xElapsedTime >= pdMS_TO_TICKS(timeout)) {
            return XF_ERR_TIMEOUT;  // 超时
        }

        xRemainingTime = pdMS_TO_TICKS(timeout) - xElapsedTime;
    }

    return XF_ERR_TIMEOUT;
}

xf_err_t xf_osal_delay(uint32_t ticks)
{
    xf_err_t stat;

    if (IRQ_Context() != 0U) {
        stat = XF_ERR_ISR;
    } else {
        stat = XF_OK;

        if (ticks != 0U) {
            vTaskDelay(ticks);
        }
    }

    /* Return execution status */
    return (stat);
}

xf_err_t xf_osal_delay_until(uint32_t ticks)
{
    TickType_t tcnt, delay;
    xf_err_t stat;

    if (IRQ_Context() != 0U) {
        stat = XF_ERR_ISR;
    } else {
        stat = XF_OK;
        tcnt = xTaskGetTickCount();

        /* Determine remaining number of ticks to delay */
        delay = (TickType_t)ticks - tcnt;

        /* Check if target tick has not expired */
        if ((delay != 0U) && (0 == (delay >> (8 * sizeof(TickType_t) - 1)))) {
            if (xTaskDelayUntil(&tcnt, delay) == pdFALSE) {
                /* Did not delay */
                stat = XF_FAIL;
            }
        } else {
            /* No delay or already expired */
            stat = XF_ERR_INVALID_ARG;
        }
    }

    /* Return execution status */
    return (stat);
}

xf_err_t xf_osal_delay_ms(uint32_t ms)
{
    return (xf_osal_delay(xf_osal_kernel_ms_to_ticks(ms)));
}

/* ==================== [Static Functions] ================================== */

#endif
