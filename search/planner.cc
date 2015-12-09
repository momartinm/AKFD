#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "ext/tree_util.hh"
#include "timer.h"
#include "utilities.h"
#include "search_engine.h"

#include <iostream>
#include <new>
using namespace std;

int main(int argc, const char **argv) {
	register_event_handlers();

  	if (argc < 2) {
    	cout << OptionParser::usage(argv[0]) << endl;
  		exit_with(EXIT_INPUT_ERROR);
 	}

 	if (string(argv[1]).compare("--help") != 0)
   		read_everything(cin);

   SearchEngine *engine = 0;
	operator_level = 0;
	Timer search_timer;

    //the input will be parsed twice:
    //once in dry-run mode, to check for simple input errors,
    //then in normal mode
	try {
    	OptionParser::parse_cmd_line(argc, argv, true);
    	engine = OptionParser::parse_cmd_line(argc, argv, false);
    } catch (ParseError &pe) {
   		cerr << pe << endl;
		exit_with(EXIT_INPUT_ERROR);
    } 

	if (g_generate_abstractions) {	
		cout << "Generating abstration using landmarks ..." << endl;
		modify_output_file(generate_abstraction_based_in_landmarks(), "abstractions.sas");
		g_timer.stop();
		exit_with(EXIT_PLAN_FOUND);
	} else if (g_learn_abstractions) {
		cout << "Learning abstration using landmarks ..." << endl;
		modify_output_file(learn_abstraction_based_in_landmarks(load_abstractions(g_abstraction_filename)), g_abstraction_filename);
		g_timer.stop();
    	exit_with(EXIT_PLAN_FOUND);
	} else {
	   	engine->search();
		search_timer.stop();
		g_timer.stop();

		engine->save_plan_if_necessary();
		engine->statistics();
		engine->heuristic_statistics();
		cout << "Search time: " << search_timer << endl;
		cout << "Total time: " << g_timer << endl;

		if (engine->found_solution()) {
			exit_with(EXIT_PLAN_FOUND);
		} else {
			exit_with(EXIT_UNSOLVED_INCOMPLETE);
		}
	}
}
