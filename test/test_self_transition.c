/*
 * Copyright 2024 Glenn Andrews
 * based on test_lib_hierarchical_smf.c
 * Copyright 2021 The Chromium OS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted for CUnit framework
 */

#include "cunit.h"
#include "smf/smf.h"

/*
 *  Hierarchical Test Transition to self:
 *
 * This implements a hierarchical state machine using UML rules and demonstrates
 * initial transitions, transitions to self (in PARENT_C) and preventing event
 * propagation (in STATE_B)
 *
 * The order of entry, exit and run actions is given in the ordering of the test_value[] array.
 */

#define SMF_RUN 5

/* Number of state transitions for each test: */
#define TEST_VALUE_NUM              22
#define TEST_PARENT_ENTRY_VALUE_NUM 1
#define TEST_PARENT_RUN_VALUE_NUM   8
#define TEST_PARENT_EXIT_VALUE_NUM  10
#define TEST_ENTRY_VALUE_NUM        2
#define TEST_RUN_VALUE_NUM          6
#define TEST_EXIT_VALUE_NUM         15

enum test_steps {
	/* Initial Setup: Testing initial transitions */
	ROOT_ENTRY = 0,
	PARENT_AB_ENTRY,
	STATE_A_ENTRY,

	/* Run 0: normal state transition */
	STATE_A_RUN,
	STATE_A_EXIT,
	STATE_B_ENTRY,

	/* Run 1: Test preventing event propagation */
	STATE_B_1ST_RUN,

	/* Run 2: Normal state transition via parent */
	STATE_B_2ND_RUN,
	PARENT_AB_RUN,
	STATE_B_EXIT,
	PARENT_AB_EXIT,
	PARENT_C_1ST_ENTRY,
	STATE_C_1ST_ENTRY,

	/* Run 3: PARENT_C executes transition to self  */
	STATE_C_1ST_RUN,
	PARENT_C_RUN,
	STATE_C_1ST_EXIT,
	PARENT_C_1ST_EXIT,
	PARENT_C_2ND_ENTRY,
	STATE_C_2ND_ENTRY,

	/* Run 4: Test transition from parent state */
	STATE_C_2ND_RUN,
	STATE_C_2ND_EXIT,
	PARENT_C_2ND_EXIT,

	/* End of run */
	FINAL_VALUE,

	/* Unused functions: Error checks if set */
	ROOT_RUN,
	ROOT_EXIT,
};

/*
 * Note: Test values are taken before the appropriate test bit for that state is set i.e. if
 * ROOT_ENTRY_BIT is BIT(0), test_value for root_entry() will be BIT_MASK(0) not BIT_MASK(1)
 */
#define BIT_MASK(n) ((1U << (n)) - 1)

static uint32_t test_value[] = {
	/* Initial Setup */
	BIT_MASK(ROOT_ENTRY),
	BIT_MASK(PARENT_AB_ENTRY),
	BIT_MASK(STATE_A_ENTRY),
	/* Run 0 */
	BIT_MASK(STATE_A_RUN),
	BIT_MASK(STATE_A_EXIT),
	BIT_MASK(STATE_B_ENTRY),
	/* Run 1 */
	BIT_MASK(STATE_B_1ST_RUN),
	/* Run 2 */
	BIT_MASK(STATE_B_2ND_RUN),
	BIT_MASK(PARENT_AB_RUN),
	BIT_MASK(STATE_B_EXIT),
	BIT_MASK(PARENT_AB_EXIT),
	BIT_MASK(PARENT_C_1ST_ENTRY),
	BIT_MASK(STATE_C_1ST_ENTRY),
	/* Run 3 */
	BIT_MASK(STATE_C_1ST_RUN),
	BIT_MASK(PARENT_C_RUN),
	BIT_MASK(STATE_C_1ST_EXIT),
	BIT_MASK(PARENT_C_1ST_EXIT),
	BIT_MASK(PARENT_C_2ND_ENTRY),
	BIT_MASK(STATE_C_2ND_ENTRY),
	/* Run 4 */
	BIT_MASK(STATE_C_2ND_RUN),
	BIT_MASK(STATE_C_2ND_EXIT),
	BIT_MASK(PARENT_C_2ND_EXIT),
	/* Post-run Check */
	BIT_MASK(FINAL_VALUE),
};

/* Forward declaration of test_states */
static const smf_state_t test_states[];

// F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_SELF_TRANSITION_FOREACH(F)                                                                                    \
	F(0, ROOT, "Root State", root_entry, root_run, root_exit, SMF_NONE_PARENT, &test_states[PARENT_AB])                     \
	F(1, PARENT_AB, "Parent AB", parent_ab_entry, parent_ab_run, parent_ab_exit, &test_states[ROOT], &test_states[STATE_A]) \
	F(2, PARENT_C, "Parent C", parent_c_entry, parent_c_run, parent_c_exit, &test_states[ROOT], &test_states[STATE_C])      \
	F(3, STATE_A, "State A", state_a_entry, state_a_run, state_a_exit, &test_states[PARENT_AB], SMF_NONE_INITIAL)           \
	F(4, STATE_B, "State B", state_b_entry, state_b_run, state_b_exit, &test_states[PARENT_AB], SMF_NONE_INITIAL)           \
	F(5, STATE_C, "State C", state_c_entry, state_c_run, state_c_exit, &test_states[PARENT_C], SMF_NONE_INITIAL)            \
	F(6, STATE_D, "State D", state_d_entry, state_d_run, state_d_exit, &test_states[ROOT], SMF_NONE_INITIAL)

