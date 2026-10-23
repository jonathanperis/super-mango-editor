/*
 * snapshot_equal.h — Field comparison for undo snapshots.
 *
 * PlacementData is a union and LevelConfigSnapshot holds floats, so neither
 * has one canonical byte pattern: padding and the union's unused tail can
 * differ when the fields do not, and a float's bits are the edit (-0 and 0
 * are different values to the property panel). Compare the fields.
 */

#ifndef SNAPSHOT_EQUAL_H
#define SNAPSHOT_EQUAL_H

#include "editor.h" /* EntityType, PlacementData, LevelConfigSnapshot */

/* 1 when the active member for `type` matches, including float bits. */
int placement_same(EntityType type, const PlacementData *a, const PlacementData *b);

/* 1 when every config field matches, including float bits. */
int config_snapshot_same(const LevelConfigSnapshot *a, const LevelConfigSnapshot *b);

#endif /* SNAPSHOT_EQUAL_H */
