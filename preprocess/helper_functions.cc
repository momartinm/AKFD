#include <cstdlib>
#include <iostream>
#include <fstream>

#include <string>
#include <vector>
using namespace std;

#include "helper_functions.h"
#include "state.h"
#include "mutex_group.h"
#include "operator.h"

#include "axiom.h"
#include "variable.h"
#include "successor_generator.h"
#include "domain_transition_graph.h"

#include <boost/algorithm/string.hpp>
using namespace boost;

static const int SAS_FILE_VERSION = 3;
static const int PRE_FILE_VERSION = SAS_FILE_VERSION;


void check_magic(istream &in, string magic) {
    string word;
    in >> word;
    if (word != magic) {
        cerr << "Failed to match magic word '" << magic << "'." << endl;
        cerr << "Got '" << word << "'." << endl;
        if (magic == "begin_version") {
            cerr << "Possible cause: you are running the preprocessor "
                 << "on a translator file from an " << endl
                 << "older version." << endl;
        }
        exit(1);
    }
}

void read_and_verify_version(istream &in) {
    int version;
    check_magic(in, "begin_version");
    in >> version;
    check_magic(in, "end_version");
    if (version != SAS_FILE_VERSION) {
        cerr << "Expected translator file version " << SAS_FILE_VERSION
             << ", got " << version << "." << endl;
        cerr << "Exiting." << endl;
        exit(1);
    }
}

void read_abstractions(istream &in, vector<string> &predicates) {
    int num_predicates;
    check_magic(in, "begin_abstractions");
	in >> num_predicates;

    predicates.resize(num_predicates);

    for (size_t i = 0; i < num_predicates; i++) {
		int num;
		in >> predicates[i] >> num; //It does not work getline. IDK
	}
    check_magic(in, "end_abstractions");
}

void read_metric(istream &in, bool &metric) {
    check_magic(in, "begin_metric");
    in >> metric;
    check_magic(in, "end_metric");
}

void read_variables(istream &in, vector<Variable> &internal_variables,
                    vector<Variable *> &variables) {
    int count;
    in >> count;
    internal_variables.reserve(count);
    // Important so that the iterators stored in variables are valid.
    for (int i = 0; i < count; i++) {
        internal_variables.push_back(Variable(in));
        variables.push_back(&internal_variables.back());
    }
}

void read_mutexes(istream &in, vector<MutexGroup> &mutexes,
                  const vector<Variable *> &variables) {
    size_t count;
    in >> count;
    for (size_t i = 0; i < count; ++i)
        mutexes.push_back(MutexGroup(in, variables));
}

void read_goal(istream &in, const vector<Variable *> &variables,
               vector<pair<Variable *, int> > &goals) {
    check_magic(in, "begin_goal");
    int count;
    in >> count;
    for (int i = 0; i < count; i++) {
        int varNo, val;
        in >> varNo >> val;
        goals.push_back(make_pair(variables[varNo], val));
    }
    check_magic(in, "end_goal");
}

void dump_goal(const vector<pair<Variable *, int> > &goals) {
    cout << "Goal Conditions:" << endl;
    for (int i = 0; i < goals.size(); i++)
        cout << "  " << goals[i].first->get_name() << ": "
             << goals[i].second << endl;
}

void read_operators(istream &in, const vector<Variable *> &variables,
                    vector<Operator> &operators) {
    int count;
    in >> count;
    for (int i = 0; i < count; i++)
        operators.push_back(Operator(in, variables));
}

void read_axioms(istream &in, const vector<Variable *> &variables,
                 vector<Axiom> &axioms) {
    int count;
    in >> count;
    for (int i = 0; i < count; i++)
        axioms.push_back(Axiom(in, variables));
}

