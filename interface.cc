#include <libqalculate/qalculate.h>
#include <iostream>
#include <vector>
#include <cstring>

using namespace std;
vector<const char*> files = {"data/prefixes.xml",
"data/currencies.xml",
"data/units.xml",
"data/functions.xml",
"data/datasets.xml",
"data/variables.xml"};

bool inited = false;
extern "C" {
const void qalc_list_units() {
	for(Unit* unit : CALCULATOR->units) {
		cout << unit->print(false,false) << ":"<<unit->isCurrency()<<endl;

	}
}
const void qalc_init() {
	if(inited) return;
	new Calculator();
	inited = true;
}
const void qalc_load_file(const char* file) {
	qalc_init();
	CALCULATOR->loadDefinitions(file, false);
}
const char* qalc(const char* inp) {
	qalc_init();
	EvaluationOptions eo;
	MathStructure result = CALCULATOR->calculate(inp,eo);
	PrintOptions po;
	result.format(po);
	string sres = result.print(po);
	char *res = (char*)malloc(sizeof(char)*(sres.size()+1));
	strcpy(res, sres.c_str());
	return res;
}
}

int main() {
	Calculator *c = new Calculator();
	for(auto f : files)
		c->loadDefinitions(f, false);
	EvaluationOptions eo;
	MathStructure result = c->calculate("1cm+1m",eo);
	PrintOptions po;
	result.format(po);
	cout << result.print(po) << endl;
	qalc_list_units();
}