/* List of all TypeC-level states */
enum test_state {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) name = code,
	STATE_SELF_TRANSITION_FOREACH(F)
#undef F
};

enum terminate_action { NONE, PARENT_ENTRY, PARENT_RUN, PARENT_EXIT, ENTRY, RUN, EXIT };

#define B_ENTRY_FIRST_TIME        (1U << 0)
#define B_RUN_FIRST_TIME          (1U << 1)
#define PARENT_C_ENTRY_FIRST_TIME (1U << 2)
#define C_RUN_FIRST_TIME          (1U << 3)
#define C_ENTRY_FIRST_TIME        (1U << 4)
#define C_EXIT_FIRST_TIME         (1U << 5)

#define FIRST_TIME_BITS (B_ENTRY_FIRST_TIME | B_RUN_FIRST_TIME | PARENT_C_ENTRY_FIRST_TIME | C_RUN_FIRST_TIME | C_ENTRY_FIRST_TIME | C_EXIT_FIRST_TIME)

/* Test data structure */
typedef struct {
	uint32_t              transition_bits;
	uint32_t              tv_idx;
	enum terminate_action terminate;
	uint32_t              first_time;
} test_data_t;

static void root_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx = 0;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Root entry failed");

	data->transition_bits |= (1U << ROOT_ENTRY);
}

static smf_state_result_t root_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Root run failed");

	data->transition_bits |= (1U << ROOT_RUN);

	/* Return to parent run state */
	return SMF_EVENT_PROPAGATE;
}

static void root_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Root exit failed");
	data->transition_bits |= (1U << ROOT_EXIT);
}

static void parent_ab_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent AB entry failed");

	if (data->terminate == PARENT_ENTRY) {
		smf_set_terminate(ctx, -1);
		return;
	}

	data->transition_bits |= (1U << PARENT_AB_ENTRY);
}

static smf_state_result_t parent_ab_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent AB run failed");

	if (data->terminate == PARENT_RUN) {
		smf_set_terminate(ctx, -1);
		return SMF_EVENT_PROPAGATE;
	}

	data->transition_bits |= (1U << PARENT_AB_RUN);

	smf_set_state(ctx, &test_states[STATE_C]);
	return SMF_EVENT_HANDLED;
}

static void parent_ab_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent AB exit failed");

	if (data->terminate == PARENT_EXIT) {
		smf_set_terminate(ctx, -1);
		return;
	}

	data->transition_bits |= (1U << PARENT_AB_EXIT);
}

static void parent_c_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent C entry failed");
	if (data->first_time & PARENT_C_ENTRY_FIRST_TIME) {
		data->first_time &= ~PARENT_C_ENTRY_FIRST_TIME;
		data->transition_bits |= (1U << PARENT_C_1ST_ENTRY);
	} else {
		data->transition_bits |= (1U << PARENT_C_2ND_ENTRY);
	}
}

static smf_state_result_t parent_c_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent C run failed");

	data->transition_bits |= (1U << PARENT_C_RUN);

	smf_set_state(ctx, &test_states[PARENT_C]);

	return SMF_EVENT_PROPAGATE;
}

static void parent_c_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent C exit failed");

	if (data->first_time & B_ENTRY_FIRST_TIME) {
		data->first_time &= ~B_ENTRY_FIRST_TIME;
		data->transition_bits |= (1U << PARENT_C_1ST_EXIT);
	} else {
		data->transition_bits |= (1U << PARENT_C_2ND_EXIT);
	}
}

static void state_a_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A entry failed");

	if (data->terminate == ENTRY) {
		smf_set_terminate(ctx, -1);
		return;
	}

	data->transition_bits |= (1U << STATE_A_ENTRY);
}

static smf_state_result_t state_a_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A run failed");

	data->transition_bits |= (1U << STATE_A_RUN);

	smf_set_state(ctx, &test_states[STATE_B]);
	return SMF_EVENT_PROPAGATE;
}

static void state_a_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A exit failed");
	data->transition_bits |= (1U << STATE_A_EXIT);
}

static void state_b_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B entry failed");

	data->transition_bits |= (1U << STATE_B_ENTRY);
}

static smf_state_result_t state_b_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B run failed");

	if (data->terminate == RUN) {
		smf_set_terminate(ctx, -1);
		return SMF_EVENT_PROPAGATE;
	}

	if (data->first_time & B_RUN_FIRST_TIME) {
		data->first_time &= ~B_RUN_FIRST_TIME;
		data->transition_bits |= (1U << STATE_B_1ST_RUN);
		return SMF_EVENT_HANDLED;
	} else {
		data->transition_bits |= (1U << STATE_B_2ND_RUN);
		/* bubble up to PARENT_AB */
	}
	return SMF_EVENT_PROPAGATE;
}

