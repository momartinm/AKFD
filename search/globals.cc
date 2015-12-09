#include "globals.h"

#include "axioms.h"
#include "causal_graph.h"
#include "domain_transition_graph.h"
#include "heuristic.h"
#include "legacy_causal_graph.h"
#include "operator.h"
#include "rng.h"
#include "state.h"
#include "successor_generator.h"
#include "timer.h"
#include "utilities.h"
#include "landmarks/landmark_graph.h"
#include "successor_generator.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <sstream>
using namespace std;

#include <ext/hash_map>
using namespace __gnu_cxx;


static const int PRE_FILE_VERSION = 3;


// TODO: This needs a proper type and should be moved to a separate
//       mutexes.cc file or similar, accessed via something called
//       g_mutexes. (Right now, the interface is via global function
//       are_mutex, which is at least better than exposing the data
//       structure globally.)

static vector<vector<set<pair<int, int> > > > g_inconsistent_facts;

bool test_goal(const State &state) {
    for (int i = 0; i < g_goal.size(); i++) {
        if (state[g_goal[i].first] != g_goal[i].second) {
            return false;
        }
    }
    return true;
}

int calculate_plan_cost(const vector<const Operator *> &plan) {
    // TODO: Refactor: this is only used by save_plan (see below)
    //       and the SearchEngine classes and hence should maybe
    //       be moved into the SearchEngine (along with save_plan).
    int plan_cost = 0;
    for (int i = 0; i < plan.size(); i++) {
        plan_cost += plan[i]->get_cost();
    }
    return plan_cost;
}

void save_plan(const vector<const Operator *> &plan, int iter) {
    // TODO: Refactor: this is only used by the SearchEngine classes
    //       and hence should maybe be moved into the SearchEngine.
    ofstream outfile;
    if (iter == 0) {
        outfile.open(g_plan_filename.c_str(), ios::out);
    } else {
        ostringstream out;
        out << g_plan_filename << "." << iter;
        outfile.open(out.str().c_str(), ios::out);
    }
    for (int i = 0; i < plan.size(); i++) {
        cout << plan[i]->get_name() << " (" << plan[i]->get_cost() << ")" << endl;
        outfile << "(" << plan[i]->get_name() << ")" << endl;
    }
    outfile.close();
    int plan_cost = calculate_plan_cost(plan);
    ofstream statusfile;
    statusfile.open("plan_numbers_and_cost", ios::out | ios::app);
    statusfile << iter << " " << plan_cost << endl;
    statusfile.close();
    cout << "Plan length: " << plan.size() << " step(s)." << endl;
    cout << "Plan cost: " << plan_cost << endl;
}

bool peek_magic(istream &in, string magic) {
    string word;
    in >> word;
    bool result = (word == magic);
    for (int i = word.size() - 1; i >= 0; i--)
        in.putback(word[i]);
    return result;
}

void check_magic(istream &in, string magic) {
    string word;
    in >> word;
    if (word != magic) {
        cout << "Failed to match magic word '" << magic << "'." << endl;
        cout << "Got '" << word << "'." << endl;
        if (magic == "begin_version") {
            cerr << "Possible cause: you are running the planner "
                 << "on a preprocessor file from " << endl
                 << "an older version." << endl;
        }
        exit_with(EXIT_INPUT_ERROR);
    }
}

