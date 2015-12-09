#include "ff_abs_heuristic.h"

#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "state.h"

#include <cassert>
#include <vector>
using namespace std;

#include <ext/hash_map>
using namespace __gnu_cxx;

// construction and destruction
FFAbsHeuristic::FFAbsHeuristic(const Options &opts)
    : AdditiveHeuristic(opts) {
}

FFAbsHeuristic::~FFAbsHeuristic() {
}

// initialization
void FFAbsHeuristic::initialize() {
    cout << "Initializing FF heuristic using different operator spaces..." << endl;
    AdditiveHeuristic::initialize();

	relaxed_plan.resize(2);
	relaxed_plan[0].resize(g_operators.size(), false);
	relaxed_plan[1].resize(g_abstract_operators.size(), false);
}

void FFAbsHeuristic::mark_preferred_operators_and_relaxed_plan(
	const State &state, Proposition *goal) { 
	if (!goal->marked) { // Only consider each subgoal once.
		goal->marked = true;

	UnaryOperator *unary_op = goal->reached_by;

	if (unary_op) { // We have not yet chained back to a start node.          
		for (int i = 0; i < unary_op->precondition.size(); i++)
			mark_preferred_operators_and_relaxed_plan(
		state, unary_op->precondition[i]);

		int operator_no = unary_op->operator_no;

		if (operator_no != -1) {
			if (operator_no <= relaxed_plan[operator_level].size()) {
				//cout << "OP: " << operator_no << " is abstract " << unary_op->is_abstract << endl;
				//cout << operator_level << endl;
				//cout << relaxed_plan[operator_level].size() << endl;
				//cout << relaxed_plan[operator_level][operator_no] << endl;
				//cout << unary_op->is_abstract << endl;

				// This is not an axiom.
				relaxed_plan[operator_level][operator_no] = true;

				if (unary_op->cost == unary_op->base_cost) {
				// This test is implied by the next but cheaper,
				// so we perform it to save work.
				// If we had no 0-cost operators and axioms to worry
				// about, it would also imply applicability.
				const Operator *op = (operator_level == 1) ? &g_abstract_operators[operator_no]:&g_operators[operator_no];
				if (op->is_applicable(state))
					set_preferred(op);
				}
				}
			}
		}
	}
}

int FFAbsHeuristic::compute_heuristic(const State &state) {
	int h_add = compute_add_and_ff(state);

    if (h_add == DEAD_END)
        return h_add;

    // Collecting the relaxed plan also sets the preferred operators.
    for (int i = 0; i < goal_propositions.size(); i++) {
   		mark_preferred_operators_and_relaxed_plan(state, goal_propositions[i]);
	}

    int h_ff = 0;
		
	for (int op_no = 0; op_no < relaxed_plan[operator_level].size(); op_no++) {
	    
		if (relaxed_plan[operator_level][op_no]) {
	        relaxed_plan[operator_level][op_no] = false; // Clean up for next computation.
			//cout << "OP[" << h_ff << "]" << endl;
			//if (operator_level == 0) g_operators[op_no].dump();
			h_ff += (operator_level == 0) ? get_adjusted_cost(g_operators[op_no]):get_adjusted_cost(g_abstract_operators[op_no]);			
		}
	}
	
	//cout << h_ff << endl;

   return h_ff;
}

static ScalarEvaluator *_parse(OptionParser &parser) {
    Heuristic::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return new FFAbsHeuristic(opts);
}

static Plugin<ScalarEvaluator> _plugin("ffabs", _parse);
