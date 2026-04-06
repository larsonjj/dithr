/**
 * \file            runtime.h
 * \brief           QuickJS-NG runtime wrapper — context, js_call, error overlay
 */

#ifndef DTR_RUNTIME_H
#define DTR_RUNTIME_H

#include "console.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Maximum length of the error message stored for the overlay */
#define DTR_ERROR_MSG_LEN 1024

/**
 * \brief           JS runtime wrapper
 */
struct dtr_runtime {
    JSRuntime     *rt;
    JSContext     *ctx;
    dtr_console_t *console;

    /* Cached global function atoms */
    JSAtom atom_init;
    JSAtom atom_update;
    JSAtom atom_draw;
    JSAtom atom_save;
    JSAtom atom_restore;

    /* Error overlay state */
    bool    error_active;
    char    error_msg[DTR_ERROR_MSG_LEN];
    int32_t error_line;
};

/* ------------------------------------------------------------------------ */
/*  Lifecycle                                                                */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Create QuickJS runtime and context with configured limits
 * \param[in]       con: Owning console (stored as context opaque)
 * \param[in]       heap_mb: JS heap limit in megabytes
 * \param[in]       stack_kb: JS stack limit in kilobytes
 * \return          Runtime pointer, or NULL on failure
 */
dtr_runtime_t *dtr_runtime_create(dtr_console_t *con, int32_t heap_mb, int32_t stack_kb);

/**
 * \brief           Destroy runtime and free all JS resources
 */
void dtr_runtime_destroy(dtr_runtime_t *rt);

/* ------------------------------------------------------------------------ */
/*  Evaluation and invocation                                                */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Evaluate a JS source string in the current context
 * \param[in]       rt: Runtime
 * \param[in]       code: JS source code
 * \param[in]       len: Length of code in bytes
 * \param[in]       filename: Source file name for error reporting
 * \return          true on success
 */
bool dtr_runtime_eval(dtr_runtime_t *rt, const char *code, size_t len, const char *filename);

/**
 * \brief           Call a named global JS function with error handling
 * \note            On exception: captures message + line, sets error_active,
 *                  renders overlay. Runtime stays alive.
 * \param[in]       rt: Runtime
 * \param[in]       name: Atom of the global function to call
 * \return          true on success, false if exception occurred
 */
bool dtr_runtime_call(dtr_runtime_t *rt, JSAtom name);

/**
 * \brief           Call a named global JS function with arguments
 * \param[in]       rt: Runtime
 * \param[in]       name: Atom of the global function to call
 * \param[in]       argc: Number of arguments
 * \param[in]       argv: Argument array (caller retains ownership)
 * \return          true on success, false if exception occurred
 */
bool dtr_runtime_call_argv(dtr_runtime_t *rt, JSAtom name, int argc, JSValue *argv);

/**
 * \brief           Drain pending QuickJS microtasks
 */
void dtr_runtime_drain_jobs(dtr_runtime_t *rt);

/**
 * \brief           Parse a JSON string using the JS context
 * \param[in]       rt: Runtime
 * \param[in]       json: JSON string
 * \param[in]       len: Length of json
 * \return          Parsed JS value (caller must JS_FreeValue)
 */
JSValue dtr_runtime_parse_json(dtr_runtime_t *rt, const char *json, size_t len);

/**
 * \brief           Clear the error state and resume execution
 */
void dtr_runtime_clear_error(dtr_runtime_t *rt);

/* ------------------------------------------------------------------------ */
/*  API registration — called by each api module                            */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Register all JS API namespaces on the global object
 * \param[in]       rt: Runtime (context must already exist)
 */
void dtr_api_register_all(dtr_runtime_t *rt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DTR_RUNTIME_H */