void read_and_verify_version(istream &in) {
    int version;
    check_magic(in, "begin_version");
    in >> version;
    check_magic(in, "end_version");
    if (version != PRE_FILE_VERSION) {
        cerr << "Expected preprocessor file version " << PRE_FILE_VERSION
             << ", got " << version << "." << endl;
        cerr << "Exiting." << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
}

void read_abstractions(istream &in) {

    check_magic(in, "begin_abstractions");
    in >> g_use_abstractions;
    check_magic(in, "end_abstractions");
}

void read_metric(istream &in) {
    check_magic(in, "begin_metric");
    in >> g_use_metric;
    check_magic(in, "end_metric");
}

void read_variables(istream &in) {
 	 int count;
    in >> count;
    for (int i = 0; i < count; i++) {
        check_magic(in, "begin_variable");
        string name;
        in >> name;
        g_variable_name.push_back(name);
        int layer;
        in >> layer;
        g_axiom_layers.push_back(layer);
        int range;
        in >> range;

        g_variable_domain.push_back(range);
        if (range > numeric_limits<state_var_t>::max()) {
            cerr << "This should not have happened!" << endl;
            cerr << "Are you using the downward script, or are you using "
                 << "downward-1 directly?" << endl;
            exit_with(EXIT_INPUT_ERROR);
        }

        in >> ws;
        vector<string> fact_names(range);
        for (size_t i = 0; i < fact_names.size(); i++){
            getline(in, fact_names[i]);
			}		
        g_fact_names.push_back(fact_names);
        check_magic(in, "end_variable");
    }
}

void read_mutexes(istream &in) {
    g_inconsistent_facts.resize(g_variable_domain.size());
    for (size_t i = 0; i < g_variable_domain.size(); ++i)
        g_inconsistent_facts[i].resize(g_variable_domain[i]);

    int num_mutex_groups;
    in >> num_mutex_groups;

    /* NOTE: Mutex groups can overlap, in which case the same mutex
       should not be represented multiple times. The current
       representation takes care of that automatically by using sets.
       If we ever change this representation, this is something to be
       aware of. */

    for (size_t i = 0; i < num_mutex_groups; ++i) {
        check_magic(in, "begin_mutex_group");
        int num_facts;
        in >> num_facts;
        vector<pair<int, int> > invariant_group;
        invariant_group.reserve(num_facts);
        for (size_t j = 0; j < num_facts; ++j) {
            int var, val;
            in >> var >> val;
            invariant_group.push_back(make_pair(var, val));
        }
        check_magic(in, "end_mutex_group");
        for (size_t j = 0; j < invariant_group.size(); ++j) {
            const pair<int, int> &fact1 = invariant_group[j];
            int var1 = fact1.first, val1 = fact1.second;
            for (size_t k = 0; k < invariant_group.size(); ++k) {
                const pair<int, int> &fact2 = invariant_group[k];
                int var2 = fact2.first;
                if (var1 != var2) {
                    /* The "different variable" test makes sure we
                       don't mark a fact as mutex with itself
                       (important for correctness) and don't include
                       redundant mutexes (important to conserve
                       memory). Note that the preprocessor removes
                       mutex groups that contain *only* redundant
                       mutexes, but it can of course generate mutex
                       groups which lead to *some* redundant mutexes,
                       where some but not all facts talk about the
                       same variable. */
                    g_inconsistent_facts[var1][val1].insert(fact2);
                }
            }
        }
    }
}

void read_goal(istream &in) {
    check_magic(in, "begin_goal");
    int count;
    in >> count;
    for (int i = 0; i < count; i++) {
        int var, val;
        in >> var >> val;
        g_goal.push_back(make_pair(var, val));
    }
    check_magic(in, "end_goal");
}

void dump_goal() {
    cout << "Goal Conditions:" << endl;
    for (int i = 0; i < g_goal.size(); i++)
        cout << "  " << g_variable_name[g_goal[i].first] << ": "
             << g_goal[i].second << endl;
}

void read_operators(istream &in) {
    int count;
    in >> count;
    for (int i = 0; i < count; i++)
        g_operators.push_back(Operator(in, false, false));
	 
	in >> count;
	for (int i = 0; i < count; i++)
		g_abstract_operators.push_back(Operator(in, false, true));

	//cout << "ABS: " << g_abstract_operators.size() << endl;
}

void read_axioms(istream &in) {
    int count;
    in >> count;
    for (int i = 0; i < count; i++)
        g_axioms.push_back(Operator(in, true, false));

    g_axiom_evaluator = new AxiomEvaluator;
    g_axiom_evaluator->evaluate(*g_initial_state);
}

vector<pair<string, int> > load_abstractions(string filename) {

	vector<pair<string, int> > predicates;

	int num_abstractions;
	ifstream in;
 	in.open(filename.c_str());

	if (in.is_open()) {
		check_magic(in, "begin_abstractions");
		in >> num_abstractions;
		predicates.resize(num_abstractions);
		for (int i = 0; i < num_abstractions; i++) {
			string name;
			int num;		
			in >> name >> num;
			predicates[i] = make_pair(name, num);
		}	
		check_magic(in, "end_abstractions");
	}
	in.close();

	return predicates;
}


void read_everything(istream &in) {
    read_and_verify_version(in);
    read_abstractions(in);  
    read_metric(in);
    read_variables(in);
    read_mutexes(in);
    g_initial_state = new State(in);
    read_goal(in);
    read_operators(in);
    read_axioms(in);
    check_magic(in, "begin_SG_0");
    //g_successor_generator = read_successor_generator(in);
    g_successor_generators.push_back(read_successor_generator(in, 0));
	check_magic(in, "end_SG_0");	
	if (g_use_abstractions) {
		check_magic(in, "begin_SG_1");
		g_successor_generators.push_back(read_successor_generator(in, 1));    	
		check_magic(in, "end_SG_1");
	}
    DomainTransitionGraph::read_all(in);
    g_legacy_causal_graph = new LegacyCausalGraph(in);

    // NOTE: causal graph is computed from the problem specification,
    // so must be built after the problem has been read in.
    g_causal_graph = new CausalGraph; 
}

void dump_everything() {
    cout << "Use metric? " << g_use_metric << endl;
    cout << "Min Action Cost: " << g_min_action_cost << endl;
    cout << "Max Action Cost: " << g_max_action_cost << endl;
    // TODO: Dump the actual fact names.
    cout << "Variables (" << g_variable_name.size() << "):" << endl;
    for (int i = 0; i < g_variable_name.size(); i++)
        cout << "  " << g_variable_name[i]
             << " (range " << g_variable_domain[i] << ")" << endl;
    cout << "Initial State (PDDL):" << endl;
    g_initial_state->dump_pddl();
    cout << "Initial State (FDR):" << endl;
    g_initial_state->dump_fdr();
    dump_goal();
    /*
    cout << "Successor Generator:" << endl;
    g_successor_generator->dump();
    for(int i = 0; i < g_variable_domain.size(); i++)
      g_transition_graphs[i]->dump();
    */
}

vector<string> split(string str, char delimiters[]) {
  vector<string> internal;
  string tok;
  int start = 0;

  for (int i=0; i<str.length(); ++i) {
	for (int j=0; j < strlen(delimiters); j++) {
		if (str[i] == delimiters[j]) {
			internal.push_back(str.substr(start, (i-start)));
			start = i+1;			
		}	
	}
  }

  return internal;
}

vector<pair<string, int> > generate_abstraction_based_in_landmarks() {
	vector<pair<string, int> > predicates;
	vector<string> goals;
	vector<string> tokens;
	int p;
	char delimiters[] = {' ', ',', '(', ')'}; 

	for (int i = 0; i < g_goal.size(); i++) {

		tokens = split(g_fact_names[g_goal[i].first][g_goal[i].second], delimiters);
		
		for (p = 0; p < goals.size(); p++) {
			if (goals[p].compare(tokens[1]) == 0) {
				break;			
			}
		}

		if (p == goals.size()) {
			goals.push_back(tokens[1]);	
		}
	}

	for (int i = 0; i < g_lm_graph->number_of_landmarks(); i++) {
		LandmarkNode *node_p = g_lm_graph->get_lm_for_index(i);
		if (!node_p->in_goal) {
			for (int j = 0; j < node_p->vars.size(); j++) {
				for (int k = 0; k < node_p->vals.size(); k++) {
					//cout << "ENTRO(" << j << ", " << k << ") = " << node_p->in_goal << endl;
					//cout << g_fact_names[node_p->vars[j]][node_p->vals[k]] << endl;
					
					tokens = split(g_fact_names[node_p->vars[j]][node_p->vals[k]], delimiters);	

					for (p = 0; p < goals.size(); p++) {			
						if (goals[p].compare(tokens[1]) == 0) {
							break;			
						}
					}	

					if (p == goals.size()) {
						for (p = 0; p < predicates.size(); p++) {
							if (predicates[p].first.compare(tokens[1]) == 0) {
								predicates[p].second += 1;
								break; //Previously inserted.
							}
						}
						if (p == predicates.size()) {
							predicates.push_back(make_pair(tokens[1], 1));				
						}
					}
				}
			}
		}
	}

	cout << "Generated " << predicates.size() << " abstractions" << endl;

	return predicates;
}


vector<pair<string, int> > learn_abstraction_based_in_landmarks(vector<pair<string, int> > predicates) {
	vector<string> goals;
	vector<string> tokens;
	char delimiters[] = {' ', ',', '(', ')'}; 

	int p;

	for (int i = 0; i < g_goal.size(); i++) {

		tokens = split(g_fact_names[g_goal[i].first][g_goal[i].second], delimiters);

		//cout << tokens[1] << endl;

		for (p = 0; p < goals.size(); p++) {
			if (goals[p].compare(tokens[1]) == 0) {
				break;			
			}
		}

		if (p == goals.size()) {
			goals.push_back(tokens[1]);	
		}
	}

	for (int i = 0; i < g_lm_graph->number_of_landmarks(); i++) {
		LandmarkNode *node_p = g_lm_graph->get_lm_for_index(i);
		if (!node_p->in_goal) {
			for (int j = 0; j < node_p->vars.size(); j++) {

				tokens = split(g_fact_names[node_p->vars[j]][node_p->vals[j]], delimiters);
				
				for (p = 0; p < goals.size(); p++) {			
					if (goals[p].compare(tokens[1]) == 0) {
						break;			
					}
				}

				if (p == goals.size()) {	
					for (p = 0; p < predicates.size(); p++) {
						if (predicates[p].first.compare(tokens[1]) == 0) {
							predicates[p].second += 1;									
							break; //Previously inserted.
						}
					}
					if (p == predicates.size()) {
						predicates.push_back(make_pair(tokens[1], 1));				
					}
				}
			}
		}
	}

	cout << "Learning " << predicates.size() << " abstractions" << endl;

	return predicates;
}

void modify_output_file(vector<pair<string, int> > predicates, string filename) {

	ofstream outFile;
  	outFile.open (filename.c_str(), ios::out);

   outFile << "begin_abstractions" << endl;
	outFile << predicates.size() << endl;
	 
	for (size_t i = 0; i < predicates.size(); i++)
	{
		outFile << predicates[i].first << " " << predicates[i].second << endl;
 	}

	outFile << "end_abstractions" << endl;	  	
	outFile.close();	

	cout << "Added abstractions info in abstractions.sas file." << endl;
}

void verify_no_axioms_no_cond_effects() {
    if (!g_axioms.empty()) {
        cerr << "Heuristic does not support axioms!" << endl << "Terminating."
             << endl;
        exit_with(EXIT_UNSUPPORTED);
    }

    for (int i = 0; i < g_operators.size(); i++) {
        const vector<PrePost> &pre_post = g_operators[i].get_pre_post();
        for (int j = 0; j < pre_post.size(); j++) {
            const vector<Prevail> &cond = pre_post[j].cond;
            if (cond.empty())
                continue;
            // Accept conditions that are redundant, but nothing else.
            // In a better world, these would never be included in the
            // input in the first place.
            int var = pre_post[j].var;
            int pre = pre_post[j].pre;
            int post = pre_post[j].post;
            if (pre == -1 && cond.size() == 1 && cond[0].var == var
                && cond[0].prev != post && g_variable_domain[var] == 2)
                continue;

            cerr << "Heuristic does not support conditional effects "
                 << "(operator " << g_operators[i].get_name() << ")" << endl
                 << "Terminating." << endl;
            exit_with(EXIT_UNSUPPORTED);
        }
    }
}

bool are_mutex(const pair<int, int> &a, const pair<int, int> &b) {
    if (a.first == b.first) // same variable: mutex iff different value
        return a.second != b.second;
    return bool(g_inconsistent_facts[a.first][a.second].count(b));
}

bool g_use_metric;
int g_min_action_cost = numeric_limits<int>::max();
int g_max_action_cost = 0;
vector<string> g_variable_name;
vector<int> g_variable_domain;
vector<vector<string> > g_fact_names;
vector<int> g_axiom_layers;
vector<int> g_default_axiom_values;
State *g_initial_state;
vector<pair<int, int> > g_goal;
vector<Operator> g_operators;
vector<Operator> g_axioms;
AxiomEvaluator *g_axiom_evaluator;
//SuccessorGenerator *g_successor_generator;
vector<DomainTransitionGraph *> g_transition_graphs;
CausalGraph *g_causal_graph;
LegacyCausalGraph *g_legacy_causal_graph;

int operator_level;
vector<SuccessorGenerator *> g_successor_generators; //Modification MMM
LandmarkGraph *g_lm_graph; //Modification MMM
bool g_generate_abstractions; //Modification MMM
bool g_use_abstractions; //Modification MMM
bool g_learn_abstractions; //Modification MMM
int g_horizon; //Modification MMM
vector<Operator> g_abstract_operators; //Modification MMM
string g_abstraction_filename;
//SuccessorGenerator *g_abstract_successor_generator; //Modification MMM

Timer g_timer;
string g_plan_filename = "sas_plan";
RandomNumberGenerator g_rng(2011); // Use an arbitrary default seed.
