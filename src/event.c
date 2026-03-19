/**
 * \file            event.c
 * \brief           Event bus — on/off/once/emit/flush with JS callbacks
 */

#include "event.h"

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

dtr_event_bus_t *dtr_event_create(JSContext *ctx)
{
    dtr_event_bus_t *bus;

    bus = DTR_CALLOC(1, sizeof(dtr_event_bus_t));
    if (bus == NULL) {
        return NULL;
    }

    bus->ctx = ctx;
    return bus;
}

void dtr_event_destroy(dtr_event_bus_t *bus)
{
    if (bus == NULL) {
        return;
    }

    for (int32_t idx = 0; idx < DTR_EVENT_MAX_HANDLERS; ++idx) {
        if (bus->handlers[idx].active) {
            JS_FreeValue(bus->ctx, bus->handlers[idx].callback);
        }
    }

    for (int32_t idx = 0; idx < bus->queue_count; ++idx) {
        JS_FreeValue(bus->ctx, bus->queue[idx].payload);
    }

    DTR_FREE(bus);
}

/* ------------------------------------------------------------------ */
/*  Internal: find free slot                                           */
/* ------------------------------------------------------------------ */

static int32_t prv_find_free_slot(dtr_event_bus_t *bus)
{
    for (int32_t idx = 0; idx < DTR_EVENT_MAX_HANDLERS; ++idx) {
        if (!bus->handlers[idx].active) {
            return idx;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Registration                                                       */
/* ------------------------------------------------------------------ */

int32_t dtr_event_on(dtr_event_bus_t *bus, const char *name, JSValue callback)
{
    int32_t              slot;
    dtr_event_handler_t *h;

    slot = prv_find_free_slot(bus);
    if (slot < 0) {
        SDL_Log("Event bus: max handlers reached");
        return -1;
    }

    h = &bus->handlers[slot];
    SDL_strlcpy(h->name, name, DTR_EVENT_NAME_LEN);
    h->callback = JS_DupValue(bus->ctx, callback);
    h->handle   = bus->next_handle;
    h->once     = false;
    h->active   = true;

    ++bus->next_handle;
    return h->handle;
}

int32_t dtr_event_once(dtr_event_bus_t *bus, const char *name, JSValue callback)
{
    int32_t handle;

    handle = dtr_event_on(bus, name, callback);
    if (handle >= 0) {
        /* Find the handler we just registered and mark it once */
        for (int32_t idx = 0; idx < DTR_EVENT_MAX_HANDLERS; ++idx) {
            if (bus->handlers[idx].active && bus->handlers[idx].handle == handle) {
                bus->handlers[idx].once = true;
                break;
            }
        }
    }
    return handle;
}

void dtr_event_off(dtr_event_bus_t *bus, int32_t handle)
{
    for (int32_t idx = 0; idx < DTR_EVENT_MAX_HANDLERS; ++idx) {
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

void dtr_event_emit(dtr_event_bus_t *bus, const char *name, JSValue payload)
{
    if (bus->queue_count >= DTR_EVENT_MAX_QUEUED) {
        SDL_Log("Event bus: queue full, dropping '%s'", name);
        JS_FreeValue(bus->ctx, payload);
        return;
    }

    {
        dtr_queued_event_t *ev;

        ev = &bus->queue[bus->queue_count];
        SDL_strlcpy(ev->name, name, DTR_EVENT_NAME_LEN);
        ev->payload = payload;
        ++bus->queue_count;
    }
}

void dtr_event_flush(dtr_event_bus_t *bus)
{
    int32_t count;

    count            = bus->queue_count;
    bus->queue_count = 0;

    for (int32_t qi = 0; qi < count; ++qi) {
        dtr_queued_event_t *ev;

        ev = &bus->queue[qi];

        for (int32_t hi = 0; hi < DTR_EVENT_MAX_HANDLERS; ++hi) {
            dtr_event_handler_t *h;
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
