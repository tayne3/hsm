#include <stdio.h>

#include "smf/smf.h"

// Application data
typedef struct {
	int tick_count;  // Run count
} app_data_t;

// F(code, name, description, entry, run, exit, parent, initial)
#define STATE_FOREACH(F)                                                                           \
	F(0, OFF, "Off State", off_entry, off_run, SMF_NONE_EXIT, SMF_NONE_PARENT, SMF_NONE_INITIAL)   \
	F(1, ON, "On State", on_entry, on_run, on_exit, SMF_NONE_PARENT, &states[STATE_IDLE])          \
	F(2, IDLE, "Idle State", idle_entry, idle_run, idle_exit, &states[STATE_ON], SMF_NONE_INITIAL) \
	F(3, WORKING, "Working State", working_entry, working_run, working_exit, &states[STATE_ON], SMF_NONE_INITIAL)

static const smf_state_t states[];

static void               off_entry(smf_ctx_t *ctx);
static smf_state_result_t off_run(smf_ctx_t *ctx);

static void               on_entry(smf_ctx_t *ctx);
static smf_state_result_t on_run(smf_ctx_t *ctx);
static void               on_exit(smf_ctx_t *ctx);

static void               idle_entry(smf_ctx_t *ctx);
static smf_state_result_t idle_run(smf_ctx_t *ctx);
static void               idle_exit(smf_ctx_t *ctx);

static void               working_entry(smf_ctx_t *ctx);
static smf_state_result_t working_run(smf_ctx_t *ctx);
static void               working_exit(smf_ctx_t *ctx);

// State enum
enum state {
#define F(code, name, ...) STATE_##name = code,
	STATE_FOREACH(F)
#undef F
};

static const smf_state_t states[] = {
#define F(code, name, desc, entry, run, exit, parent, initial) [code] = SMF_CREATE_STATE(entry, run, exit, parent, initial),
	STATE_FOREACH(F)
#undef F
};

// ========== State Function Implementations ==========

// OFF state
static void off_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("Entering OFF state\n");
}

static smf_state_result_t off_run(smf_ctx_t *ctx) {
	(void)ctx;
	return SMF_EVENT_HANDLED;
}

// ON state (parent state)
static void on_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("Entering ON state\n");
}

static smf_state_result_t on_run(smf_ctx_t *ctx) {
	// Parent state handles common logic for all child states
	app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

	// Shut down after 8 cycles
	if (data->tick_count >= 8) {
		printf("Battery low, shutting down\n");
		smf_set_state(ctx, &states[STATE_OFF]);
		return SMF_EVENT_HANDLED;
	}

	return SMF_EVENT_PROPAGATE;
}

static void on_exit(smf_ctx_t *ctx) {
	(void)ctx;
	printf("Leaving ON state\n");
}

// IDLE state (child state)
static void idle_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("Entering IDLE state\n");
}

static smf_state_result_t idle_run(smf_ctx_t *ctx) {
	app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

	// Start working after 3 cycles
	if (data->tick_count == 3) {
		printf("Work requested\n");
		smf_set_state(ctx, &states[STATE_WORKING]);  // [OK] Transition state in run
		return SMF_EVENT_HANDLED;
	}

	return SMF_EVENT_PROPAGATE;  // Propagate to parent state (ON) for handling
}

static void idle_exit(smf_ctx_t *ctx) {
	(void)ctx;
	// [Warning] Do not call smf_set_state() here
	printf("Leaving IDLE state\n");
}

// WORKING state (child state)
static void working_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("Entering WORKING state\n");
}

static smf_state_result_t working_run(smf_ctx_t *ctx) {
	app_data_t *data = (app_data_t *)smf_get_userdata(ctx);

	// Finish after 2 cycles of work
	if (data->tick_count == 5) {
		printf("Work done\n");
		smf_set_state(ctx, &states[STATE_IDLE]);
		return SMF_EVENT_HANDLED;
	}

	return SMF_EVENT_PROPAGATE;
}

static void working_exit(smf_ctx_t *ctx) {
	(void)ctx;
	printf("Leaving WORKING state\n");
}

// ========== Main Function ==========

int main(void) {
	smf_ctx_t  ctx;
	app_data_t data = {0};

	// Initialize state machine to ON state
	// Since ON's initial state is IDLE, it will automatically enter IDLE
	smf_set_initial(&ctx, &states[STATE_ON], &data);

	// Run state machine
	for (int i = 0; i < 10 && !ctx.terminate_val; i++) {
		printf("--- Tick %d ---\n", data.tick_count);
		smf_run_state(&ctx);
		data.tick_count++;
	}

	return 0;
}
