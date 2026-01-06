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
 * Hierarchical Test Transition:
 *
 * PARENT_AB_ENTRY --> A_ENTRY --> A_RUN --> PARENT_AB_RUN ---|
 *                                                            |
 * |----------------------------------------------------------|
 * |
 * |--> B_ENTRY --> B_RUN --> B_EXIT --> PARENT_AB_EXIT ------|
 *                                                            |
 * |----------------------------------------------------------|
 * |
 * |--> PARENT_C_ENTRY --> C_ENTRY --> C_RUN --> C_EXIT ------|
 *                                                            |
 * |----------------------------------------------------------|
 * |
 * |--> PARENT_C_EXIT --> D_ENTRY
 */

#define SMF_RUN 3

#define PARENT_AB_ENTRY_BIT (1U << 0)
#define STATE_A_ENTRY_BIT   (1U << 1)
#define STATE_A_RUN_BIT     (1U << 2)
#define PARENT_AB_RUN_BIT   (1U << 3)
#define STATE_A_EXIT_BIT    (1U << 4)
#define STATE_B_ENTRY_BIT   (1U << 5)
#define STATE_B_RUN_BIT     (1U << 6)
#define STATE_B_EXIT_BIT    (1U << 7)
#define PARENT_AB_EXIT_BIT  (1U << 8)
#define PARENT_C_ENTRY_BIT  (1U << 9)
#define STATE_C_ENTRY_BIT   (1U << 10)
#define STATE_C_RUN_BIT     (1U << 11)
#define STATE_C_EXIT_BIT    (1U << 12)
#define PARENT_C_EXIT_BIT   (1U << 13)

#define TEST_PARENT_ENTRY_VALUE_NUM 0
#define TEST_PARENT_RUN_VALUE_NUM   3
#define TEST_PARENT_EXIT_VALUE_NUM  8
#define TEST_ENTRY_VALUE_NUM        1
#define TEST_RUN_VALUE_NUM          6
#define TEST_EXIT_VALUE_NUM         12
#define TEST_VALUE_NUM              14

static uint32_t test_value[] = {
	0x00,   /* PARENT_AB_ENTRY */
	0x01,   /* STATE_A_ENTRY */
	0x03,   /* STATE_A_RUN */
	0x07,   /* PARENT_AB_RUN */
	0x0f,   /* STATE_A_EXIT */
	0x1f,   /* STATE_B_ENTRY */
	0x3f,   /* STATE_B_RUN */
	0x7f,   /* STATE_B_EXIT */
	0xff,   /* STATE_AB_EXIT */
	0x1ff,  /* PARENT_C_ENTRY */
	0x3ff,  /* STATE_C_ENTRY */
	0x7ff,  /* STATE_C_RUN */
	0xfff,  /* STATE_C_EXIT */
	0x1fff, /* PARENT_C_EXIT */
	0x3fff, /* FINAL VALUE */
};

/* Forward declaration of test_states */
static const smf_state_t test_states[];

// F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_HIERARCHICAL_FOREACH(F)                                                                               \
	F(0, PARENT_AB, "Parent AB", parent_ab_entry, parent_ab_run, parent_ab_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL) \
	F(1, PARENT_C, "Parent C", parent_c_entry, parent_c_run, parent_c_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL)      \
	F(2, STATE_A, "State A", state_a_entry, state_a_run, state_a_exit, &test_states[PARENT_AB], SMF_NONE_INITIAL)   \
	F(3, STATE_B, "State B", state_b_entry, state_b_run, state_b_exit, &test_states[PARENT_AB], SMF_NONE_INITIAL)   \
	F(4, STATE_C, "State C", state_c_entry, state_c_run, state_c_exit, &test_states[PARENT_C], SMF_NONE_INITIAL)    \
	F(5, STATE_D, "State D", state_d_entry, state_d_run, state_d_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL)

/* List of all TypeC-level states */
enum test_state {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) name = code,
	STATE_HIERARCHICAL_FOREACH(F)
#undef F
};

enum terminate_action {
	NONE,
	PARENT_ENTRY,
	PARENT_RUN,
	PARENT_EXIT,
	ENTRY,
	RUN,
	EXIT,
};

/* Test data structure */
typedef struct {
	uint32_t              transition_bits;
	uint32_t              tv_idx;
	enum terminate_action terminate;
} test_data_t;

static void parent_ab_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[PARENT_AB],
				  "Fail to get the currently-executing state at entry. Expected: State PARENT_AB");

	data->tv_idx = 0;
	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent AB entry failed");

	if (data->terminate == PARENT_ENTRY) {
		smf_set_terminate(ctx, -1);
		return;
	}

	data->transition_bits |= PARENT_AB_ENTRY_BIT;
}

static smf_state_result_t parent_ab_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[PARENT_AB], "Fail to get the currently-executing state at run. Expected: State PARENT_AB");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent AB run failed");

	if (data->terminate == PARENT_RUN) {
		smf_set_terminate(ctx, -1);
		return SMF_EVENT_PROPAGATE;
	}

	data->transition_bits |= PARENT_AB_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_B]);
	return SMF_EVENT_PROPAGATE;
}

static void parent_ab_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[PARENT_AB],
				  "Fail to get the currently-executing state at exit. Expected: State PARENT_AB");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent AB exit failed");

	if (data->terminate == PARENT_EXIT) {
		smf_set_terminate(ctx, -1);
		return;
	}

	data->transition_bits |= PARENT_AB_EXIT_BIT;
}