void read_preprocessed_problem_description(istream &in,
                                           bool &metric,
                                           vector<Variable> &internal_variables,
                                           vector<Variable *> &variables,
                                           vector<MutexGroup> &mutexes,
                                           State &initial_state,
                                           vector<pair<Variable *, int> > &goals,
                                           vector<Operator> &operators,
                                           vector<Axiom> &axioms,
														 vector<string> &predicates,
														 bool use_abstractions) {
    read_and_verify_version(in);
    read_metric(in, metric);
    read_variables(in, internal_variables, variables);
    read_mutexes(in, mutexes, variables);
    initial_state = State(in, variables);
    read_goal(in, variables, goals);
    read_operators(in, variables, operators);
    read_axioms(in, variables, axioms);
    if (use_abstractions) read_abstractions(in, predicates);
}

void dump_preprocessed_problem_description(const vector<Variable *> &variables,
                                           const State &initial_state,
                                           const vector<pair<Variable *, int> > &goals,
                                           const vector<Operator> &operators,
                                           const vector<Axiom> &axioms) {
    cout << "Variables (" << variables.size() << "):" << endl;
    for (int i = 0; i < variables.size(); i++)
        variables[i]->dump();

    cout << "Initial State:" << endl;
    initial_state.dump();
    dump_goal(goals);

    for (int i = 0; i < operators.size(); i++)
        operators[i].dump();
    for (int i = 0; i < axioms.size(); i++)
        axioms[i].dump();
}

void dump_DTGs(const vector<Variable *> &ordering,
               vector<DomainTransitionGraph> &transition_graphs) {
    for (int i = 0; i < transition_graphs.size(); i++) {
        cout << "Domain transition graph for " << ordering[i]->get_name() << ":" << endl;
        transition_graphs[i].dump();
    }
}

void generate_cpp_input(bool /*solvable_in_poly_time*/,
                        const vector<Variable *> &ordered_vars,
                        const bool &metric,
                        const vector<MutexGroup> &mutexes,
                        const State &initial_state,
                        const vector<pair<Variable *, int> > &goals,
                        const vector<Operator> &operators,
								const vector<Operator> &abstract_operators,
                        const vector<Axiom> &axioms,
                        const SuccessorGenerator &sg,
								const SuccessorGenerator &sga,
                        const vector<DomainTransitionGraph> transition_graphs,
                        const CausalGraph &cg,
								/*const int horizon,*/
								const bool use_abstractions) 
{
    /* NOTE: solvable_in_poly_time flag is no longer included in output,
       since the planner doesn't handle it specially any more anyway. */

    ofstream outfile;
    outfile.open("output", ios::out);

    outfile << "begin_version" << endl;
    outfile << PRE_FILE_VERSION << endl;
    outfile << "end_version" << endl;

	outfile << "begin_abstractions" << endl;
	if (use_abstractions) {
		outfile << "1" << endl; //use abstractions
    }
	else {
		outfile << "0" << endl; //use abstractions
	}	 
    outfile << "end_abstractions" << endl;

    outfile << "begin_metric" << endl;
    outfile << metric << endl;
    outfile << "end_metric" << endl;

    outfile << ordered_vars.size() << endl;
    for (int i = 0; i < ordered_vars.size(); i++)
        ordered_vars[i]->generate_cpp_input(outfile);

    outfile << mutexes.size() << endl;
    for (int i = 0; i < mutexes.size(); i++)
        mutexes[i].generate_cpp_input(outfile);

    int var_count = ordered_vars.size();
    outfile << "begin_state" << endl;
    for (int i = 0; i < var_count; i++)
        outfile << initial_state[ordered_vars[i]] << endl;  // for axioms default value
    outfile << "end_state" << endl;

    vector<int> ordered_goal_values;
    ordered_goal_values.resize(var_count, -1);
    for (int i = 0; i < goals.size(); i++) {
        int var_index = goals[i].first->get_level();
        ordered_goal_values[var_index] = goals[i].second;
    }
    outfile << "begin_goal" << endl;
    outfile << goals.size() << endl;
    for (int i = 0; i < var_count; i++)
        if (ordered_goal_values[i] != -1)
            outfile << i << " " << ordered_goal_values[i] << endl;
    outfile << "end_goal" << endl;

    outfile << operators.size() << endl;
    for (int i = 0; i < operators.size(); i++)
        operators[i].generate_cpp_input(outfile);
    
	 outfile << abstract_operators.size() << endl;
    if (use_abstractions) {
	 	for (int i = 0; i < abstract_operators.size(); i++)
      	abstract_operators[i].generate_cpp_input(outfile);
	 }

    outfile << axioms.size() << endl;
    for (int i = 0; i < axioms.size(); i++)
        axioms[i].generate_cpp_input(outfile);

	 outfile << "begin_SG_0" << endl;
    sg.generate_cpp_input(outfile);
    outfile << "end_SG_0" << endl;

	 if (use_abstractions) {
		outfile << "begin_SG_1" << endl;
    	sga.generate_cpp_input(outfile);
    	outfile << "end_SG_1" << endl;
	 }

    for (int i = 0; i < var_count; i++) {
        outfile << "begin_DTG" << endl;
        transition_graphs[i].generate_cpp_input(outfile);
        outfile << "end_DTG" << endl;
    }

    outfile << "begin_CG" << endl;
    cg.generate_cpp_input(outfile, ordered_vars);
    outfile << "end_CG" << endl;

    outfile.close();
}

