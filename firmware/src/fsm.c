/* fsm.c - table-driven state machine stubs (PICO355-R7). Default table: CFG_FSM_TRANSITION_TABLE. */
#include "fsm.h"

#define FSM_ROW(from, ev, to) { (from), (ev), (to) },

/* Default rows from config.h, then one terminating sentinel row (the table may be empty while TBD). */
static const fsm_transition_t fsm_default_table[] = {
    CFG_FSM_TRANSITION_TABLE(FSM_ROW)
    { FSM_STATE_UNSET, FSM_EV_NONE, FSM_STATE_UNSET }
};
#define FSM_DEFAULT_LEN (sizeof fsm_default_table / sizeof fsm_default_table[0] - 1u)

/* Real state values only (UNSET excluded). */
static int fsm_is_real_state(fsm_state_t s)
{
    return (int)s >= (int)FSM_STATE_IDLE && (int)s < (int)FSM_STATE_COUNT;
}

/* Legal policy values only (UNSET is a TBD sentinel, accepted by init as TBD). */
static int fsm_is_legal_policy(fsm_policy_t p)
{
    return (int)p >= (int)FSM_POLICY_UNSET && (int)p <= (int)FSM_POLICY_FAULT;
}

fsm_rc_t fsm_init(fsm_t *fsm, const fsm_transition_t *table, size_t len,
                  fsm_state_t initial, fsm_policy_t policy)
{
    if (fsm == NULL) return FSM_ERR_ARG;
    fsm->state = FSM_STATE_UNSET;
    fsm->table = table;
    fsm->table_len = len;
    fsm->policy = policy;

    /* ARG checks come first (fsm.h). */
    if (table == NULL && len > 0) return FSM_ERR_ARG;
    if (initial != FSM_STATE_UNSET && !fsm_is_real_state(initial)) return FSM_ERR_ARG;
    if (!fsm_is_legal_policy(policy)) return FSM_ERR_ARG;

    /* TBD: no table rows, or sentinel initial state / policy. Stays UNSET. */
    if (len == 0 || initial == FSM_STATE_UNSET || policy == FSM_POLICY_UNSET) return FSM_ERR_TBD;

    fsm->state = initial;
    return FSM_OK;
}

fsm_rc_t fsm_init_default(fsm_t *fsm)
{
    return fsm_init(fsm, fsm_default_table, FSM_DEFAULT_LEN,
                    CFG_FSM_INITIAL_STATE, CFG_FSM_UNKNOWN_EVENT_POLICY);
}

fsm_rc_t fsm_dispatch(fsm_t *fsm, fsm_event_t ev, fsm_state_t *next)
{
    size_t i;

    if (fsm == NULL || next == NULL) return FSM_ERR_ARG;
    if ((int)ev < 0 || ev >= FSM_EV_COUNT) return FSM_ERR_ARG;

    /* Not initialised (state UNSET): TBD, *next reports UNSET. */
    if (fsm->state == FSM_STATE_UNSET) {
        *next = FSM_STATE_UNSET;
        return FSM_ERR_TBD;
    }

    /* First matching row wins. */
    for (i = 0; i < fsm->table_len; i++) {
        const fsm_transition_t *row = &fsm->table[i];
        if (row->from == fsm->state && row->event == ev) {
            fsm->state = row->to;
            *next = fsm->state;
            return FSM_OK;
        }
    }

    /* No row: policy decides. IGNORE stays; FAULT moves to the fault state. */
    if (fsm->policy == FSM_POLICY_FAULT) {
        fsm->state = FSM_STATE_FAULT;
    }
    *next = fsm->state;
    return FSM_ERR_NO_TRANSITION;
}

fsm_state_t fsm_state(const fsm_t *fsm)
{
    return fsm == NULL ? FSM_STATE_UNSET : fsm->state;
}
