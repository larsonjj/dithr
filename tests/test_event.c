/**
 * \file            test_event.c
 * \brief           Unit tests for the event bus subsystem
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "event.h"
#include "quickjs.h"
#include "test_harness.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static JSRuntime *s_rt;
static JSContext * s_ctx;

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
    mvn_event_bus_t *bus;

    prv_setup();
    bus = mvn_event_create(s_ctx);
    MVN_ASSERT(bus != NULL);
    MVN_ASSERT_EQ_INT(bus->queue_count, 0);
    mvn_event_destroy(bus);
    prv_teardown();
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Emit without handlers — should not crash                           */
/* ------------------------------------------------------------------ */

static void test_event_emit_no_handlers(void)
{
    mvn_event_bus_t *bus;

    prv_setup();
    bus = mvn_event_create(s_ctx);

    mvn_event_emit(bus, "test:noop", JS_UNDEFINED);
    MVN_ASSERT_EQ_INT(bus->queue_count, 1);

    mvn_event_flush(bus);
    MVN_ASSERT_EQ_INT(bus->queue_count, 0);

    mvn_event_destroy(bus);
    prv_teardown();
    MVN_PASS();
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
    mvn_event_bus_t *bus;
    JSValue          handler;

    prv_setup();

    /* Install handler function in JS */
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus = mvn_event_create(s_ctx);

    handler = prv_get_handler(s_ctx);
    mvn_event_on(bus, "test:ping", handler);
    JS_FreeValue(s_ctx, handler);

    /* Emit and flush */
    mvn_event_emit(bus, "test:ping", JS_UNDEFINED);
    mvn_event_flush(bus);

    MVN_ASSERT_EQ_INT(prv_get_called(s_ctx), 1);

    /* Emit again — handler persists */
    mvn_event_emit(bus, "test:ping", JS_UNDEFINED);
    mvn_event_flush(bus);

    MVN_ASSERT_EQ_INT(prv_get_called(s_ctx), 2);

    mvn_event_destroy(bus);
    prv_teardown();
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  once — handler fires once then auto-removes                        */
/* ------------------------------------------------------------------ */

static void test_event_once(void)
{
    mvn_event_bus_t *bus;
    JSValue          handler;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus     = mvn_event_create(s_ctx);
    handler = prv_get_handler(s_ctx);
    mvn_event_once(bus, "test:one", handler);
    JS_FreeValue(s_ctx, handler);

    mvn_event_emit(bus, "test:one", JS_UNDEFINED);
    mvn_event_flush(bus);
    MVN_ASSERT_EQ_INT(prv_get_called(s_ctx), 1);

    /* Second emit should not fire the handler */
    mvn_event_emit(bus, "test:one", JS_UNDEFINED);
    mvn_event_flush(bus);
    MVN_ASSERT_EQ_INT(prv_get_called(s_ctx), 1);

    mvn_event_destroy(bus);
    prv_teardown();
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  off — remove handler by handle                                     */
/* ------------------------------------------------------------------ */

static void test_event_off(void)
{
    mvn_event_bus_t *bus;
    JSValue          handler;
    int32_t          handle;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus     = mvn_event_create(s_ctx);
    handler = prv_get_handler(s_ctx);
    handle  = mvn_event_on(bus, "test:rem", handler);
    JS_FreeValue(s_ctx, handler);

    /* Remove before any emit */
    mvn_event_off(bus, handle);

    mvn_event_emit(bus, "test:rem", JS_UNDEFINED);
    mvn_event_flush(bus);
    MVN_ASSERT_EQ_INT(prv_get_called(s_ctx), 0);

    mvn_event_destroy(bus);
    prv_teardown();
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Wrong event name — handler should not fire                         */
/* ------------------------------------------------------------------ */

static void test_event_wrong_name(void)
{
    mvn_event_bus_t *bus;
    JSValue          handler;

    prv_setup();
    {
        JSValue r = JS_Eval(s_ctx, HANDLER_SRC, strlen(HANDLER_SRC), "<test>", JS_EVAL_TYPE_GLOBAL);
        JS_FreeValue(s_ctx, r);
    }

    bus     = mvn_event_create(s_ctx);
    handler = prv_get_handler(s_ctx);
    mvn_event_on(bus, "test:expected", handler);
    JS_FreeValue(s_ctx, handler);

    mvn_event_emit(bus, "test:other", JS_UNDEFINED);
    mvn_event_flush(bus);
    MVN_ASSERT_EQ_INT(prv_get_called(s_ctx), 0);

    mvn_event_destroy(bus);
    prv_teardown();
    MVN_PASS();
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== test_event ===\n");

    test_event_create_destroy();
    test_event_emit_no_handlers();
    test_event_on_and_flush();
    test_event_once();
    test_event_off();
    test_event_wrong_name();

    printf("All event tests passed.\n");
    return 0;
}
