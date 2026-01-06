/*
 * Copyright 2021 The Chromium OS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted for CUnit framework
 */

#include "cunit.h"
#include "smf/smf.h"

/*
 * Flat Test Transition:
 *
 * A_ENTRY --> A_RUN --> A_EXIT --> B_ENTRY --> B_RUN --|
 *                                                      |
 * |----------------------------------------------------|
 * |
 * |--> B_EXIT --> C_ENTRY --> C_RUN --> C_EXIT --> D_ENTRY
 *
 */

#define SMF_RUN 3

#define STATE_A_ENTRY_BIT (1U << 0)
#define STATE_A_RUN_BIT   (1U << 1)
#define STATE_A_EXIT_BIT  (1U << 2)

#define STATE_B_ENTRY_BIT (1U << 3)
#define STATE_B_RUN_BIT   (1U << 4)
#define STATE_B_EXIT_BIT  (1U << 5)

#define STATE_C_ENTRY_BIT (1U << 6)
#define STATE_C_RUN_BIT   (1U << 7)
#define STATE_C_EXIT_BIT  (1U << 8)

#define TEST_ENTRY_VALUE_NUM 0
#define TEST_RUN_VALUE_NUM   4
#define TEST_EXIT_VALUE_NUM  8
#define TEST_VALUE_NUM       9

static uint32_t test_value[] = {
	0x00,  /* STATE_A_ENTRY */
	0x01,  /* STATE_A_RUN */
	0x03,  /* STATE_A_EXIT */
	0x07,  /* STATE_B_ENTRY */
	0x0f,  /* STATE_B_RUN */
	0x1f,  /* STATE_B_EXIT */
	0x3f,  /* STATE_C_ENTRY */
	0x7f,  /* STATE_C_RUN */
	0xff,  /* STATE_C_EXIT */
	0x1ff, /* FINAL VALUE */
};

/* Forward declaration of test_states */
static const smf_state_t test_states[];

// F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_FLAT_FOREACH(F)                                                                             \
	F(0, STATE_A, "State A", state_a_entry, state_a_run, state_a_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL) \
	F(1, STATE_B, "State B", state_b_entry, state_b_run, state_b_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL) \
	F(2, STATE_C, "State C", state_c_entry, state_c_run, state_c_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL) \
	F(3, STATE_D, "State D", state_d_entry, state_d_run, state_d_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL)

/* List of all TypeC-level states */
enum test_state {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) name = code,
	STATE_FLAT_FOREACH(F)
#undef F
};

enum terminate_action { NONE, ENTRY, RUN, EXIT };

/* Test data structure (separate from smf_ctx_t) */
typedef struct {
	uint32_t              transition_bits;
	uint32_t              tv_idx;
	enum terminate_action terminate;
} test_data_t;

static void state_a_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_A], "Fail to get the currently-executing state at entry. Expected: State A");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_A], "Fail to get the current leaf state at entry. Expected: State A");

	data->tv_idx = 0;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A entry failed");

	if (data->terminate == ENTRY) {
		smf_set_terminate(ctx, -1);
		return;
	}

	data->transition_bits |= STATE_A_ENTRY_BIT;
}

static smf_state_result_t state_a_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_A], "Fail to get the currently-executing state at run. Expected: State A");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_A], "Fail to get the current leaf state at run. Expected: State A");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A run failed");

	data->transition_bits |= STATE_A_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_B]);
	return SMF_EVENT_HANDLED;
}

static void state_a_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_A], "Fail to get the currently-executing state at exit. Expected: State A");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_A], "Fail to get the current leaf state at exit. Expected: State A");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A exit failed");

	data->transition_bits |= STATE_A_EXIT_BIT;
}

static void state_b_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_B], "Fail to get the currently-executing state at entry. Expected: State B");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_B], "Fail to get the current leaf state at entry. Expected: State B");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B entry failed");

	data->transition_bits |= STATE_B_ENTRY_BIT;
}

static smf_state_result_t state_b_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_B], "Fail to get the currently-executing state at run. Expected: State B");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_B], "Fail to get the current leaf state at run. Expected: State B");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B run failed");

	if (data->terminate == RUN) {
		smf_set_terminate(ctx, -1);
		return SMF_EVENT_HANDLED;
	}

	data->transition_bits |= STATE_B_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_C]);
	return SMF_EVENT_HANDLED;
}

static void state_b_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_B], "Fail to get the currently-executing state at exit. Expected: State B");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_B], "Fail to get the current leaf state at exit. Expected: State B");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B exit failed");
	data->transition_bits |= STATE_B_EXIT_BIT;
}

static void state_c_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_C], "Fail to get the currently-executing state at entry. Expected: State C");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_C], "Fail to get the current leaf state at entry. Expected: State C");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C entry failed");
	data->transition_bits |= STATE_C_ENTRY_BIT;
}

static smf_state_result_t state_c_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_C], "Fail to get the currently-executing state at run. Expected: State C");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_C], "Fail to get the current leaf state at run. Expected: State C");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C run failed");
	data->transition_bits |= STATE_C_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_D]);
	return SMF_EVENT_HANDLED;
}

static void state_c_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_C], "Fail to get the currently-executing state at exit. Expected: State C");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_C], "Fail to get the current leaf state at exit. Expected: State C");

	data->tv_idx++;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C exit failed");

	if (data->terminate == EXIT) {
		smf_set_terminate(ctx, -1);
		return;
	}

	data->transition_bits |= STATE_C_EXIT_BIT;
}

static void state_d_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;
}

static smf_state_result_t state_d_run(smf_ctx_t *ctx) {
	(void)ctx;
	return SMF_EVENT_HANDLED;
}

static void state_d_exit(smf_ctx_t *ctx) {
	(void)ctx;
}

static const smf_state_t test_states[] = {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) [code] = SMF_CREATE_STATE(ENTRY, RUN, EXIT, PARENT, INITIAL),
	STATE_FLAT_FOREACH(F)
#undef F
};

/* ========== Test Cases ========== */

void test_flat_transitions(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = NONE,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) != 0) { break; }
	}

	assert_int_eq(TEST_VALUE_NUM, data.tv_idx, "Incorrect test value index");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final state not reached");
}

void test_flat_entry_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = ENTRY,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) != 0) { break; }
	}

	assert_int_eq(TEST_ENTRY_VALUE_NUM, data.tv_idx, "Incorrect test value index for entry termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final entry termination state not reached");
}

void test_flat_run_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = RUN,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) != 0) { break; }
	}

	assert_int_eq(TEST_RUN_VALUE_NUM, data.tv_idx, "Incorrect test value index for run termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final run termination state not reached");
}

void test_flat_exit_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = EXIT,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) != 0) { break; }
	}

	assert_int_eq(TEST_EXIT_VALUE_NUM, data.tv_idx, "Incorrect test value index for exit termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final exit termination state not reached");
}

int main(void) {
	cunit_init();

	CUNIT_SUITE_BEGIN("Flat State Machine", NULL, NULL)
	CUNIT_TEST("State Transitions", test_flat_transitions)
	CUNIT_TEST("Entry Termination", test_flat_entry_termination)
	CUNIT_TEST("Run Termination", test_flat_run_termination)
	CUNIT_TEST("Exit Termination", test_flat_exit_termination)
	CUNIT_SUITE_END()

	return cunit_run();
}