void detect_abstractions_by_predicates(const vector<Variable *> &ordered_vars, const vector<string> predicates) {
	vector<string> parts;
	
	for (int i = 0; i < ordered_vars.size(); i++){		

		for (int j = 0; j < ordered_vars[i]->get_range(); j++) {

			string fact_name = ordered_vars[i]->get_fact_name(j);

			boost::split(parts, fact_name, boost::is_any_of("(), "));
					
			for (int p = 0; p < predicates.size(); p++) {
				if (predicates[p].compare(parts[1]) == 0) {
					ordered_vars[i]->mark_fact_as_abstract(j);						
					break;
				}
			}
		}
	}
}

vector<Operator::Prevail> generate_new_prevail(const vector<Operator::Prevail> &prevail) {
	vector<Operator::Prevail> new_prevail;

	for (int i = 0; i < prevail.size(); i++) {
		if (!prevail[i].var->is_fact_abstracted(prevail[i].prev))
			new_prevail.push_back(Operator::Prevail(prevail[i].var, prevail[i].prev));
	}

	return new_prevail;
}

vector<Operator::PrePost> generate_new_prepost(const vector<Operator::PrePost> prepost) {
	vector<Operator::PrePost> new_prepost;
	for (int i = 0; i < prepost.size(); i++) {		
		if (prepost[i].pre != -1) {		
			if (!prepost[i].var->is_fact_abstracted(prepost[i].pre)) {
				if (!prepost[i].var->is_fact_abstracted(prepost[i].post)) {				
					new_prepost.push_back(Operator::PrePost(prepost[i].var, prepost[i].effect_conds, prepost[i].pre, prepost[i].post));
				}		
			}
			else if (!prepost[i].var->is_fact_abstracted(prepost[i].post)) {
				if (!prepost[i].var->is_fact_abstracted(prepost[i].post)) {
					new_prepost.push_back(Operator::PrePost(prepost[i].var, prepost[i].effect_conds, -1, prepost[i].post));
				}
			}		
		}
		else {//-1 if there is no particular value that the variable must have
			if (!prepost[i].var->is_fact_abstracted(prepost[i].post))
				new_prepost.push_back(Operator::PrePost(prepost[i].var, prepost[i].effect_conds, prepost[i].pre, prepost[i].post));
		}
	}
	return new_prepost;
}

void generate_abstract_operators(const vector<Operator> &operators, vector<Operator> &abstract_operators) {
	std::vector<Operator::Prevail> prevail;
	std::vector<Operator::PrePost> prepost;
	vector<string> parts;
	string name;

	for (int i = 0; i < operators.size(); i++) {
		prepost = generate_new_prepost(operators[i].get_pre_post());

		if (prepost.size() > 0) {
			prevail = generate_new_prevail(operators[i].get_prevail());
			abstract_operators.push_back(Operator(operators[i].get_name(), operators[i].get_cost(), prevail, prepost, 2));
		}
	}

	cout << abstract_operators.size() << " abstract operators of " << operators.size() << " operators." << endl;	
}
