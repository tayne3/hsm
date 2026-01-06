#include "cunit.h"
#include "smf/smf.h"

// Maximum number of call records
#define TEST_MAX_CALLS 1000

// Call type
typedef enum {
	CALL_ENTRY,
	CALL_RUN,
	CALL_EXIT,
} call_type_t;

// Call record
typedef struct {
	call_type_t        type;
	const smf_state_t *state;
} call_record_t;

// Test context
typedef struct {
	call_record_t      calls[TEST_MAX_CALLS];
	int                call_count;
	int                userdata;
	const smf_state_t *executing_state;
} test_data_t;

static test_data_t gt_ctx;

void test_data_clear(void) {
	gt_ctx.call_count      = 0;
	gt_ctx.userdata        = 0;
	gt_ctx.executing_state = NULL;
}

int test_call_count(void) {
	return gt_ctx.call_count;
}

const call_record_t *test_call(int index) {
	return &gt_ctx.calls[index];
}

int test_get_userdata(void) {
	return gt_ctx.userdata;
}

void test_set_userdata(int userdata) {
	gt_ctx.userdata = userdata;
}

const smf_state_t *test_get_executing_state(void) {
	return gt_ctx.executing_state;
}

void test_set_executing_state(const smf_state_t *state) {
	gt_ctx.executing_state = state;
}

void test_log_call(call_type_t type, const smf_state_t *state) {
	if (gt_ctx.call_count >= TEST_MAX_CALLS) { return; }
	gt_ctx.calls[gt_ctx.call_count].type  = type;
	gt_ctx.calls[gt_ctx.call_count].state = state;
	gt_ctx.call_count++;
}

int test_verify_call(int index, call_type_t type, const smf_state_t *state) {
	if (index >= gt_ctx.call_count) {
		return 0;
	} else if (gt_ctx.calls[index].type != type) {
		return 0;
	} else if (gt_ctx.calls[index].state != state) {
		return 0;
	}
	return 1;
}

// F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_FLAT_FOREACH(F)                                                                                      \
	F(0, IDLE, "Idle State", state_idle_entry, state_idle_run, state_idle_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL) \
	F(1, ACTIVE, "Active State", state_active_entry, state_active_run, state_active_exit, SMF_NONE_PARENT, SMF_NONE_INITIAL)

static void               state_idle_entry(smf_ctx_t *test_ctx);
static smf_state_result_t state_idle_run(smf_ctx_t *test_ctx);
static void               state_idle_exit(smf_ctx_t *test_ctx);

static void               state_active_entry(smf_ctx_t *test_ctx);
static smf_state_result_t state_active_run(smf_ctx_t *test_ctx);
static void               state_active_exit(smf_ctx_t *test_ctx);

enum state_flat {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) STATE_FLAT_##name = code,
	STATE_FLAT_FOREACH(F)
#undef F
};

static const smf_state_t state_flat_table[] = {
#define F(code, name, describe, ENTRY, RUN, EXIT, PARENT, INITIAL) [code] = SMF_CREATE_STATE(ENTRY, RUN, EXIT, PARENT, INITIAL),
	STATE_FLAT_FOREACH(F)
#undef F
};

static void state_idle_entry(smf_ctx_t *test_ctx) {
	(void)test_ctx;
	test_log_call(CALL_ENTRY, &state_flat_table[STATE_FLAT_IDLE]);
}

static smf_state_result_t state_idle_run(smf_ctx_t *test_ctx) {
	(void)test_ctx;
	test_log_call(CALL_RUN, &state_flat_table[STATE_FLAT_IDLE]);
	return SMF_EVENT_HANDLED;
}

static void state_idle_exit(smf_ctx_t *test_ctx) {
	(void)test_ctx;
	test_log_call(CALL_EXIT, &state_flat_table[STATE_FLAT_IDLE]);
}

static void state_active_entry(smf_ctx_t *test_ctx) {
	(void)test_ctx;
	test_log_call(CALL_ENTRY, &state_flat_table[STATE_FLAT_ACTIVE]);
}

static smf_state_result_t state_active_run(smf_ctx_t *test_ctx) {
	(void)test_ctx;
	test_log_call(CALL_RUN, &state_flat_table[STATE_FLAT_ACTIVE]);
	return SMF_EVENT_HANDLED;
}

static void state_active_exit(smf_ctx_t *test_ctx) {
	(void)test_ctx;
	test_log_call(CALL_EXIT, &state_flat_table[STATE_FLAT_ACTIVE]);
}

