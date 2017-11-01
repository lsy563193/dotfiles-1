//
// Created by lsy563193 on 7/7/17.
//

#ifndef PP_MOVETYPE_H
#define PP_MOVETYPE_H

#include "mathematics.h"
#include "path_planning.h"

typedef enum {
	CM_LINEARMOVE = 0,
	CM_CURVEMOVE,
	CM_FOLLOW_LEFT_WALL,
	CM_FOLLOW_RIGHT_WALL,
	CM_GO_TO_CHARGER,
} CMMoveType;

bool mt_is_right();

bool mt_is_left();

bool mt_is_follow_wall();

bool mt_is_linear();

bool mt_is_go_to_charger();

CMMoveType mt_get();

void mt_update(const Cell_t& curr, PPTargetType& path);

void mt_set(CMMoveType mt);
/*
class MoveType {

};
*/


#endif //PP_MOVETYPE_H
