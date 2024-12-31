/**
 * @file xf_osal_kernel.c
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

#if XF_OSAL_KERNEL_IS_ENABLE

/* ==================== [Defines] =========================================== */

#define KERNEL_VERSION            (((uint32_t)(tskKERNEL_VERSION_MAJOR|0xff) * 10000000UL) | \
                                   ((uint32_t)(tskKERNEL_VERSION_MINOR|0xff) *    10000UL) | \
                                   ((uint32_t)(tskKERNEL_VERSION_BUILD|0xff) *        1UL))

#define KERNEL_ID                 ("FreeRTOS " tskKERNEL_VERSION_NUMBER)

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

xf_err_t xf_osal_kernel_get_info(xf_osal_version_t *version, char *id_buf, uint32_t id_size)
{
    if (version != NULL) {
        /* Version encoding is major.minor.rev: mmnnnrrrr dec */
        version->api    = KERNEL_VERSION;
        version->kernel = KERNEL_VERSION;
    }

    if ((id_buf != NULL) && (id_size != 0U)) {
        /* Buffer for retrieving identification string is provided */
        if (id_size > sizeof(KERNEL_ID)) {
            id_size = sizeof(KERNEL_ID);
        }
        /* Copy kernel identification string into provided buffer */
        xf_memcpy(id_buf, KERNEL_ID, id_size);
    }

    /* Return execution status */
    return (XF_OK);
}

xf_osal_state_t xf_osal_kernel_get_state(void)
{
    xf_osal_state_t state;

    switch (xTaskGetSchedulerState()) {
    case taskSCHEDULER_RUNNING:
        state = XF_OSAL_RUNNING;
        break;

    case taskSCHEDULER_SUSPENDED:
        state = XF_OSAL_BLOCKED;
        break;

    case taskSCHEDULER_NOT_STARTED:
    default:
        state = XF_OSAL_INACTIVE;
        break;
    }
    return (state);
}

xf_err_t xf_osal_kernel_lock(void)
{
    xf_err_t lock = XF_OK;

    if (IRQ_Context() != 0U) {
        lock = XF_ERR_ISR;
    } else {
        switch (xTaskGetSchedulerState()) {
        case taskSCHEDULER_SUSPENDED:
            break;

        case taskSCHEDULER_RUNNING:
            vTaskSuspendAll();
            break;

        case taskSCHEDULER_NOT_STARTED:
        default:
            lock = XF_FAIL;
            break;
        }
    }

    return (lock);
}

xf_err_t xf_osal_kernel_unlock(void)
{
    xf_err_t lock = XF_OK;

    if (IRQ_Context() != 0U) {
        lock = (int32_t)XF_ERR_ISR;
    } else {
        switch (xTaskGetSchedulerState()) {
        case taskSCHEDULER_SUSPENDED:

            if (xTaskResumeAll() != pdTRUE) {
                if (xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED) {
                    lock = (int32_t)XF_FAIL;
                }
            }
            break;

        case taskSCHEDULER_RUNNING:
            break;

        case taskSCHEDULER_NOT_STARTED:
        default:
            lock = (int32_t)XF_FAIL;
            break;
        }
    }

    /* Return previous lock state */
    return (lock);
}

uint32_t xf_osal_kernel_get_tick_count(void)
{
    TickType_t ticks;

    if (IRQ_Context() != 0U) {
        ticks = xTaskGetTickCountFromISR();
    } else {
        ticks = xTaskGetTickCount();
    }

    /* Return kernel tick count */
    return (ticks);
}

uint32_t xf_osal_kernel_get_tick_freq(void)
{
    /* Return frequency in hertz */
    return (configTICK_RATE_HZ);
}

uint32_t xf_osal_kernel_ticks_to_ms(uint32_t ticks)
{
    return pdTICKS_TO_MS(ticks);
}

uint32_t xf_osal_kernel_ms_to_ticks(uint32_t ms)
{
    return pdMS_TO_TICKS(ms);
}

/* ==================== [Static Functions] ================================== */

#endif
