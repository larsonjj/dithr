/**
 * \file            runtime.h
 * \brief           QuickJS-NG runtime wrapper — context, js_call, error overlay
 */

#ifndef MVN_RUNTIME_H
#define MVN_RUNTIME_H

#include "console.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Maximum length of the error message stored for the overlay */
#define MVN_ERROR_MSG_LEN 512

/**
 * \brief           JS runtime wrapper
 */
typedef struct mvn_runtime {
    JSRuntime *    rt;
    JSContext *    ctx;
    mvn_console_t *console;

    /* Cached global function atoms */
    JSAtom atom_init;
    JSAtom atom_update;
    JSAtom atom_draw;

    /* Error overlay state */
    bool    error_active;
    char    error_msg[MVN_ERROR_MSG_LEN];
    int32_t error_line;
} mvn_runtime_t;

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
mvn_runtime_t *mvn_runtime_create(mvn_console_t *con, int32_t heap_mb, int32_t stack_kb);

/**
 * \brief           Destroy runtime and free all JS resources
 */
void mvn_runtime_destroy(mvn_runtime_t *rt);

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
bool mvn_runtime_eval(mvn_runtime_t *rt, const char *code, size_t len, const char *filename);

/**
 * \brief           Call a named global JS function with error handling
 * \note            On exception: captures message + line, sets error_active,
 *                  renders overlay. Runtime stays alive.
 * \param[in]       rt: Runtime
 * \param[in]       name: Atom of the global function to call
 * \return          true on success, false if exception occurred
 */
bool mvn_runtime_call(mvn_runtime_t *rt, JSAtom name);

/**
 * \brief           Drain pending QuickJS microtasks
 */
void mvn_runtime_drain_jobs(mvn_runtime_t *rt);

/**
 * \brief           Parse a JSON string using the JS context
 * \param[in]       rt: Runtime
 * \param[in]       json: JSON string
 * \param[in]       len: Length of json
 * \return          Parsed JS value (caller must JS_FreeValue)
 */
JSValue mvn_runtime_parse_json(mvn_runtime_t *rt, const char *json, size_t len);

/**
 * \brief           Clear the error state and resume execution
 */
void mvn_runtime_clear_error(mvn_runtime_t *rt);

/* ------------------------------------------------------------------------ */
/*  API registration — called by each api/*.c module                         */
/* ------------------------------------------------------------------------ */

/**
 * \brief           Register all JS API namespaces on the global object
 * \param[in]       rt: Runtime (context must already exist)
 */
void mvn_api_register_all(mvn_runtime_t *rt);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MVN_RUNTIME_H */
