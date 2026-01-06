#include <stdio.h>

#include "device_sm.h"

// ========================================
// Helper Functions
// ========================================

static void print_header(void) {
	printf("\n");
	printf("================================================\n");
	printf("  SMF Hierarchical State Machine Demo\n");
	printf("  Device Control Example\n");
	printf("================================================\n");
	printf("\n");
}

static void print_scenario(const char *title) {
	printf("\n");
	printf("------------------------------------------------\n");
	printf("Scenario: %s\n", title);
	printf("------------------------------------------------\n");
	printf("\n");
}

static void print_key_point(const char *message) {
	printf("\n[KEY POINT]\n");
	printf("  %s\n", message);
}

static void print_current_state(const char *state_name) {
	printf("  Current State: %s\n", state_name);
}

static void print_footer(void) {
	printf("\n");
	printf("================================================\n");
	printf("  Demo Complete!\n");
	printf("================================================\n");
	printf("\n");
}

// ========================================
// Demo Scenarios
// ========================================

/**
 * @brief Scenario 1: Power On & Initial Transition
 *
 * Showcased features:
 * - Initial transition (ON automatically enters IDLE)
 * - Entry function call order
 */
static void demo_scenario_1(smf_ctx_t *ctx, device_data_t *data) {
	print_scenario("Power On & Initial Transition");

	printf("Initial State: DEVICE/OFF\n");
	print_current_state("DEVICE/OFF");

	printf("\n[EVENT] Power Button Pressed\n\n");

	device_sm_set_state(ctx, DEVICE_STATE_ON);
	print_current_state("DEVICE/ON/IDLE");

	print_key_point("ON state automatically transitions to IDLE (initial state).\n"
					"   Entry functions are called in order: ON -> IDLE");
}

/**
 * @brief Scenario 2: Task Execution
 *
 * Showcased features:
 * - Basic state transitions
 * - Entry/Exit call order
 */
static void demo_scenario_2(smf_ctx_t *ctx, device_data_t *data) {
	print_scenario("Task Execution");

	printf("Current State: DEVICE/ON/IDLE\n");

	printf("\n[EVENT] Start Task\n\n");

	device_sm_set_state(ctx, DEVICE_STATE_WORKING);
	print_current_state("DEVICE/ON/WORKING");

	printf("\nExecuting task...\n");
	for (int i = 0; i < 4; i++) { smf_run_state(ctx); }

	print_current_state("DEVICE/ON/IDLE");

	print_key_point("Task completed and automatically returned to IDLE.\n"
					"   Exit/Entry functions maintain clean state transitions.");
}

/**
 * @brief Scenario 3: Event Propagation
 *
 * Showcased features:
 * - Parent state handling shared events
 * - Event propagation mechanism
 */
static void demo_scenario_3(smf_ctx_t *ctx, device_data_t *data) {
	print_scenario("Event Propagation - Parent State Handling");

	printf("Setup: Starting a task...\n");
	device_sm_set_state(ctx, DEVICE_STATE_WORKING);
	data->task_progress = 50;
	print_current_state("DEVICE/ON/WORKING");

	printf("\n[EVENT] Battery Low Detected\n\n");
	data->battery_low = true;

	printf("Processing event...\n");
	smf_run_state(ctx);

	print_current_state("DEVICE/OFF");

	print_key_point("WORKING state doesn't handle battery_low event.\n"
					"   Event propagates to ON state, which handles it.\n"
					"   This demonstrates shared behavior in parent states.");

	// Reset state
	data->battery_low = false;
}

/**
 * @brief Scenario 4: Error Recovery
 *
 * Showcased features:
 * - Error state handling
 * - State recovery mechanism
 */
static void demo_scenario_4(smf_ctx_t *ctx, device_data_t *data) {
	print_scenario("Error Recovery");

	printf("Setup: Power on and start task...\n");
	device_sm_set_state(ctx, DEVICE_STATE_ON);
	device_sm_set_state(ctx, DEVICE_STATE_WORKING);
	print_current_state("DEVICE/ON/WORKING");

	printf("\n[EVENT] Error Occurred\n\n");
	data->error_occurred = true;

	device_sm_set_state(ctx, DEVICE_STATE_ERROR);
	print_current_state("DEVICE/ON/ERROR");

	printf("\n[EVENT] Reset\n\n");

	device_sm_set_state(ctx, DEVICE_STATE_IDLE);
	print_current_state("DEVICE/ON/IDLE");

	print_key_point("Error state allows graceful error handling.\n"
					"   System can recover and return to normal operation.");
}

/**
 * @brief Scenario 5: Hierarchical Exit
 *
 * Showcased features:
 * - Exit function call order
 * - Proper cleanup of hierarchical states
 */
static void demo_scenario_5(smf_ctx_t *ctx, device_data_t *data) {
	print_scenario("Hierarchical Exit - Cleanup Order");

	printf("Current State: DEVICE/ON/IDLE\n");
	printf("Starting task to demonstrate deep state...\n");
	device_sm_set_state(ctx, DEVICE_STATE_WORKING);
	print_current_state("DEVICE/ON/WORKING");

	printf("\n[EVENT] Power Off\n\n");

	device_sm_set_state(ctx, DEVICE_STATE_OFF);
	print_current_state("DEVICE/OFF");

	print_key_point("Exit functions are called in order: WORKING -> ON.\n"
					"   Child states are cleaned up before parent states.\n"
					"   This ensures proper resource cleanup.");
}

// ========================================
// Main Function
// ========================================

int main(void) {
	// Separate state machine context and user data
	smf_ctx_t     ctx;
	device_data_t data;

	// Initialize state machine
	device_sm_init(&ctx, &data);

	print_header();

	// Run all demo scenarios
	demo_scenario_1(&ctx, &data);
	demo_scenario_2(&ctx, &data);
	demo_scenario_3(&ctx, &data);
	demo_scenario_4(&ctx, &data);
	demo_scenario_5(&ctx, &data);

	print_footer();
	return 0;
}
