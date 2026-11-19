/*
 * fsm.h - table-driven firmware state machine (model state def FirmwareStates).
 *
 * Requirement: PICO355-R7 (states idle, configuring, streaming, fault).
 * Event names are placeholders taken from the model actions; triggers TODO(TBD).
 *
 * Table supply: fsm_init() takes the transition table, its length, the initial
 * state and the unknown-event policy as arguments, so host tests inject their
 * own table. fsm_init_default() passes the TODO(TBD) defaults from config.h
 * (CFG_FSM_TRANSITION_TABLE, CFG_FSM_INITIAL_STATE, CFG_FSM_UNKNOWN_EVENT_POLICY);
 * main.c uses it. The table is not copied: it must outlive the fsm_t.
 *
 * Behaviour (FS implements, PF tests):
 *   fsm_init: FSM_ERR_ARG for fsm NULL, table NULL with len > 0, initial not
 *     UNSET and not a real state, or policy not a fsm_policy_t value;
 *     FSM_ERR_TBD (state left FSM_STATE_UNSET) when len == 0, initial is
 *     FSM_STATE_UNSET or policy is FSM_POLICY_UNSET; else FSM_OK, state = initial.
 *   fsm_dispatch: FSM_ERR_ARG for NULL or ev >= FSM_EV_COUNT; FSM_ERR_TBD when
 *     the fsm is not initialised (state UNSET); the first row with
 *     (from == state, event == ev) moves to row.to and returns FSM_OK; no row:
 *     FSM_POLICY_IGNORE stays, FSM_POLICY_FAULT moves to FSM_STATE_FAULT, both
 *     return FSM_ERR_NO_TRANSITION. *next always gets the resulting state.
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

/** One transition row (the default rows come from CFG_FSM_TRANSITION_TABLE(X)). */
typedef struct {
    fsm_state_t from;
    fsm_event_t event;
    fsm_state_t to;
} fsm_transition_t;

/** Machine instance; fields are private to fsm.c. */
typedef struct {
    fsm_state_t state;
    const fsm_transition_t *table;
    size_t table_len;
    fsm_policy_t policy;
} fsm_t;

/** Bind table[0..len), policy, and enter initial (rules above). */
fsm_rc_t fsm_init(fsm_t *fsm, const fsm_transition_t *table, size_t len,
                  fsm_state_t initial, fsm_policy_t policy);

/** fsm_init with the config.h defaults; FSM_ERR_TBD while they are TODO(TBD). */
fsm_rc_t fsm_init_default(fsm_t *fsm);

/** Look up (state, event) in the table and move; *next = resulting state. */
fsm_rc_t fsm_dispatch(fsm_t *fsm, fsm_event_t ev, fsm_state_t *next);

/** Current state (FSM_STATE_UNSET for NULL). */
fsm_state_t fsm_state(const fsm_t *fsm);

#endif /* PICO355_FSM_H */
