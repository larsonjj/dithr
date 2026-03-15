/**
 * \file            event.h
 * \brief           Event bus — on, off, once, emit with JS handler support
 */

#ifndef MVN_EVENT_H
#define MVN_EVENT_H

#include "console.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MVN_EVENT_MAX_HANDLERS 128
#define MVN_EVENT_MAX_QUEUED   64
#define MVN_EVENT_NAME_LEN     48

/**
 * \brief           A single registered event handler
 */
typedef struct mvn_event_handler {
    char    name[MVN_EVENT_NAME_LEN];
    JSValue callback; /**< JS function (ref-counted) */
    int32_t handle;   /**< Unique handle for off() */
    bool    once;     /**< Auto-remove after first fire */
    bool    active;   /**< Slot in use */
} mvn_event_handler_t;

/**
 * \brief           A queued event to be dispatched during flush
 */
typedef struct mvn_queued_event {
    char    name[MVN_EVENT_NAME_LEN];
    JSValue payload; /**< JS value payload (ref-counted) */
} mvn_queued_event_t;

/**
 * \brief           Event bus state
 */
struct mvn_event_bus {
    mvn_event_handler_t handlers[MVN_EVENT_MAX_HANDLERS];
    mvn_queued_event_t  queue[MVN_EVENT_MAX_QUEUED];
    int32_t             queue_count;
    int32_t             next_handle;
    JSContext *         ctx;
};

mvn_event_bus_t *mvn_event_create(JSContext *ctx);
void             mvn_event_destroy(mvn_event_bus_t *bus);

/**
 * \brief           Register a handler, returns a unique handle
 */
int32_t mvn_event_on(mvn_event_bus_t *bus, const char *name, JSValue callback);

/**
 * \brief           Register a one-shot handler
 */
int32_t mvn_event_once(mvn_event_bus_t *bus, const char *name, JSValue callback);

/**
 * \brief           Remove a handler by its handle
 */
void mvn_event_off(mvn_event_bus_t *bus, int32_t handle);

/**
 * \brief           Queue an event for dispatch during flush
 */
void mvn_event_emit(mvn_event_bus_t *bus, const char *name, JSValue payload);

/**
 * \brief           Dispatch all queued events to registered handlers
 */
void mvn_event_flush(mvn_event_bus_t *bus);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_EVENT_H */
