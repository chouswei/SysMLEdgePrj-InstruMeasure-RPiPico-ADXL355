/* fsm.c - table-driven state machine stubs (PICO355-R7). Table: CFG_FSM_TRANSITION_TABLE. */
#include "fsm.h"

#define FSM_ROW(from, ev, to) { (from), (ev), (to) },

/* Rows from config.h, then one terminating sentinel row (the table may be empty while TBD). */
static const fsm_transition_t fsm_table[] = {
    CFG_FSM_TRANSITION_TABLE(FSM_ROW)
    { FSM_STATE_UNSET, FSM_EV_NONE, FSM_STATE_UNSET }
};

fsm_rc_t fsm_init(fsm_t *fsm)
{
    if (fsm == NULL) return FSM_ERR_ARG;
    fsm->state = CFG_FSM_INITIAL_STATE;
    return fsm->state == FSM_STATE_UNSET ? FSM_ERR_TBD : FSM_OK;
}

fsm_rc_t fsm_dispatch(fsm_t *fsm, fsm_event_t ev, fsm_state_t *next)
{
    if (fsm == NULL || next == NULL) return FSM_ERR_ARG;
    (void)ev;
    (void)fsm_table;
    *next = fsm->state;
    return FSM_ERR_NOT_IMPLEMENTED;
}

fsm_state_t fsm_state(const fsm_t *fsm)
{
    return fsm == NULL ? FSM_STATE_UNSET : fsm->state;
}
