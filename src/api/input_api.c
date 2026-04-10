/**
 * \file            input_api.c
 * \brief           JS input namespace — virtual input actions
 */

#include "../input.h"
#include "api_common.h"

#define INP(ctx) (dtr_api_get_console(ctx)->input)

static JSValue js_input_btn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *action;
    bool        result;

    (void)this_val;
    if (argc < 1) {
        return JS_FALSE;
    }

    action = JS_ToCString(ctx, argv[0]);
    if (action == NULL) {
        return JS_FALSE;
    }

    result = dtr_input_btn(INP(ctx), action);
    JS_FreeCString(ctx, action);
    return JS_NewBool(ctx, result);
}

static JSValue js_input_btnp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *action;
    bool        result;

    (void)this_val;
    if (argc < 1) {
        return JS_FALSE;
    }

    action = JS_ToCString(ctx, argv[0]);
    if (action == NULL) {
        return JS_FALSE;
    }

    result = dtr_input_btnp(INP(ctx), action);
    JS_FreeCString(ctx, action);
    return JS_NewBool(ctx, result);
}

static JSValue js_input_axis(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *action;
    float       result;

    (void)this_val;
    if (argc < 1) {
        return JS_NewFloat64(ctx, 0.0);
    }

    action = JS_ToCString(ctx, argv[0]);
    if (action == NULL) {
        return JS_NewFloat64(ctx, 0.0);
    }

    result = dtr_input_axis(INP(ctx), action);
    JS_FreeCString(ctx, action);
    return JS_NewFloat64(ctx, (double)result);
}

static JSValue js_input_map(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char   *action;
    dtr_binding_t bindings[DTR_INPUT_MAX_BINDINGS];
    int32_t       bind_count;

    (void)this_val;
    if (argc < 2) {
        return JS_UNDEFINED;
    }

    action = JS_ToCString(ctx, argv[0]);
    if (action == NULL) {
        return JS_UNDEFINED;
    }

    /* argv[1] should be an array of binding objects */
    bind_count = 0;
    if (JS_IsArray(argv[1])) {
        JSValue len_val;
        int32_t arr_len;

        len_val = JS_GetPropertyStr(ctx, argv[1], "length");
        JS_ToInt32(ctx, &arr_len, len_val);
        JS_FreeValue(ctx, len_val);

        if (arr_len > DTR_INPUT_MAX_BINDINGS) {
            arr_len = DTR_INPUT_MAX_BINDINGS;
        }

        for (int32_t idx = 0; idx < arr_len; ++idx) {
            JSValue bobj;
            JSValue type_val;
            JSValue code_val;
            JSValue thresh_val;

            bobj = JS_GetPropertyUint32(ctx, argv[1], (uint32_t)idx);
            if (!JS_IsObject(bobj)) {
                JS_FreeValue(ctx, bobj);
                continue;
            }

            type_val   = JS_GetPropertyStr(ctx, bobj, "type");
            code_val   = JS_GetPropertyStr(ctx, bobj, "code");
            thresh_val = JS_GetPropertyStr(ctx, bobj, "threshold");

            JS_ToInt32(ctx, (int32_t *)&bindings[bind_count].type, type_val);
            JS_ToInt32(ctx, &bindings[bind_count].code, code_val);

            {
                double th;

                if (JS_ToFloat64(ctx, &th, thresh_val) == 0) {
                    bindings[bind_count].threshold = (float)th;
                } else {
                    bindings[bind_count].threshold = 0.5f;
                }
            }

            JS_FreeValue(ctx, type_val);
            JS_FreeValue(ctx, code_val);
            JS_FreeValue(ctx, thresh_val);
            JS_FreeValue(ctx, bobj);

            ++bind_count;
        }
    }

    dtr_input_map(INP(ctx), action, bindings, bind_count);
    JS_FreeCString(ctx, action);
    return JS_UNDEFINED;
}

static JSValue js_input_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc >= 1 && JS_IsString(argv[0])) {
        const char *action;

        action = JS_ToCString(ctx, argv[0]);
        if (action != NULL) {
            dtr_input_clear_action(INP(ctx), action);
            JS_FreeCString(ctx, action);
        }
    } else {
        dtr_input_clear_all(INP(ctx));
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_input_funcs[] = {
    JS_CFUNC_DEF("btn", 1, js_input_btn),
    JS_CFUNC_DEF("btnp", 1, js_input_btnp),
    JS_CFUNC_DEF("axis", 1, js_input_axis),
    JS_CFUNC_DEF("map", 2, js_input_map),
    JS_CFUNC_DEF("clear", 1, js_input_clear),
};

/* Binding type constants */
void dtr_input_api_register(JSContext *ctx, JSValue global)
{
    JSValue ns;

    ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ns, js_input_funcs, countof(js_input_funcs));

    JS_SetPropertyStr(ctx, ns, "KEY", JS_NewInt32(ctx, DTR_BIND_KEY));
    JS_SetPropertyStr(ctx, ns, "PAD_BTN", JS_NewInt32(ctx, DTR_BIND_PAD_BTN));
    JS_SetPropertyStr(ctx, ns, "PAD_AXIS", JS_NewInt32(ctx, DTR_BIND_PAD_AXIS));
    JS_SetPropertyStr(ctx, ns, "MOUSE_BTN", JS_NewInt32(ctx, DTR_BIND_MOUSE_BTN));
    JS_SetPropertyStr(ctx, ns, "TOUCH", JS_NewInt32(ctx, DTR_BIND_TOUCH));

    JS_SetPropertyStr(ctx, global, "input", ns);
}
