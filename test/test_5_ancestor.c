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
 * Hierarchical 5 Ancestor State Test Transition:
 *
 *   	P05_ENTRY --> P04_ENTRY --> P03_ENTRY --> P02_ENTRY ---------|
 *                                                                   |
 *      |------------------------------------------------------------|
 *      |
 *      |--> P01_ENTRY --> A_ENTRY --> A_RUN --> A_EXIT -------------|
 *                                                                   |
 *      |------------------------------------------------------------|
 *      |
 *      |--> B_ENTRY --> B_RUN --> P01_RUN --> P02_RUN --> P03_RUN --|
 *                                                                   |
 *      |------------------------------------------------------------|
 *      |
 *      |--> P04_RUN --> P05_RUN --> B_EXIT --> P01_EXIT ------------|
 *                                                                   |
 *      |------------------------------------------------------------|
 *      |
 *      |--> P02_EXIT --> P03_EXIT --> P04_EXIT --> P05_EXIT --------|
 *                                                                   |
 *      |------------------------------------------------------------|
 *      |
 *      |--> C_ENTRY --> C_RUN --> C_EXIT --> D_ENTRY
 */

#define SMF_RUN 3

#define P05_ENTRY_BIT (1U << 0)
#define P04_ENTRY_BIT (1U << 1)
#define P03_ENTRY_BIT (1U << 2)
#define P02_ENTRY_BIT (1U << 3)
#define P01_ENTRY_BIT (1U << 4)
#define A_ENTRY_BIT   (1U << 5)
#define A_RUN_BIT     (1U << 6)
#define A_EXIT_BIT    (1U << 7)
#define B_ENTRY_BIT   (1U << 8)
#define B_RUN_BIT     (1U << 9)
#define P01_RUN_BIT   (1U << 10)
#define P02_RUN_BIT   (1U << 11)
#define P03_RUN_BIT   (1U << 12)
#define P04_RUN_BIT   (1U << 13)
#define P05_RUN_BIT   (1U << 14)
#define B_EXIT_BIT    (1U << 15)
#define P01_EXIT_BIT  (1U << 16)
#define P02_EXIT_BIT  (1U << 17)
#define P03_EXIT_BIT  (1U << 18)
#define P04_EXIT_BIT  (1U << 19)
#define P05_EXIT_BIT  (1U << 20)
#define C_ENTRY_BIT   (1U << 21)
#define C_RUN_BIT     (1U << 22)
#define C_EXIT_BIT    (1U << 23)

#define TEST_VALUE_NUM 24

static uint32_t test_value[] = {
	0x00000000, /* P05_ENTRY */
	0x00000001, /* P04_ENTRY */
	0x00000003, /* P03_ENTRY */
	0x00000007, /* P02_ENTRY */
	0x0000000f, /* P01_ENTRY */
	0x0000001f, /*   A_ENTRY */
	0x0000003f, /*   A_RUN   */
	0x0000007f, /*   A_EXIT  */
	0x000000ff, /*   B_ENTRY */
	0x000001ff, /*   B_RUN   */
	0x000003ff, /* P01_RUN   */
	0x000007ff, /* P02_RUN   */
	0x00000fff, /* P03_RUN   */
	0x00001fff, /* P04_RUN   */
	0x00003fff, /* P05_RUN   */
	0x00007fff, /*   B_EXIT  */
	0x0000ffff, /* P01_EXIT  */
	0x0001ffff, /* P02_EXIT  */
	0x0003ffff, /* P03_EXIT  */
	0x0007ffff, /* P04_EXIT  */
	0x000fffff, /* P05_EXIT  */
	0x001fffff, /*   C_ENTRY */
	0x003fffff, /*   C_RUN   */
	0x007fffff, /*   C_EXIT  */
	0x00ffffff, /*   D_ENTRY */
};

// F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_FOREACH(F)                                                                            \
	F(0, P05, "Parent 05", p05_entry, p05_run, p05_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL)         \
	F(1, P04, "Parent 04", p04_entry, p04_run, p04_exit, &test_states[STATE_P05], SMF_NONE_INITIAL) \
	F(2, P03, "Parent 03", p03_entry, p03_run, p03_exit, &test_states[STATE_P04], SMF_NONE_INITIAL) \
	F(3, P02, "Parent 02", p02_entry, p02_run, p02_exit, &test_states[STATE_P03], SMF_NONE_INITIAL) \
	F(4, P01, "Parent 01", p01_entry, p01_run, p01_exit, &test_states[STATE_P02], SMF_NONE_INITIAL) \
	F(5, A, "State A", a_entry, a_run, a_exit, &test_states[STATE_P01], SMF_NONE_INITIAL)           \
	F(6, B, "State B", b_entry, b_run, b_exit, &test_states[STATE_P01], SMF_NONE_INITIAL)           \
	F(7, C, "State C", c_entry, c_run, c_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL)                   \
	F(8, D, "State D", d_entry, SMF_NONE_RUN, SMF_NONE_EXIT, SMF_NONE_PARENT, SMF_NONE_INITIAL)

static void               p05_entry(smf_ctx_t *ctx);
static smf_state_result_t p05_run(smf_ctx_t *ctx);
static void               p05_exit(smf_ctx_t *ctx);

static void               p04_entry(smf_ctx_t *ctx);
static smf_state_result_t p04_run(smf_ctx_t *ctx);
static void               p04_exit(smf_ctx_t *ctx);

static void               p03_entry(smf_ctx_t *ctx);
static smf_state_result_t p03_run(smf_ctx_t *ctx);
static void               p03_exit(smf_ctx_t *ctx);

