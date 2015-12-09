/* Main file, keeps all important variables.
 * Calls functions from "helper_functions" to read in input (variables, operators,
 * goals, initial state),
 * then calls functions to build causal graph, domain_transition_graphs and
 * successor generator
 * finally prints output to file "output"
 */

#include "helper_functions.h"
#include "successor_generator.h"
#include "causal_graph.h"
#include "domain_transition_graph.h"
#include "state.h"
#include "mutex_group.h"
#include "operator.h"
#include "axiom.h"
#include "variable.h"
#include <iostream>
#include <stdlib.h>


using namespace std;

int main(int argc, const char **argv) {
	bool metric;
    vector<Variable *> variables;
    vector<Variable> internal_variables;
    State initial_state;
    vector<pair<Variable *, int> > goals;
    vector<MutexGroup> mutexes;
    vector<Operator> operators;
	vector<Operator> abstract_operators;
    vector<Axiom> axioms;
    vector<DomainTransitionGraph> transition_graphs;
	vector<SuccessorGenerator> generators; 

	bool use_abstractions = false;
	vector<string> predicates; 

	if (argc == 2) {
		use_abstractions = (string(argv[1]).compare("--use-abstractions") == 0) ? true:false;
	} else if (argc == 1) {
		cout << "*** do not perform relevance analysis ***" << endl;
     	g_do_not_prune_variables = true;
		use_abstractions = false;
	} else {
		cout << "*** do not perform relevance analysis ***" << endl;
     	g_do_not_prune_variables = true;
	}

    read_preprocessed_problem_description
        (cin, metric, internal_variables, variables, mutexes, initial_state, goals, operators, axioms, predicates, use_abstractions);
    //dump_preprocessed_problem_description
    //  (variables, initial_state, goals, operators, axioms);

    cout << "Building causal graph..." << endl;
    CausalGraph causal_graph(variables, operators, axioms, goals);
    const vector<Variable *> &ordering = causal_graph.get_variable_ordering();
    bool cg_acyclic = causal_graph.is_acyclic();

    // Remove unnecessary effects from operators and axioms, then remove
    // operators and axioms without effects.
    strip_mutexes(mutexes);
    strip_operators(operators);
    strip_axioms(axioms);

    cout << "Building domain transition graphs..." << endl;
    build_DTGs(ordering, operators, axioms, transition_graphs);
    //dump_DTGs(ordering, transition_graphs);
    bool solveable_in_poly_time = false;
    if (cg_acyclic)
        solveable_in_poly_time = are_DTGs_strongly_connected(transition_graphs);
    /*
      TODO: The above test doesn't seem to be quite ok because it
      ignores axioms and it ignores non-unary operators. (Note that the
      causal graph computed here does *not* contain arcs between
      effects, only arcs from preconditions to effects.)

      So solveable_in_poly_time [sic] should also be set to false if
      there are any derived variables or non-unary operators.
     */

    //TODO: genauer machen? (highest level var muss nicht scc sein...gemacht)
    //nur Werte, die wichtig sind fuer drunterliegende vars muessen in scc sein

	if (use_abstractions) {
    	cout << "Generating abstractions based on predicates.." << endl;
	 	detect_abstractions_by_predicates(ordering, predicates);
	 	generate_abstract_operators(operators, abstract_operators);
	 	cout << "Preprocessor abstract operators: " << abstract_operators.size() << endl;   
    }
    cout << "solveable in poly time " << solveable_in_poly_time << endl;
	 
	 cout << "Building successor generator for standard operators ..." << endl;
    SuccessorGenerator successor_generator(ordering, operators);
    //successor_generator.dump();

	 cout << "Building successor generator for abstract operators ..." << endl;
	 SuccessorGenerator successor_generator_abstract(ordering, abstract_operators);

    // Output some task statistics
    int facts = 0;
    int derived_vars = 0;
    for (int i = 0; i < ordering.size(); i++) {
        facts += ordering[i]->get_range();
        if (ordering[i]->is_derived())
            derived_vars++;
    }
    cout << "Preprocessor facts: " << facts << endl;
    cout << "Preprocessor derived variables: " << derived_vars << endl;

    // Calculate the problem size
    int task_size = ordering.size() + facts + goals.size();

    for (int i = 0; i < mutexes.size(); i++)
        task_size += mutexes[i].get_encoding_size();

    for (int i = 0; i < operators.size(); i++)
        task_size += operators[i].get_encoding_size();

    for (int i = 0; i < axioms.size(); i++)
        task_size += axioms[i].get_encoding_size();

    cout << "Preprocessor task size: " << task_size << endl;

    cout << "Writing output..." << endl;

    generate_cpp_input(solveable_in_poly_time, ordering, metric,
                       mutexes, initial_state, goals,
                       operators, abstract_operators, axioms, successor_generator, successor_generator_abstract,
                       transition_graphs, causal_graph, use_abstractions);
    cout << "done" << endl << endl;
}
