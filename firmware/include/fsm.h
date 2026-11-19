/*
 * fsm.h - table-driven firmware state machine (model state def FirmwareStates).
 *
 * Requirement: PICO355-R7 (states idle, configuring, streaming, fault).
 * Initial state, transition table and unknown-event policy are TODO(TBD) state
 * transition details in config.h (CFG_FSM_*). Event names are placeholders
 * taken from the model actions; triggers are TODO(TBD).
 * Header deps: stdint, stdbool, stddef, config.h.
 */
#ifndef PICO355_FSM_H
#define PICO355_FSM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

/** Return codes of the fsm module. */
typedef enum {
    FSM_OK = 0,
    FSM_ERR_ARG,
    FSM_ERR_NO_TRANSITION,   /**< no row for (state, event); handled per CFG_FSM_UNKNOWN_EVENT_POLICY */
    FSM_ERR_TBD,             /**< initial state or table still TODO(TBD) */
    FSM_ERR_NOT_IMPLEMENTED
} fsm_rc_t;

/** States from the model; UNSET is a sentinel, not a state. */
typedef enum {
    FSM_STATE_UNSET = -1,
    FSM_STATE_IDLE = 0,       /**< idleState */
    FSM_STATE_CONFIGURING,    /**< configuringState */
    FSM_STATE_STREAMING,      /**< streamingState */
    FSM_STATE_FAULT,          /**< faultState */
    FSM_STATE_COUNT
} fsm_state_t;

/** Events (placeholders; TODO(TBD) state transition details decide the real triggers). */
typedef enum {
    FSM_EV_NONE = 0,
    FSM_EV_CONFIG_COMMAND,    /**< handleCommand received a configuration command */
    FSM_EV_CONFIG_DONE,       /**< configureSensor finished */
    FSM_EV_CONFIG_FAILED,     /**< configureSensor failed */
    FSM_EV_STREAM_ERROR,      /**< acquisition or streaming error */
    FSM_EV_COUNT
} fsm_event_t;

/** Unknown-event policy values for CFG_FSM_UNKNOWN_EVENT_POLICY. */
typedef enum {
    FSM_POLICY_UNSET = 0,     /**< TODO(TBD) sentinel */
    FSM_POLICY_IGNORE,        /**< stay in the current state */
    FSM_POLICY_FAULT          /**< go to FSM_STATE_FAULT */
} fsm_policy_t;

/** One transition row; table rows come from CFG_FSM_TRANSITION_TABLE(X). */
typedef struct {
    fsm_state_t from;
    fsm_event_t event;
    fsm_state_t to;
} fsm_transition_t;

/** Machine instance. */
typedef struct {
    fsm_state_t state;
} fsm_t;

/** Enter CFG_FSM_INITIAL_STATE; FSM_ERR_TBD while it is FSM_STATE_UNSET. */
fsm_rc_t fsm_init(fsm_t *fsm);

/** Look up (state, event) in the table and move; *next = resulting state. */
fsm_rc_t fsm_dispatch(fsm_t *fsm, fsm_event_t ev, fsm_state_t *next);

/** Current state (FSM_STATE_UNSET for NULL). */
fsm_state_t fsm_state(const fsm_t *fsm);

#endif /* PICO355_FSM_H */
