#ifndef VARIABLE_H
#define VARIABLE_H

#include <iostream>
#include <vector>
using namespace std;

class Variable {
    vector<string> values;
    bool is_abstracted;
    vector<bool> abstract_values;
    string name;
    int layer;
    int level;
    bool necessary;
public:
    Variable(istream &in);
    void set_level(int level);
    void set_necessary();
    int get_level() const;
    bool is_necessary() const;
    int get_range() const;
    string get_name() const;
    void mark_fact_as_abstract(int fact);
    int get_layer() const {return layer; }
    bool is_derived() const {return layer != -1; }
    void generate_cpp_input(ofstream &outfile) const;
    void dump() const;
    string get_fact_name(int value) const {return values[value];}
	 bool is_fact_abstracted(int fact) {return abstract_values[fact];}
	 bool is_abstract() {return is_abstracted;}
};

#endif