void test_init_simple_state(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_flat_table[STATE_FLAT_IDLE], NULL));

	assert_ptr_eq(smf_get_current_leaf_state(&ctx), &state_flat_table[STATE_FLAT_IDLE]);
	assert_int_eq(1, test_call_count());
	assert_true(test_verify_call(0, CALL_ENTRY, &state_flat_table[STATE_FLAT_IDLE]));
}

void test_simple_transition(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_flat_table[STATE_FLAT_IDLE], NULL));
	test_data_clear();

	assert_int_eq(0, smf_set_state(&ctx, &state_flat_table[STATE_FLAT_ACTIVE]));
	assert_ptr_eq(smf_get_current_leaf_state(&ctx), &state_flat_table[STATE_FLAT_ACTIVE]);
	assert_ptr_eq(smf_get_previous_leaf_state(&ctx), &state_flat_table[STATE_FLAT_IDLE]);

	assert_int_eq(2, test_call_count());
	assert_true(test_verify_call(0, CALL_EXIT, &state_flat_table[STATE_FLAT_IDLE]));
	assert_true(test_verify_call(1, CALL_ENTRY, &state_flat_table[STATE_FLAT_ACTIVE]));
}

void test_entry_run_exit_order(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_flat_table[STATE_FLAT_IDLE], NULL));
	assert_int_eq(0, smf_set_state(&ctx, &state_flat_table[STATE_FLAT_ACTIVE]));
	test_data_clear();

	assert_int_eq(0, smf_run_state(&ctx));  // Run once
	assert_int_eq(1, test_call_count());
	assert_true(test_verify_call(0, CALL_RUN, &state_flat_table[STATE_FLAT_ACTIVE]));
}

void test_run_state_returns_zero(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_flat_table[STATE_FLAT_IDLE], NULL));
	assert_int_eq(0, smf_run_state(&ctx));
}

void test_terminate_mechanism(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_flat_table[STATE_FLAT_IDLE], NULL));

	smf_set_terminate(&ctx, 42);
	assert_int_eq(42, smf_run_state(&ctx));
}

void test_self_transition(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_flat_table[STATE_FLAT_IDLE], NULL));
	test_data_clear();

	assert_int_eq(0, smf_set_state(&ctx, &state_flat_table[STATE_FLAT_IDLE]));
	assert_int_eq(2, test_call_count());
	assert_true(test_verify_call(0, CALL_EXIT, &state_flat_table[STATE_FLAT_IDLE]));
	assert_true(test_verify_call(1, CALL_ENTRY, &state_flat_table[STATE_FLAT_IDLE]));
}

static void test_init_null_state(void) {
	smf_ctx_t ctx;
	assert_int_eq(-1, smf_set_initial(&ctx, NULL, NULL));
}

static void test_set_state_null(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_flat_table[STATE_FLAT_IDLE], NULL));

	assert_int_eq(-1, smf_set_state(&ctx, NULL));
	assert_ptr_eq(smf_get_current_leaf_state(&ctx), &state_flat_table[STATE_FLAT_IDLE]);  // State should not change
}

static void bad_exit(smf_ctx_t *ctx) {
	assert_int_eq(-1, smf_set_state(ctx, &state_flat_table[STATE_FLAT_ACTIVE]));
	smf_set_userdata(ctx, ctx);
}

static void test_set_state_from_exit(void) {
	const smf_state_t bad_state = SMF_CREATE_STATE(NULL, NULL, bad_exit, NULL, NULL);

	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &bad_state, NULL));
	assert_ptr_eq(NULL, smf_get_userdata(&ctx));

	assert_int_eq(0, smf_set_state(&ctx, &state_flat_table[STATE_FLAT_IDLE]));
	assert_ptr_eq(&ctx, smf_get_userdata(&ctx));
}

static void state_terminate_entry(smf_ctx_t *ctx) {
	smf_set_terminate(ctx, 99);
}

static void test_terminate_in_entry(void) {
	const smf_state_t term_state_entry = SMF_CREATE_STATE(state_terminate_entry, NULL, NULL, NULL, NULL);

	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &term_state_entry, NULL));

	assert_int_eq(99, smf_run_state(&ctx));  // Verify termination
}

static smf_state_result_t state_terminate_run(smf_ctx_t *ctx) {
	smf_set_terminate(ctx, 77);
	return SMF_EVENT_HANDLED;
}

static void test_terminate_in_run(void) {
	const smf_state_t term_state_run = SMF_CREATE_STATE(NULL, state_terminate_run, NULL, NULL, NULL);

	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &term_state_run, NULL));

	assert_int_eq(77, smf_run_state(&ctx));
}

