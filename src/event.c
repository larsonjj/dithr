/**
 * \file            event.c
 * \brief           Event bus — on/off/once/emit/flush with JS callbacks
 */

#include "event.h"

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

mvn_event_bus_t *mvn_event_create(JSContext *ctx)
{
    mvn_event_bus_t *bus;

    bus = MVN_CALLOC(1, sizeof(mvn_event_bus_t));
    if (bus == NULL) {
        return NULL;
    }

    bus->ctx = ctx;
    return bus;
}

void mvn_event_destroy(mvn_event_bus_t *bus)
{
    if (bus == NULL) {
        return;
    }

    for (int32_t idx = 0; idx < MVN_EVENT_MAX_HANDLERS; ++idx) {
        if (bus->handlers[idx].active) {
            JS_FreeValue(bus->ctx, bus->handlers[idx].callback);
        }
    }

    for (int32_t idx = 0; idx < bus->queue_count; ++idx) {
        JS_FreeValue(bus->ctx, bus->queue[idx].payload);
    }

    MVN_FREE(bus);
}

/* ------------------------------------------------------------------ */
/*  Internal: find free slot                                           */
/* ------------------------------------------------------------------ */

static int32_t prv_find_free_slot(mvn_event_bus_t *bus)
{
    for (int32_t idx = 0; idx < MVN_EVENT_MAX_HANDLERS; ++idx) {
        if (!bus->handlers[idx].active) {
            return idx;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Registration                                                       */
/* ------------------------------------------------------------------ */

int32_t mvn_event_on(mvn_event_bus_t *bus, const char *name, JSValue callback)
{
    int32_t              slot;
    mvn_event_handler_t *h;

    slot = prv_find_free_slot(bus);
    if (slot < 0) {
        SDL_Log("Event bus: max handlers reached");
        return -1;
    }

    h = &bus->handlers[slot];
    SDL_strlcpy(h->name, name, MVN_EVENT_NAME_LEN);
    h->callback = JS_DupValue(bus->ctx, callback);
    h->handle   = bus->next_handle;
    h->once     = false;
    h->active   = true;

    ++bus->next_handle;
    return h->handle;
}

int32_t mvn_event_once(mvn_event_bus_t *bus, const char *name, JSValue callback)
{
    int32_t handle;

    handle = mvn_event_on(bus, name, callback);
    if (handle >= 0) {
        /* Find the handler we just registered and mark it once */
        for (int32_t idx = 0; idx < MVN_EVENT_MAX_HANDLERS; ++idx) {
            if (bus->handlers[idx].active && bus->handlers[idx].handle == handle) {
                bus->handlers[idx].once = true;
                break;
            }
        }
    }
    return handle;
}

void mvn_event_off(mvn_event_bus_t *bus, int32_t handle)
{
    for (int32_t idx = 0; idx < MVN_EVENT_MAX_HANDLERS; ++idx) {
        if (bus->handlers[idx].active && bus->handlers[idx].handle == handle) {
            JS_FreeValue(bus->ctx, bus->handlers[idx].callback);
            bus->handlers[idx].active = false;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Emit / flush                                                       */
/* ------------------------------------------------------------------ */

void mvn_event_emit(mvn_event_bus_t *bus, const char *name, JSValue payload)
{
    if (bus->queue_count >= MVN_EVENT_MAX_QUEUED) {
        SDL_Log("Event bus: queue full, dropping '%s'", name);
        JS_FreeValue(bus->ctx, payload);
        return;
    }

    {
        mvn_queued_event_t *ev;

        ev = &bus->queue[bus->queue_count];
        SDL_strlcpy(ev->name, name, MVN_EVENT_NAME_LEN);
        ev->payload = payload;
        ++bus->queue_count;
    }
}

void mvn_event_flush(mvn_event_bus_t *bus)
{
    int32_t count;

    count            = bus->queue_count;
    bus->queue_count = 0;

    for (int32_t qi = 0; qi < count; ++qi) {
        mvn_queued_event_t *ev;

        ev = &bus->queue[qi];

        for (int32_t hi = 0; hi < MVN_EVENT_MAX_HANDLERS; ++hi) {
            mvn_event_handler_t *h;
            JSValue              result;

            h = &bus->handlers[hi];
            if (!h->active) {
                continue;
            }
            if (SDL_strcmp(h->name, ev->name) != 0) {
                continue;
            }

            result = JS_Call(bus->ctx, h->callback, JS_UNDEFINED, 1, &ev->payload);
            if (JS_IsException(result)) {
                JSValue     exc;
                const char *msg;

                exc = JS_GetException(bus->ctx);
                msg = JS_ToCString(bus->ctx, exc);
                if (msg != NULL) {
                    SDL_Log("Event '%s' handler exception: %s", ev->name, msg);
                    JS_FreeCString(bus->ctx, msg);
                }
                JS_FreeValue(bus->ctx, exc);
            }
            JS_FreeValue(bus->ctx, result);

            if (h->once) {
                JS_FreeValue(bus->ctx, h->callback);
                h->active = false;
            }
        }

        JS_FreeValue(bus->ctx, ev->payload);
    }
}
