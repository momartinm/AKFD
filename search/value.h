#ifndef OPERATOR_H
#define OPERATOR_H

#include <string>
#include <vector>

#include "globals.h"

class Value {
    std::string type;
    std::vector<std::string> values;
    std::string name;

public:
	Value(std::string fact);
	std::string get_name(){return name;}
	std::string get_type(){return type;}
};

#endif