// F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_HANDLED_FOREACH(F)                                   \
	F(0, PARENT, NULL, state_parent_run_handled, NULL, NULL, NULL) \
	F(1, CHILD, NULL, state_child_run_handled, NULL, &state_table_handled[STATE_HANDLED_PARENT], NULL)

static smf_state_result_t state_parent_run_handled(smf_ctx_t *ctx);
static smf_state_result_t state_child_run_handled(smf_ctx_t *ctx);

enum state_handled {
#define F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL) STATE_HANDLED_##name = code,
	STATE_HANDLED_FOREACH(F)
#undef F
};

static const smf_state_t state_table_handled[] = {
#define F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL) [code] = SMF_CREATE_STATE(ENTRY, RUN, EXIT, PARENT, INITIAL),
	STATE_HANDLED_FOREACH(F)
#undef F
};

static smf_state_result_t state_parent_run_handled(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_RUN, &state_table_handled[STATE_HANDLED_PARENT]);
	test_set_userdata(100);
	return SMF_EVENT_HANDLED;
}

static smf_state_result_t state_child_run_handled(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_RUN, &state_table_handled[STATE_HANDLED_CHILD]);
	return SMF_EVENT_HANDLED;
}

void test_event_handled_stops_propagation(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_table_handled[STATE_HANDLED_CHILD], NULL));
	test_data_clear();  // Reset

	smf_run_state(&ctx);
	assert_int_eq(1, test_call_count());
	assert_true(test_verify_call(0, CALL_RUN, &state_table_handled[STATE_HANDLED_CHILD]));

	assert_int_eq(0, test_get_userdata());
}

// F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_PROPAGATE_FOREACH(F)                                   \
	F(0, PARENT, NULL, state_parent_run_propagate, NULL, NULL, NULL) \
	F(1, CHILD, NULL, state_child_run_propagate, NULL, &state_table_propagate[STATE_PROPAGATE_PARENT], NULL)

static smf_state_result_t state_parent_run_propagate(smf_ctx_t *ctx);
static smf_state_result_t state_child_run_propagate(smf_ctx_t *ctx);

enum state_propagate {
#define F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL) STATE_PROPAGATE_##name = code,
	STATE_PROPAGATE_FOREACH(F)
#undef F
};

static const smf_state_t state_table_propagate[] = {
#define F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL) [code] = SMF_CREATE_STATE(ENTRY, RUN, EXIT, PARENT, INITIAL),
	STATE_PROPAGATE_FOREACH(F)
#undef F
};

static smf_state_result_t state_parent_run_propagate(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_RUN, &state_table_propagate[STATE_PROPAGATE_PARENT]);
	test_set_userdata(100);  // Handle event
	return SMF_EVENT_HANDLED;
}

static smf_state_result_t state_child_run_propagate(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_RUN, &state_table_propagate[STATE_PROPAGATE_CHILD]);
	return SMF_EVENT_PROPAGATE;
}

void test_event_propagate_to_parent(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_table_propagate[STATE_PROPAGATE_CHILD], NULL));
	test_data_clear();  // Reset

	smf_run_state(&ctx);
	assert_int_eq(2, test_call_count());
	assert_true(test_verify_call(0, CALL_RUN, &state_table_propagate[STATE_PROPAGATE_CHILD]));
	assert_true(test_verify_call(1, CALL_RUN, &state_table_propagate[STATE_PROPAGATE_PARENT]));

	assert_int_eq(100, test_get_userdata());
}

// Structure:
//   |-- PARENT
//   |   |-- CHILD1 (initial)
//   |   |-- CHILD2

// F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL)
#define STATE_HIERARCHICAL_FOREACH(F)                                                                                                 \
	F(0, PARENT, state_parent_entry, state_parent_run, state_parent_exit, NULL, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]) \
	F(1, CHILD1, state_child1_entry, state_child1_run, state_child1_exit, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT], NULL) \
	F(2, CHILD2, state_child2_entry, state_child2_run, state_child2_exit, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT], NULL)

static void               state_parent_entry(smf_ctx_t *ctx);
static smf_state_result_t state_parent_run(smf_ctx_t *ctx);
static void               state_parent_exit(smf_ctx_t *ctx);
static void               state_child1_entry(smf_ctx_t *ctx);
static smf_state_result_t state_child1_run(smf_ctx_t *ctx);
static void               state_child1_exit(smf_ctx_t *ctx);
static void               state_child2_entry(smf_ctx_t *ctx);
static smf_state_result_t state_child2_run(smf_ctx_t *ctx);
static void               state_child2_exit(smf_ctx_t *ctx);

enum state_hierarchical {
#define F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL) STATE_HIERARCHICAL_##name = code,
	STATE_HIERARCHICAL_FOREACH(F)
#undef F
};

