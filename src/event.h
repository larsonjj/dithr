/**
 * \file            event.h
 * \brief           Event bus — on, off, once, emit with JS handler support
 */

#ifndef DTR_EVENT_H
#define DTR_EVENT_H

#include "console.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DTR_EVENT_MAX_HANDLERS 128
#define DTR_EVENT_MAX_QUEUED   64
#define DTR_EVENT_NAME_LEN     48
#define DTR_EVENT_HASH_BUCKETS 32

/**
 * \brief           A single registered event handler
 */
typedef struct dtr_event_handler {
    char    name[DTR_EVENT_NAME_LEN];
    JSValue callback;  /**< JS function (ref-counted) */
    int32_t handle;    /**< Unique handle for off() */
    int32_t hash_next; /**< Next handler in same hash bucket, or -1 */
    bool    once;      /**< Auto-remove after first fire */
    bool    active;    /**< Slot in use */
} dtr_event_handler_t;

/**
 * \brief           A queued event to be dispatched during flush
 */
typedef struct dtr_queued_event {
    char    name[DTR_EVENT_NAME_LEN];
    JSValue payload; /**< JS value payload (ref-counted) */
} dtr_queued_event_t;

/**
 * \brief           Event bus state
 */
struct dtr_event_bus {
    dtr_event_handler_t handlers[DTR_EVENT_MAX_HANDLERS];
    int32_t             hash_heads[DTR_EVENT_HASH_BUCKETS]; /**< -1 = empty */
    dtr_queued_event_t  queue[DTR_EVENT_MAX_QUEUED];
    int32_t             queue_count;
    int32_t             next_handle;
    JSContext          *ctx;
};

dtr_event_bus_t *dtr_event_create(JSContext *ctx);
void             dtr_event_destroy(dtr_event_bus_t *bus);

/**
 * \brief           Clear all handlers and queued events, update context pointer.
 *                  The bus itself stays alive — used during hot reload.
 */
void dtr_event_clear(dtr_event_bus_t *bus, JSContext *new_ctx);

/**
 * \brief           Register a handler, returns a unique handle
 */
int32_t dtr_event_on(dtr_event_bus_t *bus, const char *name, JSValue callback);

/**
 * \brief           Register a one-shot handler
 */
int32_t dtr_event_once(dtr_event_bus_t *bus, const char *name, JSValue callback);

/**
 * \brief           Remove a handler by its handle
 */
void dtr_event_off(dtr_event_bus_t *bus, int32_t handle);

/**
 * \brief           Queue an event for dispatch during flush
 */
void dtr_event_emit(dtr_event_bus_t *bus, const char *name, JSValue payload);

/**
 * \brief           Dispatch all queued events to registered handlers
 */
void dtr_event_flush(dtr_event_bus_t *bus);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_EVENT_H */