static void state_b_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B exit failed");

	data->transition_bits |= (1U << STATE_B_EXIT);
}

static void state_c_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C entry failed");
	if (data->first_time & C_ENTRY_FIRST_TIME) {
		data->first_time &= ~C_ENTRY_FIRST_TIME;
		data->transition_bits |= (1U << STATE_C_1ST_ENTRY);
	} else {
		data->transition_bits |= (1U << STATE_C_2ND_ENTRY);
	}
}

static smf_state_result_t state_c_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C run failed");

	if (data->first_time & C_RUN_FIRST_TIME) {
		data->first_time &= ~C_RUN_FIRST_TIME;
		data->transition_bits |= (1U << STATE_C_1ST_RUN);
		/* Do nothing, Let parent handle it */
	} else {
		data->transition_bits |= (1U << STATE_C_2ND_RUN);
		smf_set_state(ctx, &test_states[STATE_D]);
	}
	return SMF_EVENT_PROPAGATE;
}

static void state_c_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C exit failed");

	if (data->terminate == EXIT) {
		smf_set_terminate(ctx, -1);
		return;
	}

	if (data->first_time & C_EXIT_FIRST_TIME) {
		data->first_time &= ~C_EXIT_FIRST_TIME;
		data->transition_bits |= (1U << STATE_C_1ST_EXIT);
	} else {
		data->transition_bits |= (1U << STATE_C_2ND_EXIT);
	}
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
	STATE_SELF_TRANSITION_FOREACH(F)
#undef F
};

/* ========== Test Cases ========== */

void test_self_transition_transitions(void) {
	test_data_t data = {
		.transition_bits = 0,
		.first_time      = FIRST_TIME_BITS,
		.terminate       = NONE,
		.tv_idx          = 0,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[PARENT_AB], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_VALUE_NUM, data.tv_idx, "Incorrect test value index");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final state not reached");
}

void test_self_transition_parent_entry_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.first_time      = FIRST_TIME_BITS,
		.terminate       = PARENT_ENTRY,
		.tv_idx          = 0,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[PARENT_AB], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_PARENT_ENTRY_VALUE_NUM, data.tv_idx, "Incorrect test value index for parent entry termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final parent entry termination state not reached");
}

void test_self_transition_parent_run_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.first_time      = FIRST_TIME_BITS,
		.terminate       = PARENT_RUN,
		.tv_idx          = 0,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[PARENT_AB], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_PARENT_RUN_VALUE_NUM, data.tv_idx, "Incorrect test value index for parent run termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final parent run termination state not reached");
}

void test_self_transition_parent_exit_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.first_time      = FIRST_TIME_BITS,
		.terminate       = PARENT_EXIT,
		.tv_idx          = 0,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[PARENT_AB], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_PARENT_EXIT_VALUE_NUM, data.tv_idx, "Incorrect test value index for parent exit termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final parent exit termination state not reached");
}

void test_self_transition_child_entry_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.first_time      = FIRST_TIME_BITS,
		.terminate       = ENTRY,
		.tv_idx          = 0,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[PARENT_AB], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_ENTRY_VALUE_NUM, data.tv_idx, "Incorrect test value index for entry termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final entry termination state not reached");
}

void test_self_transition_child_run_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.first_time      = FIRST_TIME_BITS,
		.terminate       = RUN,
		.tv_idx          = 0,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[PARENT_AB], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_RUN_VALUE_NUM, data.tv_idx, "Incorrect test value index for run termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final run termination state not reached");
}

void test_self_transition_child_exit_termination(void) {
	test_data_t data = {
		.transition_bits = 0,
		.first_time      = FIRST_TIME_BITS,
		.terminate       = EXIT,
		.tv_idx          = 0,
	};

	smf_ctx_t ctx;
	smf_set_initial(&ctx, &test_states[PARENT_AB], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_EXIT_VALUE_NUM, data.tv_idx, "Incorrect test value index for exit termination");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final exit termination state not reached");
}

int main(void) {
	cunit_init();

	CUNIT_SUITE_BEGIN("Self-Transition State Machine", NULL, NULL)
	CUNIT_TEST("State Transitions", test_self_transition_transitions)
	CUNIT_TEST("Parent Entry Termination", test_self_transition_parent_entry_termination)
	CUNIT_TEST("Parent Run Termination", test_self_transition_parent_run_termination)
	CUNIT_TEST("Parent Exit Termination", test_self_transition_parent_exit_termination)
	CUNIT_TEST("Child Entry Termination", test_self_transition_child_entry_termination)
	CUNIT_TEST("Child Run Termination", test_self_transition_child_run_termination)
	CUNIT_TEST("Child Exit Termination", test_self_transition_child_exit_termination)
	CUNIT_SUITE_END()

	return cunit_run();
}
