/*
 * Copyright 2021 The Chromium OS Authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include "smf/smf.h"

#define SMF_ON  (1)
#define SMF_OFF (0)

// -------------------------[STATIC DECLARATION]-------------------------

/**
 * @brief Check if test_state is a descendant of target_state.
 *
 * @param test_state State to test
 * @param target_state Ancestor state to test
 * @return true if test_state is a descendant of target_state, false otherwise
 */
static int smf__is_descendant_of(const smf_state_t *test_state, const smf_state_t *target_state);

/**
 * @brief Get the direct child of a given parent state.
 *
 * @param states State array (actually used to traverse ancestors)
 * @param parent Parent state
 * @return The child state if found, NULL otherwise
 */
static const smf_state_t *smf__get_child_of(const smf_state_t *states, const smf_state_t *parent);

/**
 * @brief Find the Lowest Common Ancestor (LCA) of two states that are not ancestors of each other.
 *
 * @param source Transition source state
 * @param dest Transition destination state
 * @return LCA state, or NULL if none exists.
 */
static const smf_state_t *smf__get_lca_of(const smf_state_t *source, const smf_state_t *dest);

/**
 * @brief Get the topmost state for the given context and new state.
 *
 * @param ctx State machine context
 * @param new_state New state
 * @return The topmost state
 */
static const smf_state_t *smf__get_topmost_of(const smf_ctx_t *ctx, const smf_state_t *new_state);

/**
 * @brief Execute all entry actions from the direct child of topmost down to new_state.
 *
 * @param ctx State machine context
 * @param new_state State we are transitioning to
 * @param topmost State we are entering from. Its entry action will not be executed.
 * @return true if the state machine should terminate, false otherwise
 */
static int smf__execute_all_entry_actions(smf_ctx_t *const ctx, const smf_state_t *new_state, const smf_state_t *topmost);

/**
 * @brief Execute run actions of all ancestors.
 *
 * @param ctx State machine context
 * @param target Target whose ancestors' run actions are executed
 * @return true if the state machine should terminate, false otherwise
 */
static int smf__execute_ancestor_run_actions(smf_ctx_t *const ctx);

/**
 * @brief Execute all exit actions from ctx->current up to the direct child of topmost.
 *
 * @param ctx State machine context
 * @param topmost State we are exiting to. Its exit action will not be executed.
 * @return true if the state machine should terminate, false otherwise
 */
static int smf__execute_all_exit_actions(smf_ctx_t *const ctx, const smf_state_t *topmost);

/**
 * @brief Reset the internal state of the state machine to default values.
 * Should be called at the entry of smf_set_initial() and smf_set_state().
 *
 * @param ctx State machine context.
 */
static void smf__clear_internal_state(smf_ctx_t *const ctx);

// -------------------------[GLOBAL DEFINITION]-------------------------

int smf_set_initial(smf_ctx_t *const ctx, const smf_state_t *init_state, void *userdata) {
	if (!ctx || !init_state) { return -1; }

	// Recursively find the deepest initial state (leaf node)
	while (init_state->initial) { init_state = init_state->initial; }

	// Find the root of the state tree (Topmost)
	const smf_state_t *const topmost = smf__get_child_of(init_state, NULL);
	if (!topmost) { return -1; }

	smf__clear_internal_state(ctx);
	ctx->current       = init_state;
	ctx->previous      = NULL;
	ctx->userdata      = userdata;
	ctx->terminate_val = 0;
	ctx->executing     = init_state;

	// Special handling: Execute the root node's Entry action
	// smf__execute_all_entry_actions defaults to starting from the child, excluding topmost,
	// so the root node needs to be handled separately here.
	if (topmost->entry) {
		ctx->executing = topmost;
		topmost->entry(ctx);
		ctx->executing = init_state;

		if (ctx->flags.terminate) {
			return 0;  // No need to continue if terminate is set
		}
	}
	// Execute Entry actions of the remaining states in the path
	if (smf__execute_all_entry_actions(ctx, init_state, topmost)) {
		return 0;  // No need to continue if terminate is set
	}
	return 0;
}

int smf_set_state(smf_ctx_t *const ctx, const smf_state_t *new_state) {
	if (new_state == NULL) { return -1; }

	if (ctx->flags.is_exit) {
		// Calling smf_set_state from the exit phase of a state is pointless,
		// as we are already in transition and would always ignore the intended state.
		return -1;
	}

	const smf_state_t *const topmost = smf__get_topmost_of(ctx, new_state);

	ctx->flags.is_exit   = SMF_ON;
	ctx->flags.new_state = SMF_ON;

	// Call all exit actions up to (but not including) topmost
	if (smf__execute_all_exit_actions(ctx, topmost)) {
		return 0;  // No need to continue if terminate was set in an exit action
	}

	// For self-transitions, call the exit action
	if ((ctx->executing == new_state) && (new_state->exit)) {
		new_state->exit(ctx);
		if (ctx->flags.terminate) { return 0; }  // No need to continue if terminate was set in an exit action
	}

	ctx->flags.is_exit = SMF_OFF;

	// For self-transitions, call the entry action
	if ((ctx->executing == new_state) && (new_state->entry)) {
		new_state->entry(ctx);
		if (ctx->flags.terminate) { return 0; }  // No need to continue if terminate was set in an entry action
	}

	// The final destination will be the deepest leaf state contained by the target.
	// Set it as the real target.
	while (new_state->initial) { new_state = new_state->initial; }

	// Update state variables
	ctx->previous  = ctx->current;
	ctx->current   = new_state;
	ctx->executing = new_state;

	// Call entry actions for all states except topmost
	if (smf__execute_all_entry_actions(ctx, new_state, topmost)) {
		return 0;  // No need to continue if terminate was set in an entry action
	}
	return 0;
}

