/**
 * @file xf_osal_internal.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-09-10
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_OSAL_INTERNAL_H__
#define __XF_OSAL_INTERNAL_H__

/* ==================== [Includes] ========================================== */

#include "xf_osal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

/*是否处于中断上下文中，一般和架构相关*/
#ifndef IS_IRQ_MODE
#define IS_IRQ_MODE()  (0U)
#endif

/*是否禁用了中断宏或函数，一般和架构相关*/
#ifndef IS_IRQ_MASKED
#define IS_IRQ_MASKED()  (0U)
#endif

/*设置系统服务调用（SVC）的优先级，ARM Cortex-M特有*/
#ifndef SVC_Setup
#define SVC_Setup()
#endif

/*这里设置线程最低优先级，方便后续映射到osPriority_t中*/
#ifndef Thread_Priority_Lowest
#define Thread_Priority_Lowest (0)
#endif

/*这里设置线程最高优先级，方便后续映射到osPriority_t中*/
#ifndef Thread_Priority_Highest
#define Thread_Priority_Highest (configMAX_PRIORITIES - 1)
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

__STATIC_INLINE uint32_t IRQ_Context(void)
{
    uint32_t irq;
    BaseType_t state;

    irq = 0U;

    if (IS_IRQ_MODE()) {
        /* Called from interrupt context */
        irq = 1U;
    } else {
        /* Get FreeRTOS scheduler state */
        state = xTaskGetSchedulerState();

        if (state != taskSCHEDULER_NOT_STARTED) {
            /* Scheduler was started */
            if (IS_IRQ_MASKED()) {
                /* Interrupts are masked */
                irq = 1U;
            }
        }
    }

    /* Return context, 0: thread context, 1: IRQ context */
    return (irq);
}

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __XF_OSAL_INTERNAL_H__
