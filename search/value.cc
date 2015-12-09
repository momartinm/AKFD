#include "globals.h"
#include "value.h"

using namespace std;

Value::Value(std::string &line) {

	std::replace_if(line.begin(), line.end(), boost::is_any_of(",()"), ' ');
}