void smf_set_terminate(smf_ctx_t *const ctx, int32_t val) {
	ctx->flags.terminate = SMF_ON;
	ctx->terminate_val   = val;
}

int32_t smf_run_state(smf_ctx_t *const ctx) {
	// No need to continue if terminate is set
	if (ctx->flags.terminate) { return ctx->terminate_val; }

	// Clear transient flags for a single run, ensuring event handling state is reset
	smf__clear_internal_state(ctx);

	ctx->executing = ctx->current;
	if (ctx->current->run) {
		smf_state_result_t rc = ctx->current->run(ctx);
		if (rc == SMF_EVENT_HANDLED) { ctx->flags.handled = SMF_ON; }
	}

	// If unhandled and no state transition occurred, bubble up to the parent state
	if (smf__execute_ancestor_run_actions(ctx)) { return ctx->terminate_val; }

	return 0;
}

// -------------------------[STATIC DEFINITION]-------------------------

static int smf__is_descendant_of(const smf_state_t *state, const smf_state_t *target_state) {
	for (; state; state = state->parent) {
		if (target_state == state) { return SMF_ON; }
	}
	return SMF_OFF;
}

static const smf_state_t *smf__get_child_of(const smf_state_t *states, const smf_state_t *parent) {
	for (; states; states = states->parent) {
		if (states->parent == parent) { return states; }
	}
	return NULL;
}

static const smf_state_t *smf__get_lca_of(const smf_state_t *source, const smf_state_t *dest) {
	for (const smf_state_t *ancestor = source->parent; ancestor; ancestor = ancestor->parent) {
		if (smf__is_descendant_of(dest, ancestor)) { return ancestor; }  // Find the Lowest Common Ancestor (LCA)
	}
	return NULL;
}

static const smf_state_t *smf__get_topmost_of(const smf_ctx_t *ctx, const smf_state_t *new_state) {
	return smf__is_descendant_of(ctx->executing, new_state) ? new_state : // new_state is a parent of our current position
		   smf__is_descendant_of(new_state, ctx->executing) ? ctx->executing : // We are a parent of new_state
															  smf__get_lca_of(ctx->executing, new_state);  // No direct relationship, find LCA
}

static int smf__execute_all_entry_actions(smf_ctx_t *const ctx, const smf_state_t *new_state, const smf_state_t *topmost) {
	if (new_state == topmost) {
		return SMF_OFF;  // No child states, so do nothing
	}

	// Find and execute entry actions from topmost downwards
	for (const smf_state_t *to_execute = smf__get_child_of(new_state, topmost); to_execute && to_execute != new_state;
		 to_execute                    = smf__get_child_of(new_state, to_execute)) {
		// Track the entry action being executed in case it calls smf_set_state()
		ctx->executing = to_execute;
		// Execute entry action for each state except topmost
		if (to_execute->entry) {
			to_execute->entry(ctx);

			// No need to continue if terminate is set
			if (ctx->flags.terminate) {
				ctx->executing = ctx->current;
				return SMF_ON;
			}
		}
	}

	// Finally execute the target state's entry
	ctx->executing = new_state;
	if (new_state->entry) {
		new_state->entry(ctx);

		// No need to continue if terminate is set
		if (ctx->flags.terminate) {
			ctx->executing = ctx->current;
			return SMF_ON;
		}
	}

	ctx->executing = ctx->current;
	return SMF_OFF;
}

static int smf__execute_ancestor_run_actions(smf_ctx_t *const ctx) {
	// Execute all run actions in reverse order

	// Return if current state has terminated
	if (ctx->flags.terminate) { return SMF_ON; }

	// Stop bubbling if a state transition occurred or event was handled
	if (ctx->flags.new_state || ctx->flags.handled) { return SMF_OFF; }

	// Traverse up parent states to execute run
	for (const smf_state_t *tmp_state = ctx->current->parent; tmp_state != NULL; tmp_state = tmp_state->parent) {
		// Track current position in case an ancestor calls smf_set_state()
		ctx->executing = tmp_state;
		// Execute parent state's run action
		if (tmp_state->run) {
			smf_state_result_t rc = tmp_state->run(ctx);

			if (rc == SMF_EVENT_HANDLED) { ctx->flags.handled = SMF_ON; }
			// No need to continue if terminate is set
			if (ctx->flags.terminate) {
				ctx->executing = ctx->current;
				return SMF_ON;
			}

			// This state handled the event. Stop propagation.
			if (ctx->flags.new_state || ctx->flags.handled) { break; }
		}
	}
	// Run actions completed

	ctx->executing = ctx->current;

	return SMF_OFF;
}

static int smf__execute_all_exit_actions(smf_ctx_t *const ctx, const smf_state_t *topmost) {
	const smf_state_t *state = ctx->executing;

	// Execute exit actions upwards until topmost (exclusive)
	for (const smf_state_t *to_execute = ctx->current; to_execute && to_execute != topmost; to_execute = to_execute->parent) {
		if (to_execute->exit) {
			ctx->executing = to_execute;
			to_execute->exit(ctx);

			// No need to continue if terminate was set in an exit action
			if (ctx->flags.terminate) {
				ctx->executing = state;
				return SMF_ON;
			}
		}
	}

	ctx->executing = state;
	return SMF_OFF;
}

static void smf__clear_internal_state(smf_ctx_t *const ctx) {
	ctx->flags.is_exit   = SMF_OFF;
	ctx->flags.terminate = SMF_OFF;
	ctx->flags.handled   = SMF_OFF;
	ctx->flags.new_state = SMF_OFF;
}
