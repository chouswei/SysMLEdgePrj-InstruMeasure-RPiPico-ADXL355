/* fsm.c - table-driven state machine stubs (PICO355-R7). Default table: CFG_FSM_TRANSITION_TABLE. */
#include "fsm.h"

#define FSM_ROW(from, ev, to) { (from), (ev), (to) },

/* Default rows from config.h, then one terminating sentinel row (the table may be empty while TBD). */
static const fsm_transition_t fsm_default_table[] = {
    CFG_FSM_TRANSITION_TABLE(FSM_ROW)
    { FSM_STATE_UNSET, FSM_EV_NONE, FSM_STATE_UNSET }
};
#define FSM_DEFAULT_LEN (sizeof fsm_default_table / sizeof fsm_default_table[0] - 1u)

fsm_rc_t fsm_init(fsm_t *fsm, const fsm_transition_t *table, size_t len,
                  fsm_state_t initial, fsm_policy_t policy)
{
    if (fsm == NULL) return FSM_ERR_ARG;
    fsm->state = FSM_STATE_UNSET;
    fsm->table = table;
    fsm->table_len = len;
    fsm->policy = policy;
    (void)initial;
    return FSM_ERR_NOT_IMPLEMENTED;
}

fsm_rc_t fsm_init_default(fsm_t *fsm)
{
    return fsm_init(fsm, fsm_default_table, FSM_DEFAULT_LEN,
                    CFG_FSM_INITIAL_STATE, CFG_FSM_UNKNOWN_EVENT_POLICY);
}

fsm_rc_t fsm_dispatch(fsm_t *fsm, fsm_event_t ev, fsm_state_t *next)
{
    if (fsm == NULL || next == NULL) return FSM_ERR_ARG;
    (void)ev;
    *next = fsm->state;
    return FSM_ERR_NOT_IMPLEMENTED;
}

fsm_state_t fsm_state(const fsm_t *fsm)
{
    return fsm == NULL ? FSM_STATE_UNSET : fsm->state;
}