static void parent_c_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[PARENT_C], "Fail to get the currently-executing state at entry. Expected: State PARENT_C");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_C], "Fail to get the current leaf state at entry. Expected: State C");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent C entry failed");
	data->transition_bits |= PARENT_C_ENTRY_BIT;
}

static smf_state_result_t parent_c_run(smf_ctx_t *ctx) {
	/* This state should not be reached */
	(void)ctx;
	assert_true(0, "Test Parent C run failed");
	return SMF_EVENT_PROPAGATE;
}

static void parent_c_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[PARENT_C], "Fail to get the currently-executing state at exit. Expected: State PARENT_C");
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &test_states[STATE_C], "Fail to get the current leaf state at exit. Expected: State C");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent C exit failed");
	data->transition_bits |= PARENT_C_EXIT_BIT;
}

static void state_a_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_A], "Fail to get the currently-executing state at entry. Expected: State A");

	data->tv_idx++;

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

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A run failed");

	data->transition_bits |= STATE_A_RUN_BIT;

	/* Return to parent run state */
	return SMF_EVENT_PROPAGATE;
}

static void state_a_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_A], "Fail to get the currently-executing state at exit. Expected: State A");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A exit failed");
	data->transition_bits |= STATE_A_EXIT_BIT;
}

static void state_b_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_B], "Fail to get the currently-executing state at entry. Expected: State B");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B entry failed");
	data->transition_bits |= STATE_B_ENTRY_BIT;
}

static smf_state_result_t state_b_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_B], "Fail to get the currently-executing state at run. Expected: State B");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B run failed");

	if (data->terminate == RUN) {
		smf_set_terminate(ctx, -1);
		return SMF_EVENT_PROPAGATE;
	}

	data->transition_bits |= STATE_B_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_C]);
	return SMF_EVENT_PROPAGATE;
}

static void state_b_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_B], "Fail to get the currently-executing state at exit. Expected: State B");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B exit failed");
	data->transition_bits |= STATE_B_EXIT_BIT;
}

static void state_c_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_C], "Fail to get the currently-executing state at entry. Expected: State C");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C entry failed");
	data->transition_bits |= STATE_C_ENTRY_BIT;
}

static smf_state_result_t state_c_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_C], "Fail to get the currently-executing state at run. Expected: State C");

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C run failed");
	data->transition_bits |= STATE_C_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_D]);
	return SMF_EVENT_PROPAGATE;
}

static void state_c_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_ptr_eq(smf_get_current_executing_state(ctx), &test_states[STATE_C], "Fail to get the currently-executing state at exit. Expected: State C");

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
	/* Do nothing */
	(void)ctx;
	return SMF_EVENT_PROPAGATE;
}

static void state_d_exit(smf_ctx_t *ctx) {
	/* Do nothing */
	(void)ctx;
}

static const smf_state_t test_states[] = {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) [code] = SMF_CREATE_STATE(ENTRY, RUN, EXIT, PARENT, INITIAL),
	STATE_HIERARCHICAL_FOREACH(F)
#undef F
};

/* ========== Test Cases ========== */

void test_hierarchical_transitions(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = NONE,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_VALUE_NUM, data.tv_idx, "Incorrect test value index");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final state not reached");
}

void test_hierarchical_parent_entry_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = PARENT_ENTRY,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_PARENT_ENTRY_VALUE_NUM, data.tv_idx, "Incorrect test value index for parent entry termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final parent entry termination state not reached");
}

void test_hierarchical_parent_run_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = PARENT_RUN,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_PARENT_RUN_VALUE_NUM, data.tv_idx, "Incorrect test value index for parent run termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final parent run termination state not reached");
}

void test_hierarchical_parent_exit_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = PARENT_EXIT,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_PARENT_EXIT_VALUE_NUM, data.tv_idx, "Incorrect test value index for parent exit termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final parent exit termination state not reached");
}

void test_hierarchical_child_entry_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = ENTRY,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_ENTRY_VALUE_NUM, data.tv_idx, "Incorrect test value index for entry termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final entry termination state not reached");
}

void test_hierarchical_child_run_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = RUN,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_RUN_VALUE_NUM, data.tv_idx, "Incorrect test value index for run termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final run termination state not reached");
}

void test_hierarchical_child_exit_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.tv_idx          = 0,
		.terminate       = EXIT,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_EXIT_VALUE_NUM, data.tv_idx, "Incorrect test value index for exit termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final exit termination state not reached");
}

int main(void) {
	cunit_init();

	CUNIT_SUITE_BEGIN("Hierarchical State Machine", NULL, NULL)
	CUNIT_TEST("State Transitions", test_hierarchical_transitions)
	CUNIT_TEST("Parent Entry Termination", test_hierarchical_parent_entry_termination)
	CUNIT_TEST("Parent Run Termination", test_hierarchical_parent_run_termination)
	CUNIT_TEST("Parent Exit Termination", test_hierarchical_parent_exit_termination)
	CUNIT_TEST("Child Entry Termination", test_hierarchical_child_entry_termination)
	CUNIT_TEST("Child Run Termination", test_hierarchical_child_run_termination)
	CUNIT_TEST("Child Exit Termination", test_hierarchical_child_exit_termination)
	CUNIT_SUITE_END()

	return cunit_run();
}
