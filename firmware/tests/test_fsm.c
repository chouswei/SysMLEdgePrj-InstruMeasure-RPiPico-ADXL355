/*
 * test_fsm.c - host test for firmware/src/fsm.c (PICO355-R7).
 * Rules from fsm.h. The transition tables below are TEST DATA passed to fsm_init();
 * they are test data; the shipped table is checked in default_table() below.
 */
#include <stdio.h>

#include "fsm.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static const fsm_transition_t g_tab[] = {
    { FSM_STATE_IDLE, FSM_EV_CONFIG_COMMAND, FSM_STATE_CONFIGURING },
    { FSM_STATE_CONFIGURING, FSM_EV_CONFIG_DONE, FSM_STATE_STREAMING },
    { FSM_STATE_CONFIGURING, FSM_EV_CONFIG_FAILED, FSM_STATE_FAULT },
    { FSM_STATE_STREAMING, FSM_EV_STREAM_ERROR, FSM_STATE_FAULT }
};
#define TAB_LEN (sizeof g_tab / sizeof g_tab[0])

static const fsm_transition_t g_dup[] = {      /* first matching row wins */
    { FSM_STATE_IDLE, FSM_EV_CONFIG_COMMAND, FSM_STATE_STREAMING },
    { FSM_STATE_IDLE, FSM_EV_CONFIG_COMMAND, FSM_STATE_FAULT }
};

/* The shipped config.h table (design decision 2026-11-19, owner go t37u): boot reaches STREAMING,
 * a command while streaming reconfigures and resumes, a stream error faults, a command recovers. */
static void default_table(void)
{
    fsm_t f;
    fsm_state_t n;
    CHECK(fsm_init_default(&f) == FSM_OK);
    CHECK(fsm_state(&f) == FSM_STATE_IDLE);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_COMMAND, &n) == FSM_OK && n == FSM_STATE_CONFIGURING);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_DONE, &n) == FSM_OK && n == FSM_STATE_STREAMING);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_COMMAND, &n) == FSM_OK && n == FSM_STATE_CONFIGURING);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_DONE, &n) == FSM_OK && n == FSM_STATE_STREAMING);
    CHECK(fsm_dispatch(&f, FSM_EV_STREAM_ERROR, &n) == FSM_OK && n == FSM_STATE_FAULT);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_COMMAND, &n) == FSM_OK && n == FSM_STATE_CONFIGURING);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_FAILED, &n) == FSM_OK && n == FSM_STATE_IDLE);
}

static void init_cases(void)
{
    fsm_t f;
    CHECK(fsm_init(NULL, g_tab, TAB_LEN, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_ERR_ARG);
    CHECK(fsm_init(&f, NULL, TAB_LEN, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_ERR_ARG);
    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_COUNT, FSM_POLICY_IGNORE) == FSM_ERR_ARG);
    CHECK(fsm_init(&f, g_tab, TAB_LEN, (fsm_state_t)99, FSM_POLICY_IGNORE) == FSM_ERR_ARG);
    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_IDLE, (fsm_policy_t)99) == FSM_ERR_ARG);
    /* ARG is checked before TBD */
    CHECK(fsm_init(&f, NULL, TAB_LEN, FSM_STATE_UNSET, FSM_POLICY_UNSET) == FSM_ERR_ARG);

    CHECK(fsm_init(&f, g_tab, 0, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_ERR_TBD);
    CHECK(fsm_state(&f) == FSM_STATE_UNSET);
    CHECK(fsm_init(&f, NULL, 0, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_ERR_TBD);
    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_UNSET, FSM_POLICY_IGNORE) == FSM_ERR_TBD);
    CHECK(fsm_state(&f) == FSM_STATE_UNSET);
    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_IDLE, FSM_POLICY_UNSET) == FSM_ERR_TBD);
    CHECK(fsm_state(&f) == FSM_STATE_UNSET);

    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_CONFIGURING, FSM_POLICY_FAULT) == FSM_OK);
    CHECK(fsm_state(&f) == FSM_STATE_CONFIGURING);
    CHECK(fsm_state(NULL) == FSM_STATE_UNSET);
}

static void default_cases(void)
{
    CHECK(fsm_init_default(NULL) == FSM_ERR_ARG);
    default_table();
}

static void dispatch_cases(void)
{
    fsm_t f;
    fsm_state_t next = FSM_STATE_FAULT;

    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_OK);
    CHECK(fsm_dispatch(NULL, FSM_EV_CONFIG_COMMAND, &next) == FSM_ERR_ARG);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_COMMAND, NULL) == FSM_ERR_ARG);
    CHECK(fsm_dispatch(&f, FSM_EV_COUNT, &next) == FSM_ERR_ARG);
    CHECK(fsm_state(&f) == FSM_STATE_IDLE);

    /* table-driven walk */
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_COMMAND, &next) == FSM_OK);
    CHECK(next == FSM_STATE_CONFIGURING && fsm_state(&f) == FSM_STATE_CONFIGURING);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_DONE, &next) == FSM_OK);
    CHECK(next == FSM_STATE_STREAMING && fsm_state(&f) == FSM_STATE_STREAMING);
    CHECK(fsm_dispatch(&f, FSM_EV_STREAM_ERROR, &next) == FSM_OK);
    CHECK(next == FSM_STATE_FAULT && fsm_state(&f) == FSM_STATE_FAULT);

    /* unknown event, policy IGNORE: stay, NO_TRANSITION, *next = current */
    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_OK);
    CHECK(fsm_dispatch(&f, FSM_EV_STREAM_ERROR, &next) == FSM_ERR_NO_TRANSITION);
    CHECK(next == FSM_STATE_IDLE && fsm_state(&f) == FSM_STATE_IDLE);
    CHECK(fsm_dispatch(&f, FSM_EV_NONE, &next) == FSM_ERR_NO_TRANSITION);
    CHECK(fsm_state(&f) == FSM_STATE_IDLE);

    /* unknown event, policy FAULT: move to FSM_STATE_FAULT, NO_TRANSITION */
    CHECK(fsm_init(&f, g_tab, TAB_LEN, FSM_STATE_IDLE, FSM_POLICY_FAULT) == FSM_OK);
    CHECK(fsm_dispatch(&f, FSM_EV_STREAM_ERROR, &next) == FSM_ERR_NO_TRANSITION);
    CHECK(next == FSM_STATE_FAULT && fsm_state(&f) == FSM_STATE_FAULT);

    /* first matching row wins */
    CHECK(fsm_init(&f, g_dup, 2, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_OK);
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_COMMAND, &next) == FSM_OK);
    CHECK(next == FSM_STATE_STREAMING);

    /* not initialised (failed init leaves UNSET): TBD, *next = UNSET */
    CHECK(fsm_init(&f, g_tab, 0, FSM_STATE_IDLE, FSM_POLICY_IGNORE) == FSM_ERR_TBD);
    next = FSM_STATE_IDLE;
    CHECK(fsm_dispatch(&f, FSM_EV_CONFIG_COMMAND, &next) == FSM_ERR_TBD);
    CHECK(next == FSM_STATE_UNSET && fsm_state(&f) == FSM_STATE_UNSET);
}

int main(void)
{
    init_cases();
    default_cases();
    dispatch_cases();
    if (g_fail) { printf("test_fsm: %d check(s) failed\n", g_fail); return 1; }
    printf("test_fsm: ok\n");
    return 0;
}
