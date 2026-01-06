#ifndef DEVICE_SM_H
#define DEVICE_SM_H

#include <stdbool.h>

#include "smf/smf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief User data for the device state machine
 */
typedef struct {
	bool battery_low;     // Battery low flag
	bool task_running;    // Task running flag
	bool error_occurred;  // Error occurred flag
	int  task_progress;   // Task progress (0-100)
} device_data_t;

/**
 * @brief State identifiers for the device state machine
 */
typedef enum device_state {
	DEVICE_STATE_DEVICE = 0,  // Root state
	DEVICE_STATE_OFF,         // Power off state
	DEVICE_STATE_ON,          // Power on state (composite state)
	DEVICE_STATE_IDLE,        // Idle state (substate of ON)
	DEVICE_STATE_WORKING,     // Working state (substate of ON)
	DEVICE_STATE_ERROR,       // Error state (substate of ON)
	DEVICE_STATE_COUNT        // Total number of states
} device_state_t;

/**
 * @brief Initializes the device state machine
 *
 * @param ctx State machine context
 * @param data User data
 */
void device_sm_init(smf_ctx_t *ctx, device_data_t *data);

/**
 * @brief Sets the device state
 *
 * @param ctx State machine context
 * @param state_id Target state ID
 * @return 0 on success, -1 on failure (invalid state ID)
 */
int device_sm_set_state(smf_ctx_t *ctx, device_state_t state_id);

#ifdef __cplusplus
}
#endif

#endif  // DEVICE_SM_H
