/**
 * \file            event.c
 * \brief           Event bus — on/off/once/emit/flush with JS callbacks
 */

#include "event.h"

/* ------------------------------------------------------------------ */
/*  Hash helper                                                        */
/* ------------------------------------------------------------------ */

static uint32_t prv_name_hash(const char *name)
{
    uint32_t h;

    h = 5381;
    while (*name != '\0') {
        h = ((h << 5) + h) + (uint32_t)(unsigned char)*name;
        ++name;
    }
    return h & (DTR_EVENT_HASH_BUCKETS - 1);
}

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
    for (int32_t idx = 0; idx < DTR_EVENT_HASH_BUCKETS; ++idx) {
        bus->hash_heads[idx] = -1;
    }
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

void dtr_event_clear(dtr_event_bus_t *bus, JSContext *new_ctx)
{
    if (bus == NULL) {
        return;
    }

    /* Free all active handler callbacks */
    for (int32_t idx = 0; idx < DTR_EVENT_MAX_HANDLERS; ++idx) {
        if (bus->handlers[idx].active) {
            JS_FreeValue(bus->ctx, bus->handlers[idx].callback);
            bus->handlers[idx].active = false;
        }
    }

    /* Reset hash table */
    for (int32_t idx = 0; idx < DTR_EVENT_HASH_BUCKETS; ++idx) {
        bus->hash_heads[idx] = -1;
    }

    /* Free queued event payloads */
    for (int32_t idx = 0; idx < bus->queue_count; ++idx) {
        JS_FreeValue(bus->ctx, bus->queue[idx].payload);
    }
    bus->queue_count = 0;

    /* Rebind to the new runtime's context */
    bus->ctx = new_ctx;
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
    uint32_t             bucket;

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

    /* Insert into hash chain */
    bucket                  = prv_name_hash(name);
    h->hash_next            = bus->hash_heads[bucket];
    bus->hash_heads[bucket] = slot;

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

static void prv_unlink_handler(dtr_event_bus_t *bus, int32_t slot)
{
    uint32_t bucket;
    int32_t *prev;

    bucket = prv_name_hash(bus->handlers[slot].name);
    prev   = &bus->hash_heads[bucket];
    while (*prev >= 0) {
        if (*prev == slot) {
            *prev = bus->handlers[slot].hash_next;
            return;
        }
        prev = &bus->handlers[*prev].hash_next;
    }
}

void dtr_event_off(dtr_event_bus_t *bus, int32_t handle)
{
    for (int32_t idx = 0; idx < DTR_EVENT_MAX_HANDLERS; ++idx) {
        if (bus->handlers[idx].active && bus->handlers[idx].handle == handle) {
            prv_unlink_handler(bus, idx);
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
        uint32_t            bucket;
        int32_t            *prev;
        int32_t             hi;

        ev     = &bus->queue[qi];
        bucket = prv_name_hash(ev->name);
        prev   = &bus->hash_heads[bucket];
        hi     = *prev;

        while (hi >= 0) {
            dtr_event_handler_t *h;
            int32_t              next;
            JSValue              result;

            h    = &bus->handlers[hi];
            next = h->hash_next;

            if (SDL_strcmp(h->name, ev->name) != 0) {
                prev = &h->hash_next;
                hi   = next;
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
                /* Unlink from chain and deactivate */
                *prev     = next;
                h->active = false;
                JS_FreeValue(bus->ctx, h->callback);
            } else {
                prev = &h->hash_next;
            }

            hi = next;
        }

        JS_FreeValue(bus->ctx, ev->payload);
    }
}
