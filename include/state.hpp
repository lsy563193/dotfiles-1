//
// Created by lsy563193 on 12/4/17.
//

#ifndef PP_STATE_HPP
#define PP_STATE_HPP

//#include <boost/shared_ptr.hpp>
class ACleanMode;
#include "move_type_plan.hpp"
class State {
public:
//	virtual IMoveType* getNextMoveType()=0;

	bool isFinish(ACleanMode* p_mode);

protected:
	static boost::shared_ptr<IMoveType> sp_move_type_;

};

class StateClean: public State {
public:
	StateClean();
//	IMoveType* getNextMoveType();

public:
};

class StateGoHomePoint: public State {
public:
	StateGoHomePoint();
//	IMoveType* getNextMoveType();

protected:
	int gh_state_{};

private:
	enum {gh_ing=0,gh_succuss,gh_faile};
};

class StateGoCharger: public State {
public:
	StateGoCharger();
//	IMoveType* getNextMoveType();

};

class StateTrapped: public State {
public:
	StateTrapped();
//	IMoveType* getNextMoveType();
};

class StateTmpSpot: public State {
public:
	StateTmpSpot();
//	IMoveType* getNextMoveType();
};

class StateSelfCheck: public State {
public:
	StateSelfCheck();
//	IMoveType* getNextMoveType();
};

class StateExploration: public State {
public:
	StateExploration();
//	IMoveType* getNextMoveType();
};

//Movement *StateClean::generateMovement(PathType path, const Cell_t &curr) {
//	return nullptr;
//}

#endif //PP_STATE_HPP
