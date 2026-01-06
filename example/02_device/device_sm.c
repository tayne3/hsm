#include "device_sm.h"

#include <stdio.h>
#include <string.h>

static const smf_state_t state_table[];

/**
 * @brief Entry function for the DEVICE root state
 */
static void device_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("[DEVICE] Entry: Initializing device...\n");
}

/**
 * @brief Entry function for the OFF state
 */
static void off_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("[OFF] Entry: Device powered off\n");
}

/**
 * @brief Entry function for the ON state
 */
static void on_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("[ON] Entry: Power on sequence started\n");
}

/**
 * @brief Run function for the ON state
 *
 * Key features:
 * - Executes after the run function of all child states
 * - Handles logic shared by all child states (battery check)
 * - Returns PROPAGATE to allow child states to handle events first
 */
static smf_state_result_t on_run(smf_ctx_t *ctx) {
	device_data_t *data = (device_data_t *)smf_get_userdata(ctx);

	printf("  [ON] Checking battery level... ");

	if (data->battery_low) {
		printf("LOW!\n");
		printf("  [ON] Initiating emergency shutdown\n");
		smf_set_state(ctx, &state_table[DEVICE_STATE_OFF]);
		return SMF_EVENT_HANDLED;
	}

	printf("OK\n");
	return SMF_EVENT_PROPAGATE;
}

/**
 * @brief Exit function for the ON state
 */
static void on_exit(smf_ctx_t *ctx) {
	(void)ctx;
	printf("[ON] Exit: Shutting down power\n");
}

/**
 * @brief Entry function for the IDLE state
 */
static void idle_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("[IDLE] Entry: Device ready, waiting for commands\n");
}

/**
 * @brief Run function for the IDLE state
 */
static smf_state_result_t idle_run(smf_ctx_t *ctx) {
	(void)ctx;
	return SMF_EVENT_PROPAGATE;  // IDLE state doesn't handle specific events, propagate to parent state
}

/**
 * @brief Exit function for the IDLE state
 */
static void idle_exit(smf_ctx_t *ctx) {
	(void)ctx;
	printf("[IDLE] Exit: Leaving idle state\n");
}

/**
 * @brief Entry function for the WORKING state
 */
static void working_entry(smf_ctx_t *ctx) {
	device_data_t *data = (device_data_t *)smf_get_userdata(ctx);
	printf("[WORKING] Entry: Task execution started\n");
	data->task_running  = true;
	data->task_progress = 0;
}

/**
 * @brief Run function for the WORKING state
 */
static smf_state_result_t working_run(smf_ctx_t *ctx) {
	device_data_t *data = (device_data_t *)smf_get_userdata(ctx);

	if (data->task_running) {
		printf("  [WORKING] Task progress: %d%%\n", data->task_progress);
		data->task_progress += 25;

		if (data->task_progress >= 100) {
			printf("  [WORKING] Task completed!\n");
			data->task_running = false;
			smf_set_state(ctx, &state_table[DEVICE_STATE_IDLE]);
			return SMF_EVENT_HANDLED;
		}
	}

	// Unhandled events propagate to the parent state
	return SMF_EVENT_PROPAGATE;
}

/**
 * @brief Exit function for the WORKING state
 */
static void working_exit(smf_ctx_t *ctx) {
	device_data_t *data = (device_data_t *)smf_get_userdata(ctx);
	printf("[WORKING] Exit: Stopping task\n");
	data->task_running = false;
}

/**
 * @brief Entry function for the ERROR state
 */
static void error_entry(smf_ctx_t *ctx) {
	(void)ctx;
	printf("[ERROR] Entry: Error handling mode activated\n");
}

/**
 * @brief Run function for the ERROR state
 */
static smf_state_result_t error_run(smf_ctx_t *ctx) {
	(void)ctx;
	printf("  [ERROR] Attempting recovery...\n");
	return SMF_EVENT_PROPAGATE;
}

/**
 * @brief Exit function for the ERROR state
 */
static void error_exit(smf_ctx_t *ctx) {
	device_data_t *data = (device_data_t *)smf_get_userdata(ctx);
	printf("[ERROR] Exit: Resetting error state\n");
	data->error_occurred = false;
}

static const smf_state_t state_table[] = {
	[DEVICE_STATE_DEVICE]  = SMF_CREATE_STATE(device_entry, SMF_NONE_RUN, SMF_NONE_EXIT, SMF_NONE_PARENT, &state_table[DEVICE_STATE_OFF]),
	[DEVICE_STATE_OFF]     = SMF_CREATE_STATE(off_entry, SMF_NONE_RUN, SMF_NONE_EXIT, &state_table[DEVICE_STATE_DEVICE], SMF_NONE_INITIAL),
	[DEVICE_STATE_ON]      = SMF_CREATE_STATE(on_entry, on_run, on_exit, &state_table[DEVICE_STATE_DEVICE], &state_table[DEVICE_STATE_IDLE]),
	[DEVICE_STATE_IDLE]    = SMF_CREATE_STATE(idle_entry, idle_run, idle_exit, &state_table[DEVICE_STATE_ON], SMF_NONE_INITIAL),
	[DEVICE_STATE_WORKING] = SMF_CREATE_STATE(working_entry, working_run, working_exit, &state_table[DEVICE_STATE_ON], SMF_NONE_INITIAL),
	[DEVICE_STATE_ERROR]   = SMF_CREATE_STATE(error_entry, error_run, error_exit, &state_table[DEVICE_STATE_ON], SMF_NONE_INITIAL),
};

void device_sm_init(smf_ctx_t *ctx, device_data_t *data) {
	memset(data, 0, sizeof(*data));
	smf_set_initial(ctx, &state_table[DEVICE_STATE_OFF], data);
}

int device_sm_set_state(smf_ctx_t *ctx, device_state_t state_id) {
	if (state_id >= DEVICE_STATE_COUNT) { return -1; }
	return smf_set_state(ctx, &state_table[state_id]);
}
