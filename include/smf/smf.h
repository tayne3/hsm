/**
 * Copyright 2021 The Chromium OS Authors
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SMF_SMF_H
#define SMF_SMF_H

#include <stddef.h>
#include <stdint.h>

#ifndef SMF_API
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(SMF_LIB_EXPORT)
#define SMF_API __declspec(dllexport)
#elif defined(SMF_SHARED)
#define SMF_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#if defined(SMF_LIB_EXPORT) || defined(SMF_SHARED)
#define SMF_API __attribute__((visibility("default")))
#endif
#endif
#endif
#ifndef SMF_API
#define SMF_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct smf_state;

/**
 * @brief Defines the current context of the state machine.
 */
typedef struct smf_ctx {
	const struct smf_state *current;        // Current state
	const struct smf_state *previous;       // Previous state
	const struct smf_state *executing;      // State currently executing its hook
	void                   *userdata;       // User data
	int32_t                 terminate_val;  // Termination value

	struct {
		uint32_t new_state : 1;
		uint32_t terminate : 1;
		uint32_t is_exit : 1;
		uint32_t handled : 1;
	} flags;
} smf_ctx_t;

/**
 * @brief Function pointer that implements the entry and exit actions of a state.
 *
 * @param ctx State machine context
 */
typedef void (*state_method_t)(smf_ctx_t *ctx);

/**
 * @brief Enum for state execution function return values.
 */
typedef enum smf_state_result {
	SMF_EVENT_HANDLED = 0,  // Event handled
	SMF_EVENT_PROPAGATE,    // Event unhandled (propagate to parent)
} smf_state_result_t;

/**
 * @brief Function pointer that implements the run action of a state.
 *
 * @param ctx State machine context
 * @return Whether the event should propagate to the parent state
 */
typedef smf_state_result_t (*state_execution_t)(smf_ctx_t *ctx);

/**
 * @brief General State
 * @note Can be used in multiple state machines simultaneously
 */
typedef struct smf_state {
	const state_method_t    entry;  // Run when entering state (optional)
	const state_execution_t run;    // Run repeatedly while in state loop (optional)
	const state_method_t    exit;   // Run when exiting state (optional)

	/**
	 * Parent state, containing common entry/run/exit implementations for child states.
	 *	entry: Parent function executes before the child's.
	 *	run:   Parent function executes after the child's.
	 *	exit:  Parent function executes after the child's.
	 *
	 *	Note: When transitioning between two child states with a shared parent state,
	 *      the parent state's exit and entry functions are not executed.
	 */
	const struct smf_state *parent;

	const struct smf_state *initial;  // Initial transition state, NULL for leaf states
} smf_state_t;

// Initialization macro
#define SMF_CREATE_STATE(_entry, _run, _exit, _parent, _initial) \
	{                                                            \
		.entry   = (_entry),                                     \
		.run     = (_run),                                       \
		.exit    = (_exit),                                      \
		.parent  = (_parent),                                    \
		.initial = (_initial),                                   \
	}

#define SMF_NONE_ENTRY   NULL
#define SMF_NONE_RUN     NULL
#define SMF_NONE_EXIT    NULL
#define SMF_NONE_PARENT  NULL
#define SMF_NONE_INITIAL NULL

/**
 * @brief Initializes the state machine and sets its initial state.
 *
 * @param ctx        State machine context
 * @param init_state Initial state for the state machine (cannot be NULL)
 * @return           0 on success, -1 on failure
 */
SMF_API int smf_set_initial(smf_ctx_t *ctx, const smf_state_t *init_state, void *userdata);

/**
 * @brief Changes the state machine's state. This handles exiting the previous state
 *        and entering the target state. For HSMs, the entry and exit actions
 *        of the Lowest Common Ancestor (LCA) will not run.
 *
 * @param ctx       State machine context
 * @param new_state State to transition to (cannot be NULL)
 * @return          0 on success, -1 on failure
 */
SMF_API int smf_set_state(smf_ctx_t *ctx, const smf_state_t *new_state);

/**
 * @brief Terminates the state machine.
 *
 * @param ctx  State machine context
 * @param val  Non-zero termination value returned by smf_run_state.
 */
SMF_API void smf_set_terminate(smf_ctx_t *ctx, int32_t val);

/**
 * @brief Runs one iteration of the state machine (including any parent states).
 *
 * @param ctx  State machine context
 * @return A non-zero value should terminate the state machine. This can indicate
 *		   reaching a terminal state or detecting an error.
 */
SMF_API int32_t smf_run_state(smf_ctx_t *ctx);

/**
 * @brief Get the current leaf state.
 *
 * @param ctx State machine context
 * @return The current leaf state.
 * @note If the initial transition is not set correctly, this might be a parent state.
 */
static inline const smf_state_t *smf_get_current_leaf_state(const smf_ctx_t *const ctx) {
	return ctx->current;
}

/**
 * @brief Get the previous leaf state.
 *
 * @param ctx State machine context
 * @return The previous leaf state.
 */
static inline const smf_state_t *smf_get_previous_leaf_state(const smf_ctx_t *const ctx) {
	return ctx->previous;
}

/**
 * @brief Get the state currently being executed.
 *
 * @param ctx State machine context
 * @return The state currently being executed.
 * @note Could be a parent state.
 */
static inline const smf_state_t *smf_get_current_executing_state(const smf_ctx_t *const ctx) {
	return ctx->executing;
}

/**
 * @brief Get user data
 *
 * @param ctx State machine context
 * @return User data
 */
static inline void *smf_get_userdata(const smf_ctx_t *const ctx) {
	return ctx->userdata;
}

/**
 * @brief Set user data
 *
 * @param ctx      State machine context
 * @param userdata User data
 */
static inline void smf_set_userdata(smf_ctx_t *ctx, void *userdata) {
	ctx->userdata = userdata;
}

#ifdef __cplusplus
}
#endif
#endif  // SMF_SMF_H