static const smf_state_t state_hierarchical_table[] = {
#define F(code, name, ENTRY, RUN, EXIT, PARENT, INITIAL) [code] = SMF_CREATE_STATE(ENTRY, RUN, EXIT, PARENT, INITIAL),
	STATE_HIERARCHICAL_FOREACH(F)
#undef F
};

static void state_parent_entry(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_ENTRY, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT]);
}

static smf_state_result_t state_parent_run(smf_ctx_t *ctx) {
	test_log_call(CALL_RUN, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT]);
	assert_ptr_eq(smf_get_current_executing_state(ctx), &state_hierarchical_table[STATE_HIERARCHICAL_PARENT]);
	assert_ptr_eq(smf_get_current_leaf_state(ctx), &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]);
	return SMF_EVENT_HANDLED;
}

static void state_parent_exit(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_EXIT, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT]);
}

static void state_child1_entry(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_ENTRY, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]);
}

static smf_state_result_t state_child1_run(smf_ctx_t *ctx) {
	test_log_call(CALL_RUN, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]);
	test_set_executing_state(smf_get_current_executing_state(ctx));
	return SMF_EVENT_PROPAGATE;  // Propagate to parent
}

static void state_child1_exit(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_EXIT, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]);
}

static void state_child2_entry(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_ENTRY, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD2]);
}

static smf_state_result_t state_child2_run(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_RUN, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD2]);
	return SMF_EVENT_PROPAGATE;  // Propagate to parent
}

static void state_child2_exit(smf_ctx_t *ctx) {
	(void)ctx;
	test_log_call(CALL_EXIT, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD2]);
}

void test_initial_transition(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT], NULL));

	assert_ptr_eq(smf_get_current_leaf_state(&ctx), &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]);
}

void test_parent_entry_before_child(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1], NULL));

	assert_int_eq(2, test_call_count());
	assert_true(test_verify_call(0, CALL_ENTRY, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT]));
	assert_true(test_verify_call(1, CALL_ENTRY, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]));
}

void test_child_run_before_parent(void) {
	smf_ctx_t ctx;
	assert_int_eq(0, smf_set_initial(&ctx, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1], NULL));
	test_data_clear();  // Reset

	smf_run_state(&ctx);

	assert_int_eq(2, test_call_count());
	assert_true(test_verify_call(0, CALL_RUN, &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]));
	assert_true(test_verify_call(1, CALL_RUN, &state_hierarchical_table[STATE_HIERARCHICAL_PARENT]));

	assert_ptr_eq(test_get_executing_state(), &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]);
	assert_ptr_eq(smf_get_current_leaf_state(&ctx), &state_hierarchical_table[STATE_HIERARCHICAL_CHILD1]);
}

int main(void) {
	cunit_init();

	CUNIT_SUITE_BEGIN("Basic Functionality", test_data_clear, NULL)
	CUNIT_TEST("Initialize Simple State", test_init_simple_state)
	CUNIT_TEST("Simple Transition", test_simple_transition)
	CUNIT_TEST("Entry/Run/Exit Order", test_entry_run_exit_order)
	CUNIT_TEST("Run State Returns Zero", test_run_state_returns_zero)
	CUNIT_TEST("Terminate Mechanism", test_terminate_mechanism)
	CUNIT_TEST("Self Transition", test_self_transition)
	CUNIT_SUITE_END()

	CUNIT_SUITE_BEGIN("Error Handling", test_data_clear, NULL)
	CUNIT_TEST("Init NULL State", test_init_null_state)
	CUNIT_TEST("Set State NULL", test_set_state_null)
	CUNIT_TEST("Set State From Exit", test_set_state_from_exit)
	CUNIT_TEST("Terminate In Entry", test_terminate_in_entry)
	CUNIT_TEST("Terminate In Run", test_terminate_in_run)
	CUNIT_SUITE_END()

	CUNIT_SUITE_BEGIN("Event Propagation", test_data_clear, NULL)
	CUNIT_TEST("Event Handled Stops Propagation", test_event_handled_stops_propagation)
	CUNIT_TEST("Event Propagate To Parent", test_event_propagate_to_parent)
	CUNIT_SUITE_END()

	CUNIT_SUITE_BEGIN("Hierarchical States", test_data_clear, NULL)
	CUNIT_TEST("Initial Transition", test_initial_transition)
	CUNIT_TEST("Parent Entry Before Child", test_parent_entry_before_child)
	CUNIT_TEST("Child Run Before Parent", test_child_run_before_parent)
	CUNIT_SUITE_END()

	return cunit_run();
}
