/**
 * \file            test_event.c
 * \brief           Unit tests for the event bus subsystem
 */

#include "event.h"
#include "quickjs.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static JSRuntime *s_rt;
static JSContext *s_ctx;

static void prv_setup(void)
{
    s_rt  = JS_NewRuntime();
    s_ctx = JS_NewContext(s_rt);
}

static void prv_teardown(void)
{
    JS_FreeContext(s_ctx);
    JS_FreeRuntime(s_rt);
    s_ctx = NULL;
    s_rt  = NULL;
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static void test_event_create_destroy(void)
{
    dtr_event_bus_t *bus;

    prv_setup();
    bus = dtr_event_create(s_ctx);
    DTR_ASSERT(bus != NULL);
    DTR_ASSERT_EQ_INT(bus->queue_count, 0);
    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Emit without handlers — should not crash                           */
/* ------------------------------------------------------------------ */

static void test_event_emit_no_handlers(void)
{
    dtr_event_bus_t *bus;

    prv_setup();
    bus = dtr_event_create(s_ctx);

    dtr_event_emit(bus, "test:noop", JS_UNDEFINED);
    DTR_ASSERT_EQ_INT(bus->queue_count, 1);

    dtr_event_flush(bus);
    DTR_ASSERT_EQ_INT(bus->queue_count, 0);

    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  on / emit / flush — handler is called                              */
/* ------------------------------------------------------------------ */

/* We detect the call by having the handler set a global flag */
static const char *HANDLER_SRC =
    "globalThis.__called = 0;\n"
    "globalThis.__handler = function(payload) { globalThis.__called++; };\n";

static int32_t prv_get_called(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue val    = JS_GetPropertyStr(ctx, global, "__called");
    int32_t result = 0;
    JS_ToInt32(ctx, &result, val);
    JS_FreeValue(ctx, val);
    JS_FreeValue(ctx, global);
    return result;
}

static JSValue prv_get_handler(JSContext *ctx)
{
    JSValue global  = JS_GetGlobalObject(ctx);
    JSValue handler = JS_GetPropertyStr(ctx, global, "__handler");
    JS_FreeValue(ctx, global);
    return handler; /* caller must free */
}

static void test_event_on_and_flush(void)
{
    dtr_event_bus_t *bus;
    JSValue          handler;

    prv_setup();

    /* Install handler function in JS */
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus = dtr_event_create(s_ctx);

    handler = prv_get_handler(s_ctx);
    dtr_event_on(bus, "test:ping", handler);
    JS_FreeValue(s_ctx, handler);

    /* Emit and flush */
    dtr_event_emit(bus, "test:ping", JS_UNDEFINED);
    dtr_event_flush(bus);

    DTR_ASSERT_EQ_INT(prv_get_called(s_ctx), 1);

    /* Emit again — handler persists */
    dtr_event_emit(bus, "test:ping", JS_UNDEFINED);
    dtr_event_flush(bus);

    DTR_ASSERT_EQ_INT(prv_get_called(s_ctx), 2);

    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  once — handler fires once then auto-removes                        */
/* ------------------------------------------------------------------ */

static void test_event_once(void)
{
    dtr_event_bus_t *bus;
    JSValue          handler;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus     = dtr_event_create(s_ctx);
    handler = prv_get_handler(s_ctx);
    dtr_event_once(bus, "test:one", handler);
    JS_FreeValue(s_ctx, handler);

    dtr_event_emit(bus, "test:one", JS_UNDEFINED);
    dtr_event_flush(bus);
    DTR_ASSERT_EQ_INT(prv_get_called(s_ctx), 1);

    /* Second emit should not fire the handler */
    dtr_event_emit(bus, "test:one", JS_UNDEFINED);
    dtr_event_flush(bus);
    DTR_ASSERT_EQ_INT(prv_get_called(s_ctx), 1);

    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  off — remove handler by handle                                     */
/* ------------------------------------------------------------------ */

static void test_event_off(void)
{
    dtr_event_bus_t *bus;
    JSValue          handler;
    int32_t          handle;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus     = dtr_event_create(s_ctx);
    handler = prv_get_handler(s_ctx);
    handle  = dtr_event_on(bus, "test:rem", handler);
    JS_FreeValue(s_ctx, handler);

    /* Remove before any emit */
    dtr_event_off(bus, handle);

    dtr_event_emit(bus, "test:rem", JS_UNDEFINED);
    dtr_event_flush(bus);
    DTR_ASSERT_EQ_INT(prv_get_called(s_ctx), 0);

    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Wrong event name — handler should not fire                         */
/* ------------------------------------------------------------------ */

static void test_event_wrong_name(void)
{
    dtr_event_bus_t *bus;
    JSValue          handler;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus     = dtr_event_create(s_ctx);
    handler = prv_get_handler(s_ctx);
    dtr_event_on(bus, "test:expected", handler);
    JS_FreeValue(s_ctx, handler);

    dtr_event_emit(bus, "test:other", JS_UNDEFINED);
    dtr_event_flush(bus);
    DTR_ASSERT_EQ_INT(prv_get_called(s_ctx), 0);

    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Queue overflow — events beyond limit are dropped                   */
/* ------------------------------------------------------------------ */

static void test_event_queue_overflow(void)
{
    dtr_event_bus_t *bus;

    prv_setup();
    bus = dtr_event_create(s_ctx);

    /* Fill the queue to capacity */
    for (int32_t idx = 0; idx < DTR_EVENT_MAX_QUEUED; ++idx) {
        dtr_event_emit(bus, "test:flood", JS_UNDEFINED);
    }
    DTR_ASSERT_EQ_INT(bus->queue_count, DTR_EVENT_MAX_QUEUED);

    /* One more emit should be silently dropped */
    dtr_event_emit(bus, "test:overflow", JS_UNDEFINED);
    DTR_ASSERT_EQ_INT(bus->queue_count, DTR_EVENT_MAX_QUEUED);

    /* Flush to clean up */
    dtr_event_flush(bus);
    DTR_ASSERT_EQ_INT(bus->queue_count, 0);

    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Handler slot exhaustion                                            */
/* ------------------------------------------------------------------ */

static void test_event_handler_overflow(void)
{
    dtr_event_bus_t *bus;
    JSValue          handler;
    int32_t          handle;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus     = dtr_event_create(s_ctx);
    handler = prv_get_handler(s_ctx);

    /* Register max handlers */
    for (int32_t idx = 0; idx < DTR_EVENT_MAX_HANDLERS; ++idx) {
        handle = dtr_event_on(bus, "test:slot", handler);
        DTR_ASSERT(handle >= 0);
    }

    /* Next registration should fail */
    handle = dtr_event_on(bus, "test:slot", handler);
    DTR_ASSERT_EQ_INT(handle, -1);

    JS_FreeValue(s_ctx, handler);
    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  destroy(NULL) — must not crash                                     */
/* ------------------------------------------------------------------ */

static void test_event_destroy_null(void)
{
    dtr_event_destroy(NULL);
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  FIFO ordering — events dispatched in emit order                    */
/* ------------------------------------------------------------------ */

static const char *FIFO_HANDLER_SRC =
    "globalThis.__order = [];\n"
    "globalThis.__fifo_handler = function(payload) {\n"
    "    globalThis.__order.push(payload);\n"
    "};\n";

static void test_event_fifo_order(void)
{
    dtr_event_bus_t *bus;
    JSValue          handler;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, FIFO_HANDLER_SRC, strlen(FIFO_HANDLER_SRC),
                            "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus = dtr_event_create(s_ctx);

    {
        JSValue global  = JS_GetGlobalObject(s_ctx);
        handler         = JS_GetPropertyStr(s_ctx, global, "__fifo_handler");
        JS_FreeValue(s_ctx, global);
    }

    dtr_event_on(bus, "test:seq", handler);
    JS_FreeValue(s_ctx, handler);

    /* Emit three events with distinct integer payloads */
    for (int32_t i = 1; i <= 3; ++i) {
        dtr_event_emit(bus, "test:seq", JS_NewInt32(s_ctx, i));
    }
    DTR_ASSERT_EQ_INT(bus->queue_count, 3);

    dtr_event_flush(bus);
    DTR_ASSERT_EQ_INT(bus->queue_count, 0);

    /* Verify __order === [1, 2, 3] */
    {
        JSValue global = JS_GetGlobalObject(s_ctx);
        JSValue arr    = JS_GetPropertyStr(s_ctx, global, "__order");
        JSValue len_v  = JS_GetPropertyStr(s_ctx, arr, "length");
        int32_t len    = 0;
        JS_ToInt32(s_ctx, &len, len_v);
        DTR_ASSERT_EQ_INT(len, 3);

        for (int32_t i = 0; i < 3; ++i) {
            JSValue  elem = JS_GetPropertyUint32(s_ctx, arr, (uint32_t)i);
            int32_t  val  = 0;
            JS_ToInt32(s_ctx, &val, elem);
            DTR_ASSERT_EQ_INT(val, i + 1);
            JS_FreeValue(s_ctx, elem);
        }

        JS_FreeValue(s_ctx, len_v);
        JS_FreeValue(s_ctx, arr);
        JS_FreeValue(s_ctx, global);
    }

    dtr_event_destroy(bus);
    prv_teardown();
    DTR_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    DTR_TEST_BEGIN("test_event");

    DTR_RUN_TEST(test_event_create_destroy);
    DTR_RUN_TEST(test_event_emit_no_handlers);
    DTR_RUN_TEST(test_event_on_and_flush);
    DTR_RUN_TEST(test_event_once);
    DTR_RUN_TEST(test_event_off);
    DTR_RUN_TEST(test_event_wrong_name);
    DTR_RUN_TEST(test_event_queue_overflow);
    DTR_RUN_TEST(test_event_handler_overflow);
    DTR_RUN_TEST(test_event_destroy_null);
    DTR_RUN_TEST(test_event_fifo_order);

    DTR_TEST_END();
}