static void               p02_entry(smf_ctx_t *ctx);
static smf_state_result_t p02_run(smf_ctx_t *ctx);
static void               p02_exit(smf_ctx_t *ctx);

static void               p01_entry(smf_ctx_t *ctx);
static smf_state_result_t p01_run(smf_ctx_t *ctx);
static void               p01_exit(smf_ctx_t *ctx);

static void               a_entry(smf_ctx_t *ctx);
static smf_state_result_t a_run(smf_ctx_t *ctx);
static void               a_exit(smf_ctx_t *ctx);

static void               b_entry(smf_ctx_t *ctx);
static smf_state_result_t b_run(smf_ctx_t *ctx);
static void               b_exit(smf_ctx_t *ctx);

static void               c_entry(smf_ctx_t *ctx);
static smf_state_result_t c_run(smf_ctx_t *ctx);
static void               c_exit(smf_ctx_t *ctx);

static void d_entry(smf_ctx_t *ctx);

/* List of all TypeC-level states */
enum test_state {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) STATE_##name = code,
	STATE_FOREACH(F)
#undef F
};

static const smf_state_t test_states[] = {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) [code] = SMF_CREATE_STATE(ENTRY, RUN, EXIT, PARENT, INITIAL),
	STATE_FOREACH(F)
#undef F
};

/* Test data structure */
typedef struct {
	uint32_t transition_bits;
	uint32_t tv_idx;
} test_data_t;

static void p05_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 05 entry failed");

	data->transition_bits |= P05_ENTRY_BIT;
}

static smf_state_result_t p05_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 05 run failed");

	data->transition_bits |= P05_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_C]);
	return SMF_EVENT_PROPAGATE;
}

static void p05_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 05 exit failed");

	data->transition_bits |= P05_EXIT_BIT;
}

static void p04_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 04 entry failed");

	data->transition_bits |= P04_ENTRY_BIT;
}

static smf_state_result_t p04_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 04 run failed");

	data->transition_bits |= P04_RUN_BIT;
	return SMF_EVENT_PROPAGATE;
}

static void p04_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 04 exit failed");

	data->transition_bits |= P04_EXIT_BIT;
}

static void p03_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 03 entry failed");

	data->transition_bits |= P03_ENTRY_BIT;
}

static smf_state_result_t p03_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 03 run failed");

	data->transition_bits |= P03_RUN_BIT;
	return SMF_EVENT_PROPAGATE;
}

static void p03_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 03 exit failed");

	data->transition_bits |= P03_EXIT_BIT;
}

static void p02_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 02 entry failed");

	data->transition_bits |= P02_ENTRY_BIT;
}

static smf_state_result_t p02_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 02 run failed");

	data->transition_bits |= P02_RUN_BIT;
	return SMF_EVENT_PROPAGATE;
}

static void p02_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 02 exit failed");

	data->transition_bits |= P02_EXIT_BIT;
}

static void p01_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 01 entry failed");

	data->transition_bits |= P01_ENTRY_BIT;
}

static smf_state_result_t p01_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 01 run failed");

	data->transition_bits |= P01_RUN_BIT;
	return SMF_EVENT_PROPAGATE;
}

static void p01_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test Parent 01 exit failed");

	data->transition_bits |= P01_EXIT_BIT;
}

static void a_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A entry failed");

	data->transition_bits |= A_ENTRY_BIT;
}

static smf_state_result_t a_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A run failed");

	data->transition_bits |= A_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_B]);
	return SMF_EVENT_PROPAGATE;
}

static void a_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State A exit failed");

	data->transition_bits |= A_EXIT_BIT;
}

static void b_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B entry failed");

	data->transition_bits |= B_ENTRY_BIT;
}

static smf_state_result_t b_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B run failed");

	data->transition_bits |= B_RUN_BIT;
	return SMF_EVENT_PROPAGATE;
}

static void b_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State B exit failed");

	data->transition_bits |= B_EXIT_BIT;
}

static void c_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C entry failed");

	data->transition_bits |= C_ENTRY_BIT;
}

static smf_state_result_t c_run(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C run failed");
	data->transition_bits |= C_RUN_BIT;

	smf_set_state(ctx, &test_states[STATE_D]);
	return SMF_EVENT_PROPAGATE;
}

static void c_exit(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;

	assert_int_eq(data->transition_bits, test_value[data->tv_idx], "Test State C exit failed");

	data->transition_bits |= C_EXIT_BIT;
}

static void d_entry(smf_ctx_t *ctx) {
	test_data_t *data = (test_data_t *)smf_get_userdata(ctx);

	data->tv_idx++;
}

/* ========== Test Cases ========== */

void test_5_ancestor_transitions(void) {
	smf_ctx_t   ctx;
	test_data_t data = {
		.tv_idx          = 0,
		.transition_bits = 0,
	};

	smf_set_initial(&ctx, &test_states[STATE_A], &data);

	for (int i = 0; i < SMF_RUN; i++) {
		if (smf_run_state(&ctx) < 0) { break; }
	}

	assert_int_eq(TEST_VALUE_NUM, data.tv_idx, "Incorrect test value index");
	assert_int_eq(data.transition_bits, test_value[data.tv_idx], "Final state not reached");
}

int main(void) {
	cunit_init();

	CUNIT_SUITE_BEGIN("5-Ancestor Hierarchical State Machine", NULL, NULL)
	CUNIT_TEST("State Transitions", test_5_ancestor_transitions)
	CUNIT_SUITE_END()

	return cunit_run();
}
