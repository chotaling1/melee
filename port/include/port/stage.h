#ifndef PORT_STAGE_H
#define PORT_STAGE_H

/// Synthetic stages built natively at runtime (no stage DAT is loaded).
///
/// MELEE_PORT_STAGE=line replaces Final Destination (St_Kind_Last /
/// Gr_Kind_Last) with a data-free "line" stage: FD's collision geometry,
/// general points (spawns, camera range, blast zones) and ground params,
/// hand-transcribed as numeric constants, with no models, particles, items
/// or yakumono. See port/src/stage_line.c.

#include <melee/gr/forward.h>

/// Returns the StageData to use for `grkind`: the synthetic stage if it
/// replaces that ground kind, else `orig`.
StageData* port_stage_override(GrKind grkind, StageData* orig);

/// Called right after grDatFiles_801C6038() for a synthetic stage (whose
/// StageData::data1 is NULL, so the DAT-less default path ran): installs
/// the native GroundParam and collision data into stage_info.
void port_stage_load(StageData* stage);

#endif
