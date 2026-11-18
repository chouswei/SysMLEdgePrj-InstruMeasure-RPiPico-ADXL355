# Agent Config: InstruMeasure / RPi Pico - ADXL2358 (Pico355)

**Scope:** SysML modeling | Raspberry Pi Pico + Analog Devices ADXL355B raw XYZ streaming to a PC over USB  
**Lane:** InstruMeasure only. Never Weft: no Weft repos, boards, libraries, or naming.  
**Layout:** standalone `SysMLEdgePrj-*` desk.  
**Models:** bound working SSOT = SysMLEdge graph (`sysmledge` / `user-sysmledge`, `projectId` `sysmledgeprj-instrumeasure-rpipico-adxl355`); backup = `sysml-models/` (export PR after a human Save); tip MemNet = agent memory, not the graph.  
**Product face:** Cursor MCP `sysmledge` after human-gated `openProject`. MUST NOT invent `projectId`. MUST NOT Foam.

The project title keeps the typo "ADXL2358". The sensor is the **ADXL355B (ADXL355BEZ)**, digital output. It is **not** the ADXL358.

## Audience (LLM-first)

You are the assistant of a human system engineer. Model first. Code and `docs/` trail the model. `docs/` is not SSOT.

**MUST:**
- SysML first: change the working model on the desk (`rev_status` / `ask` / `gql` / product `pin_map`, then `propose`) before code or docs.
- Propose only. The owner (Szu-Wei Chou) merges every desk proposal (human Save via `mergeProposal`) and every PR in this repo.
- Mark every unconfirmed number as TBD (clock speeds, pins, framing, protocol, HMI form, filter settings).

**MUST NOT:**
- Invent numbers, specs, part numbers, SKUs or prices. Cite the datasheet or the owner.
- Push to `main`, force push, or merge PRs.
- Treat `sysml-models/` (backup) or `sysml-models/proposals/` as SSOT, or hand-edit the backup as the master copy.
- Touch any Weft surface. tip != face.

## Packages

Requirement prefix `PICO355-R`. Packages: `Pico355` (structure), `Pico355Requirements`, `Pico355Root` (private imports only, last in load order).

## Where to look

| Need | Path |
|------|------|
| Identity | `project.toml` (`[project].desk_project_id`) |
| Working model SSOT (bound) | SysMLEdge graph via `rev_status` / `ask` / `gql` / product `pin_map` / `propose` |
| Backup export | `sysml-models/models/`, `sysml-models/applied/` (human Save copies desk -> backup) |
| Agent proposals (not SSOT) | `propose` on product `sysmledge` |
| Operator notes | `docs/` (not SSOT) |

## Confirmed facts (owner, 2026-11-18)

- Board: Raspberry Pi Pico.
- Sensor: ADXL355B, digital (SPI or I2C), 20-bit, +/-2/+/-4/+/-8 g, 2.25 V to 3.6 V supply. Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/adxl354_adxl355.pdf (Rev. D).
- Goal: stream raw XYZ to a PC over USB at 4000 Hz (full ODR) by default.
- A PC-side HMI configures ODR, range and filters at runtime over USB. Protocol and HMI form: TBD.
- Proposed (pending owner approval): SPI to the Pico, FIFO burst reads, USB CDC serial.

## Skill pick

1. `sysml-modeling-session-checklist`
2. `sysml-modeling-workflow`
3. Pack `sysmledge` (propose-only; human Save + reproject)

Product face is `sysmledge`, never `foam-beachhead`. `sysmledgraph` is abandoned.
