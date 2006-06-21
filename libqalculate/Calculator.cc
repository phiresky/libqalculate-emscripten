/*
    Qalculate    

    Copyright (C) 2003-2006  Niklas Knutsson (nq@altern.org)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "support.h"

#include "Calculator.h"
#include "BuiltinFunctions.h"
#include "util.h"
#include "MathStructure.h"
#include "Unit.h"
#include "Variable.h"
#include "Function.h"
#include "DataSet.h"
#include "ExpressionItem.h"
#include "Prefix.h"
#include "Number.h"

#include <locale.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <wait.h>
#include <queue>
#include <glib.h>
//#include <dlfcn.h>

#include <cln/cln.h>
using namespace cln;

#define XML_GET_PREC_FROM_PROP(node, i)			value = xmlGetProp(node, (xmlChar*) "precision"); if(value) {i = s2i((char*) value); xmlFree(value);} else {i = -1;}
#define XML_GET_APPROX_FROM_PROP(node, b)		value = xmlGetProp(node, (xmlChar*) "approximate"); if(value) {b = !xmlStrcmp(value, (const xmlChar*) "true");} else {value = xmlGetProp(node, (xmlChar*) "precise"); if(value) {b = xmlStrcmp(value, (const xmlChar*) "true");} else {b = false;}} if(value) xmlFree(value);
#define XML_GET_FALSE_FROM_PROP(node, name, b)		value = xmlGetProp(node, (xmlChar*) name); if(value && !xmlStrcmp(value, (const xmlChar*) "false")) {b = false;} else {b = true;} if(value) xmlFree(value);
#define XML_GET_TRUE_FROM_PROP(node, name, b)		value = xmlGetProp(node, (xmlChar*) name); if(value && !xmlStrcmp(value, (const xmlChar*) "true")) {b = true;} else {b = false;} if(value) xmlFree(value);
#define XML_GET_BOOL_FROM_PROP(node, name, b)		value = xmlGetProp(node, (xmlChar*) name); if(value && !xmlStrcmp(value, (const xmlChar*) "false")) {b = false;} else if(value && !xmlStrcmp(value, (const xmlChar*) "true")) {b = true;} if(value) xmlFree(value);
#define XML_GET_FALSE_FROM_TEXT(node, b)		value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); if(value && !xmlStrcmp(value, (const xmlChar*) "false")) {b = false;} else {b = true;} if(value) xmlFree(value);
#define XML_GET_TRUE_FROM_TEXT(node, b)			value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); if(value && !xmlStrcmp(value, (const xmlChar*) "true")) {b = true;} else {b = false;} if(value) xmlFree(value);
#define XML_GET_BOOL_FROM_TEXT(node, b)			value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); if(value && !xmlStrcmp(value, (const xmlChar*) "false")) {b = false;} else if(value && !xmlStrcmp(value, (const xmlChar*) "true")) {b = true;} if(value) xmlFree(value);
#define XML_GET_STRING_FROM_PROP(node, name, str)	value = xmlGetProp(node, (xmlChar*) name); if(value) {str = (char*) value; remove_blank_ends(str); xmlFree(value);} else str = ""; 
#define XML_GET_STRING_FROM_TEXT(node, str)		value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); if(value) {str = (char*) value; remove_blank_ends(str); xmlFree(value);} else str = "";
#define XML_DO_FROM_PROP(node, name, action)		value = xmlGetProp(node, (xmlChar*) name); if(value) action((char*) value); else action(""); if(value) xmlFree(value);
#define XML_DO_FROM_TEXT(node, action)			value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); if(value) {action((char*) value); xmlFree(value);} else action("");
#define XML_GET_INT_FROM_PROP(node, name, i)		value = xmlGetProp(node, (xmlChar*) name); if(value) {i = s2i((char*) value); xmlFree(value);}
#define XML_GET_INT_FROM_TEXT(node, i)			value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); if(value) {i = s2i((char*) value); xmlFree(value);}
#define XML_GET_LOCALE_STRING_FROM_TEXT(node, str, best, next_best)		value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); lang = xmlNodeGetLang(node); if(!best) {if(!lang) {if(!next_best) {if(value) {str = (char*) value; remove_blank_ends(str);} else str = ""; if(locale.empty()) {best = true;}}} else {if(locale == (char*) lang) {best = true; if(value) {str = (char*) value; remove_blank_ends(str);} else str = "";} else if(!next_best && strlen((char*) lang) >= 2 && fulfilled_translation == 0 && lang[0] == localebase[0] && lang[1] == localebase[1]) {next_best = true; if(value) {str = (char*) value; remove_blank_ends(str);} else str = "";} else if(!next_best && str.empty() && value) {str = (char*) value; remove_blank_ends(str);}}} if(value) xmlFree(value); if(lang) xmlFree(lang);
#define XML_GET_LOCALE_STRING_FROM_TEXT_REQ(node, str, best, next_best)		value = xmlNodeListGetString(doc, node->xmlChildrenNode, 1); lang = xmlNodeGetLang(node); if(!best) {if(!lang) {if(!next_best) {if(value) {str = (char*) value; remove_blank_ends(str);} else str = ""; if(locale.empty()) {best = true;}}} else {if(locale == (char*) lang) {best = true; if(value) {str = (char*) value; remove_blank_ends(str);} else str = "";} else if(!next_best && strlen((char*) lang) >= 2 && fulfilled_translation == 0 && lang[0] == localebase[0] && lang[1] == localebase[1]) {next_best = true; if(value) {str = (char*) value; remove_blank_ends(str);} else str = "";} else if(!next_best && str.empty() && value && !require_translation) {str = (char*) value; remove_blank_ends(str);}}} if(value) xmlFree(value); if(lang) xmlFree(lang);

const string &PrintOptions::comma() const {if(comma_sign.empty()) return CALCULATOR->getComma(); return comma_sign;}
const string &PrintOptions::decimalpoint() const {if(decimalpoint_sign.empty()) return CALCULATOR->getDecimalPoint(); return decimalpoint_sign;}

/*#include <time.h>
#include <sys/time.h>

struct timeval tvtime;
long int usecs, secs, usecs2, usecs3;

#define PRINT_TIME(x) gettimeofday(&tvtime, NULL); usecs2 = tvtime.tv_usec - usecs + (tvtime.tv_sec - secs) * 1000000; printf("%s %li\n", x, usecs2);
#define PRINT_TIMEDIFF(x) gettimeofday(&tvtime, NULL); printf("%s %li\n", x, tvtime.tv_usec - usecs + (tvtime.tv_sec - secs) * 1000000 - usecs2); usecs2 = tvtime.tv_usec - usecs + (tvtime.tv_sec - secs) * 1000000; 
#define ADD_TIME1 gettimeofday(&tvtime, NULL); usecs2 = tvtime.tv_usec - usecs + (tvtime.tv_sec - secs) * 1000000; 
#define ADD_TIME2 gettimeofday(&tvtime, NULL); usecs3 += tvtime.tv_usec - usecs + (tvtime.tv_sec - secs) * 1000000 - usecs2; */

typedef void (*CREATEPLUG_PROC)();

PlotParameters::PlotParameters() {
	auto_y_min = true;
	auto_x_min = true;
	auto_y_max = true;
	auto_x_max = true;
	y_log = false;
	x_log = false;
	y_log_base = 10;
	x_log_base = 10;
	grid = false;
	color = true;
	linewidth = -1;
	show_all_borders = false;
	legend_placement = PLOT_LEGEND_TOP_RIGHT;
}
PlotDataParameters::PlotDataParameters() {
	yaxis2 = false;
	xaxis2 = false;
}

CalculatorMessage::CalculatorMessage(string message_, MessageType type_) {
	mtype = type_;
	smessage = message_;
}
CalculatorMessage::CalculatorMessage(const CalculatorMessage &e) {
	mtype = e.type();
	smessage = e.message();
}
string CalculatorMessage::message() const {
	return smessage;
}
const char* CalculatorMessage::c_message() const {
	return smessage.c_str();
}
MessageType CalculatorMessage::type() const {
	return mtype;
}

void Calculator::addStringAlternative(string replacement, string standard) {
	signs.push_back(replacement);
	real_signs.push_back(standard);
}
bool Calculator::delStringAlternative(string replacement, string standard) {
	for(size_t i = 0; i < signs.size(); i++) {
		if(signs[i] == replacement && real_signs[i] == standard) {
			signs.erase(signs.begin() + i);
			real_signs.erase(real_signs.begin() + i);
			return true;
		}
	}
	return false;
}
void Calculator::addDefaultStringAlternative(string replacement, string standard) {
	default_signs.push_back(replacement);
	default_real_signs.push_back(standard);
}
bool Calculator::delDefaultStringAlternative(string replacement, string standard) {
	for(size_t i = 0; i < default_signs.size(); i++) {
		if(default_signs[i] == replacement && default_real_signs[i] == standard) {
			default_signs.erase(default_signs.begin() + i);
			default_real_signs.erase(default_real_signs.begin() + i);
			return true;
		}
	}
	return false;
}

Calculator *calculator;

MathStructure m_undefined, m_empty_vector, m_empty_matrix, m_zero, m_one, m_minus_one;
Number nr_zero, nr_one, nr_minus_one;
EvaluationOptions no_evaluation;
ExpressionName empty_expression_name;

enum {
	PROC_RPN_ADD,
	PROC_RPN_SET,
	PROC_RPN_OPERATION_1,
	PROC_RPN_OPERATION_2,
	PROC_NO_COMMAND
};

void autoConvert(const MathStructure &morig, MathStructure &mconv, const EvaluationOptions &eo) {
	switch(eo.auto_post_conversion) {
		case POST_CONVERSION_BEST: {
			mconv.set(CALCULATOR->convertToBestUnit(morig, eo));
		}
		case POST_CONVERSION_BASE: {
			mconv.set(CALCULATOR->convertToBaseUnits(morig, eo));
		}
		default: {}
	}
}

void *calculate_proc(void *pipe) {
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	FILE *calculate_pipe = (FILE*) pipe;
	while(true) {
		bool b_parse = true;
		fread(&b_parse, sizeof(bool), 1, calculate_pipe);
		void *x = NULL;
		fread(&x, sizeof(void*), 1, calculate_pipe);
		MathStructure *mstruct = (MathStructure*) x;
		if(b_parse) {
			mstruct->set(_("aborted"));
			if(CALCULATOR->tmp_parsedstruct) CALCULATOR->tmp_parsedstruct->set(_("aborted"));
			if(CALCULATOR->tmp_tostruct) CALCULATOR->tmp_tostruct->setUndefined();
			mstruct->set(CALCULATOR->calculate(CALCULATOR->expression_to_calculate, CALCULATOR->tmp_evaluationoptions, CALCULATOR->tmp_parsedstruct, CALCULATOR->tmp_tostruct, CALCULATOR->tmp_maketodivision));
		} else {
			MathStructure meval(*mstruct);
			mstruct->set(_("aborted"));
			meval.eval(CALCULATOR->tmp_evaluationoptions);
			if(CALCULATOR->tmp_evaluationoptions.auto_post_conversion == POST_CONVERSION_NONE) mstruct->set(meval);
			else autoConvert(meval, *mstruct, CALCULATOR->tmp_evaluationoptions);
		}
		switch(CALCULATOR->tmp_proc_command) {
			case PROC_RPN_ADD: {
				CALCULATOR->RPNStackEnter(mstruct, false);
				break;
			}
			case PROC_RPN_SET: {
				CALCULATOR->setRPNRegister(CALCULATOR->tmp_rpnindex, mstruct, false);
				break;
			}
			case PROC_RPN_OPERATION_1: {
				if(CALCULATOR->RPNStackSize() > 0) {
					CALCULATOR->setRPNRegister(1, mstruct, false);
				} else {
					CALCULATOR->RPNStackEnter(mstruct, false);
				}
				break;
			}
			case PROC_RPN_OPERATION_2: {
				if(CALCULATOR->RPNStackSize() > 1) {
					CALCULATOR->deleteRPNRegister(1);
				}
				if(CALCULATOR->RPNStackSize() > 0) {
					CALCULATOR->setRPNRegister(1, mstruct, false);
				} else {
					CALCULATOR->RPNStackEnter(mstruct, false);
				}
				break;
			}
			case PROC_NO_COMMAND: {}
		}
		CALCULATOR->b_busy = false;
	}
	return NULL;
}
void *print_proc(void *pipe) {
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	FILE *print_pipe = (FILE*) pipe;
	while(true) {
		void *x = NULL;
		fread(&x, sizeof(void*), 1, print_pipe);
		const MathStructure *mstruct = (const MathStructure*) x;
		MathStructure mstruct2(*mstruct);
		mstruct2.format();
		CALCULATOR->tmp_print_result = mstruct2.print(CALCULATOR->tmp_printoptions);
		CALCULATOR->b_busy = false;
	}
	return NULL;
}

Calculator::Calculator() {

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

	has_gnomevfs = -1;
	exchange_rates_warning_issued = false;

	setPrecision(DEFAULT_PRECISION);

	addStringAlternative(SIGN_POWER_1, "^(1)");
	addStringAlternative(SIGN_POWER_2, "^(2)");
	addStringAlternative(SIGN_POWER_3, "^(3)");
	addStringAlternative(SIGN_INFINITY, "infinity");
	addStringAlternative(SIGN_DIVISION, DIVISION);	
	addStringAlternative(SIGN_DIVISION_SLASH, DIVISION);	
	addStringAlternative(SIGN_MULTIPLICATION, MULTIPLICATION);		
	addStringAlternative(SIGN_MULTIDOT, MULTIPLICATION);
	addStringAlternative(SIGN_MULTIBULLET, MULTIPLICATION);
	addStringAlternative(SIGN_SMALLCIRCLE, MULTIPLICATION);
	addStringAlternative(SIGN_MINUS, MINUS);		
	addStringAlternative(SIGN_PLUS, PLUS);
	addStringAlternative(SIGN_NOT_EQUAL, " " NOT EQUALS);		
	addStringAlternative(SIGN_GREATER_OR_EQUAL, GREATER EQUALS);	
	addStringAlternative(SIGN_LESS_OR_EQUAL, LESS EQUALS);
	addStringAlternative("(+infinity)", "plus_infinity");
	addStringAlternative("(-infinity)", "minus_infinity");
	addStringAlternative(";", COMMA);
	addStringAlternative("\t", SPACE);
	addStringAlternative("\n", SPACE);
	addStringAlternative("**", POWER);
	
	per_str = _("per");
	per_str_len = per_str.length();
	times_str = _("times");
	times_str_len = times_str.length();
	plus_str = _("plus");
	plus_str_len = plus_str.length();
	minus_str = _("minus");
	minus_str_len = minus_str.length();
	and_str = _("and");
	and_str_len = and_str.length();
	AND_str = "AND";
	AND_str_len = AND_str.length();
	or_str = _("or");
	or_str_len = or_str.length();
	OR_str = "OR";
	OR_str_len = OR_str.length();
	XOR_str = "XOR";
	XOR_str_len = OR_str.length();

	saved_locale = strdup(setlocale(LC_NUMERIC, NULL));
	struct lconv *lc = localeconv();
	place_currency_code_before = lc->int_p_cs_precedes;
	place_currency_code_before_negative = lc->int_n_cs_precedes;
	place_currency_sign_before = lc->p_cs_precedes;
	place_currency_sign_before_negative = lc->n_cs_precedes;
	default_dot_as_separator = strcmp(lc->thousands_sep, ".") == 0;
	if(strcmp(lc->decimal_point, ",") == 0) {
		DOT_STR = ",";
		DOT_S = ".,";	
		COMMA_STR = ";";
		COMMA_S = ";";		
	} else {
		DOT_STR = ".";	
		DOT_S = ".";	
		COMMA_STR = ",";
		COMMA_S = ",;";		
	}	
	setlocale(LC_NUMERIC, "C");

	NAME_NUMBER_PRE_S = "_#";
	NAME_NUMBER_PRE_STR = "_";
	
	string str = _(" to ");
	local_to = (str != " to ");
	
	ids_i = 0;
	
	decimal_null_prefix = new DecimalPrefix(0, "", "");
	binary_null_prefix = new BinaryPrefix(0, "", "");
	m_undefined.setUndefined();
	m_empty_vector.clearVector();
	m_empty_matrix.clearMatrix();
	m_zero.clear();
	m_one.set(1, 1);
	m_minus_one.set(-1, 1);
	nr_zero.clear();
	nr_one.set(1, 1);
	nr_minus_one.set(-1, 1);
	no_evaluation.approximation = APPROXIMATION_EXACT;
	no_evaluation.structuring = STRUCTURING_NONE;
	no_evaluation.sync_units = false;

	save_printoptions.decimalpoint_sign = ".";
	save_printoptions.comma_sign = ",";
	save_printoptions.use_reference_names = true;
	save_printoptions.show_ending_zeroes = true;
	save_printoptions.number_fraction_format = FRACTION_DECIMAL_EXACT;
	save_printoptions.short_multiplication = false;

	default_assumptions = new Assumptions;
	default_assumptions->setType(ASSUMPTION_TYPE_REAL);
	default_assumptions->setSign(ASSUMPTION_SIGN_UNKNOWN);
	
	u_rad = NULL; u_gra = NULL; u_deg = NULL;
	
	b_save_called = false;

	ILLEGAL_IN_NAMES = DOT_S + RESERVED OPERATORS SPACES PARENTHESISS VECTOR_WRAPS;
	ILLEGAL_IN_NAMES_MINUS_SPACE_STR = DOT_S + RESERVED OPERATORS PARENTHESISS VECTOR_WRAPS;	
	ILLEGAL_IN_UNITNAMES = ILLEGAL_IN_NAMES + NUMBERS;			
	b_argument_errors = true;
	calculator = this;
	srand48(time(0));
	
	addBuiltinVariables();
	addBuiltinFunctions();
	addBuiltinUnits();

	disable_errors_ref = 0;
	b_busy = false;
	b_gnuplot_open = false;
	gnuplot_pipe = NULL;
	
	calculate_thread_stopped = true;
	pthread_attr_init(&calculate_thread_attr);
	int pipe_wr[] = {0, 0};
	pipe(pipe_wr);
	calculate_pipe_r = fdopen(pipe_wr[0], "r");
	calculate_pipe_w = fdopen(pipe_wr[1], "w");

	print_thread_stopped = true;
	pthread_attr_init(&print_thread_attr);
	pipe(pipe_wr);
	print_pipe_r = fdopen(pipe_wr[0], "r");
	print_pipe_w = fdopen(pipe_wr[1], "w");

}
Calculator::~Calculator() {
	closeGnuplot();
}

Unit *Calculator::getGraUnit() {
	if(!u_gra) u_gra = getUnit("gra");
	if(!u_gra) {
		CALCULATOR->error(true, _("Gradians unit is missing. Creating one for this session."), NULL);
		u_gra = addUnit(new AliasUnit(_("Angle/Plane Angle"), "gra", "gradians", "gradian", "Gradian", getRadUnit(), "pi/200", 1, "", false, true, true));	
	}
	return u_gra;
}
Unit *Calculator::getRadUnit() {
	if(!u_rad) u_rad = getUnit("rad");
	if(!u_rad) {
		CALCULATOR->error(true, _("Radians unit is missing. Creating one for this session."), NULL);
		u_rad = addUnit(new Unit(_("Angle/Plane Angle"), "rad", "radians", "radian", "Radian", false, true, true));
	}
	return u_rad;
}
Unit *Calculator::getDegUnit() {
	if(!u_deg) u_deg = getUnit("deg");
	if(!u_deg) {
		CALCULATOR->error(true, _("Degrees unit is missing. Creating one for this session."), NULL);
		u_deg = addUnit(new AliasUnit(_("Angle/Plane Angle"), "deg", "degrees", "degree", "Degree", getRadUnit(), "pi/180", 1, "", false, true, true));
	}
	return u_deg;
}

bool Calculator::utf8_pos_is_valid_in_name(char *pos) {
	if(is_in(ILLEGAL_IN_NAMES, pos[0])) {
		return false;
	}
	if((unsigned char) pos[0] >= 0xC0) {
		string str;
		str += pos[0];
		while((unsigned char) pos[1] >= 0x80 && (unsigned char) pos[1] <= 0xBF) {
			str += pos[1];
			pos++;
		}
		return str != SIGN_DIVISION && str != SIGN_DIVISION_SLASH && str != SIGN_MULTIPLICATION && str != SIGN_MULTIDOT && str != SIGN_SMALLCIRCLE && str != SIGN_MULTIBULLET && str != SIGN_MINUS && str != SIGN_PLUS && str != SIGN_NOT_EQUAL && str != SIGN_GREATER_OR_EQUAL && str != SIGN_LESS_OR_EQUAL;
	}
	return true;
}

bool Calculator::showArgumentErrors() const {
	return b_argument_errors;
}
void Calculator::beginTemporaryStopMessages() {
	disable_errors_ref++;
	stopped_errors_count.push_back(0);
	stopped_warnings_count.push_back(0);
	stopped_messages_count.push_back(0);
}
int Calculator::endTemporaryStopMessages(int *message_count, int *warning_count) {
	if(disable_errors_ref <= 0) return -1;
	disable_errors_ref--;
	int ret = stopped_errors_count[disable_errors_ref];
	if(message_count) *message_count = stopped_messages_count[disable_errors_ref];
	if(warning_count) *warning_count = stopped_warnings_count[disable_errors_ref];
	stopped_errors_count.pop_back();
	stopped_warnings_count.pop_back();
	stopped_messages_count.pop_back();
	return ret;
}
Variable *Calculator::getVariable(size_t index) const {
	if(index < variables.size()) {
		return variables[index];
	}
	return NULL;
}
bool Calculator::hasVariable(Variable *v) {
	for(size_t i = 0; i < variables.size(); i++) {
		if(variables[i] == v) return true;
	}
	return false;
}
bool Calculator::hasUnit(Unit *u) {
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i] == u) return true;
	}
	return false;
}
bool Calculator::hasFunction(MathFunction *f) {
	for(size_t i = 0; i < functions.size(); i++) {
		if(functions[i] == f) return true;
	}
	return false;
}
bool Calculator::stillHasVariable(Variable *v) {
	for(vector<Variable*>::iterator it = deleted_variables.begin(); it != deleted_variables.end(); ++it) {
		if(*it == v) return false;
	}
	return true;
}
bool Calculator::stillHasUnit(Unit *u) {
	for(vector<Unit*>::iterator it = deleted_units.begin(); it != deleted_units.end(); ++it) {
		if(*it == u) return false;
	}
	return true;
}
bool Calculator::stillHasFunction(MathFunction *f) {
	for(vector<MathFunction*>::iterator it = deleted_functions.begin(); it != deleted_functions.end(); ++it) {
		if(*it == f) return false;
	}
	return true;
}
void Calculator::saveFunctionCalled() {
	b_save_called = true;
}
bool Calculator::checkSaveFunctionCalled() {
	if(b_save_called) {
		b_save_called = false;
		return true;
	}
	return false;
}
ExpressionItem *Calculator::getActiveExpressionItem(ExpressionItem *item) {
	if(!item) return NULL;
	for(size_t i = 1; i <= item->countNames(); i++) {
		ExpressionItem *item2 = getActiveExpressionItem(item->getName(i).name, item);
		if(item2) {
			return item2;
		}
	}
	return NULL;
}
ExpressionItem *Calculator::getActiveExpressionItem(string name, ExpressionItem *item) {
	if(name.empty()) return NULL;
	for(size_t index = 0; index < variables.size(); index++) {
		if(variables[index] != item && variables[index]->isActive() && variables[index]->hasName(name)) {
			return variables[index];
		}
	}
	for(size_t index = 0; index < functions.size(); index++) {
		if(functions[index] != item && functions[index]->isActive() && functions[index]->hasName(name)) {
			return functions[index];
		}
	}
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i] != item && units[i]->isActive() && units[i]->hasName(name)) {
			return units[i];
		}
	}
	return NULL;
}
ExpressionItem *Calculator::getInactiveExpressionItem(string name, ExpressionItem *item) {
	if(name.empty()) return NULL;
	for(size_t index = 0; index < variables.size(); index++) {
		if(variables[index] != item && !variables[index]->isActive() && variables[index]->hasName(name)) {
			return variables[index];
		}
	}
	for(size_t index = 0; index < functions.size(); index++) {
		if(functions[index] != item && !functions[index]->isActive() && functions[index]->hasName(name)) {
			return functions[index];
		}
	}
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i] != item && !units[i]->isActive() && units[i]->hasName(name)) {
			return units[i];
		}
	}
	return NULL;
}
ExpressionItem *Calculator::getExpressionItem(string name, ExpressionItem *item) {
	if(name.empty()) return NULL;
	Variable *v = getVariable(name);
	if(v && v != item) return v;
	MathFunction *f = getFunction(name);
	if(f && f != item) return f;
	Unit *u = getUnit(name);
	if(u && u != item) return u;
	u = getCompositeUnit(name);
	if(u && u != item) return u;
	return NULL;
}
Unit *Calculator::getUnit(size_t index) const {
	if(index < units.size()) {
		return units[index];
	}
	return NULL;
}
MathFunction *Calculator::getFunction(size_t index) const {
	if(index < functions.size()) {
		return functions[index];
	}
	return NULL;
}

void Calculator::setDefaultAssumptions(Assumptions *ass) {
	if(default_assumptions) delete default_assumptions;
	default_assumptions = ass;
}
Assumptions *Calculator::defaultAssumptions() {
	return default_assumptions;
}

Prefix *Calculator::getPrefix(size_t index) const {
	if(index < prefixes.size()) {
		return prefixes[index];
	}
	return NULL;
}
Prefix *Calculator::getPrefix(string name_) const {
	for(size_t i = 0; i < prefixes.size(); i++) {
		if(prefixes[i]->shortName(false) == name_ || prefixes[i]->longName(false) == name_ || prefixes[i]->unicodeName(false) == name_) {
			return prefixes[i];
		}
	}
	return NULL;
}
DecimalPrefix *Calculator::getExactDecimalPrefix(int exp10, int exp) const {
	for(size_t i = 0; i < decimal_prefixes.size(); i++) {
		if(decimal_prefixes[i]->exponent(exp) == exp10) {
			return decimal_prefixes[i];
		} else if(decimal_prefixes[i]->exponent(exp) > exp10) {
			break;
		}
	}
	return NULL;
}
BinaryPrefix *Calculator::getExactBinaryPrefix(int exp2, int exp) const {
	for(size_t i = 0; i < binary_prefixes.size(); i++) {
		if(binary_prefixes[i]->exponent(exp) == exp2) {
			return binary_prefixes[i];
		} else if(binary_prefixes[i]->exponent(exp) > exp2) {
			break;
		}
	}
	return NULL;
}
Prefix *Calculator::getExactPrefix(const Number &o, int exp) const {
	ComparisonResult c;
	for(size_t i = 0; i < prefixes.size(); i++) {
		c = o.compare(prefixes[i]->value(exp));
		if(c == COMPARISON_RESULT_EQUAL) {
			return prefixes[i];
		} else if(c == COMPARISON_RESULT_GREATER) {
			break;
		}
	}
	return NULL;
}
DecimalPrefix *Calculator::getNearestDecimalPrefix(int exp10, int exp) const {
	if(decimal_prefixes.size() <= 0) return NULL;
	int i = 0;
	if(exp < 0) {
		i = decimal_prefixes.size() - 1;
	}
	while((exp < 0 && i >= 0) || (exp >= 0 && i < (int) decimal_prefixes.size())) {	
		if(decimal_prefixes[i]->exponent(exp) == exp10) {
			return decimal_prefixes[i];
		} else if(decimal_prefixes[i]->exponent(exp) > exp10) {
			if(i == 0) {
				return decimal_prefixes[i];
			} else if(exp10 - decimal_prefixes[i - 1]->exponent(exp) < decimal_prefixes[i]->exponent(exp) - exp10) {
				return decimal_prefixes[i - 1];
			} else {
				return decimal_prefixes[i];
			}
		}
		if(exp < 0) {
			i--;
		} else {
			i++;
		}
	}
	return decimal_prefixes[decimal_prefixes.size() - 1];
}
DecimalPrefix *Calculator::getBestDecimalPrefix(int exp10, int exp, bool all_prefixes) const {
	if(decimal_prefixes.size() <= 0 || exp10 == 0) return NULL;
	int i = 0;
	if(exp < 0) {
		i = decimal_prefixes.size() - 1;
	}
	DecimalPrefix *p = NULL, *p_prev = NULL;
	int exp10_1, exp10_2;
	while((exp < 0 && i >= 0) || (exp >= 0 && i < (int) decimal_prefixes.size())) {
		if(all_prefixes || decimal_prefixes[i]->exponent() % 3 == 0) {
			p = decimal_prefixes[i];
			if(p_prev && p_prev->exponent() >= 0 != p->exponent() >= 0 && p_prev->exponent() != 0) {
				if(exp < 0) {
					i++;
				} else {
					i--;
				}
				p = decimal_null_prefix;
			}
			if(p->exponent(exp) == exp10) {
				if(p == decimal_null_prefix) return NULL;
				return p;
			} else if(p->exponent(exp) > exp10) {
				if(i == 0) {
					if(p == decimal_null_prefix) return NULL;
					return p;
				}
				exp10_1 = exp10;
				if(p_prev) {
					exp10_1 -= p_prev->exponent(exp);
				}
				exp10_2 = p->exponent(exp);
				exp10_2 -= exp10;
				exp10_2 *= 2;
				exp10_2 += 2;
				if(exp10_1 < exp10_2) {
					if(p_prev == decimal_null_prefix) return NULL;
					return p_prev;
				} else {
					return p;
				}
			}
			p_prev = p;
		}
		if(exp < 0) {
			i--;
		} else {
			i++;
		}
	}
	return p_prev;
}
DecimalPrefix *Calculator::getBestDecimalPrefix(const Number &exp10, const Number &exp, bool all_prefixes) const {
	if(decimal_prefixes.size() <= 0 || exp10.isZero()) return NULL;
	int i = 0;
	ComparisonResult c;
	if(exp.isNegative()) {
		i = decimal_prefixes.size() - 1;
	}
	DecimalPrefix *p = NULL, *p_prev = NULL;
	Number exp10_1, exp10_2;
	while((exp.isNegative() && i >= 0) || (!exp.isNegative() && i < (int) decimal_prefixes.size())) {
		if(all_prefixes || decimal_prefixes[i]->exponent() % 3 == 0) {
			p = decimal_prefixes[i];
			if(p_prev && p_prev->exponent() >= 0 != p->exponent() >= 0 && p_prev->exponent() != 0) {
				if(exp.isNegative()) {
					i++;
				} else {
					i--;
				}
				p = decimal_null_prefix;
			}
			c = exp10.compare(p->exponent(exp));
			if(c == COMPARISON_RESULT_EQUAL) {
				if(p == decimal_null_prefix) return NULL;
				return p;
			} else if(c == COMPARISON_RESULT_GREATER) {
				if(i == 0) {
					if(p == decimal_null_prefix) return NULL;
					return p;
				}
				exp10_1 = exp10;
				if(p_prev) {
					exp10_1 -= p_prev->exponent(exp);
				}
				exp10_2 = p->exponent(exp);
				exp10_2 -= exp10;
				exp10_2 *= 2;
				exp10_2 += 2;
				if(exp10_1.isLessThan(exp10_2)) {
					if(p_prev == decimal_null_prefix) return NULL;
					return p_prev;
				} else {
					return p;
				}
			}
			p_prev = p;
		}
		if(exp.isNegative()) {
			i--;
		} else {
			i++;
		}
	}
	return p_prev;
}
BinaryPrefix *Calculator::getNearestBinaryPrefix(int exp2, int exp) const {
	if(binary_prefixes.size() <= 0) return NULL;
	int i = 0;
	if(exp < 0) {
		i = binary_prefixes.size() - 1;
	}
	while((exp < 0 && i >= 0) || (exp >= 0 && i < (int) binary_prefixes.size())) {	
		if(binary_prefixes[i]->exponent(exp) == exp2) {
			return binary_prefixes[i];
		} else if(binary_prefixes[i]->exponent(exp) > exp2) {
			if(i == 0) {
				return binary_prefixes[i];
			} else if(exp2 - binary_prefixes[i - 1]->exponent(exp) < binary_prefixes[i]->exponent(exp) - exp2) {
				return binary_prefixes[i - 1];
			} else {
				return binary_prefixes[i];
			}
		}
		if(exp < 0) {
			i--;
		} else {
			i++;
		}
	}
	return binary_prefixes[binary_prefixes.size() - 1];
}
BinaryPrefix *Calculator::getBestBinaryPrefix(int exp2, int exp) const {
	if(binary_prefixes.size() <= 0 || exp2 == 0) return NULL;
	int i = 0;
	if(exp < 0) {
		i = binary_prefixes.size() - 1;
	}
	BinaryPrefix *p = NULL, *p_prev = NULL;
	int exp2_1, exp2_2;
	while((exp < 0 && i >= 0) || (exp >= 0 && i < (int) binary_prefixes.size())) {
		p = binary_prefixes[i];
		if(p_prev && p_prev->exponent() >= 0 != p->exponent() >= 0 && p_prev->exponent() != 0) {
			if(exp < 0) {
				i++;
			} else {
				i--;
			}
			p = binary_null_prefix;
		}
		if(p->exponent(exp) == exp2) {
			if(p == binary_null_prefix) return NULL;
			return p;
		} else if(p->exponent(exp) > exp2) {
			if(i == 0) {
				if(p == binary_null_prefix) return NULL;
				return p;
			}
			exp2_1 = exp2;
			if(p_prev) {
				exp2_1 -= p_prev->exponent(exp);
			}
			exp2_2 = p->exponent(exp);
			exp2_2 -= exp2;
			exp2_2 *= 2;
			exp2_2 += 2;
			if(exp2_1 < exp2_2) {
				if(p_prev == binary_null_prefix) return NULL;
				return p_prev;
			} else {
				return p;
			}
		}
		p_prev = p;
		if(exp < 0) {
			i--;
		} else {
			i++;
		}
	}
	return p_prev;
}
BinaryPrefix *Calculator::getBestBinaryPrefix(const Number &exp2, const Number &exp) const {
	if(binary_prefixes.size() <= 0 || exp2.isZero()) return NULL;
	int i = 0;
	ComparisonResult c;
	if(exp.isNegative()) {
		i = binary_prefixes.size() - 1;
	}
	BinaryPrefix *p = NULL, *p_prev = NULL;
	Number exp2_1, exp2_2;
	while((exp.isNegative() && i >= 0) || (!exp.isNegative() && i < (int) binary_prefixes.size())) {
		p = binary_prefixes[i];
		if(p_prev && p_prev->exponent() >= 0 != p->exponent() >= 0 && p_prev->exponent() != 0) {
			if(exp.isNegative()) {
				i++;
			} else {
				i--;
			}
			p = binary_null_prefix;
		}
		c = exp2.compare(p->exponent(exp));
		if(c == COMPARISON_RESULT_EQUAL) {
			if(p == binary_null_prefix) return NULL;
			return p;
		} else if(c == COMPARISON_RESULT_GREATER) {
			if(i == 0) {
				if(p == binary_null_prefix) return NULL;
				return p;
			}
			exp2_1 = exp2;
			if(p_prev) {
				exp2_1 -= p_prev->exponent(exp);
			}
			exp2_2 = p->exponent(exp);
			exp2_2 -= exp2;
			exp2_2 *= 2;
			exp2_2 += 2;
			if(exp2_1.isLessThan(exp2_2)) {
				if(p_prev == binary_null_prefix) return NULL;
				return p_prev;
			} else {
				return p;
			}
		}
		p_prev = p;
		if(exp.isNegative()) {
			i--;
		} else {
			i++;
		}
	}
	return p_prev;
}
Prefix *Calculator::addPrefix(Prefix *p) {
	if(p->type() == PREFIX_DECIMAL) {
		decimal_prefixes.push_back((DecimalPrefix*) p);
	} else if(p->type() == PREFIX_BINARY) {
		binary_prefixes.push_back((BinaryPrefix*) p);
	}
	prefixes.push_back(p);
	prefixNameChanged(p, true);
	return p;	
}
void Calculator::prefixNameChanged(Prefix *p, bool new_item) {
	size_t l2;
	if(!new_item) delPrefixUFV(p);
	if(!p->longName(false).empty()) {
		l2 = p->longName(false).length();
		if(l2 > UFV_LENGTHS) {
			size_t i = 0, l;
			for(vector<void*>::iterator it = ufvl.begin(); ; ++it) {
				l = 0;
				if(it != ufvl.end()) {
					if(ufvl_t[i] == 'v')
						l = ((Variable*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'f')
						l = ((MathFunction*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'u')
						l = ((Unit*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'p')
						l = ((Prefix*) (*it))->shortName(false).length();
					else if(ufvl_t[i] == 'P')
						l = ((Prefix*) (*it))->longName(false).length();
					else if(ufvl_t[i] == 'q')
						l = ((Prefix*) (*it))->unicodeName(false).length();
				}
				if(it == ufvl.end()) {
					ufvl.push_back((void*) p);
					ufvl_t.push_back('P');
					ufvl_i.push_back(1);
					break;
				} else if(l <= l2) {			
					ufvl.insert(it, (void*) p);
					ufvl_t.insert(ufvl_t.begin() + i, 'P');
					ufvl_i.insert(ufvl_i.begin() + i, 1);
					break;
				}
				i++;
			}
		} else if(l2 > 0) {
			l2--;
			ufv[0][l2].push_back((void*) p);
			ufv_i[0][l2].push_back(1);
		}
	}
	if(!p->shortName(false).empty()) {
		l2 = p->shortName(false).length();
		if(l2 > UFV_LENGTHS) {
			size_t i = 0, l;
			for(vector<void*>::iterator it = ufvl.begin(); ; ++it) {
				l = 0;
				if(it != ufvl.end()) {
					if(ufvl_t[i] == 'v')
						l = ((Variable*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'f')
						l = ((MathFunction*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'u')
						l = ((Unit*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'p')
						l = ((Prefix*) (*it))->shortName(false).length();
					else if(ufvl_t[i] == 'P')
						l = ((Prefix*) (*it))->longName(false).length();
					else if(ufvl_t[i] == 'q')
						l = ((Prefix*) (*it))->unicodeName(false).length();
				}
				if(it == ufvl.end()) {
					ufvl.push_back((void*) p);
					ufvl_t.push_back('p');
					ufvl_i.push_back(1);
					break;
				} else if(l <= l2) {			
					ufvl.insert(it, (void*) p);
					ufvl_t.insert(ufvl_t.begin() + i, 'p');
					ufvl_i.insert(ufvl_i.begin() + i, 1);
					break;
				}
				i++;
			}
		} else if(l2 > 0) {
			l2--;
			ufv[0][l2].push_back((void*) p);
			ufv_i[0][l2].push_back(2);
		}
	}
	if(!p->unicodeName(false).empty()) {
		l2 = p->unicodeName(false).length();
		if(l2 > UFV_LENGTHS) {
			size_t i = 0, l;
			for(vector<void*>::iterator it = ufvl.begin(); ; ++it) {
				l = 0;
				if(it != ufvl.end()) {
					if(ufvl_t[i] == 'v')
						l = ((Variable*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'f')
						l = ((MathFunction*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'u')
						l = ((Unit*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'p')
						l = ((Prefix*) (*it))->shortName(false).length();
					else if(ufvl_t[i] == 'P')
						l = ((Prefix*) (*it))->longName(false).length();
					else if(ufvl_t[i] == 'q')
						l = ((Prefix*) (*it))->unicodeName(false).length();
				}
				if(it == ufvl.end()) {
					ufvl.push_back((void*) p);
					ufvl_t.push_back('q');
					ufvl_i.push_back(1);
					break;
				} else if(l <= l2) {			
					ufvl.insert(it, (void*) p);
					ufvl_t.insert(ufvl_t.begin() + i, 'q');
					ufvl_i.insert(ufvl_i.begin() + i, 1);
					break;
				}
				i++;
			}
		} else if(l2 > 0) {
			l2--;
			ufv[0][l2].push_back((void*) p);
			ufv_i[0][l2].push_back(3);
		}
	}
}

void Calculator::setPrecision(int precision) {
	if(precision <= 0) precision = DEFAULT_PRECISION;
/*	if(precision < 10) {
		cln::default_float_format = float_format(precision + (10 - precision) + 5);	*/
	if(precision < cln::float_format_lfloat_min) {
		cln::default_float_format = cln::float_format(cln::float_format_lfloat_min + 5);
	} else {
		cln::default_float_format = cln::float_format(precision + 5);	
	}
	i_precision = precision;
}
int Calculator::getPrecision() const {
	return i_precision;
}

const string &Calculator::getDecimalPoint() const {return DOT_STR;}
const string &Calculator::getComma() const {return COMMA_STR;}
string Calculator::localToString() const {
	return _(" to ");
}
void Calculator::setLocale() {
	setlocale(LC_NUMERIC, saved_locale);
	lconv *locale = localeconv();
	if(strcmp(locale->decimal_point, ",") == 0) {
		DOT_STR = ",";
		DOT_S = ".,";	
		COMMA_STR = ";";
		COMMA_S = ";";		
	} else {
		DOT_STR = ".";	
		DOT_S = ".";	
		COMMA_STR = ",";
		COMMA_S = ",;";		
	}
	setlocale(LC_NUMERIC, "C");
}
void Calculator::useDecimalComma() {
	DOT_STR = ",";
	DOT_S = ".,";
	COMMA_STR = ";";
	COMMA_S = ";";
}
void Calculator::useDecimalPoint() {
	DOT_STR = ".";	
	DOT_S = ".";
	COMMA_STR = ",";
	COMMA_S = ",;";
}
void Calculator::unsetLocale() {
	COMMA_STR = ",";
	COMMA_S = ",;";	
	DOT_STR = ".";
	DOT_S = ".";
}

size_t Calculator::addId(MathStructure *mstruct, bool persistent) {
	size_t id = 0;
	if(freed_ids.size() > 0) {
		id = freed_ids.back();
		freed_ids.pop_back();
	} else {
		ids_i++;
		id = ids_i;
	}
	ids_p[id] = persistent;
	id_structs[id] = mstruct;
	return id;
}
size_t Calculator::parseAddId(MathFunction *f, const string &str, const ParseOptions &po, bool persistent) {
	size_t id = 0;
	if(freed_ids.size() > 0) {
		id = freed_ids.back();
		freed_ids.pop_back();
	} else {
		ids_i++;
		id = ids_i;
	}
	ids_p[id] = persistent;
	id_structs[id] = new MathStructure();
	f->parse(*id_structs[id], str, po);
	return id;
}
size_t Calculator::parseAddIdAppend(MathFunction *f, const MathStructure &append_mstruct, const string &str, const ParseOptions &po, bool persistent) {
	size_t id = 0;
	if(freed_ids.size() > 0) {
		id = freed_ids.back();
		freed_ids.pop_back();
	} else {
		ids_i++;
		id = ids_i;
	}
	ids_p[id] = persistent;
	id_structs[id] = new MathStructure();
	f->parse(*id_structs[id], str, po);
	id_structs[id]->addChild(append_mstruct);
	return id;
}
size_t Calculator::parseAddVectorId(const string &str, const ParseOptions &po, bool persistent) {
	size_t id = 0;
	if(freed_ids.size() > 0) {
		id = freed_ids.back();
		freed_ids.pop_back();
	} else {
		ids_i++;
		id = ids_i;
	}
	ids_p[id] = persistent;
	id_structs[id] = new MathStructure();
	f_vector->args(str, *id_structs[id], po);
	return id;
}
MathStructure *Calculator::getId(size_t id) {
	if(id_structs.find(id) != id_structs.end()) {
		if(ids_p[id]) {
			return new MathStructure(*id_structs[id]);
		} else {
			MathStructure *mstruct = id_structs[id];
			freed_ids.push_back(id);
			id_structs.erase(id);
			ids_p.erase(id);
			return mstruct;
		}
	}
	return NULL;
}

void Calculator::delId(size_t id) {
	if(ids_p.find(id) != ids_p.end()) {	
		freed_ids.push_back(id);
		id_structs[id]->unref();
		id_structs.erase(id);
		ids_p.erase(id);
	}
}

void Calculator::resetVariables() {
	variables.clear();
	addBuiltinVariables();
}
void Calculator::resetFunctions() {
	functions.clear();
	addBuiltinFunctions();
}
void Calculator::resetUnits() {
	units.clear();
	addBuiltinUnits();
}
void Calculator::reset() {
	resetVariables();
	resetFunctions();
	resetUnits();
}
void Calculator::addBuiltinVariables() {

	v_e = (KnownVariable*) addVariable(new EVariable());
	v_pi = (KnownVariable*) addVariable(new PiVariable());	
	Number nr(1, 1);
	MathStructure mstruct;
	mstruct.number().setImaginaryPart(nr);
	v_i = (KnownVariable*) addVariable(new KnownVariable("", "i", mstruct, "Imaginary i (sqrt(-1))", false, true));
	mstruct.number().setInfinity();
	v_inf = (KnownVariable*) addVariable(new KnownVariable("", "infinity", mstruct, "Infinity", false, true));
	mstruct.number().setPlusInfinity();
	v_pinf = (KnownVariable*) addVariable(new KnownVariable("", "plus_infinity", mstruct, "+Infinity", false, true));
	mstruct.number().setMinusInfinity();
	v_minf = (KnownVariable*) addVariable(new KnownVariable("", "minus_infinity", mstruct, "-Infinity", false, true));
	mstruct.setUndefined();
	v_undef = (KnownVariable*) addVariable(new KnownVariable("", "undefined", mstruct, "Undefined", false, true));
	addVariable(new EulerVariable());
	addVariable(new CatalanVariable());
	v_x = (UnknownVariable*) addVariable(new UnknownVariable("", "x", "", true, false));
	v_y = (UnknownVariable*) addVariable(new UnknownVariable("", "y", "", true, false));
	v_z = (UnknownVariable*) addVariable(new UnknownVariable("", "z", "", true, false));
	
}
void Calculator::addBuiltinFunctions() {

	f_vector = addFunction(new VectorFunction());
	f_sort = addFunction(new SortFunction());
	f_rank = addFunction(new RankFunction());
	f_limits = addFunction(new LimitsFunction());
	//f_component = addFunction(new ComponentFunction());
	f_dimension = addFunction(new DimensionFunction());
	f_merge_vectors = addFunction(new MergeVectorsFunction());
	f_matrix = addFunction(new MatrixFunction());
	f_matrix_to_vector = addFunction(new MatrixToVectorFunction());
	f_area = addFunction(new AreaFunction());
	f_rows = addFunction(new RowsFunction());
	f_columns = addFunction(new ColumnsFunction());
	f_row = addFunction(new RowFunction());
	f_column = addFunction(new ColumnFunction());
	f_elements = addFunction(new ElementsFunction());
	f_element = addFunction(new ElementFunction());
	f_transpose = addFunction(new TransposeFunction());
	f_identity = addFunction(new IdentityFunction());
	f_determinant = addFunction(new DeterminantFunction());
	f_permanent = addFunction(new PermanentFunction());
	f_adjoint = addFunction(new AdjointFunction());
	f_cofactor = addFunction(new CofactorFunction());
	f_inverse = addFunction(new InverseFunction());

	f_factorial = addFunction(new FactorialFunction());
	f_factorial2 = addFunction(new DoubleFactorialFunction());
	f_multifactorial = addFunction(new MultiFactorialFunction());
	f_binomial = addFunction(new BinomialFunction());
	
	f_xor = addFunction(new XorFunction());
	f_bitxor = addFunction(new BitXorFunction());
	f_even = addFunction(new EvenFunction());
	f_odd = addFunction(new OddFunction());
	f_shift = addFunction(new ShiftFunction());
	
	f_abs = addFunction(new AbsFunction());
	f_signum = addFunction(new SignumFunction());
	f_gcd = addFunction(new GcdFunction());
	f_lcm = addFunction(new LcmFunction());
	f_round = addFunction(new RoundFunction());
	f_floor = addFunction(new FloorFunction());
	f_ceil = addFunction(new CeilFunction());
	f_trunc = addFunction(new TruncFunction());
	f_int = addFunction(new IntFunction());
	f_frac = addFunction(new FracFunction());
	f_rem = addFunction(new RemFunction());
	f_mod = addFunction(new ModFunction());
	
	f_polynomial_unit = addFunction(new PolynomialUnitFunction());
	f_polynomial_primpart = addFunction(new PolynomialPrimpartFunction());
	f_polynomial_content = addFunction(new PolynomialContentFunction());
	f_coeff = addFunction(new CoeffFunction());
	f_lcoeff = addFunction(new LCoeffFunction());
	f_tcoeff = addFunction(new TCoeffFunction());
	f_degree = addFunction(new DegreeFunction());
	f_ldegree = addFunction(new LDegreeFunction());

	f_re = addFunction(new ReFunction());
	f_im = addFunction(new ImFunction());
	//f_arg = addFunction(new ArgFunction());
	f_numerator = addFunction(new NumeratorFunction());
	f_denominator = addFunction(new DenominatorFunction());	

	f_sqrt = addFunction(new SqrtFunction());
	f_sq = addFunction(new SquareFunction());

	f_exp = addFunction(new ExpFunction());

	f_ln = addFunction(new LogFunction());
	f_logn = addFunction(new LognFunction());

	f_sin = addFunction(new SinFunction());
	f_cos = addFunction(new CosFunction());
	f_tan = addFunction(new TanFunction());
	f_asin = addFunction(new AsinFunction());
	f_acos = addFunction(new AcosFunction());
	f_atan = addFunction(new AtanFunction());
	f_sinh = addFunction(new SinhFunction());
	f_cosh = addFunction(new CoshFunction());
	f_tanh = addFunction(new TanhFunction());
	f_asinh = addFunction(new AsinhFunction());
	f_acosh = addFunction(new AcoshFunction());
	f_atanh = addFunction(new AtanhFunction());
	f_radians_to_default_angle_unit = addFunction(new RadiansToDefaultAngleUnitFunction());

	f_zeta = addFunction(new ZetaFunction());
	f_gamma = addFunction(new GammaFunction());
	f_beta = addFunction(new BetaFunction());

	f_total = addFunction(new TotalFunction());
	f_percentile = addFunction(new PercentileFunction());
	f_min = addFunction(new MinFunction());
	f_max = addFunction(new MaxFunction());
	f_mode = addFunction(new ModeFunction());
	f_rand = addFunction(new RandFunction());

	f_isodate = addFunction(new ISODateFunction());
	f_localdate = addFunction(new LocalDateFunction());
	f_timestamp = addFunction(new TimestampFunction());
	f_stamptodate = addFunction(new TimestampToDateFunction());
	f_days = addFunction(new DaysFunction());
	f_yearfrac = addFunction(new YearFracFunction());
	f_week = addFunction(new WeekFunction());
	f_weekday = addFunction(new WeekdayFunction());
	f_month = addFunction(new MonthFunction());
	f_day = addFunction(new DayFunction());
	f_year = addFunction(new YearFunction());
	f_yearday = addFunction(new YeardayFunction());
	f_time = addFunction(new TimeFunction());

	f_base = addFunction(new BaseFunction());
	f_bin = addFunction(new BinFunction());
	f_oct = addFunction(new OctFunction());
	f_hex = addFunction(new HexFunction());
	f_roman = addFunction(new RomanFunction());

	f_ascii = addFunction(new AsciiFunction());
	f_char = addFunction(new CharFunction());

	f_length = addFunction(new LengthFunction());
	f_concatenate = addFunction(new ConcatenateFunction());
		
	f_replace = addFunction(new ReplaceFunction());
	f_stripunits = addFunction(new StripUnitsFunction());

	f_genvector = addFunction(new GenerateVectorFunction());
	f_for = addFunction(new ForFunction());
	f_sum = addFunction(new SumFunction());
	f_product = addFunction(new ProductFunction());
	f_process = addFunction(new ProcessFunction());
	f_process_matrix = addFunction(new ProcessMatrixFunction());
	f_csum = addFunction(new CustomSumFunction());
	f_function = addFunction(new FunctionFunction());
	f_select = addFunction(new SelectFunction());
	f_title = addFunction(new TitleFunction());
	f_if = addFunction(new IFFunction());	
	f_error = addFunction(new ErrorFunction());
	f_warning = addFunction(new WarningFunction());
	f_message = addFunction(new MessageFunction());
	
	f_save = addFunction(new SaveFunction());
	f_load = addFunction(new LoadFunction());
	f_export = addFunction(new ExportFunction());

	f_register = addFunction(new RegisterFunction());
	f_stack = addFunction(new StackFunction());

	f_diff = addFunction(new DeriveFunction());
	f_integrate = addFunction(new IntegrateFunction());
	f_solve = addFunction(new SolveFunction());
	f_multisolve = addFunction(new SolveMultipleFunction());

	/*void *plugin = dlopen("/home/nq/Source/qalculate/plugins/pluginfunction.so", RTLD_NOW);
	if(plugin) {
		CREATEPLUG_PROC createproc = (CREATEPLUG_PROC) dlsym(plugin, "createPlugin");
		if (dlerror() != NULL) {
			dlclose(plugin);
			printf( "dlsym error\n");
		} else {
			createproc();
		}
	} else {
		printf( "dlopen error\n");
	}*/

}
void Calculator::addBuiltinUnits() {
	u_euro = addUnit(new Unit(_("Currency"), "EUR", "euros", "euro", "European Euros", false, true, true));
}
void Calculator::error(bool critical, const char *TEMPLATE, ...) {
	if(disable_errors_ref > 0) {
		stopped_messages_count[disable_errors_ref - 1]++;
		if(critical) {
			stopped_errors_count[disable_errors_ref - 1]++;
		} else {
			stopped_warnings_count[disable_errors_ref - 1]++;
		}		
		return;
	}
	string error_str = TEMPLATE;
	va_list ap;
	va_start(ap, TEMPLATE);
	size_t i = 0;
	while(true) {
		i = error_str.find("%", i);
		if(i == string::npos || i + 1 == error_str.length()) break;
		switch(error_str[i + 1]) {
			case 's': {
				const char *str = va_arg(ap, const char*);
				if(!str) {
					i++;
				} else {
					error_str.replace(i, 2, str);
					i += strlen(str);
				}
				break;
			}
			case 'c': {
				char c = (char) va_arg(ap, int);
				if(c > 0) {
					error_str.replace(i, 2, 1, c);
				}
				i++;
				break;
			}
			default: {
				i++;
				break;
			}
		}
	}
	va_end(ap);
	bool dup_error = false;
	for(i = 0; i < messages.size(); i++) {
		if(error_str == messages[i].message()) {
			dup_error = true;
			break;
		}
	}
	if(!dup_error) {
		if(critical) messages.push_back(CalculatorMessage(error_str, MESSAGE_ERROR));
		else messages.push_back(CalculatorMessage(error_str, MESSAGE_WARNING));
	}
}
void Calculator::message(MessageType mtype, const char *TEMPLATE, ...) {
	if(disable_errors_ref > 0) {
		stopped_messages_count[disable_errors_ref - 1]++;
		if(mtype == MESSAGE_ERROR) {
			stopped_errors_count[disable_errors_ref - 1]++;
		} else if(mtype == MESSAGE_WARNING) {
			stopped_warnings_count[disable_errors_ref - 1]++;
		}
		return;
	}
	string error_str = TEMPLATE;
	va_list ap;
	va_start(ap, TEMPLATE);
	size_t i = 0;
	while(true) {
		i = error_str.find("%", i);
		if(i == string::npos || i + 1 == error_str.length()) break;
		switch(error_str[i + 1]) {
			case 's': {
				const char *str = va_arg(ap, const char*);
				if(!str) {
					i++;
				} else {
					error_str.replace(i, 2, str);
					i += strlen(str);
				}
				break;
			}
			case 'c': {
				char c = (char) va_arg(ap, int);
				if(c > 0) {
					error_str.replace(i, 2, 1, c);
				}
				i++;
				break;
			}
			default: {
				i++;
				break;
			}
		}
	}
	va_end(ap);
	bool dup_error = false;
	for(i = 0; i < messages.size(); i++) {
		if(error_str == messages[i].message()) {
			dup_error = true;
			break;
		}
	}
	if(!dup_error) {
		messages.push_back(CalculatorMessage(error_str, mtype));
	}
}
CalculatorMessage* Calculator::message() {
	if(!messages.empty()) {
		return &messages[0];
	}
	return NULL;
}
CalculatorMessage* Calculator::nextMessage() {
	if(!messages.empty()) {
		messages.erase(messages.begin());
		if(!messages.empty()) {
			return &messages[0];
		}
	}
	return NULL;
}
void Calculator::deleteName(string name_, ExpressionItem *object) {
	Variable *v2 = getVariable(name_);
	if(v2 == object) {
		return;
	}
	if(v2 != NULL) {
		v2->destroy();
	} else {
		MathFunction *f2 = getFunction(name_);
		if(f2 == object)
			return;
		if(f2 != NULL) {
			f2->destroy();
		}
	}
	deleteName(name_, object);
}
void Calculator::deleteUnitName(string name_, Unit *object) {
	Unit *u2 = getUnit(name_);
	if(u2) {
		if(u2 != object) {
			u2->destroy();
		}
		return;
	} 
	u2 = getCompositeUnit(name_);	
	if(u2) {
		if(u2 != object) {
			u2->destroy();
		}
	}
	deleteUnitName(name_, object);
}
void Calculator::saveState() {
}
void Calculator::restoreState() {
}
void Calculator::clearBuffers() {
	for(Sgi::hash_map<size_t, bool>::iterator it = ids_p.begin(); it != ids_p.end(); ++it) {
		if(!it->second) {
			freed_ids.push_back(it->first);
			id_structs.erase(it->first);
			ids_p.erase(it);
		}
	}
}
void Calculator::abort() {
	if(calculate_thread_stopped) {
		b_busy = false;
	} else {
		pthread_cancel(calculate_thread);
		restoreState();
		stopped_messages_count.clear();
		stopped_warnings_count.clear();
		stopped_errors_count.clear();
		disable_errors_ref = 0;
		clearBuffers();
		if(tmp_rpn_mstruct) tmp_rpn_mstruct->unref();
		tmp_rpn_mstruct = NULL;
		b_busy = false;
		pthread_create(&calculate_thread, &calculate_thread_attr, calculate_proc, calculate_pipe_r);
	}
}
void Calculator::abort_this() {
	restoreState();
	stopped_messages_count.clear();
	stopped_warnings_count.clear();
	stopped_errors_count.clear();
	disable_errors_ref = 0;
	clearBuffers();
	if(tmp_rpn_mstruct) tmp_rpn_mstruct->unref();
	tmp_rpn_mstruct = NULL;
	b_busy = false;
	calculate_thread_stopped = true;
	pthread_exit(PTHREAD_CANCELED);
}
bool Calculator::busy() {
	return b_busy;
}
void Calculator::terminateThreads() {
	if(!calculate_thread_stopped) {
		pthread_cancel(calculate_thread);
	}
	if(!print_thread_stopped) {
		pthread_cancel(print_thread);
	}
}

string Calculator::localizeExpression(string str) const {
	if(DOT_STR == DOT && COMMA_STR == COMMA) return str;
	vector<size_t> q_begin;
	vector<size_t> q_end;
	size_t i3 = 0;
	while(true) {
		i3 = str.find_first_of("\"\'", i3);
		if(i3 == string::npos) {
			break;
		}
		q_begin.push_back(i3);
		i3 = str.find(str[i3], i3 + 1);
		if(i3 == string::npos) {
			q_end.push_back(str.length() - 1);
			break;
		}
		q_end.push_back(i3);
		i3++;
	}
	if(COMMA_STR != COMMA) {
		size_t ui = str.find(COMMA);
		while(ui != string::npos) {
			bool b = false;
			for(size_t ui2 = 0; ui2 < q_end.size(); ui2++) {
				if(ui <= q_end[ui2] && ui >= q_begin[ui2]) {
					ui = str.find(COMMA, q_end[ui2] + 1);
					b = true;
					break;
				}
			}
			if(!b) {
				str.replace(ui, strlen(COMMA), COMMA_STR);
				ui = str.find(COMMA, ui + COMMA_STR.length());
			}
		}
	}
	if(DOT_STR != DOT) {
		size_t ui = str.find(DOT);
		while(ui != string::npos) {
			bool b = false;
			for(size_t ui2 = 0; ui2 < q_end.size(); ui2++) {
				if(ui <= q_end[ui2] && ui >= q_begin[ui2]) {
					ui = str.find(DOT, q_end[ui2] + 1);
					b = true;
					break;
				}
			}
			if(!b) {
				str.replace(ui, strlen(DOT), DOT_STR);
				ui = str.find(DOT, ui + DOT_STR.length());
			}
		}
	}
	return str;
}
string Calculator::unlocalizeExpression(string str, const ParseOptions &po) const {
	if(DOT_STR == DOT && COMMA_STR == COMMA) return str;
	vector<size_t> q_begin;
	vector<size_t> q_end;
	size_t i3 = 0;
	while(true) {
		i3 = str.find_first_of("\"\'", i3);
		if(i3 == string::npos) {
			break;
		}
		q_begin.push_back(i3);
		i3 = str.find(str[i3], i3 + 1);
		if(i3 == string::npos) {
			q_end.push_back(str.length() - 1);
			break;
		}
		q_end.push_back(i3);
		i3++;
	}
	if(DOT_STR != DOT) {
		if(po.dot_as_separator) {
			size_t ui = str.find(DOT);
			while(ui != string::npos) {
				bool b = false;
				for(size_t ui2 = 0; ui2 < q_end.size(); ui2++) {
					if(ui <= q_end[ui2] && ui >= q_begin[ui2]) {
						ui = str.find(DOT, q_end[ui2] + 1);
						b = true;
						break;
					}
				}
				if(!b) {
					str.replace(ui, strlen(DOT), SPACE);
					ui = str.find(DOT, ui + strlen(SPACE));
				}
			}
		}
		size_t ui = str.find(DOT_STR);
		while(ui != string::npos) {
			bool b = false;
			for(size_t ui2 = 0; ui2 < q_end.size(); ui2++) {
				if(ui <= q_end[ui2] && ui >= q_begin[ui2]) {
					ui = str.find(DOT_STR, q_end[ui2] + 1);
					b = true;
					break;
				}
			}
			if(!b) {
				str.replace(ui, DOT_STR.length(), DOT);
				ui = str.find(DOT_STR, ui + strlen(DOT));
			}
		}
	}
	if(COMMA_STR != COMMA) {
		size_t ui = str.find(COMMA_STR);
		while(ui != string::npos) {
			bool b = false;
			for(size_t ui2 = 0; ui2 < q_end.size(); ui2++) {
				if(ui <= q_end[ui2] && ui >= q_begin[ui2]) {
					ui = str.find(COMMA_STR, q_end[ui2] + 1);
					b = true;
					break;
				}
			}
			if(!b) {
				str.replace(ui, COMMA_STR.length(), COMMA);
				ui = str.find(COMMA_STR, ui + strlen(COMMA));
			}
		}
	}
	return str;
}

bool Calculator::calculateRPNRegister(size_t index, int msecs, const EvaluationOptions &eo) {
	if(index <= 0 || index > rpn_stack.size()) return false;
	return calculateRPN(new MathStructure(*rpn_stack[rpn_stack.size() - index]), PROC_RPN_SET, index, msecs, eo);
}

bool Calculator::calculateRPN(MathStructure *mstruct, int command, size_t index, int msecs, const EvaluationOptions &eo) {
	saveState();
	b_busy = true;
	if(calculate_thread_stopped) {
		pthread_create(&calculate_thread, &calculate_thread_attr, calculate_proc, calculate_pipe_r);
		calculate_thread_stopped = false;
	}
	bool had_msecs = msecs > 0;
	tmp_evaluationoptions = eo;
	tmp_proc_command = command;
	tmp_rpnindex = index;
	tmp_rpn_mstruct = mstruct;
	bool b_parse = false;
	fwrite(&b_parse, sizeof(bool), 1, calculate_pipe_w);
	void *x = (void*) mstruct;
	fwrite(&x, sizeof(void*), 1, calculate_pipe_w);
	fflush(calculate_pipe_w);
	struct timespec rtime;
	rtime.tv_sec = 0;
	rtime.tv_nsec = 1000000;
	while(msecs > 0 && b_busy) {
		nanosleep(&rtime, NULL);
		msecs -= 1;
	}	
	if(had_msecs && b_busy) {
		abort();
		return false;
	}
	return true;
}
bool Calculator::calculateRPN(string str, int command, size_t index, int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct, MathStructure *to_struct, bool make_to_division) {
	MathStructure *mstruct = new MathStructure();
	saveState();
	b_busy = true;
	if(calculate_thread_stopped) {
		pthread_create(&calculate_thread, &calculate_thread_attr, calculate_proc, calculate_pipe_r);
		calculate_thread_stopped = false;
	}
	bool had_msecs = msecs > 0;
	expression_to_calculate = str;
	tmp_evaluationoptions = eo;
	tmp_proc_command = command;
	tmp_rpnindex = index;
	tmp_rpn_mstruct = mstruct;
	tmp_parsedstruct = parsed_struct;
	tmp_tostruct = to_struct;
	tmp_maketodivision = make_to_division;
	bool b_parse = true;
	fwrite(&b_parse, sizeof(bool), 1, calculate_pipe_w);
	void *x = (void*) mstruct;
	fwrite(&x, sizeof(void*), 1, calculate_pipe_w);
	fflush(calculate_pipe_w);
	struct timespec rtime;
	rtime.tv_sec = 0;
	rtime.tv_nsec = 1000000;
	while(msecs > 0 && b_busy) {
		nanosleep(&rtime, NULL);
		msecs -= 1;
	}	
	if(had_msecs && b_busy) {
		abort();
		return false;
	}
	return true;
}

bool Calculator::calculateRPN(MathOperation op, int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct;
	if(rpn_stack.size() == 0) {
		mstruct = new MathStructure();
		mstruct->add(m_zero, op);
		if(parsed_struct) parsed_struct->clear();
	} else if(rpn_stack.size() == 1) {
		if(parsed_struct) {
			parsed_struct->clear();
			if(op == OPERATION_SUBTRACT) {
				parsed_struct->transform(STRUCT_ADDITION, *rpn_stack.back());
				(*parsed_struct)[1].transform(STRUCT_NEGATE);
			} else if(op == OPERATION_DIVIDE) {
				parsed_struct->transform(STRUCT_DIVISION, *rpn_stack.back());
			} else {
				parsed_struct->add(*rpn_stack.back(), op);
			}
		}
		mstruct = new MathStructure();
		mstruct->add(*rpn_stack.back(), op);
	} else {
		if(parsed_struct) {
			parsed_struct->set(*rpn_stack[rpn_stack.size() - 2]);
			if(op == OPERATION_SUBTRACT) {
				parsed_struct->transform(STRUCT_ADDITION, *rpn_stack.back());
				(*parsed_struct)[1].transform(STRUCT_NEGATE);
			} else if(op == OPERATION_DIVIDE) {
				parsed_struct->transform(STRUCT_DIVISION, *rpn_stack.back());
			} else {
				parsed_struct->add(*rpn_stack.back(), op);
			}
		}
		mstruct = new MathStructure(*rpn_stack[rpn_stack.size() - 2]);
		mstruct->add(*rpn_stack.back(), op);
	}
	return calculateRPN(mstruct, PROC_RPN_OPERATION_2, 0, msecs, eo);
}
bool Calculator::calculateRPN(MathFunction *f, int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct = new MathStructure(f, NULL);
	if(f->args() != 0) {
		if(rpn_stack.size() == 0) mstruct->addChild(m_zero);
		else mstruct->addChild(*rpn_stack.back());
		f->appendDefaultValues(*mstruct);
		if(f->getArgumentDefinition(1) && f->getArgumentDefinition(1)->type() == ARGUMENT_TYPE_ANGLE) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {
					(*mstruct)[0].multiply(getDegUnit());
					break;
				}
				case ANGLE_UNIT_GRADIANS: {
					(*mstruct)[0].multiply(getGraUnit());
					break;
				}
				case ANGLE_UNIT_RADIANS: {
					(*mstruct)[0].multiply(getRadUnit());
					break;
				}
				default: {}
			}
		}
	}
	if(parsed_struct) parsed_struct->set(*mstruct);
	return calculateRPN(mstruct, PROC_RPN_OPERATION_1, 0, msecs, eo);
}
bool Calculator::calculateRPNBitwiseNot(int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct;
	if(rpn_stack.size() == 0) {
		mstruct = new MathStructure();
		mstruct->setBitwiseNot();
	} else {
		mstruct = new MathStructure(*rpn_stack.back());
		mstruct->setBitwiseNot();
	}
	if(parsed_struct) parsed_struct->set(*mstruct);
	return calculateRPN(mstruct, PROC_RPN_OPERATION_1, 0, msecs, eo);
}
bool Calculator::calculateRPNLogicalNot(int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct;
	if(rpn_stack.size() == 0) {
		mstruct = new MathStructure();
		mstruct->setLogicalNot();
	} else {
		mstruct = new MathStructure(*rpn_stack.back());
		mstruct->setLogicalNot();
	}
	if(parsed_struct) parsed_struct->set(*rpn_stack.back());
	return calculateRPN(mstruct, PROC_RPN_OPERATION_1, 0, msecs, eo);
}
MathStructure *Calculator::calculateRPN(MathOperation op, const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct;
	if(rpn_stack.size() == 0) {
		mstruct = new MathStructure();
		mstruct->add(m_zero, op);
		if(parsed_struct) parsed_struct->clear();
	} else if(rpn_stack.size() == 1) {
		if(parsed_struct) {
			parsed_struct->clear();
			if(op == OPERATION_SUBTRACT) {
				parsed_struct->transform(STRUCT_ADDITION, *rpn_stack.back());
				(*parsed_struct)[1].transform(STRUCT_NEGATE);
			} else if(op == OPERATION_DIVIDE) {
				parsed_struct->transform(STRUCT_DIVISION, *rpn_stack.back());
			} else {
				parsed_struct->add(*rpn_stack.back(), op);
			}
		}
		mstruct = new MathStructure();
		mstruct->add(*rpn_stack.back(), op);
	} else {
		if(parsed_struct) {
			parsed_struct->set(*rpn_stack[rpn_stack.size() - 2]);
			if(op == OPERATION_SUBTRACT) {
				parsed_struct->transform(STRUCT_ADDITION, *rpn_stack.back());
				(*parsed_struct)[1].transform(STRUCT_NEGATE);
			} else if(op == OPERATION_DIVIDE) {
				parsed_struct->transform(STRUCT_DIVISION, *rpn_stack.back());
			} else {
				parsed_struct->add(*rpn_stack.back(), op);
			}
		}
		mstruct = new MathStructure(*rpn_stack[rpn_stack.size() - 2]);
		mstruct->add(*rpn_stack.back(), op);
	}
	mstruct->eval(eo);
	autoConvert(*mstruct, *mstruct, eo);
	if(rpn_stack.size() > 1) {
		rpn_stack.back()->unref();
		rpn_stack.erase(rpn_stack.begin() + (rpn_stack.size() - 1));
	}
	if(rpn_stack.size() > 0) {
		rpn_stack.back()->unref();
		rpn_stack.back() = mstruct;
	} else {
		rpn_stack.push_back(mstruct);
	}
	return rpn_stack.back();
}
MathStructure *Calculator::calculateRPN(MathFunction *f, const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct = new MathStructure(f, NULL);
	if(f->args() != 0) {
		if(rpn_stack.size() == 0) mstruct->addChild(m_zero);
		else mstruct->addChild(*rpn_stack.back());
		f->appendDefaultValues(*mstruct);
		if(f->getArgumentDefinition(1) && f->getArgumentDefinition(1)->type() == ARGUMENT_TYPE_ANGLE) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {
					(*mstruct)[0].multiply(getDegUnit());
					break;
				}
				case ANGLE_UNIT_GRADIANS: {
					(*mstruct)[0].multiply(getGraUnit());
					break;
				}
				case ANGLE_UNIT_RADIANS: {
					(*mstruct)[0].multiply(getRadUnit());
					break;
				}
				default: {}
			}
		}
	}
	if(parsed_struct) parsed_struct->set(*mstruct);
	mstruct->eval(eo);
	autoConvert(*mstruct, *mstruct, eo);
	if(rpn_stack.size() == 0) {
		rpn_stack.push_back(mstruct);
	} else {
		rpn_stack.back()->unref();
		rpn_stack.back() = mstruct;
	}
	return rpn_stack.back();
}
MathStructure *Calculator::calculateRPNBitwiseNot(const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct;
	if(rpn_stack.size() == 0) {
		mstruct = new MathStructure();
		mstruct->setBitwiseNot();
	} else {
		mstruct = new MathStructure(*rpn_stack.back());
		mstruct->setBitwiseNot();
	}
	if(parsed_struct) parsed_struct->set(*mstruct);
	mstruct->eval(eo);
	autoConvert(*mstruct, *mstruct, eo);
	if(rpn_stack.size() == 0) {
		rpn_stack.push_back(mstruct);
	} else {
		rpn_stack.back()->unref();
		rpn_stack.back() = mstruct;
	}
	return rpn_stack.back();
}
MathStructure *Calculator::calculateRPNLogicalNot(const EvaluationOptions &eo, MathStructure *parsed_struct) {
	MathStructure *mstruct;
	if(rpn_stack.size() == 0) {
		mstruct = new MathStructure();
		mstruct->setLogicalNot();
	} else {
		mstruct = new MathStructure(*rpn_stack.back());
		mstruct->setLogicalNot();
	}
	if(parsed_struct) parsed_struct->set(*mstruct);
	mstruct->eval(eo);
	autoConvert(*mstruct, *mstruct, eo);
	if(rpn_stack.size() == 0) {
		rpn_stack.push_back(mstruct);
	} else {
		rpn_stack.back()->unref();
		rpn_stack.back() = mstruct;
	}
	return rpn_stack.back();
}
bool Calculator::RPNStackEnter(MathStructure *mstruct, int msecs, const EvaluationOptions &eo) {
	return calculateRPN(mstruct, PROC_RPN_ADD, 0, msecs, eo);
}
bool Calculator::RPNStackEnter(string str, int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct, MathStructure *to_struct, bool make_to_division) {
	return calculateRPN(str, PROC_RPN_ADD, 0, msecs, eo, parsed_struct, to_struct, make_to_division);
}
void Calculator::RPNStackEnter(MathStructure *mstruct, bool eval, const EvaluationOptions &eo) {
	if(eval) {
		mstruct->eval();
		autoConvert(*mstruct, *mstruct, eo);
	}
	rpn_stack.push_back(mstruct);
}
void Calculator::RPNStackEnter(string str, const EvaluationOptions &eo, MathStructure *parsed_struct, MathStructure *to_struct, bool make_to_division) {
	rpn_stack.push_back(new MathStructure(calculate(str, eo, parsed_struct, to_struct, make_to_division)));
}
bool Calculator::setRPNRegister(size_t index, MathStructure *mstruct, int msecs, const EvaluationOptions &eo) {
	if(mstruct == NULL) {
		deleteRPNRegister(index);
		return true;
	}
	if(index <= 0 || index > rpn_stack.size()) return false;
	return calculateRPN(mstruct, PROC_RPN_SET, index, msecs, eo);
}
bool Calculator::setRPNRegister(size_t index, string str, int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct, MathStructure *to_struct, bool make_to_division) {
	if(index <= 0 || index > rpn_stack.size()) return false;
	return calculateRPN(str, PROC_RPN_OPERATION_2, index, msecs, eo, parsed_struct, to_struct, make_to_division);
}
void Calculator::setRPNRegister(size_t index, MathStructure *mstruct, bool eval, const EvaluationOptions &eo) {
	if(mstruct == NULL) {
		deleteRPNRegister(index);
		return;
	}
	if(eval) {
		mstruct->eval();
		autoConvert(*mstruct, *mstruct, eo);
	}
	if(index <= 0 || index > rpn_stack.size()) return;
	index = rpn_stack.size() - index;
	rpn_stack[index]->unref();
	rpn_stack[index] = mstruct;
}
void Calculator::setRPNRegister(size_t index, string str, const EvaluationOptions &eo, MathStructure *parsed_struct, MathStructure *to_struct, bool make_to_division) {
	if(index <= 0 || index > rpn_stack.size()) return;
	index = rpn_stack.size() - index;
	MathStructure *mstruct = new MathStructure(calculate(str, eo, parsed_struct, to_struct, make_to_division));
	rpn_stack[index]->unref();
	rpn_stack[index] = mstruct;
}
void Calculator::deleteRPNRegister(size_t index) {
	if(index <= 0 || index > rpn_stack.size()) return;
	index = rpn_stack.size() - index;
	rpn_stack[index]->unref();
	rpn_stack.erase(rpn_stack.begin() + index);
}
MathStructure *Calculator::getRPNRegister(size_t index) const {
	if(index > 0 && index <= rpn_stack.size()) {
		index = rpn_stack.size() - index;
		return rpn_stack[index];
	}
	return NULL;
}
size_t Calculator::RPNStackSize() const {
	return rpn_stack.size();
}
void Calculator::clearRPNStack() {
	for(size_t i = 0; i < rpn_stack.size(); i++) {
		rpn_stack[i]->unref();
	}
	rpn_stack.clear();
}
void Calculator::moveRPNRegister(size_t old_index, size_t new_index) {
	if(old_index == new_index) return;
	if(old_index > 0 && old_index <= rpn_stack.size()) {
		old_index = rpn_stack.size() - old_index;
		MathStructure *mstruct = rpn_stack[old_index];
		if(new_index > rpn_stack.size()) {
			new_index = 0;
		} else if(new_index <= 1) {
			rpn_stack.push_back(mstruct);
			rpn_stack.erase(rpn_stack.begin() + old_index);
			return;
		} else {
			new_index = rpn_stack.size() - new_index;
		}
		if(new_index > old_index) {
			rpn_stack.erase(rpn_stack.begin() + old_index);
			rpn_stack.insert(rpn_stack.begin() + new_index, mstruct);
		} else if(new_index < old_index) {
			rpn_stack.insert(rpn_stack.begin() + new_index, mstruct);
			rpn_stack.erase(rpn_stack.begin() + (old_index + 1));
		}
	}
}
void Calculator::moveRPNRegisterUp(size_t index) {
	if(index > 1 && index <= rpn_stack.size()) {
		index = rpn_stack.size() - index;
		MathStructure *mstruct = rpn_stack[index];
		rpn_stack.erase(rpn_stack.begin() + index);
		index++;
		if(index == rpn_stack.size()) rpn_stack.push_back(mstruct);
		else rpn_stack.insert(rpn_stack.begin() + index, mstruct);
	}
}
void Calculator::moveRPNRegisterDown(size_t index) {
	if(index > 0 && index < rpn_stack.size()) {
		index = rpn_stack.size() - index;
		MathStructure *mstruct = rpn_stack[index];
		rpn_stack.erase(rpn_stack.begin() + index);
		index--;
		rpn_stack.insert(rpn_stack.begin() + index, mstruct);
	}
}

bool Calculator::calculate(MathStructure *mstruct, string str, int msecs, const EvaluationOptions &eo, MathStructure *parsed_struct, MathStructure *to_struct, bool make_to_division) {
	mstruct->set(string(_("calculating...")));
	saveState();
	b_busy = true;
	if(calculate_thread_stopped) {
		pthread_create(&calculate_thread, &calculate_thread_attr, calculate_proc, calculate_pipe_r);
		calculate_thread_stopped = false;
	}
	bool had_msecs = msecs > 0;
	expression_to_calculate = str;
	tmp_evaluationoptions = eo;
	tmp_proc_command = PROC_NO_COMMAND;
	tmp_rpn_mstruct = NULL;
	tmp_parsedstruct = parsed_struct;
	tmp_tostruct = to_struct;
	tmp_maketodivision = make_to_division;
	bool b_parse = true;
	fwrite(&b_parse, sizeof(bool), 1, calculate_pipe_w);
	void *x = (void*) mstruct;
	fwrite(&x, sizeof(void*), 1, calculate_pipe_w);
	fflush(calculate_pipe_w);
	struct timespec rtime;
	rtime.tv_sec = 0;
	rtime.tv_nsec = 1000000;
	while(msecs > 0 && b_busy) {
		nanosleep(&rtime, NULL);
		msecs -= 1;
	}	
	if(had_msecs && b_busy) {
		abort();
		mstruct->set(string(_("aborted")));
		return false;
	}
	return true;
}
bool Calculator::separateToExpression(string &str, string &to_str, const EvaluationOptions &eo) const {
	to_str = "";
	size_t i = 0;
	if(eo.parse_options.units_enabled && (i = str.find(_(" to "))) != string::npos) {
		size_t l = strlen(_(" to "));
		to_str = str.substr(i + l, str.length() - i - l);		
		remove_blank_ends(to_str);
		if(!to_str.empty()) {
			str = str.substr(0, i);
			return true;
		}
	} else if(local_to && eo.parse_options.units_enabled && (i = str.find(" to ")) != string::npos) {
		size_t l = strlen(" to ");
		to_str = str.substr(i + l, str.length() - i - l);		
		remove_blank_ends(to_str);
		if(!to_str.empty()) {
			str = str.substr(0, i);
			return true;
		}
	}
	return false;
}
MathStructure Calculator::calculate(string str, const EvaluationOptions &eo, MathStructure *parsed_struct, MathStructure *to_struct, bool make_to_division) {
	string str2;
	separateToExpression(str, str2, eo);
	MathStructure mstruct;
	parse(&mstruct, str, eo.parse_options);
	if(parsed_struct) {
		beginTemporaryStopMessages();
		ParseOptions po = eo.parse_options;
		po.preserve_format = true;
		parse(parsed_struct, str, po);
		endTemporaryStopMessages();
	}
	mstruct.eval(eo);
	if(!str2.empty()) {
		Unit *u = getUnit(str2);
		if(u) {
			if(to_struct) to_struct->set(u);
			return convert(mstruct, u, eo);
		}
		for(size_t i = 0; i < signs.size(); i++) {
			if(str2 == signs[i]) {
				u = getUnit(real_signs[i]);
				break;
			}
		}
		if(u) {
			if(to_struct) to_struct->set(u);
			return convert(mstruct, u, eo);
		}
		CompositeUnit cu("", "temporary_composite_convert", "", str2);
		if(to_struct) to_struct->set(cu.generateMathStructure(make_to_division));
		if(cu.countUnits() > 0)	return convertToCompositeUnit(mstruct, &cu, eo);
	} else {
		if(to_struct) to_struct->setUndefined();
		switch(eo.auto_post_conversion) {
			case POST_CONVERSION_BEST: {
				return convertToBestUnit(mstruct, eo);
			}
			case POST_CONVERSION_BASE: {
				return convertToBaseUnits(mstruct, eo);
			}
			default: {}
		}
	}
	return mstruct;
}
string Calculator::printMathStructureTimeOut(const MathStructure &mstruct, int msecs, const PrintOptions &po) {
	tmp_printoptions = po;
	saveState();
	b_busy = true;
	if(print_thread_stopped) {
		pthread_create(&print_thread, &print_thread_attr, print_proc, print_pipe_r);
		print_thread_stopped = false;
	}
	void *x = (void*) &mstruct;
	fwrite(&x, sizeof(void*), 1, print_pipe_w);
	fflush(print_pipe_w);
	struct timespec rtime;
	rtime.tv_sec = 0;
	rtime.tv_nsec = 1000000;
	while(msecs > 0 && b_busy) {
		nanosleep(&rtime, NULL);
		msecs -= 1;
	}
	if(b_busy) {
		pthread_cancel(print_thread);
		restoreState();
		clearBuffers();
		b_busy = false;
		pthread_create(&print_thread, &print_thread_attr, print_proc, print_pipe_r);
		tmp_print_result = _("timed out");
	}
	return tmp_print_result;
}

MathStructure Calculator::convert(double value, Unit *from_unit, Unit *to_unit, const EvaluationOptions &eo) {
	return convert(value, from_unit, to_unit, eo);
}
MathStructure Calculator::convert(string str, Unit *from_unit, Unit *to_unit, const EvaluationOptions &eo) {
	MathStructure mstruct;
	parse(&mstruct, str, eo.parse_options);
	mstruct *= from_unit;
	mstruct.eval(eo);
	mstruct.convert(to_unit, true);
	mstruct.divide(to_unit, true);
	mstruct.eval(eo);
	return mstruct;
}
MathStructure Calculator::convert(const MathStructure &mstruct, Unit *to_unit, const EvaluationOptions &eo, bool always_convert) {
	if(to_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) return convertToCompositeUnit(mstruct, (CompositeUnit*) to_unit, eo, always_convert);
	if(to_unit->subtype() != SUBTYPE_ALIAS_UNIT || (((AliasUnit*) to_unit)->baseUnit()->subtype() != SUBTYPE_COMPOSITE_UNIT && ((AliasUnit*) to_unit)->baseExponent() == 1)) {
		MathStructure mstruct_new(mstruct);
		if(!mstruct_new.convert(to_unit, true)) {
			mstruct_new = mstruct;
		} else {
			mstruct_new.eval(eo);
			return mstruct_new;
		}
	}
	MathStructure mstruct_new(mstruct);
	if(mstruct_new.isAddition()) {
		for(size_t i = 0; i < mstruct_new.size(); i++) {
			mstruct_new[i] = convert(mstruct_new[i], to_unit, eo, false);
		}
		mstruct_new.childrenUpdated();
		EvaluationOptions eo2 = eo;
		//eo2.calculate_functions = false;
		eo2.sync_units = false;
		eo2.keep_prefixes = true;
		mstruct_new.eval(eo2);
	} else {
		bool b = false;
		if(mstruct_new.convert(to_unit) || always_convert) {
			b = true;
		} else if(to_unit->subtype() == SUBTYPE_ALIAS_UNIT && ((AliasUnit*) to_unit)->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			CompositeUnit *cu = (CompositeUnit*) ((AliasUnit*) to_unit)->baseUnit();
			switch(mstruct.type()) {
				case STRUCT_UNIT: {
					if(cu->containsRelativeTo(mstruct_new.unit())) {
						b = true;
					}
					break;
				} 
				case STRUCT_MULTIPLICATION: {
					for(size_t i = 1; i <= mstruct_new.countChildren(); i++) {
						if(mstruct_new.getChild(i)->isUnit() && cu->containsRelativeTo(mstruct_new.getChild(i)->unit())) {
							b = true;
							break;
						}
						if(mstruct_new.getChild(i)->isPower() && mstruct_new.getChild(i)->base()->isUnit() && cu->containsRelativeTo(mstruct_new.getChild(i)->base()->unit())) {
							b = true;
							break;
						}
					}
					break;
				}
				case STRUCT_POWER: {				
					if(mstruct_new.base()->isUnit() && cu->containsRelativeTo(mstruct_new.base()->unit())) {
						b = true;
					}
					break;
				}
				default: {}
			}
		}
		if(b) {
			mstruct_new.divide(to_unit);
			EvaluationOptions eo2 = eo;
			//eo2.calculate_functions = false;
			eo2.sync_units = true;
			eo2.keep_prefixes = false;
			mstruct_new.eval(eo2);
			if(mstruct_new.isOne()) mstruct_new.set(to_unit);
			else mstruct_new.multiply(to_unit, true);
			eo2.sync_units = false;
			eo2.keep_prefixes = true;
			mstruct_new.eval(eo2);
		}
	}
	return mstruct_new;
}
MathStructure Calculator::convertToBaseUnits(const MathStructure &mstruct, const EvaluationOptions &eo) {
	MathStructure mstruct_new(mstruct);
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->subtype() == SUBTYPE_BASE_UNIT) {
			mstruct_new.convert(units[i], true);
		}
	}
	EvaluationOptions eo2 = eo;
	//eo2.calculate_functions = false;
	mstruct_new.eval(eo2);
	return mstruct_new;
}
Unit *Calculator::getBestUnit(Unit *u, bool allow_only_div) {
	switch(u->subtype()) {
		case SUBTYPE_BASE_UNIT: {
			return u;
		}
		case SUBTYPE_ALIAS_UNIT: {
			AliasUnit *au = (AliasUnit*) u;
			if(au->baseExponent() == 1 && au->baseUnit()->subtype() == SUBTYPE_BASE_UNIT) {
				return (Unit*) au->baseUnit();
			} else if(au->firstBaseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT || au->firstBaseExponent() != 1) {
				return u;
			} else {
				return getBestUnit((Unit*) au->firstBaseUnit());
			}
		}
		case SUBTYPE_COMPOSITE_UNIT: {
			CompositeUnit *cu = (CompositeUnit*) u;
			int exp, b_exp;
			int points = 0;
			bool minus = false;
			int new_points;
			int new_points_m;
			int max_points = 0;
			for(size_t i = 1; i <= cu->countUnits(); i++) {
				cu->get(i, &exp);
				if(exp < 0) {
					max_points -= exp;
				} else {
					max_points += exp;
				}
			}
			Unit *best_u = NULL;
			Unit *bu;
			AliasUnit *au;
			for(size_t i = 0; i < units.size(); i++) {
				if(units[i]->subtype() == SUBTYPE_BASE_UNIT && (points == 0 || (points == 1 && minus))) {
					new_points = 0;
					for(size_t i2 = 1; i2 <= cu->countUnits(); i2++) {
						if(cu->get(i2)->baseUnit() == units[i]) {
							points = 1;
							best_u = units[i];
							minus = false;
							break;
						}
					}
				} else if(!units[i]->isSIUnit()) {
				} else if(units[i]->subtype() == SUBTYPE_ALIAS_UNIT) {
					au = (AliasUnit*) units[i];
					bu = (Unit*) au->baseUnit();
					b_exp = au->baseExponent();
					new_points = 0;
					new_points_m = 0;
					if(b_exp != 1 || bu->subtype() == SUBTYPE_COMPOSITE_UNIT) {
						if(bu->subtype() == SUBTYPE_BASE_UNIT) {
							for(size_t i2 = 1; i2 <= cu->countUnits(); i2++) {
								if(cu->get(i2, &exp) == bu) {
									bool m = false;
									if(b_exp < 0 && exp < 0) {
										b_exp = -b_exp;
										exp = -exp;
									} else if(b_exp < 0) {
										b_exp = -b_exp;	
										m = true;
									} else if(exp < 0) {
										exp = -exp;
										m = true;
									}
									new_points = exp - b_exp;
									if(new_points < 0) {
										new_points = -new_points;
									}
									new_points = exp - new_points;
									if(!allow_only_div && m && new_points >= max_points) {
										new_points = -1;
									}
									if(new_points > points || (!m && minus && new_points == points)) {
										points = new_points;
										minus = m;
										best_u = units[i];
									}
									break;
								}
							}
						} else if(au->firstBaseExponent() != 1 || au->firstBaseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
							MathStructure cu_mstruct = ((CompositeUnit*) bu)->generateMathStructure();
							cu_mstruct.raise(b_exp);
							cu_mstruct = convertToBaseUnits(cu_mstruct);
							if(cu_mstruct.isMultiplication()) {
								for(size_t i2 = 1; i2 <= cu_mstruct.countChildren(); i2++) {
									bu = NULL;
									if(cu_mstruct.getChild(i2)->isUnit()) {
										bu = cu_mstruct.getChild(i2)->unit();
										b_exp = 1;
									} else if(cu_mstruct.getChild(i2)->isPower() && cu_mstruct.getChild(i2)->base()->isUnit() && cu_mstruct.getChild(i2)->exponent()->isNumber() && cu_mstruct.getChild(i2)->exponent()->number().isInteger()) {
										bu = cu_mstruct.getChild(i2)->base()->unit();
										b_exp = cu_mstruct.getChild(i2)->exponent()->number().intValue();
									}
									if(bu) {
										bool b = false;
										for(size_t i3 = 1; i3 <= cu->countUnits(); i3++) {
											if(cu->get(i3, &exp) == bu) {
												b = true;
												bool m = false;
												if(exp < 0 && b_exp > 0) {
													new_points -= b_exp;
													exp = -exp;
													m = true;
												} else if(exp > 0 && b_exp < 0) {
													new_points += b_exp;
													b_exp = -b_exp;
													m = true;
												} else {
													if(b_exp < 0) new_points_m += b_exp;
													else new_points_m -= b_exp;
												}
												if(exp < 0) {
													exp = -exp;
													b_exp = -b_exp;
												}
												if(exp >= b_exp) {
													if(m) new_points_m += exp - (exp - b_exp);
													else new_points += exp - (exp - b_exp);
												} else {
													if(m) new_points_m += exp - (b_exp - exp);
													else new_points += exp - (b_exp - exp);
												}
												break;
											}
										}
										if(!b) {
											if(b_exp < 0) b_exp = -b_exp;
											new_points -= b_exp;	
											new_points_m -= b_exp;
										}
									}
								}
								if(!allow_only_div && new_points_m >= max_points) {
									new_points_m = -1;
								}
								if(new_points > points && new_points >= new_points_m) {
									minus = false;
									points = new_points;
									best_u = au;
								} else if(new_points_m > points || (new_points_m == points && minus)) {
									minus = true;
									points = new_points_m;
									best_u = au;
								}
							}
						}
					}
				}
				if(points >= max_points && !minus) break;
			}
			best_u = getBestUnit(best_u);
			if(points > 1 && points < max_points - 1) {
				CompositeUnit *cu_new = new CompositeUnit("", "temporary_composite_convert");
				bool return_cu = minus;
				if(minus) {
					cu_new->add(best_u, -1);
				} else {
					cu_new->add(best_u);
				}
				MathStructure cu_mstruct = ((CompositeUnit*) u)->generateMathStructure();
				if(minus) cu_mstruct *= best_u;
				else cu_mstruct /= best_u;
				cu_mstruct = convertToBaseUnits(cu_mstruct);
				CompositeUnit *cu2 = new CompositeUnit("", "temporary_composite_convert_to_best_unit");
				bool b = false;
				for(size_t i = 1; i <= cu_mstruct.countChildren(); i++) {
					if(cu_mstruct.getChild(i)->isUnit()) {
						b = true;
						cu2->add(cu_mstruct.getChild(i)->unit());
					} else if(cu_mstruct.getChild(i)->isPower() && cu_mstruct.getChild(i)->base()->isUnit() && cu_mstruct.getChild(i)->exponent()->isNumber() && cu_mstruct.getChild(i)->exponent()->number().isInteger()) {
						b = true;
						cu2->add(cu_mstruct.getChild(i)->base()->unit(), cu_mstruct.getChild(i)->exponent()->number().intValue());
					}
				}
				if(b) {
					Unit *u2 = getBestUnit(cu2, true);
					b = false;
					if(u2->subtype() == SUBTYPE_COMPOSITE_UNIT) {
						for(size_t i3 = 1; i3 <= ((CompositeUnit*) u2)->countUnits(); i3++) {
							Unit *cu_unit = ((CompositeUnit*) u2)->get(i3, &exp);
							for(size_t i4 = 1; i4 <= cu_new->countUnits(); i4++) {
								if(cu_new->get(i4, &b_exp) == cu_unit) {
									b = true;
									cu_new->setExponent(i4, b_exp + exp);
									break;
								}
							}
							if(!b) cu_new->add(cu_unit, exp);
						}
						return_cu = true;
						delete u2;
					} else if(u2->subtype() == SUBTYPE_ALIAS_UNIT) {
						return_cu = true;
						for(size_t i3 = 1; i3 <= cu_new->countUnits(); i3++) {
							if(cu_new->get(i3, &exp) == u2) {
								b = true;
								cu_new->setExponent(i3, exp + 1);
								break;
							}
						}
						if(!b) cu_new->add(u2);
					}
				}
				delete cu2;
				if(return_cu) {
					return cu_new;
				} else {
					delete cu_new;
					return best_u;
				}
			}
			if(minus) {
				CompositeUnit *cu_new = new CompositeUnit("", "temporary_composite_convert");
				cu_new->add(best_u, -1);
				return cu_new;
			} else {
				return best_u;
			}
		}
	}
	return u;	
}
MathStructure Calculator::convertToBestUnit(const MathStructure &mstruct, const EvaluationOptions &eo) {
	EvaluationOptions eo2 = eo;
	//eo2.calculate_functions = false;
	eo2.sync_units = false;
	switch(mstruct.type()) {
		case STRUCT_BITWISE_XOR: {}
		case STRUCT_BITWISE_OR: {}
		case STRUCT_BITWISE_AND: {}
		case STRUCT_BITWISE_NOT: {}
		case STRUCT_LOGICAL_XOR: {}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_AND: {}
		case STRUCT_LOGICAL_NOT: {}
		case STRUCT_COMPARISON: {}
		case STRUCT_FUNCTION: {}
		case STRUCT_VECTOR: {}
		case STRUCT_ADDITION: {
			MathStructure mstruct_new(mstruct);
			for(size_t i = 0; i < mstruct_new.size(); i++) {
				mstruct_new[i] = convertToBestUnit(mstruct_new[i], eo);
			}
			mstruct_new.childrenUpdated();
			mstruct_new.eval(eo2);
			return mstruct_new;
		}
		case STRUCT_POWER: {
			MathStructure mstruct_new(mstruct);
			if(mstruct_new.base()->isUnit() && mstruct_new.exponent()->isNumber() && mstruct_new.exponent()->number().isInteger()) {
				CompositeUnit *cu = new CompositeUnit("", "temporary_composite_convert_to_best_unit");
				cu->add(mstruct_new.base()->unit(), mstruct_new.exponent()->number().intValue());
				mstruct_new = convert(mstruct_new, getBestUnit(cu), eo, true);
				delete cu;
			} else {
				mstruct_new[0] = convertToBestUnit(mstruct_new[0], eo);
				mstruct_new[1] = convertToBestUnit(mstruct_new[1], eo);
				mstruct_new.childrenUpdated();
				mstruct_new.eval(eo2);
			}
			return mstruct_new;
		}
		case STRUCT_UNIT: {
			return convert(mstruct, getBestUnit(mstruct.unit()), eo, true);
		}
		case STRUCT_MULTIPLICATION: {
			MathStructure mstruct_new(convertToBaseUnits(mstruct, eo));
			CompositeUnit *cu = new CompositeUnit("", "temporary_composite_convert_to_best_unit");
			bool b = false;
			for(size_t i = 1; i <= mstruct_new.countChildren(); i++) {
				if(mstruct_new.getChild(i)->isUnit()) {
					b = true;
					cu->add(mstruct_new.getChild(i)->unit());
				} else if(mstruct_new.getChild(i)->isPower() && mstruct_new.getChild(i)->base()->isUnit() && mstruct_new.getChild(i)->exponent()->isNumber() && mstruct_new.getChild(i)->exponent()->number().isInteger()) {
					b = true;
					cu->add(mstruct_new.getChild(i)->base()->unit(), mstruct_new.getChild(i)->exponent()->number().intValue());
				} else {
					mstruct_new[i - 1] = convertToBestUnit(mstruct_new[i - 1], eo);
					mstruct_new.childUpdated(i);
				}
			}
			if(b) mstruct_new = convert(mstruct_new, getBestUnit(cu), eo, true);
			delete cu;
			mstruct_new.eval(eo2);
			return mstruct_new;
		}
		default: {}
	}
	return mstruct;
}
MathStructure Calculator::convertToCompositeUnit(const MathStructure &mstruct, CompositeUnit *cu, const EvaluationOptions &eo, bool always_convert) {
	if(cu->countUnits() == 0) return mstruct;
	MathStructure mstruct_cu(cu->generateMathStructure());
	MathStructure mstruct_new(mstruct);
	if(mstruct_new.isAddition()) {
		for(size_t i = 0; i < mstruct_new.size(); i++) {
			mstruct_new[i] = convertToCompositeUnit(mstruct_new[i], cu, eo, false);
		}
		mstruct_new.childrenUpdated();
		EvaluationOptions eo2 = eo;
		//eo2.calculate_functions = false;
		eo2.sync_units = false;
		eo2.keep_prefixes = true;
		mstruct_new.eval(eo2);
	} else {
		bool b = false;
		if(mstruct_new.convert(cu, true) || always_convert) {	
			b = true;
		} else {
			switch(mstruct_new.type()) {
				case STRUCT_UNIT: {
					if(cu->containsRelativeTo(mstruct_new.unit())) {
						b = true;
					}
					break;
				} 
				case STRUCT_MULTIPLICATION: {
					for(size_t i = 1; i <= mstruct_new.countChildren(); i++) {
						if(mstruct_new.getChild(i)->isUnit() && cu->containsRelativeTo(mstruct_new.getChild(i)->unit())) {
							b = true;
						}
						if(mstruct_new.getChild(i)->isPower() && mstruct_new.getChild(i)->base()->isUnit() && cu->containsRelativeTo(mstruct_new.getChild(i)->base()->unit())) {
							b = true;
						}
					}
					break;
				}
				case STRUCT_POWER: {
					if(mstruct_new.base()->isUnit() && cu->containsRelativeTo(mstruct_new.base()->unit())) {
						b = true;
					}
					break;				
				}
				default: {}
			}
		}
		if(b) {	
			mstruct_new.divide(mstruct_cu);
			EvaluationOptions eo2 = eo;
			//eo2.calculate_functions = false;
			eo2.sync_units = true;
			eo2.keep_prefixes = false;
			mstruct_new.eval(eo2);
			if(mstruct_new.isOne()) mstruct_new = mstruct_cu;
			else mstruct_new.multiply(mstruct_cu, true);
			eo2.sync_units = false;
			eo2.keep_prefixes = true;
			mstruct_new.eval(eo2);
		}
	}
	return mstruct_new;
}
MathStructure Calculator::convert(const MathStructure &mstruct, string composite_, const EvaluationOptions &eo) {
	remove_blank_ends(composite_);
	if(composite_.empty()) return mstruct;
	Unit *u = getUnit(composite_);
	if(u) return convert(mstruct, u, eo);
	for(size_t i = 0; i < signs.size(); i++) {
		if(composite_ == signs[i]) {
			u = getUnit(real_signs[i]);
			break;
		}
	}
	if(u) return convert(mstruct, u, eo);
	CompositeUnit cu("", "temporary_composite_convert", "", composite_);
	return convertToCompositeUnit(mstruct, &cu, eo);
}
Unit* Calculator::addUnit(Unit *u, bool force, bool check_names) {
	if(check_names) {
		for(size_t i = 1; i <= u->countNames(); i++) {
			u->setName(getName(u->getName(i).name, u, force), i);
		}
	}
	if(!u->isLocal() && units.size() > 0 && units[units.size() - 1]->isLocal()) {
		units.insert(units.begin(), u);
	} else {	
		units.push_back(u);
	}
	unitNameChanged(u, true);
	for(vector<Unit*>::iterator it = deleted_units.begin(); it != deleted_units.end(); ++it) {
		if(*it == u) {
			deleted_units.erase(it);
			break;
		}
	}
	u->setRegistered(true);
	u->setChanged(false);
	return u;
}
void Calculator::delPrefixUFV(Prefix *object) {
	int i = 0;
	for(vector<void*>::iterator it = ufvl.begin(); ; ++it) {
		del_ufvl:
		if(it == ufvl.end()) {
			break;
		}
		if(*it == object) {
			it = ufvl.erase(it);
			ufvl_t.erase(ufvl_t.begin() + i);
			ufvl_i.erase(ufvl_i.begin() + i);
			if(it == ufvl.end()) break;
			goto del_ufvl;
		}
		i++;
	}
	for(size_t i2 = 0; i2 < UFV_LENGTHS; i2++) {
		i = 0;
		for(vector<void*>::iterator it = ufv[0][i2].begin(); ; ++it) {
			del_ufv:
			if(it == ufv[0][i2].end()) {
				break;
			}
			if(*it == object) {
				it = ufv[0][i2].erase(it);
				ufv_i[0][i2].erase(ufv_i[0][i2].begin() + i);
				if(it == ufv[0][i2].end()) break;
				goto del_ufv;
			}
			i++;
		}
	}
}
void Calculator::delUFV(ExpressionItem *object) {
	int i = 0;
	for(vector<void*>::iterator it = ufvl.begin(); ; ++it) {
		del_ufvl:
		if(it == ufvl.end()) {
			break;
		}
		if(*it == object) {
			it = ufvl.erase(it);
			ufvl_t.erase(ufvl_t.begin() + i);
			ufvl_i.erase(ufvl_i.begin() + i);
			if(it == ufvl.end()) break;
			goto del_ufvl;
		}
		i++;
	}
	int i3 = 0;
	switch(object->type()) {
		case TYPE_FUNCTION: {i3 = 1; break;}
		case TYPE_UNIT: {i3 = 2; break;}
		case TYPE_VARIABLE: {i3 = 3; break;}
	}
	for(size_t i2 = 0; i2 < UFV_LENGTHS; i2++) {
		i = 0;
		for(vector<void*>::iterator it = ufv[i3][i2].begin(); ; ++it) {
			del_ufv:
			if(it == ufv[i3][i2].end()) {
				break;
			}
			if(*it == object) {
				it = ufv[i3][i2].erase(it);
				ufv_i[i3][i2].erase(ufv_i[i3][i2].begin() + i);
				if(it == ufv[i3][i2].end()) break;
				goto del_ufv;
			}
			i++;
		}
	}
}
Unit* Calculator::getUnit(string name_) {
	if(name_.empty()) return NULL;
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->subtype() != SUBTYPE_COMPOSITE_UNIT && (units[i]->hasName(name_))) {
			return units[i];
		}
	}
	return NULL;
}
Unit* Calculator::getActiveUnit(string name_) {
	if(name_.empty()) return NULL;
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->isActive() && units[i]->subtype() != SUBTYPE_COMPOSITE_UNIT && units[i]->hasName(name_)) {
			return units[i];
		}
	}
	return NULL;
}
Unit* Calculator::getCompositeUnit(string internal_name_) {
	if(internal_name_.empty()) return NULL;
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->subtype() == SUBTYPE_COMPOSITE_UNIT && units[i]->hasName(internal_name_)) {
			return units[i];
		}
	}
	return NULL;
}

Variable* Calculator::addVariable(Variable *v, bool force, bool check_names) {
	if(check_names) {
		for(size_t i = 1; i <= v->countNames(); i++) {
			v->setName(getName(v->getName(i).name, v, force), i);
		}
	}
	if(!v->isLocal() && variables.size() > 0 && variables[variables.size() - 1]->isLocal()) {
		variables.insert(variables.begin(), v);
	} else {
		variables.push_back(v);
	}
	variableNameChanged(v, true);
	for(vector<Variable*>::iterator it = deleted_variables.begin(); it != deleted_variables.end(); ++it) {
		if(*it == v) {
			deleted_variables.erase(it);
			break;
		}
	}
	v->setRegistered(true);
	v->setChanged(false);
	return v;
}
void Calculator::expressionItemDeactivated(ExpressionItem *item) {
	delUFV(item);
}
void Calculator::expressionItemActivated(ExpressionItem *item) {
	ExpressionItem *item2 = getActiveExpressionItem(item);
	if(item2) {
		item2->setActive(false);
	}
	nameChanged(item);
}
void Calculator::expressionItemDeleted(ExpressionItem *item) {
	switch(item->type()) {
		case TYPE_VARIABLE: {
			for(vector<Variable*>::iterator it = variables.begin(); it != variables.end(); ++it) {
				if(*it == item) {
					variables.erase(it);
					deleted_variables.push_back((Variable*) item);
					break;
				}
			}
			break;
		}
		case TYPE_FUNCTION: {
			for(vector<MathFunction*>::iterator it = functions.begin(); it != functions.end(); ++it) {
				if(*it == item) {
					functions.erase(it);
					deleted_functions.push_back((MathFunction*) item);
					break;
				}
			}
			if(item->subtype() == SUBTYPE_DATA_SET) {
				for(vector<DataSet*>::iterator it = data_sets.begin(); it != data_sets.end(); ++it) {
					if(*it == item) {
						data_sets.erase(it);
						break;
					}
				}	
			}
			break;
		}		
		case TYPE_UNIT: {
			for(vector<Unit*>::iterator it = units.begin(); it != units.end(); ++it) {
				if(*it == item) {
					units.erase(it);
					deleted_units.push_back((Unit*) item);
					break;
				}
			}		
			break;
		}		
	}
	delUFV(item);
}
void Calculator::nameChanged(ExpressionItem *item, bool new_item) {
	if(!item->isActive() || item->countNames() == 0) return;
	if(item->type() == TYPE_UNIT && ((Unit*) item)->subtype() == SUBTYPE_COMPOSITE_UNIT) {
		return;
	}
	size_t l2;
	if(!new_item) delUFV(item);
	for(size_t i2 = 1; i2 <= item->countNames(); i2++) {
		l2 = item->getName(i2).name.length();
		if(l2 > UFV_LENGTHS) {
			size_t i = 0, l = 0;
			for(vector<void*>::iterator it = ufvl.begin(); ; ++it) {
				if(it != ufvl.end()) {
					if(ufvl_t[i] == 'v')
						l = ((Variable*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'f')
						l = ((MathFunction*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'u')
						l = ((Unit*) (*it))->getName(ufvl_i[i]).name.length();
					else if(ufvl_t[i] == 'p')
						l = ((Prefix*) (*it))->shortName(false).length();
					else if(ufvl_t[i] == 'P')
						l = ((Prefix*) (*it))->longName(false).length();				
					else if(ufvl_t[i] == 'q')
						l = ((Prefix*) (*it))->unicodeName(false).length();
				}
				if(it == ufvl.end()) {
					ufvl.push_back((void*) item);
					switch(item->type()) {
						case TYPE_VARIABLE: {ufvl_t.push_back('v'); break;}
						case TYPE_FUNCTION: {ufvl_t.push_back('f'); break;}		
						case TYPE_UNIT: {ufvl_t.push_back('u'); break;}		
					}
					ufvl_i.push_back(i2);
					break;
				} else {
					if(l < l2 
					|| (item->type() == TYPE_VARIABLE && l == l2 && ufvl_t[i] == 'v')
					|| (item->type() == TYPE_FUNCTION && l == l2 && (ufvl_t[i] != 'p' && ufvl_t[i] != 'P' && ufvl_t[i] != 'q'))
					|| (item->type() == TYPE_UNIT && l == l2 && (ufvl_t[i] != 'p' && ufvl_t[i] != 'P' && ufvl_t[i] != 'q' && ufvl_t[i] != 'f'))
					) {
						ufvl.insert(it, (void*) item);
						switch(item->type()) {
							case TYPE_VARIABLE: {ufvl_t.insert(ufvl_t.begin() + i, 'v'); break;}
							case TYPE_FUNCTION: {ufvl_t.insert(ufvl_t.begin() + i, 'f'); break;}		
							case TYPE_UNIT: {ufvl_t.insert(ufvl_t.begin() + i, 'u'); break;}		
						}
						ufvl_i.insert(ufvl_i.begin() + i, i2);
						break;
					}
				}
				i++;
			}
		} else if(l2 > 0) {
			l2--;
			switch(item->type()) {			
				case TYPE_VARIABLE: {
					ufv[3][l2].push_back((void*) item);
					ufv_i[3][l2].push_back(i2);
					break;
				}
				case TYPE_FUNCTION:  {
					ufv[1][l2].push_back((void*) item);
					ufv_i[1][l2].push_back(i2);
					break;
				}
				case TYPE_UNIT:  {
					ufv[2][l2].push_back((void*) item);
					ufv_i[2][l2].push_back(i2);
					break;
				}
			}			
		}
	}
}
void Calculator::variableNameChanged(Variable *v, bool new_item) {
	nameChanged(v, new_item);
}
void Calculator::functionNameChanged(MathFunction *f, bool new_item) {
	nameChanged(f, new_item);
}
void Calculator::unitNameChanged(Unit *u, bool new_item) {
	nameChanged(u, new_item);
}

Variable* Calculator::getVariable(string name_) {
	if(name_.empty()) return NULL;
	for(size_t i = 0; i < variables.size(); i++) {
		if(variables[i]->hasName(name_)) {
			return variables[i];
		}
	}
	return NULL;
}
Variable* Calculator::getActiveVariable(string name_) {
	if(name_.empty()) return NULL;
	for(size_t i = 0; i < variables.size(); i++) {
		if(variables[i]->isActive() && variables[i]->hasName(name_)) {
			return variables[i];
		}
	}
	return NULL;
}
ExpressionItem* Calculator::addExpressionItem(ExpressionItem *item, bool force) {
	switch(item->type()) {
		case TYPE_VARIABLE: {
			return addVariable((Variable*) item, force);
		}
		case TYPE_FUNCTION: {
			if(item->subtype() == item->subtype() == SUBTYPE_DATA_SET) return addDataSet((DataSet*) item, force);
			else return addFunction((MathFunction*) item, force);
		}		
		case TYPE_UNIT: {
			return addUnit((Unit*) item, force);
		}		
	}
	return NULL;
}
MathFunction* Calculator::addFunction(MathFunction *f, bool force, bool check_names) {
	if(check_names) {
		for(size_t i = 1; i <= f->countNames(); i++) {
			f->setName(getName(f->getName(i).name, f, force), i);
		}
	}
	if(!f->isLocal() && functions.size() > 0 && functions[functions.size() - 1]->isLocal()) {
		functions.insert(functions.begin(), f);
	} else {
		functions.push_back(f);
	}
	functionNameChanged(f, true);
	for(vector<MathFunction*>::iterator it = deleted_functions.begin(); it != deleted_functions.end(); ++it) {
		if(*it == f) {
			deleted_functions.erase(it);
			break;
		}
	}
	f->setRegistered(true);
	f->setChanged(false);
	return f;
}
DataSet* Calculator::addDataSet(DataSet *dc, bool force, bool check_names) {
	addFunction(dc, force, check_names);
	data_sets.push_back(dc);
	return dc;
}
DataSet* Calculator::getDataSet(size_t index) {
	if(index > 0 && index <= data_sets.size()) {
		return data_sets[index - 1];
	}
	return 0;
}
DataSet* Calculator::getDataSet(string name) {
	if(name.empty()) return NULL;
	for(size_t i = 0; i < data_sets.size(); i++) {
		if(data_sets[i]->hasName(name)) {
			return data_sets[i];
		}
	}
	return NULL;
}
MathFunction* Calculator::getFunction(string name_) {
	if(name_.empty()) return NULL;
	for(size_t i = 0; i < functions.size(); i++) {
		if(functions[i]->hasName(name_)) {
			return functions[i];
		}
	}
	return NULL;
}
MathFunction* Calculator::getActiveFunction(string name_) {
	if(name_.empty()) return NULL;
	for(size_t i = 0; i < functions.size(); i++) {
		if(functions[i]->isActive() && functions[i]->hasName(name_)) {
			return functions[i];
		}
	}
	return NULL;
}
bool Calculator::variableNameIsValid(const string &name_) {
	return name_.find_first_of(ILLEGAL_IN_NAMES) == string::npos && !is_in(NUMBERS, name_[0]);
}
bool Calculator::functionNameIsValid(const string &name_) {
	return name_.find_first_of(ILLEGAL_IN_NAMES) == string::npos && !is_in(NUMBERS, name_[0]);
}
bool Calculator::unitNameIsValid(const string &name_) {
	return name_.find_first_of(ILLEGAL_IN_UNITNAMES) == string::npos;
}
bool Calculator::variableNameIsValid(const char *name_) {
	if(is_in(NUMBERS, name_[0])) return false;
	for(size_t i = 0; name_[i] != '\0'; i++) {
		if(is_in(ILLEGAL_IN_NAMES, name_[i])) return false;
	}
	return true;
}
bool Calculator::functionNameIsValid(const char *name_) {
	if(is_in(NUMBERS, name_[0])) return false;
	for(size_t i = 0; name_[i] != '\0'; i++) {
		if(is_in(ILLEGAL_IN_NAMES, name_[i])) return false;
	}
	return true;
}
bool Calculator::unitNameIsValid(const char *name_) {
	for(size_t i = 0; name_[i] != '\0'; i++) {
		if(is_in(ILLEGAL_IN_UNITNAMES, name_[i])) return false;
	}
	return true;
}
#define VERSION_BEFORE(i1, i2, i3) (version_numbers[0] < i1 || (version_numbers[0] == i1 && (version_numbers[1] < i2 || (version_numbers[1] == i2 && version_numbers[2] < i3))))
bool Calculator::variableNameIsValid(const string &name_, int version_numbers[3], bool is_user_defs) {
	return variableNameIsValid(name_.c_str(), version_numbers, is_user_defs);
}
bool Calculator::functionNameIsValid(const string &name_, int version_numbers[3], bool is_user_defs) {
	return functionNameIsValid(name_.c_str(), version_numbers, is_user_defs);
}
bool Calculator::unitNameIsValid(const string &name_, int version_numbers[3], bool is_user_defs) {
	return unitNameIsValid(name_.c_str(), version_numbers, is_user_defs);
}
bool Calculator::variableNameIsValid(const char *name_, int version_numbers[3], bool is_user_defs) {
	if(is_in(NUMBERS, name_[0])) return false;
	bool b = false;
	for(size_t i = 0; name_[i] != '\0'; i++) {
		if(is_in(ILLEGAL_IN_NAMES, name_[i])) {
			if(is_user_defs && VERSION_BEFORE(0, 8, 1) && name_[i] == BITWISE_NOT_CH) {
				b = true;	
			} else {
				return false;
			}
		}
	}
	if(b) {
		error(true, _("\"%s\" is not allowed in names anymore. Please change the name of \"%s\", or the variable will be lost."), BITWISE_NOT, name_, NULL);
	}
	return true;
}
bool Calculator::functionNameIsValid(const char *name_, int version_numbers[3], bool is_user_defs) {
	if(is_in(NUMBERS, name_[0])) return false;
	bool b = false;
	for(size_t i = 0; name_[i] != '\0'; i++) {
		if(is_in(ILLEGAL_IN_NAMES, name_[i])) {
			if(is_user_defs && VERSION_BEFORE(0, 8, 1) && name_[i] == BITWISE_NOT_CH) {
				b = true;	
			} else {
				return false;
			}
		}
	}
	if(b) {
		error(true, _("\"%s\" is not allowed in names anymore. Please change the name \"%s\", or the function will be lost."), BITWISE_NOT, name_, NULL);
	}
	return true;
}
bool Calculator::unitNameIsValid(const char *name_, int version_numbers[3], bool is_user_defs) {
	bool b = false;
	for(size_t i = 0; name_[i] != '\0'; i++) {
		if(is_in(ILLEGAL_IN_UNITNAMES, name_[i])) {
			if(is_user_defs && VERSION_BEFORE(0, 8, 1) && name_[i] == BITWISE_NOT_CH) {
				b = true;	
			} else {
				return false;
			}
		}
	}
	if(b) {
		error(true, _("\"%s\" is not allowed in names anymore. Please change the name \"%s\", or the unit will be lost."), BITWISE_NOT, name_, NULL);
	}
	return true;
}
string Calculator::convertToValidVariableName(string name_) {
	size_t i = 0;
	while(true) {
		i = name_.find_first_of(ILLEGAL_IN_NAMES_MINUS_SPACE_STR, i);
		if(i == string::npos)
			break;
		name_.erase(name_.begin() + i);
	}
	gsub(SPACE, UNDERSCORE, name_);
	while(is_in(NUMBERS, name_[0])) {
		name_.erase(name_.begin());
	}
	return name_;
}
string Calculator::convertToValidFunctionName(string name_) {
	return convertToValidVariableName(name_);
}
string Calculator::convertToValidUnitName(string name_) {
	size_t i = 0;
	string stmp = ILLEGAL_IN_NAMES_MINUS_SPACE_STR + NUMBERS;
	while(true) {
		i = name_.find_first_of(stmp, i);
		if(i == string::npos)
			break;
		name_.erase(name_.begin() + i);
	}
	gsub(SPACE, UNDERSCORE, name_);
	return name_;
}
bool Calculator::nameTaken(string name, ExpressionItem *object) {
	if(name.empty()) return false;
	if(object) {
		switch(object->type()) {
			case TYPE_VARIABLE: {}
			case TYPE_UNIT: {
				for(size_t index = 0; index < variables.size(); index++) {
					if(variables[index]->isActive() && variables[index]->hasName(name)) {
						return variables[index] != object;
					}
				}
				for(size_t i = 0; i < units.size(); i++) {
					if(units[i]->isActive() && units[i]->hasName(name)) {
						return units[i] != object;
					}
				}
				break;
			}
			case TYPE_FUNCTION: {
				for(size_t index = 0; index < functions.size(); index++) {
					if(functions[index]->isActive() && functions[index]->hasName(name)) {
						return functions[index] != object;
					}
				}
				break;
			}
		}
	} else {
		return getActiveExpressionItem(name) != NULL;
	}
	return false;
}
bool Calculator::variableNameTaken(string name, Variable *object) {
	if(name.empty()) return false;
	for(size_t index = 0; index < variables.size(); index++) {
		if(variables[index]->isActive() && variables[index]->hasName(name)) {
			return variables[index] != object;
		}
	}
	
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->isActive() && units[i]->hasName(name)) {
			return true;
		}
	}
	return false;
}
bool Calculator::unitNameTaken(string name, Unit *object) {
	if(name.empty()) return false;
	for(size_t index = 0; index < variables.size(); index++) {
		if(variables[index]->isActive() && variables[index]->hasName(name)) {
			return true;
		}
	}
	
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->isActive() && units[i]->hasName(name)) {
			return units[i] == object;
		}
	}
	return false;
}
bool Calculator::functionNameTaken(string name, MathFunction *object) {
	if(name.empty()) return false;
	for(size_t index = 0; index < functions.size(); index++) {
		if(functions[index]->isActive() && functions[index]->hasName(name)) {
			return functions[index] != object;
		}
	}
	return false;
}
bool Calculator::unitIsUsedByOtherUnits(const Unit *u) const {
	const Unit *u2;
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i] != u) {
			u2 = units[i];
			while(u2->subtype() == SUBTYPE_ALIAS_UNIT) {
				u2 = ((AliasUnit*) u2)->firstBaseUnit();
				if(u2 == u) {
					return true;
				}
			}
		}
	}
	return false;
}

bool compare_name(const string &name, const string &str, const size_t &name_length, const size_t &str_index) {
	if(name_length == 0) return false;
	if(name[0] != str[str_index]) return false;
	if(name_length == 1) return true;
	for(size_t i = 1; i < name_length; i++) {
		if(name[i] != str[str_index + i]) return false;
	}
	return true;
}
bool compare_name_no_case(const string &name, const string &str, const size_t &name_length, const size_t &str_index) {
	if(name_length == 0) return false;
	if(name[0] < 0 && name_length > 1) {
		if(str[str_index] >= 0) return false;
		size_t i2 = 1;
		while(i2 < name_length && name[i2] < 0) {
			if(str[str_index + i2] >= 0) return false;
			i2++;
		}
		gchar *gstr1 = g_utf8_strdown(name.c_str(), i2);
		gchar *gstr2 = g_utf8_strdown(str.c_str() + (sizeof(char) * str_index), i2);
		if(strcmp(gstr1, gstr2) != 0) return false;
		g_free(gstr1);
		g_free(gstr2);
	} else if(name[0] != str[str_index] && !((name[0] >= 'a' && name[0] <= 'z') && name[0] - 32 == str[str_index]) && !((name[0] <= 'Z' && name[0] >= 'A') && name[0] + 32 == str[str_index])) {
		return false;
	}
	if(name_length == 1) return true;
	size_t i = 1;
	while(name[i - 1] < 0 && i <= name_length) {
		i++;
	}
	for(; i < name_length; i++) {
		if(name[i] < 0 && i + 1 < name_length) {
			if(str[str_index + i] >= 0) return false;
			size_t i2 = 1;
			while(i2 + i < name_length && name[i2 + i] < 0) {
				if(str[str_index + i2 + i] >= 0) return false;
				i2++;
			}
			gchar *gstr1 = g_utf8_strdown(name.c_str() + (sizeof(char) * i), i2);
			gchar *gstr2 = g_utf8_strdown(str.c_str() + (sizeof(char) * (str_index + i)), i2);
			if(strcmp(gstr1, gstr2) != 0) return false;
			g_free(gstr1);
			g_free(gstr2);
			i += i2 - 1;
		} else if(name[i] != str[str_index + i] && !((name[i] >= 'a' && name[i] <= 'z') && name[i] - 32 == str[str_index + i]) && !((name[i] <= 'Z' && name[i] >= 'A') && name[i] + 32 == str[str_index + i])) {
			return false;
		}
	}
	return true;
}

void Calculator::parseSigns(string &str) const {
	vector<size_t> q_begin;
	vector<size_t> q_end;
	size_t quote_index = 0;
	while(true) {
		quote_index = str.find_first_of("\"\'", quote_index);
		if(quote_index == string::npos) {
			break;
		}
		q_begin.push_back(quote_index);
		quote_index = str.find(str[quote_index], quote_index + 1);
		if(quote_index == string::npos) {
			q_end.push_back(str.length() - 1);
			break;
		}
		q_end.push_back(quote_index);
		quote_index++;
	}
	int index_shift = 0;
	for(size_t i = 0; i < signs.size(); i++) {
		size_t ui = str.find(signs[i]);
		while(ui != string::npos) {
			bool b = false;
			for(size_t ui2 = 0; ui2 < q_end.size(); ui2++) {
				if(ui <= q_end[ui2] + index_shift && ui >= q_begin[ui2] + index_shift) {
					ui = str.find(signs[i], q_end[ui2] + 1 + index_shift);
					b = true;
					break;
				}
			}
			if(!b) {
				index_shift += real_signs[i].length() - signs[i].length();
				str.replace(ui, signs[i].length(), real_signs[i]);
				ui = str.find(signs[i], ui + real_signs[i].length());
			}
		}
	}
}

MathStructure Calculator::parse(string str, const ParseOptions &po) {
	
	MathStructure mstruct;
	parse(&mstruct, str, po);	
	return mstruct;
	
}

void Calculator::parse(MathStructure *mstruct, string str, const ParseOptions &parseoptions) {

	ParseOptions po = parseoptions;
	MathStructure *unended_function = po.unended_function;
	po.unended_function = NULL;

	mstruct->clear();

	const string *name = NULL;
	string stmp, stmp2;

	parseSigns(str);

	for(size_t str_index = 0; str_index < str.length(); str_index++) {		
		if(str[str_index] == LEFT_VECTOR_WRAP_CH) {
			int i4 = 1;
			size_t i3 = str_index;
			while(true) {
				i3 = str.find_first_of(LEFT_VECTOR_WRAP RIGHT_VECTOR_WRAP, i3 + 1);
				if(i3 == string::npos) {
					for(; i4 > 0; i4--) {
						str += RIGHT_VECTOR_WRAP;
					}
					i3 = str.length() - 1;
				} else if(str[i3] == LEFT_VECTOR_WRAP_CH) {
					i4++;
				} else if(str[i3] == RIGHT_VECTOR_WRAP_CH) {
					i4--;
					if(i4 > 0) {
						unsigned i5 = str.find_first_not_of(SPACE, i3 + 1);
						if(i5 != string::npos && str[i5] == LEFT_VECTOR_WRAP_CH) {
							str.insert(i5, COMMA);
						}
					}
				}
				if(i4 == 0) {
					stmp2 = str.substr(str_index + 1, i3 - str_index - 1);
					stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
					stmp += i2s(parseAddVectorId(stmp2, po));
					stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
					str.replace(str_index, i3 + 1 - str_index, stmp);
					str_index += stmp.length() - 1;
					break;
				}
			}	
		} else if(str[str_index] == '\"' || str[str_index] == '\'') {
			if(str_index == str.length() - 1) {
				str.erase(str_index, 1);
			} else {
				size_t i = str.find(str[str_index], str_index + 1);
				size_t name_length;
				if(i == string::npos) {
					i = str.length();
					name_length = i - str_index;
				} else {
					name_length = i - str_index + 1;
				}
				stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
				MathStructure *mstruct = new MathStructure(str.substr(str_index + 1, i - str_index - 1));
				stmp += i2s(addId(mstruct));
				stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
				str.replace(str_index, name_length, stmp);
				str_index += stmp.length() - 1;
			}
		} else if(po.base >= 2 && po.base <= 10 && str[str_index] == '!' && po.functions_enabled) {
			if(str_index > 0 && (str.length() - str_index == 1 || str[str_index + 1] != EQUALS_CH)) {
				stmp = "";
				size_t i5 = str.find_last_not_of(SPACE, str_index - 1);
				size_t i3;
				if(i5 == string::npos) {
				} else if(str[i5] == RIGHT_PARENTHESIS_CH) {
					if(i5 == 0) {
						stmp2 = str.substr(0, i5 + 1);
					} else {
						i3 = i5 - 1;
						size_t i4 = 1;
						while(true) {
							i3 = str.find_last_of(LEFT_PARENTHESIS RIGHT_PARENTHESIS, i3);
							if(i3 == string::npos) {
								stmp2 = str.substr(0, i5 + 1);
								break;
							}
							if(str[i3] == RIGHT_PARENTHESIS_CH) {
								i4++;
							} else {
								i4--;
								if(i4 == 0) {
									stmp2 = str.substr(i3, i5 + 1 - i3);
									break;
								}
							}
							if(i3 == 0) {
								stmp2 = str.substr(0, i5 + 1);
								break;
							}
							i3--;
						}
					}
				} else if(str[i5] == ID_WRAP_RIGHT_CH && (i3 = str.find_last_of(ID_WRAP_LEFT, i5 - 1)) != string::npos) {
					stmp2 = str.substr(i3, i5 + 1 - i3);
				} else if(is_not_in(RESERVED OPERATORS SPACES VECTOR_WRAPS PARENTHESISS COMMAS, str[i5])) {
					i3 = str.find_last_of(RESERVED OPERATORS SPACES VECTOR_WRAPS PARENTHESISS COMMAS, i5);
					if(i3 == string::npos) {
						stmp2 = str.substr(0, i5 + 1);
					} else {
						stmp2 = str.substr(i3 + 1, i5 - i3);
					}
				}
				if(!stmp2.empty()) {
					stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
					int ifac = 1;
					i3 = str_index + 1;
					size_t i4 = i3;
					while((i3 = str.find_first_not_of(SPACE, i3)) != string::npos && str[i3] == '!') {
						ifac++;
						i3++;
						i4 = i3;						
					}
					if(ifac == 2) stmp += i2s(parseAddId(f_factorial2, stmp2, po));
					else if(ifac == 1) stmp += i2s(parseAddId(f_factorial, stmp2, po));
					else stmp += i2s(parseAddIdAppend(f_multifactorial, MathStructure(ifac, 1), stmp2, po));
					stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
					str.replace(i5 - stmp2.length() + 1, stmp2.length() + i4 - i5 - 1, stmp);
					str_index = stmp.length() + i5 - stmp2.length();
				}
			}
		} else if(str[str_index] == SPACE_CH) {
			size_t i = str.find(SPACE, str_index + 1);
			if(i != string::npos) {
				i -= str_index + 1;
				if(i == per_str_len && compare_name_no_case(per_str, str, per_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, DIVISION);
					str_index++;
				} else if(i == times_str_len && compare_name_no_case(times_str, str, times_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, MULTIPLICATION);
					str_index++;
				} else if(i == plus_str_len && compare_name_no_case(plus_str, str, plus_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, PLUS);
					str_index++;
				} else if(i == minus_str_len && compare_name_no_case(minus_str, str, minus_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, MINUS);
					str_index++;
				} else if(i == and_str_len && compare_name_no_case(and_str, str, and_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, LOGICAL_AND);
					str_index++;
				} else if(i == AND_str_len && compare_name_no_case(AND_str, str, AND_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, LOGICAL_AND);
					str_index++;
				} else if(i == or_str_len && compare_name_no_case(or_str, str, or_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, LOGICAL_OR);
					str_index++;
				} else if(i == OR_str_len && compare_name_no_case(OR_str, str, OR_str_len, str_index + 1)) {
					str.replace(str_index + 1, i, LOGICAL_OR);
					str_index++;
//				} else if(compare_name_no_case(XOR_str, str, XOR_str_len, str_index + 1)) {
				}
			}
		} else if(str_index > 0 && po.base >= 2 && po.base <= 10 && is_in(EXPS, str[str_index]) && str_index + 1 < str.length() && (is_in(NUMBERS, str[str_index + 1]) || (is_in(PLUS MINUS, str[str_index + 1]) && str_index + 2 < str.length() && is_in(NUMBERS, str[str_index + 2]))) && is_in(NUMBER_ELEMENTS, str[str_index - 1])) {
			//don't do anything when e is used instead of E for EXP
		} else if(po.base == BASE_DECIMAL && str[str_index] == '0' && (str_index == 0 || is_in(OPERATORS SPACE, str[str_index - 1]))) {
			if(str_index + 2 < str.length() && (str[str_index + 1] == 'x' || str[str_index + 1] == 'X') && is_in(NUMBERS, str[str_index + 2])) {
				//hexadecimal number 0x...
				size_t i = str.find_first_not_of(SPACE NUMBER_ELEMENTS "abcdefABCDEF", str_index + 2);
				size_t name_length;
				if(i == string::npos) i = str.length();
				name_length = i - str_index;
				ParseOptions po_hex = po;
				po_hex.base = BASE_HEXADECIMAL;
				stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
				MathStructure *mstruct = new MathStructure(Number(str.substr(str_index, i - str_index), po_hex));
				stmp += i2s(addId(mstruct));
				stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
				str.replace(str_index, name_length, stmp);
				str_index += stmp.length() - 1;
			} else if(str_index + 2 < str.length() && (str[str_index + 1] == 'b' || str[str_index + 1] == 'B') && is_in("01", str[str_index + 2])) {
				//binary number 0b...
				size_t i = str.find_first_not_of(SPACE NUMBER_ELEMENTS, str_index + 2);
				size_t name_length;
				if(i == string::npos) i = str.length();
				name_length = i - str_index;
				ParseOptions po_bin = po;
				po_bin.base = BASE_BINARY;
				stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
				MathStructure *mstruct = new MathStructure(Number(str.substr(str_index, i - str_index), po_bin));
				stmp += i2s(addId(mstruct));
				stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
				str.replace(str_index, name_length, stmp);
				str_index += stmp.length() - 1;
			} else if(str_index + 2 < str.length() && (str[str_index + 1] == 'o' || str[str_index + 1] == 'O') && is_in(NUMBERS, str[str_index + 2])) {
				//octal number 0o...
				size_t i = str.find_first_not_of(SPACE NUMBER_ELEMENTS, str_index + 2);
				size_t name_length;
				if(i == string::npos) i = str.length();
				name_length = i - str_index;
				ParseOptions po_oct = po;
				po_oct.base = BASE_OCTAL;
				stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
				MathStructure *mstruct = new MathStructure(Number(str.substr(str_index, i - str_index), po_oct));
				stmp += i2s(addId(mstruct));
				stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
				str.replace(str_index, name_length, stmp);
				str_index += stmp.length() - 1;
			/*} else if(str_index + 1 < str.length() && is_in("123456789", str[str_index + 1])) {
				//octal number 0...
				size_t i = str.find_first_not_of(SPACE NUMBER_ELEMENTS, str_index + 1);
				size_t name_length;
				if(i == string::npos) i = str.length();
				name_length = i - str_index;
				ParseOptions po_oct = po;
				po_oct.base = BASE_OCTAL;
				stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
				MathStructure *mstruct = new MathStructure(Number(str.substr(str_index, i - str_index), po_oct));
				stmp += i2s(addId(mstruct));
				stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
				str.replace(str_index, name_length, stmp);
				str_index += stmp.length() - 1;*/
			}
		} else if(po.base >= 2 && po.base <= 10 && is_not_in(NUMBERS NOT_IN_NAMES, str[str_index])) {
			bool p_mode = false;
			void *best_p_object = NULL;
			Prefix *best_p = NULL;
			size_t best_pl = 0;
			size_t best_pnl = 0;
			bool moved_forward = false;
			const string *found_function_name = NULL;
			bool case_sensitive = false;
			size_t found_function_name_length = 0;
			void *found_function = NULL, *object = NULL;
			int vt2 = -1;
			size_t ufv_index;
			size_t name_length;
			size_t vt3 = 0;
			char ufvt = 0;
			size_t last_name_char = str.find_first_of(NOT_IN_NAMES, str_index + 1);
			if(last_name_char == string::npos) {
				last_name_char = str.length() - 1;
			} else {
				last_name_char--;
			}
			size_t last_unit_char = str.find_last_not_of(NUMBERS, last_name_char);
			size_t name_chars_left = last_name_char - str_index + 1;
			size_t unit_chars_left = last_unit_char - str_index + 1;
			if(name_chars_left <= UFV_LENGTHS) {
				ufv_index = name_chars_left - 1;
				vt2 = 0;
			} else {
				ufv_index = 0;
			}
			Prefix *p = NULL;
			while(vt2 < 4) {
				name = NULL;
				p = NULL;
				switch(vt2) {
					case -1: {
						if(ufv_index < ufvl.size()) {
							switch(ufvl_t[ufv_index]) {
								case 'v': {
									if(po.variables_enabled && !p_mode) {
										name = &((ExpressionItem*) ufvl[ufv_index])->getName(ufvl_i[ufv_index]).name;
										case_sensitive = ((ExpressionItem*) ufvl[ufv_index])->getName(ufvl_i[ufv_index]).case_sensitive;
										name_length = name->length();
										if(name_length < found_function_name_length) {
											name = NULL;
										} else if(po.limit_implicit_multiplication) {
											if(name_length != name_chars_left && name_length != unit_chars_left) name = NULL;
										} else if(name_length > name_chars_left) {
											name = NULL;
										}
									}
									break;
								}
								case 'f': {
									if(po.functions_enabled && !found_function_name && !p_mode) {
										name = &((ExpressionItem*) ufvl[ufv_index])->getName(ufvl_i[ufv_index]).name;
										case_sensitive = ((ExpressionItem*) ufvl[ufv_index])->getName(ufvl_i[ufv_index]).case_sensitive;
										name_length = name->length();
										if(po.limit_implicit_multiplication) {
											if(name_length != name_chars_left && name_length != unit_chars_left) name = NULL;
										} else if(name_length > name_chars_left || name_length < found_function_name_length) {
											name = NULL;
										}
									}
									break;
								}
								case 'u': {
									if(po.units_enabled && !p_mode) {
										name = &((ExpressionItem*) ufvl[ufv_index])->getName(ufvl_i[ufv_index]).name;
										case_sensitive = ((ExpressionItem*) ufvl[ufv_index])->getName(ufvl_i[ufv_index]).case_sensitive;
										name_length = name->length();
										if(name_length < found_function_name_length) {
											name = NULL;
										} else if(po.limit_implicit_multiplication || ((ExpressionItem*) ufvl[ufv_index])->getName(ufvl_i[ufv_index]).plural) {
											if(name_length != unit_chars_left) name = NULL;
										} else if(name_length > unit_chars_left) {
											name = NULL;
										}
									}
									break;
								}
								case 'p': {
									if(!p && po.units_enabled) {
										name = &((Prefix*) ufvl[ufv_index])->shortName();
										name_length = name->length();
										if(name_length >= unit_chars_left || name_length < found_function_name_length) {
											name = NULL;
										}
									}
									case_sensitive = true;
									break;
								}
								case 'P': {
									if(!p && po.units_enabled) {
										name = &((Prefix*) ufvl[ufv_index])->longName();
										name_length = name->length();
										if(name_length >= unit_chars_left || name_length < found_function_name_length) {
											name = NULL;
										}
									}
									case_sensitive = false;
									break;
								}
								case 'q': {
									if(!p && po.units_enabled) {
										name = &((Prefix*) ufvl[ufv_index])->unicodeName();
										name_length = name->length();
										if(name_length >= unit_chars_left || name_length < found_function_name_length) {
											name = NULL;
										}
									}
									case_sensitive = true;
									break;
								}
							}
							ufvt = ufvl_t[ufv_index];
							object = ufvl[ufv_index];
							ufv_index++;
							break;
						} else {
							if(found_function_name) {
								vt2 = 4;
								break;
							}
							vt2 = 0;
							vt3 = 0;
							if(po.limit_implicit_multiplication && unit_chars_left <= UFV_LENGTHS) {
								ufv_index = unit_chars_left - 1;
							} else {
								ufv_index = UFV_LENGTHS - 1;
							}
						}
					}
					case 0: {
						if(po.units_enabled && ufv_index < unit_chars_left - 1 && vt3 < ufv[vt2][ufv_index].size()) {
							object = ufv[vt2][ufv_index][vt3];
							switch(ufv_i[vt2][ufv_index][vt3]) {
								case 1: {
									ufvt = 'P';
									name = &((Prefix*) object)->longName();
									name_length = name->length();
									case_sensitive = false;
									break;
								}
								case 2: {
									ufvt = 'p';
									name = &((Prefix*) object)->shortName();
									name_length = name->length();
									case_sensitive = true;
									break;
								}
								case 3: {
									ufvt = 'q';
									name = &((Prefix*) object)->unicodeName();
									name_length = name->length();
									case_sensitive = true;
									break;
								}
							}
							vt3++;
							break;
						}
						vt2 = 1;
						vt3 = 0;
					}
					case 1: {
						if(!found_function_name && po.functions_enabled && !p_mode && (!po.limit_implicit_multiplication || ufv_index + 1 == unit_chars_left || ufv_index + 1 == name_chars_left) && vt3 < ufv[vt2][ufv_index].size()) {
							object = ufv[vt2][ufv_index][vt3];
							ufvt = 'f';
							name = &((MathFunction*) object)->getName(ufv_i[vt2][ufv_index][vt3]).name;
							name_length = name->length();
							case_sensitive = ((MathFunction*) object)->getName(ufv_i[vt2][ufv_index][vt3]).case_sensitive;
							vt3++;
							break;
						}
						vt2 = 2;
						vt3 = 0;
					}
					case 2: {
						if(po.units_enabled && !p_mode && (!po.limit_implicit_multiplication || ufv_index + 1 == unit_chars_left) && ufv_index < unit_chars_left && vt3 < ufv[vt2][ufv_index].size()) {							
							object = ufv[vt2][ufv_index][vt3];
							if(ufv_index + 1 == unit_chars_left || !((Unit*) object)->getName(ufv_i[vt2][ufv_index][vt3]).plural) {
								ufvt = 'u';					
								name = &((Unit*) object)->getName(ufv_i[vt2][ufv_index][vt3]).name;
								name_length = name->length();
								case_sensitive = ((Unit*) object)->getName(ufv_i[vt2][ufv_index][vt3]).case_sensitive;
							}
							vt3++;
							break;
						}
						vt2 = 3;
						vt3 = 0;
					}
					case 3: {
						if(po.variables_enabled && !p_mode && (!po.limit_implicit_multiplication || ufv_index + 1 == unit_chars_left || ufv_index + 1 == name_chars_left) && vt3 < ufv[vt2][ufv_index].size()) {
							object = ufv[vt2][ufv_index][vt3];
							ufvt = 'v';
							name = &((Variable*) object)->getName(ufv_i[vt2][ufv_index][vt3]).name;
							name_length = name->length();
							case_sensitive = ((Variable*) object)->getName(ufv_i[vt2][ufv_index][vt3]).case_sensitive;
							vt3++;
							break;
						}						
						if(ufv_index == 0 || found_function_name) {
							vt2 = 4;
						} else {
							ufv_index--;
							vt3 = 0;
							vt2 = 0;
						}						
					}
				}
				if(name && name_length >= found_function_name_length && ((case_sensitive && compare_name(*name, str, name_length, str_index)) || (!case_sensitive && compare_name_no_case(*name, str, name_length, str_index)))) {
					moved_forward = false;
					switch(ufvt) {
						case 'v': {
							stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
							stmp += i2s(addId(new MathStructure((Variable*) object)));
							stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
							str.replace(str_index, name_length, stmp);
							str_index += stmp.length();
							moved_forward = true;
							break;
						}
						case 'f': {
							size_t not_space_index;
							if((not_space_index = str.find_first_not_of(SPACES, str_index + name_length)) == string::npos || str[not_space_index] != LEFT_PARENTHESIS_CH) {
								found_function = object;
								found_function_name = name;
								found_function_name_length = name_length;
								break;
							}
							set_function:
							MathFunction *f = (MathFunction*) object;
							int i4 = -1;
							size_t i6;
							if(f->args() == 0) {
								size_t i7 = str.find_first_not_of(SPACES, str_index + name_length);
								if(i7 != string::npos && str[i7] == LEFT_PARENTHESIS_CH) {
									i7 = str.find_first_not_of(SPACES, i7 + 1);
									if(i7 != string::npos && str[i7] == RIGHT_PARENTHESIS_CH) {
										i4 = i7 - str_index + 1;
									}
								}
								stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
								stmp += i2s(parseAddId(f, empty_string, po));
								stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
								if(i4 < 0) i4 = name_length;
							} else if(po.rpn && f->args() == 1 && str_index > 0 && str[str_index - 1] == SPACE_CH && (str_index + name_length >= str.length() || str[str_index + name_length] != LEFT_PARENTHESIS_CH) && (i6 = str.find_last_not_of(SPACE, str_index - 1)) != string::npos) {
								size_t i7 = str.rfind(SPACE, i6);
								if(i7 == string::npos) {
									stmp2 = str.substr(0, i6 + 1);	
								} else {
									stmp2 = str.substr(i7 + 1, i6 - i7);
								}
								stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
								if(f == f_vector) stmp += i2s(parseAddVectorId(stmp2, po));
								else stmp += i2s(parseAddId(f, stmp2, po));
								stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
								if(i7 == string::npos) {
									str.replace(0, str_index + name_length, stmp);
								} else {
									str.replace(i7 + 1, str_index + name_length - i7 - 1, stmp);
								}
								str_index += name_length;
								moved_forward = true;
							} else {
								bool b = false, b_unended_function = false;
								size_t i5 = 1;
								i6 = 0;
								while(!b) {
									if(i6 + str_index + name_length >= str.length()) {
										b = true;
										i5 = 2;
										i6++;
										b_unended_function = true;
										break;
									} else {
										char c = str[str_index + name_length + i6];
										if(c == LEFT_PARENTHESIS_CH) {
											if(i5 < 2) b = true;
											else i5++;
										} else if(c == RIGHT_PARENTHESIS_CH) {
											if(i5 <= 2) b = true;
											else i5--;
										} else if(c == ' ') {
											if(i5 == 2) b = true;
										} else if(i5 < 2) {
											i5 = 2;
										}
									}
									i6++;
								}
								if(b && i5 >= 2) {
									stmp2 = str.substr(str_index + name_length, i6 - 1);
									stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
									if(b_unended_function && unended_function) {
										po.unended_function = unended_function;
									}
									if(f == f_vector) {
										stmp += i2s(parseAddVectorId(stmp2, po));
									} else {
										stmp += i2s(parseAddId(f, stmp2, po));										
									}
									po.unended_function = NULL;
									stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
									i4 = i6 + 1 + name_length - 2;
									b = false;
								}
								size_t i9 = i6;
								if(b) {
									b = false;
									i6 = i6 + 1 + str_index + name_length;
									size_t i7 = i6 - 1;
									size_t i8 = i7;
									while(true) {
										i5 = str.find(RIGHT_PARENTHESIS_CH, i7);
										if(i5 == string::npos) {
											b_unended_function = true;
											//str.append(1, RIGHT_PARENTHESIS_CH);
											//i5 = str.length() - 1;
											i5 = str.length();
										}
										if(i5 < (i6 = str.find(LEFT_PARENTHESIS_CH, i8)) || i6 == string::npos) {
											i6 = i5;
											b = true;
											break;
										}
										i7 = i5 + 1;
										i8 = i6 + 1;
									}
									if(!b) {
										b_unended_function = false;
									}
								}
								if(b) {
									stmp2 = str.substr(str_index + name_length + i9, i6 - (str_index + name_length + i9));
									stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
									if(b_unended_function && unended_function) {
										po.unended_function = unended_function;
									}
									if(f == f_vector) {
										stmp += i2s(parseAddVectorId(stmp2, po));
									} else {
										stmp += i2s(parseAddId(f, stmp2, po));
									}
									po.unended_function = NULL;
									stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
									i4 = i6 + 1 - str_index;
								}
							}
							if(i4 > 0) {
								str.replace(str_index, i4, stmp);
								str_index += stmp.length();
								moved_forward = true;
							}
							break;
						}
						case 'u': {
							replace_text_by_unit_place:
							if(str.length() > str_index + name_length && is_in(NUMBERS, str[str_index + name_length]) && !((Unit*) object)->isCurrency()) {
								str.insert(str_index + name_length, 1, POWER_CH);
							}
							stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
							stmp += i2s(addId(new MathStructure((Unit*) object, p)));
							stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
							str.replace(str_index, name_length, stmp);
							str_index += stmp.length();
							moved_forward = true;
							p = NULL;
							break;
						}
						case 'p': {}
						case 'q': {}
						case 'P': {
							if(str_index + name_length == str.length() || is_in(NOT_IN_NAMES, str[str_index + name_length])) {
								break;
							}
							p = (Prefix*) object;
							str_index += name_length;
							unit_chars_left = last_unit_char - str_index + 1;
							size_t name_length_old = name_length;
							int index = 0; 
							if(unit_chars_left > UFV_LENGTHS) {
								for(size_t ufv_index2 = 0; ufv_index2 < ufvl.size(); ufv_index2++) {
									name = NULL;
									switch(ufvl_t[ufv_index2]) {
										case 'u': {
											name = &((Unit*) ufvl[ufv_index2])->getName(ufvl_i[ufv_index2]).name;
											case_sensitive = ((Unit*) ufvl[ufv_index2])->getName(ufvl_i[ufv_index2]).case_sensitive;
											name_length = name->length();
											if(po.limit_implicit_multiplication || ((Unit*) ufvl[ufv_index2])->getName(ufvl_i[ufv_index2]).plural) {
												if(name_length != unit_chars_left) name = NULL;
											} else if(name_length > unit_chars_left) {
												name = NULL;
											}
											break;
										}
									}								
									if(name && ((case_sensitive && compare_name(*name, str, name_length, str_index)) || (!case_sensitive && compare_name_no_case(*name, str, name_length, str_index)))) {
										if((!p_mode && name_length_old > 1) || (p_mode && (name_length + name_length_old > best_pl || ((ufvt != 'P' || !((Unit*) ufvl[ufv_index2])->getName(ufvl_i[ufv_index2]).abbreviation) && name_length + name_length_old == best_pl)))) {
											p_mode = true;
											best_p = p;
											best_p_object = ufvl[ufv_index2];
											best_pl = name_length + name_length_old;
											best_pnl = name_length_old;
											index = -1;
											break;
										}
										if(!p_mode) {
											str.erase(str_index - name_length_old, name_length_old);
											str_index -= name_length_old;
											object = ufvl[ufv_index2];
											goto replace_text_by_unit_place;
										}
									}
								}
							}
							if(index < 0) {							
							} else if(UFV_LENGTHS >= unit_chars_left) {
								index = unit_chars_left - 1;
							} else if(po.limit_implicit_multiplication) {
								index = -1;
							} else {
								index = UFV_LENGTHS - 1;
							}
							for(; index >= 0; index--) {
								for(size_t ufv_index2 = 0; ufv_index2 < ufv[2][index].size(); ufv_index2++) {
									name = &((Unit*) ufv[2][index][ufv_index2])->getName(ufv_i[2][index][ufv_index2]).name;
									case_sensitive = ((Unit*) ufv[2][index][ufv_index2])->getName(ufv_i[2][index][ufv_index2]).case_sensitive;
									name_length = name->length();
									if(index + 1 == (int) unit_chars_left || !((Unit*) ufv[2][index][ufv_index2])->getName(ufv_i[2][index][ufv_index2]).plural) {
										if(name_length <= unit_chars_left && ((case_sensitive && compare_name(*name, str, name_length, str_index)) || (!case_sensitive && compare_name_no_case(*name, str, name_length, str_index)))) {
											if((!p_mode && name_length_old > 1) || (p_mode && (name_length + name_length_old > best_pl || ((ufvt != 'P' || !((Unit*) ufv[2][index][ufv_index2])->getName(ufv_i[2][index][ufv_index2]).abbreviation) && name_length + name_length_old == best_pl)))) {
												p_mode = true;
												best_p = p;
												best_p_object = ufv[2][index][ufv_index2];
												best_pl = name_length + name_length_old;
												best_pnl = name_length_old;
												index = -1;
											}
											if(!p_mode) {
												str.erase(str_index - name_length_old, name_length_old);
												str_index -= name_length_old;
												object = ufv[2][index][ufv_index2];
												goto replace_text_by_unit_place;
											}
										}
									}
								}
								if(po.limit_implicit_multiplication || (p_mode && index + 1 + name_length_old < best_pl)) {
									break;
								}
							}
							str_index -= name_length_old;
							unit_chars_left = last_unit_char - str_index + 1;
							break;
						}
					}
					if(moved_forward) {
						str_index--;
						break;
					}
				}
			}
			if(!moved_forward && p_mode) {
				object = best_p_object;
				p = best_p;
				str.erase(str_index, best_pnl);
				name_length = best_pl - best_pnl;
				goto replace_text_by_unit_place;
			} else if(!moved_forward && found_function) {
				object = found_function;
				name = found_function_name;
				name_length = found_function_name_length;
				goto set_function;
			}
			if(!moved_forward) {
				if(po.limit_implicit_multiplication) {
					if(po.unknowns_enabled && (unit_chars_left > 1 || (str[str_index] != EXP_CH && str[str_index] != EXP2_CH))) {
						stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
						stmp += i2s(addId(new MathStructure(str.substr(str_index, unit_chars_left))));
						stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
						str.replace(str_index, unit_chars_left, stmp);
						str_index += stmp.length() - 1;	
					} else {
						str_index += unit_chars_left - 1;
					}
				} else if(po.unknowns_enabled && str[str_index] != EXP_CH && str[str_index] != EXP2_CH) {
					size_t i = 1;
					if(str[str_index + 1] < 0) {
						i++;
						while(i <= unit_chars_left && (unsigned char) str[str_index + i] >= 0x80 && (unsigned char) str[str_index + i] <= 0xBF) {
							i++;
						}
					}
					stmp = LEFT_PARENTHESIS ID_WRAP_LEFT;
					stmp += i2s(addId(new MathStructure(str.substr(str_index, i))));
					stmp += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
					str.replace(str_index, i, stmp);
					str_index += stmp.length() - 1;	
				}	
			}
		}
	}
	if(po.rpn) {
		size_t rpn_i = str.find(SPACE, 0);
		while(rpn_i != string::npos) {
			if(rpn_i == 0 || is_in(OPERATORS, str[rpn_i - 1]) || rpn_i + 1 == str.length() || is_in(SPACE OPERATORS, str[rpn_i + 1])) {
				str.erase(rpn_i, 1);
			} else {
				rpn_i++;
			}
			rpn_i = str.find(SPACE, rpn_i);
		}
	} else {
		remove_blanks(str);
	}

	parseOperators(mstruct, str, po);

}

bool Calculator::parseNumber(MathStructure *mstruct, string str, const ParseOptions &po) {
	mstruct->clear();
	if(str.empty()) return false;
	if(str.find_first_not_of(OPERATORS SPACE) == string::npos) {
		if(disable_errors_ref > 0) {
			stopped_messages_count[disable_errors_ref - 1]++;
			stopped_warnings_count[disable_errors_ref - 1]++;
		} else {
			error(false, _("Misplaced operator(s) \"%s\" ignored"), str.c_str(), NULL);
		}
		return false;
	}
	int minus_count = 0;
	bool has_sign = false, had_non_sign = false;
	size_t i = 0;
	while(i < str.length()) {
		if(!had_non_sign && str[i] == MINUS_CH) {
			has_sign = true;
			minus_count++;
			str.erase(i, 1);
		} else if(!had_non_sign && str[i] == PLUS_CH) {
			has_sign = true;
			str.erase(i, 1);
		} else if(str[i] == SPACE_CH) {
			str.erase(i, 1);
		} else if(str[i] == COMMA_CH && DOT_S == ".") {
			str.erase(i, 1);
		} else if(is_in(OPERATORS, str[i])) {
			if(disable_errors_ref > 0) {
				stopped_messages_count[disable_errors_ref - 1]++;
				stopped_warnings_count[disable_errors_ref - 1]++;
			} else {
				error(false, _("Misplaced '%c' ignored"), str[i], NULL);
			}
			str.erase(i, 1);
		} else {
			had_non_sign = true;
			i++;
		}
	}
	if(str.empty()) {
		if(minus_count % 2 == 1 && !po.preserve_format) {
			mstruct->set(-1, 1);
		} else if(has_sign) {
			mstruct->set(1, 1);
			if(po.preserve_format) {
				while(minus_count > 0) {
					mstruct->transform(STRUCT_NEGATE);
					minus_count--;
				}
			}
		}
		return false;
	}
	if(str[0] == ID_WRAP_LEFT_CH && str.length() > 2 && str[str.length() - 1] == ID_WRAP_RIGHT_CH) {
		int id = s2i(str.substr(1, str.length() - 2));
		MathStructure *m_temp = getId((size_t) id);
		if(!m_temp) {
			mstruct->setUndefined();
			error(true, _("Internal id %s does not exist."), i2s(id).c_str(), NULL);
			return true;
		}
		mstruct->set_nocopy(*m_temp);
		m_temp->unref();
		if(po.preserve_format) {
			while(minus_count > 0) {
				mstruct->transform(STRUCT_NEGATE);
				minus_count--;
			}
		} else if(minus_count % 2 == 1) {
			mstruct->negate();
		}
		return true;
	}
	size_t itmp;
	if(po.base >= 2 && po.base <= 10 && (itmp = str.find_first_not_of(NUMBER_ELEMENTS MINUS, 0)) != string::npos) {
		if(itmp == 0) {
			error(true, _("\"%s\" is not a valid variable/function/unit."), str.c_str(), NULL);
			if(minus_count % 2 == 1 && !po.preserve_format) {
				mstruct->set(-1, 1);
			} else if(has_sign) {
				mstruct->set(1, 1);
				if(po.preserve_format) {
					while(minus_count > 0) {
						mstruct->transform(STRUCT_NEGATE);
						minus_count--;
					}
				}
			}
			return false;
		} else {
			string stmp = str.substr(itmp, str.length() - itmp);
			error(true, _("Trailing characters \"%s\" (not a valid variable/function/unit) in number \"%s\" was ignored."), stmp.c_str(), str.c_str(), NULL);
			str.erase(itmp, str.length() - itmp);
		}
	}
	Number nr(str, po);
	if(!po.preserve_format && minus_count % 2 == 1) {
		nr.negate();
	}
	mstruct->set(nr);
	if(po.preserve_format) {
		while(minus_count > 0) {
			mstruct->transform(STRUCT_NEGATE);
			minus_count--;
		}
	}
	return true;
	
}

bool Calculator::parseAdd(string &str, MathStructure *mstruct, const ParseOptions &po) {
	if(str.length() > 0) {
		size_t i;
		if(po.base >= 2 && po.base <= 10) {
			i = str.find_first_of(SPACE MULTIPLICATION_2 OPERATORS PARENTHESISS EXPS ID_WRAP_LEFT, 1);
		} else {
			i = str.find_first_of(SPACE MULTIPLICATION_2 OPERATORS PARENTHESISS ID_WRAP_LEFT, 1);
		}
		if(i == string::npos && str[0] != LOGICAL_NOT_CH && str[0] != BITWISE_NOT_CH && !(str[0] == ID_WRAP_LEFT_CH && str.find(ID_WRAP_RIGHT) < str.length() - 1)) {
			return parseNumber(mstruct, str, po);
		} else {
			return parseOperators(mstruct, str, po);
		}
	}	
	return false;
}
bool Calculator::parseAdd(string &str, MathStructure *mstruct, const ParseOptions &po, MathOperation s) {
	if(str.length() > 0) {
		size_t i;
		if(po.base >= 2 && po.base <= 10) {
			i = str.find_first_of(SPACE MULTIPLICATION_2 OPERATORS PARENTHESISS EXPS ID_WRAP_LEFT, 1);
		} else {
			i = str.find_first_of(SPACE MULTIPLICATION_2 OPERATORS PARENTHESISS ID_WRAP_LEFT, 1);
		}
		if(i == string::npos && str[0] != LOGICAL_NOT_CH && str[0] != BITWISE_NOT_CH && !(str[0] == ID_WRAP_LEFT_CH && str.find(ID_WRAP_RIGHT) < str.length() - 1)) {
			if(s == OPERATION_EXP10 && po.read_precision == ALWAYS_READ_PRECISION) {
				ParseOptions po2 = po;
				po2.read_precision = READ_PRECISION_WHEN_DECIMALS;
				MathStructure *mstruct2 = new MathStructure();
				if(!parseNumber(mstruct2, str, po2)) {
					mstruct2->unref();
					return false;
				}
				if(s == OPERATION_DIVIDE && po.preserve_format) {
					mstruct->transform_nocopy(STRUCT_DIVISION, mstruct2);
				} else if(s == OPERATION_SUBTRACT && po.preserve_format) {
					mstruct2->transform(STRUCT_NEGATE);
					mstruct->add_nocopy(mstruct2, OPERATION_ADD, true);
				} else {
					mstruct->add_nocopy(mstruct2, s, true);
				}
			} else {
				MathStructure *mstruct2 = new MathStructure();
				if(!parseNumber(mstruct2, str, po)) {
					mstruct2->unref();
					return false;
				}
				if(s == OPERATION_DIVIDE && po.preserve_format) {
					mstruct->transform_nocopy(STRUCT_DIVISION, mstruct2);
				} else if(s == OPERATION_SUBTRACT && po.preserve_format) {
					mstruct2->transform(STRUCT_NEGATE);
					mstruct->add_nocopy(mstruct2, OPERATION_ADD, true);
				} else {
					mstruct->add_nocopy(mstruct2, s, true);
				}
			}
		} else {
			MathStructure *mstruct2 = new MathStructure();
			if(!parseOperators(mstruct2, str, po)) {
				mstruct2->unref();
				return false;
			}
			if(s == OPERATION_DIVIDE && po.preserve_format) {
				mstruct->transform_nocopy(STRUCT_DIVISION, mstruct2);
			} else if(s == OPERATION_SUBTRACT && po.preserve_format) {
				mstruct2->transform(STRUCT_NEGATE);
				mstruct->add_nocopy(mstruct2, OPERATION_ADD, true);
			} else {
				mstruct->add_nocopy(mstruct2, s, true);
			}
		}
	}
	return true;
}

bool Calculator::parseOperators(MathStructure *mstruct, string str, const ParseOptions &po) {
	string save_str = str;
	mstruct->clear();
	size_t i = 0, i2 = 0, i3 = 0;
	string str2, str3;
	while(true) {
		//find first right parenthesis and then the last left parenthesis before
		i2 = str.find(RIGHT_PARENTHESIS_CH);
		if(i2 == string::npos) {
			i = str.rfind(LEFT_PARENTHESIS_CH);	
			if(i == string::npos) {
				//if no parenthesis break
				break;
			} else {
				//right parenthesis missing -- append
				str += RIGHT_PARENTHESIS_CH;
				i2 = str.length() - 1;
			}
		} else {
			if(i2 > 0) {
				i = str.rfind(LEFT_PARENTHESIS_CH, i2 - 1);
			} else {
				i = string::npos;
			}
			if(i == string::npos) {
				//left parenthesis missing -- prepend
				str.insert(str.begin(), 1, LEFT_PARENTHESIS_CH);
				i = 0;
				i2++;
			}
		}
		while(true) {
			//remove unnecessary double parenthesis and the found parenthesis
			if(i > 0 && i2 + 1 < str.length() && str[i - 1] == LEFT_PARENTHESIS_CH && str[i2 + 1] == RIGHT_PARENTHESIS_CH) {
				str.erase(str.begin() + (i - 1));
				i--; i2--;
				str.erase(str.begin() + (i2 + 1));
			} else {
				break;
			}
		}
		if(i > 0 && is_not_in(MULTIPLICATION_2 OPERATORS PARENTHESISS SPACE, str[i - 1]) && (po.base > 10 || po.base < 2 || (str[i - 1] != EXP_CH && str[i - 1] != EXP2_CH))) {
			if(po.rpn) {
				str.insert(i2 + 1, MULTIPLICATION);	
				str.insert(i, SPACE);
				i++;
				i2++;		
			} else {
				str.insert(i, MULTIPLICATION_2);
				i++;
				i2++;
			}
		}
		if(i2 + 1 < str.length() && is_not_in(MULTIPLICATION_2 OPERATORS PARENTHESISS SPACE, str[i2 + 1]) && (po.base > 10 || po.base < 2 || (str[i2 + 1] != EXP_CH && str[i2 + 1] != EXP2_CH))) {
			if(po.rpn) {
				i3 = str.find(SPACE, i2 + 1);
				if(i3 == string::npos) {
					str += MULTIPLICATION;
				} else {
					str.replace(i3, 1, MULTIPLICATION);
				}
				str.insert(i2 + 1, SPACE);
			} else {
				str.insert(i2 + 1, MULTIPLICATION_2);
			}
		}
		if(po.rpn && i > 0 && i2 + 1 == str.length() && is_not_in(PARENTHESISS SPACE, str[i - 1])) {
			str += MULTIPLICATION_CH;
		}
		str2 = str.substr(i + 1, i2 - (i + 1));
		MathStructure *mstruct2 = new MathStructure();
		if(str2.empty()) {
			CALCULATOR->error(false, "Empty expression in parentheses interpreted as zero.", NULL);
		} else {
			parseOperators(mstruct2, str2, po);
		}
		str2 = ID_WRAP_LEFT;
		str2 += i2s(addId(mstruct2));
		str2 += ID_WRAP_RIGHT;
		str.replace(i, i2 - i + 1, str2);
		mstruct->clear();
	}
	if((i = str.find(LOGICAL_AND, 1)) != string::npos && i + 2 != str.length()) {
		bool b = false;
		while(i != string::npos && i + 2 != str.length()) {
			str2 = str.substr(0, i);
			str = str.substr(i + 2, str.length() - (i + 2));
			if(b) {
				parseAdd(str2, mstruct, po, OPERATION_LOGICAL_AND);
			} else {
				parseAdd(str2, mstruct, po);
				b = true;
			}
			i = str.find(LOGICAL_AND, 1);
		}
		if(b) {
			parseAdd(str, mstruct, po, OPERATION_LOGICAL_AND);
		} else {
			parseAdd(str, mstruct, po);
		}
		return true;
	}
	if((i = str.find(LOGICAL_OR, 1)) != string::npos && i + 2 != str.length()) {
		bool b = false;
		while(i != string::npos && i + 2 != str.length()) {
			str2 = str.substr(0, i);
			str = str.substr(i + 2, str.length() - (i + 2));
			if(b) {
				parseAdd(str2, mstruct, po, OPERATION_LOGICAL_OR);
			} else {
				parseAdd(str2, mstruct, po);
				b = true;
			}
			i = str.find(LOGICAL_OR, 1);
		}
		if(b) {
			parseAdd(str, mstruct, po, OPERATION_LOGICAL_OR);
		} else {
			parseAdd(str, mstruct, po);
		}
		return true;
	}	
	if(str[0] == LOGICAL_NOT_CH) {
		str.erase(str.begin());
		parseAdd(str, mstruct, po);
		mstruct->setLogicalNot();
		return true;
	}
	if((i = str.find_first_of(LESS GREATER EQUALS NOT, 0)) != string::npos) {
		while(i != string::npos && (str[i] == LESS_CH && i + 1 < str.length() && str[i + 1] == LESS_CH) || (str[i] == GREATER_CH && i + 1 < str.length() && str[i + 1] == GREATER_CH)) {
			i = str.find_first_of(LESS GREATER NOT EQUALS, i + 2);
		}
	}
	if(i != string::npos) {
		bool b = false;
		bool c = false;
		while(i != string::npos && str[i] == NOT_CH && str.length() > i + 1 && str[i + 1] == NOT_CH) {
			i++;
			if(i + 1 == str.length()) {
				c = true;
			}
		}
		MathOperation s = OPERATION_ADD;
		while(!c) {
			while(i != string::npos && (str[i] == LESS_CH && i + 1 < str.length() && str[i + 1] == LESS_CH) || (str[i] == GREATER_CH && i + 1 < str.length() && str[i + 1] == GREATER_CH)) {
				i = str.find_first_of(LESS GREATER NOT EQUALS, i + 2);
				while(i != string::npos && str[i] == NOT_CH && str.length() > i + 1 && str[i + 1] == NOT_CH) {
					i++;
					if(i + 1 == str.length()) {
						i = string::npos;
					}
				}
			}
			if(i == string::npos) {
				str2 = str.substr(0, str.length());
			} else {
				str2 = str.substr(0, i);
			}
			if(b) {
				switch(i3) {
					case EQUALS_CH: {s = OPERATION_EQUALS; break;}
					case GREATER_CH: {s = OPERATION_GREATER; break;}
					case LESS_CH: {s = OPERATION_LESS; break;}
					case GREATER_CH * EQUALS_CH: {s = OPERATION_EQUALS_GREATER; break;}
					case LESS_CH * EQUALS_CH: {s = OPERATION_EQUALS_LESS; break;}
					case GREATER_CH * LESS_CH: {s = OPERATION_NOT_EQUALS; break;}
				}
				parseAdd(str2, mstruct, po, s);
			}
			if(i == string::npos) {
				return true;
			}
			if(!b) {
				parseAdd(str2, mstruct, po);
				b = true;
			}
			if(str.length() > i + 1 && is_in(LESS GREATER NOT EQUALS, str[i + 1])) {
				if(str[i] == str[i + 1]) {
					i3 = str[i];
				} else {
					i3 = str[i] * str[i + 1];
					if(i3 == NOT_CH * EQUALS_CH) {
						i3 = GREATER_CH * LESS_CH;
					} else if(i3 == NOT_CH * LESS_CH) {
						i3 = GREATER_CH;
					} else if(i3 == NOT_CH * GREATER_CH) {
						i3 = LESS_CH;
					}
				}
				i++;
			} else {
				i3 = str[i];
			}
			str = str.substr(i + 1, str.length() - (i + 1));
			i = str.find_first_of(LESS GREATER NOT EQUALS, 0);
			while(i != string::npos && str[i] == NOT_CH && str.length() > i + 1 && str[i + 1] == NOT_CH) {
				i++;
				if(i + 1 == str.length()) {
					i = string::npos;
				}
			}
		}
	}
	
	if((i = str.find(BITWISE_OR, 1)) != string::npos && i + 1 != str.length()) {
		bool b = false;
		while(i != string::npos && i + 1 != str.length()) {
			str2 = str.substr(0, i);
			str = str.substr(i + 1, str.length() - (i + 1));
			if(b) {
				parseAdd(str2, mstruct, po, OPERATION_BITWISE_OR);
			} else {
				parseAdd(str2, mstruct, po);
				b = true;
			}
			i = str.find(BITWISE_OR, 1);
		}
		if(b) {
			parseAdd(str, mstruct, po, OPERATION_BITWISE_OR);
		} else {
			parseAdd(str, mstruct, po);
		}
		return true;
	}
	if((i = str.find(BITWISE_AND, 1)) != string::npos && i + 1 != str.length()) {
		bool b = false;
		while(i != string::npos && i + 1 != str.length()) {
			str2 = str.substr(0, i);
			str = str.substr(i + 1, str.length() - (i + 1));
			if(b) {
				parseAdd(str2, mstruct, po, OPERATION_BITWISE_AND);
			} else {
				parseAdd(str2, mstruct, po);
				b = true;
			}
			i = str.find(BITWISE_AND, 1);
		}
		if(b) {
			parseAdd(str, mstruct, po, OPERATION_BITWISE_AND);
		} else {
			parseAdd(str, mstruct, po);
		}
		return true;
	}
	i = str.find(SHIFT_LEFT, 1);
	i2 = str.find(SHIFT_RIGHT, 1);
	if(i != string::npos && i + 2 != str.length() && (i2 == string::npos || i < i2)) {
		MathStructure mstruct1, mstruct2;
		str2 = str.substr(0, i);
		str = str.substr(i + 2, str.length() - (i + 2));
		parseAdd(str2, &mstruct1, po);
		parseAdd(str, &mstruct2, po);
		mstruct->set(f_shift, &mstruct1, &mstruct2, NULL);
		return true;
	}
	i = i2;
	if(i != string::npos && i + 2 != str.length()) {
		MathStructure mstruct1, mstruct2;
		str2 = str.substr(0, i);
		str = str.substr(i + 2, str.length() - (i + 2));
		parseAdd(str2, &mstruct1, po);
		parseAdd(str, &mstruct2, po);
		if(po.preserve_format) mstruct2.transform(STRUCT_NEGATE);
		else mstruct2.negate();
		mstruct->set(f_shift, &mstruct1, &mstruct2, NULL);
		return true;
	}
	if(str[0] == BITWISE_NOT_CH) {
		str.erase(str.begin());
		parseAdd(str, mstruct, po);
		mstruct->setBitwiseNot();
		return true;
	}
			
	i = 0;
	i3 = 0;	
	if(po.rpn) {
		ParseOptions po2 = po;
		po2.rpn = false;
		vector<MathStructure*> mstack;
		bool b = false;
		char last_operator = 0;
		while(true) {
			i = str.find_first_of(OPERATORS SPACE, i3 + 1);
			if(i == string::npos) {
				if(!b) {
					parseAdd(str, mstruct, po2);
					return true;
				}
				if(i3 != 0) {
					str2 = str.substr(i3 + 1, str.length() - i3 - 1);
				} else {
					str2 = str.substr(i3, str.length() - i3);
				}
				remove_blank_ends(str2);
				if(!str2.empty()) {
					error(false, _("RPN syntax error. Values left at the end of the RPN expression."), NULL);
				} else if(mstack.size() > 1) {
					if(last_operator == 0) {
						error(false, _("Unused stack values."), NULL);
					} else {
						while(mstack.size() > 1) {
							switch(last_operator) {
								case PLUS_CH: {
									mstack[mstack.size() - 2]->add_nocopy(mstack.back()); 
									mstack.pop_back(); 
									break;
								}
								case MINUS_CH: {
									if(po.preserve_format) {
										mstack.back()->transform(STRUCT_NEGATE);
										mstack[mstack.size() - 2]->add_nocopy(mstack.back()); 
									} else {
										mstack[mstack.size() - 2]->subtract_nocopy(mstack.back()); 
									}
									mstack.pop_back(); 
									break;
								}
								case MULTIPLICATION_CH: {
									mstack[mstack.size() - 2]->multiply_nocopy(mstack.back()); 
									mstack.pop_back(); 
									break;
								}
								case DIVISION_CH: {
									if(po.preserve_format) {
										mstack[mstack.size() - 2]->transform_nocopy(STRUCT_DIVISION, mstack.back());
									} else {
										mstack[mstack.size() - 2]->divide_nocopy(mstack.back());
									}
									mstack.pop_back(); 
									break;
								}
								case POWER_CH: {
									mstack[mstack.size() - 2]->raise_nocopy(mstack.back()); 
									mstack.pop_back(); 
									break;
								}
							}
						}
					}
				}
				mstruct->set_nocopy(*mstack.back());
				while(!mstack.empty()) {
					mstack.back()->unref();
					mstack.pop_back();
				}
				return true;
			}
			b = true;
			if(i3 != 0) {
				str2 = str.substr(i3 + 1, i - i3 - 1);
			} else {
				str2 = str.substr(i3, i - i3);
			}
			remove_blank_ends(str2);
			if(!str2.empty()) {
				mstack.push_back(new MathStructure());
				parseAdd(str2, mstack.back(), po2);
			}
			if(str[i] != SPACE_CH) {
				if(mstack.size() < 1) {
					error(true, _("RPN syntax error. Stack is empty."), NULL);
				} else if(mstack.size() < 2) {
					error(false, _("RPN syntax error. Operator ignored as there where only one stack value."), NULL);
				} else {
					switch(str[i]) {
						case PLUS_CH: {
							mstack[mstack.size() - 2]->add_nocopy(mstack.back()); 
							mstack.pop_back(); 
							break;
						}
						case MINUS_CH: {
							if(po.preserve_format) {
								mstack.back()->transform(STRUCT_NEGATE);
								mstack[mstack.size() - 2]->add_nocopy(mstack.back()); 
							} else {
								mstack[mstack.size() - 2]->subtract_nocopy(mstack.back()); 
							}
							mstack.pop_back(); 
							break;
						}
						case MULTIPLICATION_CH: {
							mstack[mstack.size() - 2]->multiply_nocopy(mstack.back()); 
							mstack.pop_back(); 
							break;
						}
						case DIVISION_CH: {
							if(po.preserve_format) {
								mstack[mstack.size() - 2]->transform_nocopy(STRUCT_DIVISION, mstack.back());
							} else {
								mstack[mstack.size() - 2]->divide_nocopy(mstack.back()); 
							}
							mstack.pop_back(); 
							break;
						}
						case POWER_CH: {
							mstack[mstack.size() - 2]->raise_nocopy(mstack.back()); 
							mstack.pop_back(); 
							break;
						}
					}
					last_operator = str[i];
				}
			}
			i3 = i;
		}
	}

	if((i = str.find_first_of(PLUS MINUS, 1)) != string::npos && i + 1 != str.length()) {
		bool b = false, c = false;
		bool min = false;
		while(i != string::npos && i + 1 != str.length()) {
			if(is_not_in(MULTIPLICATION_2 OPERATORS EXPS, str[i - 1])) {
				str2 = str.substr(0, i);
				if(!c && b) {
					if(min) {
						parseAdd(str2, mstruct, po, OPERATION_SUBTRACT);
					} else {
						parseAdd(str2, mstruct, po, OPERATION_ADD);
					}
				} else {
					if(!b && str2.empty()) {
						c = true;
					} else {
						parseAdd(str2, mstruct, po);
						if(c && min) {
							if(po.preserve_format) mstruct->transform(STRUCT_NEGATE);
							else mstruct->negate();
						}
						c = false;
					}
					b = true;
				}
				min = str[i] == MINUS_CH;
				str = str.substr(i + 1, str.length() - (i + 1));
				i = str.find_first_of(PLUS MINUS, 1);
			} else {
				i = str.find_first_of(PLUS MINUS, i + 1);
			}
		}
		if(b) {
			if(c) {
				b = parseAdd(str, mstruct, po);
				if(min) {
					if(po.preserve_format) mstruct->transform(STRUCT_NEGATE);
					else mstruct->negate();
				}
				return b;
			} else {
				if(min) {
					parseAdd(str, mstruct, po, OPERATION_SUBTRACT);
				} else {
					parseAdd(str, mstruct, po, OPERATION_ADD);
				}
			}
			return true;
		}
	}
	if((i = str.find_first_of(MULTIPLICATION DIVISION, 0)) != string::npos && i + 1 != str.length()) {
		bool b = false;
		bool div = false;
		while(i != string::npos && i + 1 != str.length()) {
			if(i < 1) {
				if(i < 1 && str.find_first_not_of(MULTIPLICATION_2 OPERATORS EXPS) == string::npos) {
					if(disable_errors_ref > 0) {
						stopped_messages_count[disable_errors_ref - 1]++;
						stopped_warnings_count[disable_errors_ref - 1]++;
					} else {
						error(false, _("Misplaced operator(s) \"%s\" ignored"), str.c_str(), NULL);
					}
					return b;
				}
				i = 1;
				while(i < str.length() && is_in(MULTIPLICATION DIVISION, str[i])) {
					i++;
				}
				if(disable_errors_ref > 0) {
					stopped_messages_count[disable_errors_ref - 1]++;
					stopped_warnings_count[disable_errors_ref - 1]++;
				} else {
					error(false, _("Misplaced operator(s) \"%s\" ignored"), str.substr(0, i).c_str(), NULL);
				}
				str = str.substr(i, str.length() - i);
				i = str.find_first_of(MULTIPLICATION DIVISION, 0);
			} else {
				str2 = str.substr(0, i);
				if(b) {
					if(div) {
						parseAdd(str2, mstruct, po, OPERATION_DIVIDE);
					} else {
						parseAdd(str2, mstruct, po, OPERATION_MULTIPLY);
					}
				} else {
					parseAdd(str2, mstruct, po);
					b = true;
				}
				if(is_in(MULTIPLICATION DIVISION, str[i + 1])) {
					i2 = 1;
					while(i2 + i + 1 != str.length() && is_in(MULTIPLICATION DIVISION, str[i2 + i + 1])) {
						i2++;
					}
					if(disable_errors_ref > 0) {
						stopped_messages_count[disable_errors_ref - 1]++;
						stopped_warnings_count[disable_errors_ref - 1]++;
					} else {
						error(false, _("Misplaced operator(s) \"%s\" ignored"), str.substr(i, i2).c_str(), NULL);
					}
					i += i2;
				}
				div = str[i] == DIVISION_CH;
				str = str.substr(i + 1, str.length() - (i + 1));
				i = str.find_first_of(MULTIPLICATION DIVISION, 0);
			}
		}
		if(b) {
			if(div) {
				parseAdd(str, mstruct, po, OPERATION_DIVIDE);
			} else {
				parseAdd(str, mstruct, po, OPERATION_MULTIPLY);
			}
			return true;
		}
	}


	if(str.empty()) return false;
	if(str.find_first_not_of(OPERATORS SPACE) == string::npos) {
		if(disable_errors_ref > 0) {
			stopped_messages_count[disable_errors_ref - 1]++;
			stopped_warnings_count[disable_errors_ref - 1]++;
		} else {
			error(false, _("Misplaced operator(s) \"%s\" ignored"), str.c_str(), NULL);
		}
		return false;
	}
	
	i = 0;
	bool ret = true;
	bool has_sign = false;
	int minus_count = 0;
	while(i < str.length()) {
		if(str[i] == MINUS_CH) {
			has_sign = true;
			minus_count++;
			str.erase(i, 1);
		} else if(str[i] == PLUS_CH) {
			has_sign = true;
			str.erase(i, 1);
		} else if(str[i] == SPACE_CH) {
			str.erase(i, 1);
		} else if(is_in(OPERATORS, str[i])) {
			if(disable_errors_ref > 0) {
				stopped_messages_count[disable_errors_ref - 1]++;
				stopped_warnings_count[disable_errors_ref - 1]++;
			} else {
				error(false, _("Misplaced '%c' ignored"), str[i], NULL);
			}
			str.erase(i, 1);
		} else {
			break;
		}
	}
	if(str.empty()) {
		if(minus_count % 2 == 1 && !po.preserve_format) {
			mstruct->set(-1, 1);
		} else if(has_sign) {
			mstruct->set(1, 1);
			if(po.preserve_format) {
				while(minus_count > 0) {
					mstruct->transform(STRUCT_NEGATE);
					minus_count--;
				}
			}
		}
		return false;
	}
	
	if((i = str.find(MULTIPLICATION_2_CH, 1)) != string::npos && i + 1 != str.length()) {
		bool b = false;
		while(i != string::npos && i + 1 != str.length()) {
			str2 = str.substr(0, i);
			str = str.substr(i + 1, str.length() - (i + 1));
			if(b) {
				parseAdd(str2, mstruct, po, OPERATION_MULTIPLY);
			} else {
				parseAdd(str2, mstruct, po);
				b = true;
			}
			i = str.find(MULTIPLICATION_2_CH, 1);
		}
		if(b) {
			parseAdd(str, mstruct, po, OPERATION_MULTIPLY);
			if(po.preserve_format) {
				while(minus_count > 0) {
					mstruct->transform(STRUCT_NEGATE);
					minus_count--;
				}
			} else if(minus_count % 2 == 1) {
				mstruct->negate();
			}
			return true;
		}
	}
	
	if((i = str.find(POWER_CH, 1)) != string::npos && i + 1 != str.length()) {
		str2 = str.substr(0, i);
		str = str.substr(i + 1, str.length() - (i + 1));		
		parseAdd(str2, mstruct, po);
		parseAdd(str, mstruct, po, OPERATION_RAISE);
	} else if(po.base >= 2 && po.base <= 10 && (i = str.find_first_of(EXPS, 1)) != string::npos && i + 1 != str.length()) {
		str2 = str.substr(0, i);
		str = str.substr(i + 1, str.length() - (i + 1));
		parseAdd(str2, mstruct, po);
		parseAdd(str, mstruct, po, OPERATION_EXP10);
	} else if((i = str.find(ID_WRAP_LEFT_CH, 1)) != string::npos && i + 1 != str.length() && str.find(ID_WRAP_RIGHT_CH, i + 1) && str.find_first_not_of(PLUS MINUS, 0) != i) {
		str2 = str.substr(0, i);
		str = str.substr(i, str.length() - i);
		parseAdd(str2, mstruct, po);
		parseAdd(str, mstruct, po, OPERATION_MULTIPLY);
	} else if(str.length() > 0 && str[0] == ID_WRAP_LEFT_CH && (i = str.find(ID_WRAP_RIGHT_CH, 1)) != string::npos && i + 1 != str.length()) {
		str2 = str.substr(0, i + 1);
		str = str.substr(i + 1, str.length() - (i + 1));
		parseAdd(str2, mstruct, po);
		parseAdd(str, mstruct, po, OPERATION_MULTIPLY);
	} else {
		ret = parseNumber(mstruct, str, po);
	}
	if(po.preserve_format) {
		while(minus_count > 0) {
			mstruct->transform(STRUCT_NEGATE);
			minus_count--;
		}
	} else if(minus_count % 2 == 1) {
		mstruct->negate();
	}
	return ret;
}

string Calculator::getName(string name, ExpressionItem *object, bool force, bool always_append) {
	ExpressionItem *item = NULL;
	if(!object) {
	} else if(object->type() == TYPE_FUNCTION) {
		item = getActiveFunction(name);
	} else {
		item = getActiveVariable(name);
		if(!item) {
			item = getActiveUnit(name);
		}
		if(!item) {
			item = getCompositeUnit(name);
		}
	}
	if(item && force && !name.empty() && item != object && object) {
		if(!item->isLocal()) {
			bool b = item->hasChanged();
			if(object->isActive()) {
				item->setActive(false);
			}
			if(!object->isLocal()) {
				item->setChanged(b);
			}
		} else {
			if(object->isActive()) {
				item->destroy();
			}
		}
		return name;
	}
	int i2 = 1;
	bool changed = false;
	if(name.empty()) {
		name = "var";
		always_append = true;
		item = NULL;
		changed = true;
	}
	string stmp = name;
	if(always_append) {
		stmp += NAME_NUMBER_PRE_STR;
		stmp += "1";
	}
	if(changed || (item && item != object)) {
		if(item) {
			i2++;
			stmp = name;
			stmp += NAME_NUMBER_PRE_STR;
			stmp += i2s(i2);
		}
		while(true) {
			if(!object) {
				item = getActiveFunction(stmp);
				if(!item) {
					item = getActiveVariable(stmp);
				}
				if(!item) {
					item = getActiveUnit(stmp);
				}
				if(!item) {
					item = getCompositeUnit(stmp);
				}
			} else if(object->type() == TYPE_FUNCTION) {
				item = getActiveFunction(stmp);
			} else {
				item = getActiveVariable(stmp);
				if(!item) {
					item = getActiveUnit(stmp);
				}
				if(!item) {
					item = getCompositeUnit(stmp);
				}
			}
			if(item && item != object) {
				i2++;
				stmp = name;
				stmp += NAME_NUMBER_PRE_STR;
				stmp += i2s(i2);
			} else {
				break;
			}
		}
	}
	if(i2 > 1 && !always_append) {
		error(false, _("Name \"%s\" is in use. Replacing with \"%s\"."), name.c_str(), stmp.c_str(), NULL);
	}
	return stmp;
}

bool Calculator::loadGlobalDefinitions() {
	string dir = PACKAGE_DATA_DIR;
	string filename;
	dir += "/qalculate/";
	filename = dir;
	filename += "prefixes.xml";
	bool b = true;
	if(!loadDefinitions(filename.c_str(), false)) {
		b = false;
	}
	filename = dir;
	filename += "currencies.xml";
	if(!loadDefinitions(filename.c_str(), false)) {
		b = false;
	}	
	filename = dir;
	filename += "units.xml";
	if(!loadDefinitions(filename.c_str(), false)) {
		b = false;
	}
	filename = dir;
	filename += "functions.xml";	
	if(!loadDefinitions(filename.c_str(), false)) {
		b = false;
	}
	filename = dir;
	filename += "datasets.xml";	
	if(!loadDefinitions(filename.c_str(), false)) {
		b = false;
	}
	filename = dir;
	filename += "variables.xml";
	if(!loadDefinitions(filename.c_str(), false)) {
		b = false;
	}
	return b;
}
bool Calculator::loadGlobalDefinitions(string filename) {
	string dir = PACKAGE_DATA_DIR;
	dir += "/qalculate/";
	dir += filename;
	return loadDefinitions(dir.c_str(), false);
}
bool Calculator::loadGlobalPrefixes() {
	return loadGlobalDefinitions("prefixes.xml");
}
bool Calculator::loadGlobalCurrencies() {
	return loadGlobalDefinitions("currencies.xml");
}
bool Calculator::loadGlobalUnits() {
	bool b = loadGlobalDefinitions("currencies.xml");
	return loadGlobalDefinitions("units.xml") && b;
}
bool Calculator::loadGlobalVariables() {
	return loadGlobalDefinitions("variables.xml");
}
bool Calculator::loadGlobalFunctions() {
	return loadGlobalDefinitions("functions.xml");
}
bool Calculator::loadGlobalDataSets() {
	return loadGlobalDefinitions("datasets.xml");
}
bool Calculator::loadLocalDefinitions() {
	string filename;
	string homedir = getLocalDir();
	homedir += "definitions/";	
	list<string> eps;
	struct dirent *ep;
	DIR *dp;
	dp = opendir(homedir.c_str());
	if(dp) {
		while((ep = readdir(dp))) {
#ifdef _DIRENT_HAVE_D_TYPE
			if(ep->d_type != DT_DIR) {
#endif
				if(strcmp(ep->d_name, "..") != 0 && strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "datasets") != 0) {
					eps.push_back(ep->d_name);
				}
#ifdef _DIRENT_HAVE_D_TYPE			
			}
#endif
		}
		closedir(dp);
	}
	eps.sort();
	for(list<string>::iterator it = eps.begin(); it != eps.end(); ++it) {
		filename = homedir;
		filename += *it;
		loadDefinitions(filename.c_str(), true);
	}
	return true;
}

#define ITEM_SAVE_BUILTIN_NAMES\
	if(!is_user_defs) {item->setRegistered(false);} \
	for(size_t i = 1; i <= item->countNames(); i++) { \
		if(item->getName(i).reference) { \
			for(size_t i2 = 0; i2 < 10; i2++) { \
				if(ref_names[i2].name.empty()) { \
					ref_names[i2] = item->getName(i); \
					break; \
				} \
			} \
		} \
	} \
	item->clearNames();

#define ITEM_SET_BEST_NAMES(validation) \
	size_t names_i = 0, i2 = 0; \
	string *str_names; \
	if(best_names == "-") {best_names = ""; nextbest_names = "";} \
	if(!best_names.empty()) {str_names = &best_names;} \
	else if(!nextbest_names.empty()) {str_names = &nextbest_names;} \
	else {str_names = &default_names;} \
	if(!str_names->empty() && (*str_names)[0] == '!') { \
		names_i = str_names->find('!', 1) + 1; \
	} \
	while(true) { \
		size_t i3 = names_i; \
		names_i = str_names->find(",", i3); \
		if(i2 == 0) { \
			i2 = str_names->find(":", i3); \
		} \
		bool case_set = false; \
		ename.unicode = false; \
		ename.abbreviation = false; \
		ename.case_sensitive = false; \
		ename.suffix = false; \
		ename.avoid_input = false; \
		ename.reference = false; \
		ename.plural = false; \
		if(i2 < names_i) { \
			bool b = true; \
			for(; i3 < i2; i3++) { \
				switch((*str_names)[i3]) { \
					case '-': {b = false; break;} \
					case 'a': {ename.abbreviation = b; b = true; break;} \
					case 'c': {ename.case_sensitive = b; b = true; case_set = true; break;} \
					case 'i': {ename.avoid_input = b; b = true; break;} \
					case 'p': {ename.plural = b; b = true; break;} \
					case 'r': {ename.reference = b; b = true; break;} \
					case 's': {ename.suffix = b; b = true; break;} \
					case 'u': {ename.unicode = b; b = true; break;} \
				} \
			} \
			i3++; \
			i2 = 0; \
		} \
		if(names_i == string::npos) {ename.name = str_names->substr(i3, str_names->length() - i3);} \
		else {ename.name = str_names->substr(i3, names_i - i3);} \
		remove_blank_ends(ename.name); \
		if(!ename.name.empty() && validation(ename.name, version_numbers, is_user_defs)) { \
			if(!case_set) { \
				ename.case_sensitive = ename.abbreviation || text_length_is_one(ename.name); \
			} \
			item->addName(ename); \
		} \
		if(names_i == string::npos) {break;} \
		names_i++; \
	}

#define ITEM_SET_BUILTIN_NAMES \
	for(size_t i = 0; i < 10; i++) { \
		if(ref_names[i].name.empty()) { \
			break; \
		} else { \
			size_t i4 = item->hasName(ref_names[i].name, ref_names[i].case_sensitive); \
			if(i4 > 0) { \
				const ExpressionName *enameptr = &item->getName(i4); \
				ref_names[i].case_sensitive = enameptr->case_sensitive; \
				ref_names[i].abbreviation = enameptr->abbreviation; \
				ref_names[i].avoid_input = enameptr->avoid_input; \
				ref_names[i].plural = enameptr->plural; \
				item->setName(ref_names[i], i4); \
			} else { \
				item->addName(ref_names[i]); \
			} \
			ref_names[i].name = ""; \
		} \
	} \
	if(!is_user_defs) { \
		item->setRegistered(true); \
		nameChanged(item); \
	}

#define ITEM_SET_REFERENCE_NAMES(validation) \
	if(str_names != &default_names && !default_names.empty()) { \
		if(default_names[0] == '!') { \
			names_i = default_names.find('!', 1) + 1; \
		} else { \
			names_i = 0; \
		} \
		i2 = 0; \
		while(true) { \
			size_t i3 = names_i; \
			names_i = default_names.find(",", i3); \
			if(i2 == 0) { \
				i2 = default_names.find(":", i3); \
			} \
			bool case_set = false; \
			ename.unicode = false; \
			ename.abbreviation = false; \
			ename.case_sensitive = false; \
			ename.suffix = false; \
			ename.avoid_input = false; \
			ename.reference = false; \
			ename.plural = false; \
			if(i2 < names_i) { \
				bool b = true; \
				for(; i3 < i2; i3++) { \
					switch(default_names[i3]) { \
						case '-': {b = false; break;} \
						case 'a': {ename.abbreviation = b; b = true; break;} \
						case 'c': {ename.case_sensitive = b; b = true; case_set = true; break;} \
						case 'i': {ename.avoid_input = b; b = true; break;} \
						case 'p': {ename.plural = b; b = true; break;} \
						case 'r': {ename.reference = b; b = true; break;} \
						case 's': {ename.suffix = b; b = true; break;} \
						case 'u': {ename.unicode = b; b = true; break;} \
					} \
				} \
				i3++; \
				i2 = 0; \
			} \
			if(ename.reference) { \
				if(names_i == string::npos) {ename.name = default_names.substr(i3, default_names.length() - i3);} \
				else {ename.name = default_names.substr(i3, names_i - i3);} \
				remove_blank_ends(ename.name); \
				size_t i4 = item->hasName(ename.name, ename.case_sensitive); \
				if(i4 > 0) { \
					const ExpressionName *enameptr = &item->getName(i4); \
					ename.suffix = enameptr->suffix; \
					ename.abbreviation = enameptr->abbreviation; \
					ename.avoid_input = enameptr->avoid_input; \
					ename.plural = enameptr->plural; \
					item->setName(ename, i4); \
				} else if(!ename.name.empty() && validation(ename.name, version_numbers, is_user_defs)) { \
					if(!case_set) { \
						ename.case_sensitive = ename.abbreviation || text_length_is_one(ename.name); \
					} \
					item->addName(ename); \
				} \
			} \
			if(names_i == string::npos) {break;} \
			names_i++; \
		} \
	}


#define ITEM_READ_NAME(validation)\
					if(!new_names && (!xmlStrcmp(child->name, (const xmlChar*) "name") || !xmlStrcmp(child->name, (const xmlChar*) "abbreviation") || !xmlStrcmp(child->name, (const xmlChar*) "plural"))) {\
						name_index = 1;\
						XML_GET_INT_FROM_PROP(child, "index", name_index)\
						if(name_index > 0 && name_index <= 10) {\
							name_index--;\
							names[name_index] = empty_expression_name;\
							ref_names[name_index] = empty_expression_name;\
							value2 = NULL;\
							bool case_set = false;\
							if(child->name[0] == 'a') {\
								names[name_index].abbreviation = true;\
								ref_names[name_index].abbreviation = true;\
							} else if(child->name[0] == 'p') {\
								names[name_index].plural = true;\
								ref_names[name_index].plural = true;\
							}\
							child2 = child->xmlChildrenNode;\
							while(child2 != NULL) {\
								if((!best_name[name_index] || (ref_names[name_index].name.empty() && !locale.empty())) && !xmlStrcmp(child2->name, (const xmlChar*) "name")) {\
									lang = xmlNodeGetLang(child2);\
									if(!lang) {\
										value2 = xmlNodeListGetString(doc, child2->xmlChildrenNode, 1);\
										if(!value2 || validation((char*) value2, version_numbers, is_user_defs)) {\
											if(locale.empty()) {\
												best_name[name_index] = true;\
												if(value2) names[name_index].name = (char*) value2;\
												else names[name_index].name = "";\
											} else if(!require_translation) {\
												if(!best_name[name_index] && !nextbest_name[name_index]) {\
													if(value2) names[name_index].name = (char*) value2;\
													else names[name_index].name = "";\
												}\
												if(value2) ref_names[name_index].name = (char*) value2;\
												else ref_names[name_index].name = "";\
											}\
										}\
									} else if(!best_name[name_index] && !locale.empty()) {\
										if(locale == (char*) lang) {\
											value2 = xmlNodeListGetString(doc, child2->xmlChildrenNode, 1);\
											if(!value2 || validation((char*) value2, version_numbers, is_user_defs)) {\
												best_name[name_index] = true;\
												if(value2) names[name_index].name = (char*) value2;\
												else names[name_index].name = "";\
											}\
										} else if(!nextbest_name[name_index] && strlen((char*) lang) >= 2 && fulfilled_translation == 0 && lang[0] == localebase[0] && lang[1] == localebase[1]) {\
											value2 = xmlNodeListGetString(doc, child2->xmlChildrenNode, 1);\
											if(!value2 || validation((char*) value2, version_numbers, is_user_defs)) {\
												nextbest_name[name_index] = true; \
												if(value2) names[name_index].name = (char*) value2;\
												else names[name_index].name = "";\
											}\
										}\
									}\
									if(value2) xmlFree(value2);\
									if(lang) xmlFree(lang);\
									value2 = NULL; lang = NULL;\
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "unicode")) {\
									XML_GET_BOOL_FROM_TEXT(child2, names[name_index].unicode)\
									ref_names[name_index].unicode = names[name_index].unicode;\
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "reference")) {\
									XML_GET_BOOL_FROM_TEXT(child2, names[name_index].reference)\
									ref_names[name_index].reference = names[name_index].reference;\
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "suffix")) {\
									XML_GET_BOOL_FROM_TEXT(child2, names[name_index].suffix)\
									ref_names[name_index].suffix = names[name_index].suffix;\
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "avoid_input")) {\
									XML_GET_BOOL_FROM_TEXT(child2, names[name_index].avoid_input)\
									ref_names[name_index].avoid_input = names[name_index].avoid_input;\
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "plural")) {\
									XML_GET_BOOL_FROM_TEXT(child2, names[name_index].plural)\
									ref_names[name_index].plural = names[name_index].plural;\
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "abbreviation")) {\
									XML_GET_BOOL_FROM_TEXT(child2, names[name_index].abbreviation)\
									ref_names[name_index].abbreviation = names[name_index].abbreviation;\
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "case_sensitive")) {\
									XML_GET_BOOL_FROM_TEXT(child2, names[name_index].case_sensitive)\
									ref_names[name_index].case_sensitive = names[name_index].case_sensitive;\
									case_set = true;\
								}\
								child2 = child2->next;\
							}\
							if(!case_set) {\
								ref_names[name_index].case_sensitive = ref_names[name_index].abbreviation || text_length_is_one(ref_names[name_index].name);\
								names[name_index].case_sensitive = names[name_index].abbreviation || text_length_is_one(names[name_index].name);\
							}\
							if(names[name_index].reference) {\
								if(!ref_names[name_index].name.empty()) {\
									if(ref_names[name_index].name == names[name_index].name) {\
										ref_names[name_index].name = "";\
									} else {\
										names[name_index].reference = false;\
									}\
								}\
							} else if(!ref_names[name_index].name.empty()) {\
								ref_names[name_index].name = "";\
							}\
						}\
					}
					
#define ITEM_READ_DTH \
					if(!xmlStrcmp(child->name, (const xmlChar*) "description")) {\
						XML_GET_LOCALE_STRING_FROM_TEXT(child, description, best_description, next_best_description)\
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "title")) {\
						XML_GET_LOCALE_STRING_FROM_TEXT_REQ(child, title, best_title, next_best_title)\
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "hidden")) {\
						XML_GET_TRUE_FROM_TEXT(child, hidden);\
					}

#define ITEM_READ_NAMES \
					if(new_names && ((best_names.empty() && fulfilled_translation != 2) || default_names.empty()) && !xmlStrcmp(child->name, (const xmlChar*) "names")) {\
						value = xmlNodeListGetString(doc, child->xmlChildrenNode, 1);\
 						lang = xmlNodeGetLang(child);\
						if(!lang) {\
							if(default_names.empty()) {\
								if(value) {\
									default_names = (char*) value;\
									remove_blank_ends(default_names);\
								} else {\
									default_names = "";\
								}\
							}\
						} else if(best_names.empty()) {\
							if(locale == (char*) lang) {\
								if(value) {\
									best_names = (char*) value;\
									remove_blank_ends(best_names);\
								} else {\
									best_names = " ";\
								}\
							} else if(nextbest_names.empty() && strlen((char*) lang) >= 2 && fulfilled_translation == 0 && lang[0] == localebase[0] && lang[1] == localebase[1]) {\
								if(value) {\
									nextbest_names = (char*) value;\
									remove_blank_ends(nextbest_names);\
								} else {\
									nextbest_names = " ";\
								}\
							} else if(nextbest_names.empty() && default_names.empty() && value && !require_translation) {\
								nextbest_names = (char*) value;\
								remove_blank_ends(nextbest_names);\
							}\
						}\
						if(value) xmlFree(value);\
						if(lang) xmlFree(lang);\
					}
				
#define ITEM_INIT_DTH \
					hidden = false;\
					title = ""; best_title = false; next_best_title = false;\
					description = ""; best_description = false; next_best_description = false;\
					if(fulfilled_translation > 0) require_translation = false; \
					else {XML_GET_TRUE_FROM_PROP(cur, "require_translation", require_translation)}

#define ITEM_INIT_NAME \
					if(new_names) {\
						best_names = "";\
						nextbest_names = "";\
						default_names = "";\
					} else {\
						for(size_t i = 0; i < 10; i++) {\
							best_name[i] = false;\
							nextbest_name[i] = false;\
						}\
					}
					
					
#define ITEM_SET_NAME_1(validation)\
					if(!name.empty() && validation(name, version_numbers, is_user_defs)) {\
						ename.name = name;\
						ename.unicode = false;\
						ename.abbreviation = false;\
						ename.case_sensitive = text_length_is_one(ename.name);\
						ename.suffix = false;\
						ename.avoid_input = false;\
						ename.reference = true;\
						ename.plural = false;\
						item->addName(ename);\
					}
					
#define ITEM_SET_NAME_2\
					for(size_t i = 0; i < 10; i++) {\
						if(!names[i].name.empty()) {\
							item->addName(names[i], i + 1);\
							names[i].name = "";\
						} else if(!ref_names[i].name.empty()) {\
							item->addName(ref_names[i], i + 1);\
							ref_names[i].name = "";\
						}\
					}
					
#define ITEM_SET_NAME_3\
					for(size_t i = 0; i < 10; i++) {\
						if(!ref_names[i].name.empty()) {\
							item->addName(ref_names[i]);\
							ref_names[i].name = "";\
						}\
					}
					
#define ITEM_SET_DTH\
					item->setDescription(description);\
					if(!title.empty() && title[0] == '!') {\
						size_t i = title.find('!', 1);\
						if(i == string::npos) {\
							item->setTitle(title);\
						} else if(i + 1 == title.length()) {\
							item->setTitle("");\
						} else {\
							item->setTitle(title.substr(i + 1, title.length() - (i + 1)));\
						}\
					} else {\
						item->setTitle(title);\
					}\
					item->setHidden(hidden);

#define ITEM_SET_SHORT_NAME\
					if(!name.empty() && unitNameIsValid(name, version_numbers, is_user_defs)) {\
						ename.name = name;\
						ename.unicode = false;\
						ename.abbreviation = true;\
						ename.case_sensitive = true;\
						ename.suffix = false;\
						ename.avoid_input = false;\
						ename.reference = true;\
						ename.plural = false;\
						item->addName(ename);\
					}
					
#define ITEM_SET_SINGULAR\
					if(!singular.empty()) {\
						ename.name = singular;\
						ename.unicode = false;\
						ename.abbreviation = false;\
						ename.case_sensitive = text_length_is_one(ename.name);\
						ename.suffix = false;\
						ename.avoid_input = false;\
						ename.reference = false;\
						ename.plural = false;\
						item->addName(ename);\
					}

#define ITEM_SET_PLURAL\
					if(!plural.empty()) {\
						ename.name = plural;\
						ename.unicode = false;\
						ename.abbreviation = false;\
						ename.case_sensitive = text_length_is_one(ename.name);\
						ename.suffix = false;\
						ename.avoid_input = false;\
						ename.reference = false;\
						ename.plural = true;\
						item->addName(ename);\
					}
					
#define BUILTIN_NAMES_1\
				if(!is_user_defs) item->setRegistered(false);\
					bool has_ref_name;\
					for(size_t i = 1; i <= item->countNames(); i++) {\
						if(item->getName(i).reference) {\
							has_ref_name = false;\
							for(size_t i2 = 0; i2 < 10; i2++) {\
								if(names[i2].name == item->getName(i).name || ref_names[i2].name == item->getName(i).name) {\
									has_ref_name = true;\
									break;\
								}\
							}\
							if(!has_ref_name) {\
								for(int i2 = 9; i2 >= 0; i2--) {\
									if(ref_names[i2].name.empty()) {\
										ref_names[i2] = item->getName(i);\
										break;\
									}\
								}\
							}\
						}\
					}\
					item->clearNames();

#define BUILTIN_UNIT_NAMES_1\
				if(!is_user_defs) item->setRegistered(false);\
					bool has_ref_name;\
					for(size_t i = 1; i <= item->countNames(); i++) {\
						if(item->getName(i).reference) {\
							has_ref_name = item->getName(i).name == singular || item->getName(i).name == plural;\
							for(size_t i2 = 0; !has_ref_name && i2 < 10; i2++) {\
								if(names[i2].name == item->getName(i).name || ref_names[i2].name == item->getName(i).name) {\
									has_ref_name = true;\
									break;\
								}\
							}\
							if(!has_ref_name) {\
								for(int i2 = 9; i2 >= 0; i2--) {\
									if(ref_names[i2].name.empty()) {\
										ref_names[i2] = item->getName(i);\
										break;\
									}\
								}\
							}\
						}\
					}\
					item->clearNames(); 
					
#define BUILTIN_NAMES_2\
				if(!is_user_defs) {\
					item->setRegistered(true);\
					nameChanged(item);\
				}
					
#define ITEM_CLEAR_NAMES\
					for(size_t i = 0; i < 10; i++) {\
						if(!names[i].name.empty()) {\
							names[i].name = "";\
						}\
						if(!ref_names[i].name.empty()) {\
							ref_names[i].name = "";\
						}\
					}					

int Calculator::loadDefinitions(const char* file_name, bool is_user_defs) {

	xmlDocPtr doc;
	xmlNodePtr cur, child, child2, child3;
	string version, stmp, name, uname, type, svalue, sexp, plural, singular, category_title, category, description, title, inverse, base, argname, usystem;
	bool best_title, next_best_title, best_category_title, next_best_category_title, best_description, next_best_description;
	bool best_plural, next_best_plural, best_singular, next_best_singular, best_argname, next_best_argname;
	bool best_proptitle, next_best_proptitle, best_propdescr, next_best_propdescr;
	string proptitle, propdescr;
	ExpressionName names[10];
	ExpressionName ref_names[10];
	string prop_names[10];
	string ref_prop_names[10];
	bool best_name[10];
	bool nextbest_name[10];
	string best_names, nextbest_names, default_names;
	string best_prop_names, nextbest_prop_names, default_prop_names;
	int name_index, prec;
	ExpressionName ename;

	string locale;
	char *clocale = setlocale(LC_MESSAGES, "");
	if(clocale) {
		locale = clocale;
		if(locale == "POSIX" || locale == "C") {
			locale = "";
		} else {
			size_t i = locale.find('.');
			if(i != string::npos) locale = locale.substr(0, i);
		}
	}

	int fulfilled_translation = 0;
	string localebase;
	if(locale.length() > 2) {
		localebase = locale.substr(0, 2);
		if(locale == "en_US") {
			fulfilled_translation = 2;
		} else if(localebase == "en") {
			fulfilled_translation = 1;
		}
	} else {
		localebase = locale;
		if(locale == "en") {
			fulfilled_translation = 2;
		}
	}
	while(localebase.length() < 2) {
		localebase += " ";
		fulfilled_translation = 2;
	}

	int exponent = 1, litmp = 0;
	bool active = false, hidden = false, b = false, require_translation = false;
	Number nr;
	ExpressionItem *item;
	MathFunction *f;
	Variable *v;
	Unit *u;
	AliasUnit *au;
	CompositeUnit *cu;
	Prefix *p;
	Argument *arg;
	DataSet *dc;
	DataProperty *dp;
	int itmp;
	IntegerArgument *iarg;
	NumberArgument *farg;	
	xmlChar *value, *lang, *value2;
	int in_unfinished = 0;
	bool done_something = false;
	doc = xmlParseFile(file_name);
	if(doc == NULL) {
		return false;
	}
	cur = xmlDocGetRootElement(doc);
	if(cur == NULL) {
		xmlFreeDoc(doc);
		return false;
	}
	while(cur != NULL) {
		if(!xmlStrcmp(cur->name, (const xmlChar*) "QALCULATE")) {
			XML_GET_STRING_FROM_PROP(cur, "version", version)
			break;
		}
		cur = cur->next;
	}
	if(cur == NULL) {
		error(true, _("File not identified as Qalculate! definitions file: %s."), file_name, NULL);
		xmlFreeDoc(doc);
		return false;
	}
	int version_numbers[] = {0, 9, 4};
	parse_qalculate_version(version, version_numbers);

	bool new_names = version_numbers[0] > 0 || version_numbers[1] > 9 || (version_numbers[1] == 9 && version_numbers[2] >= 4);
	
	ParseOptions po;

	vector<xmlNodePtr> unfinished_nodes;
	vector<string> unfinished_cats;
	queue<xmlNodePtr> sub_items;
	vector<queue<xmlNodePtr> > nodes;

	category = "";
	nodes.resize(1);

	while(true) {
		if(!in_unfinished) {
			category_title = ""; best_category_title = false; next_best_category_title = false;	
			child = cur->xmlChildrenNode;
			while(child != NULL) {
				if(!xmlStrcmp(child->name, (const xmlChar*) "title")) {
					XML_GET_LOCALE_STRING_FROM_TEXT(child, category_title, best_category_title, next_best_category_title)
				} else if(!xmlStrcmp(child->name, (const xmlChar*) "category")) {
					nodes.back().push(child);
				} else {
					sub_items.push(child);
				}
				child = child->next;
			}
			if(!category.empty()) {
				category += "/";
			}
			if(!category_title.empty() && category_title[0] == '!') {\
				size_t i = category_title.find('!', 1);
				if(i == string::npos) {
					category += category_title;
				} else if(i + 1 < category_title.length()) {
					category += category_title.substr(i + 1, category_title.length() - (i + 1));
				}
			} else {
				category += category_title;
			}
		}
		while(!sub_items.empty() || (in_unfinished && cur)) {
			if(!in_unfinished) {
				cur = sub_items.front();
				sub_items.pop();
			}
			if(!xmlStrcmp(cur->name, (const xmlChar*) "activate")) {
				XML_GET_STRING_FROM_TEXT(cur, name)
				ExpressionItem *item = getInactiveExpressionItem(name);
				if(item && !item->isLocal()) {
					item->setActive(true);
					done_something = true;
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "deactivate")) {
				XML_GET_STRING_FROM_TEXT(cur, name)
				ExpressionItem *item = getActiveExpressionItem(name);
				if(item && !item->isLocal()) {
					item->setActive(false);
					done_something = true;
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "function")) {
				if(VERSION_BEFORE(0, 6, 3)) {
					XML_GET_STRING_FROM_PROP(cur, "name", name)
				} else {
					name = "";
				}
				XML_GET_FALSE_FROM_PROP(cur, "active", active)
				f = new UserFunction(category, "", "", is_user_defs, 0, "", "", 0, active);
				item = f;
				done_something = true;
				child = cur->xmlChildrenNode;
				ITEM_INIT_DTH
				ITEM_INIT_NAME
				while(child != NULL) {
					if(!xmlStrcmp(child->name, (const xmlChar*) "expression")) {
						XML_DO_FROM_TEXT(child, ((UserFunction*) f)->setFormula);
						XML_GET_PREC_FROM_PROP(child, prec)
						f->setPrecision(prec);
						XML_GET_APPROX_FROM_PROP(child, b)
						f->setApproximate(b);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "condition")) {
						XML_DO_FROM_TEXT(child, f->setCondition);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "subfunction")) {
						XML_GET_FALSE_FROM_PROP(child, "precalculate", b);
						value = xmlNodeListGetString(doc, child->xmlChildrenNode, 1); 
						if(value) ((UserFunction*) f)->addSubfunction((char*) value, b); 
						else ((UserFunction*) f)->addSubfunction("", true); 
						if(value) xmlFree(value);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "argument")) {
						farg = NULL; iarg = NULL;
						XML_GET_STRING_FROM_PROP(child, "type", type);
						if(type == "text") {
							arg = new TextArgument();
						} else if(type == "symbol") {
							arg = new SymbolicArgument();
						} else if(type == "date") {
							arg = new DateArgument();
						} else if(type == "integer") {
							iarg = new IntegerArgument();
							arg = iarg;
						} else if(type == "number") {
							farg = new NumberArgument();
							arg = farg;
						} else if(type == "vector") {
							arg = new VectorArgument();
						} else if(type == "matrix") {
							arg = new MatrixArgument();
						} else if(type == "boolean") {
							arg = new BooleanArgument();
						} else if(type == "function") {
							arg = new FunctionArgument();
						} else if(type == "unit") {
							arg = new UnitArgument();
						} else if(type == "variable") {
							arg = new VariableArgument();
						} else if(type == "object") {
							arg = new ExpressionItemArgument();
						} else if(type == "angle") {
							arg = new AngleArgument();
						} else if(type == "data-object") {
							arg = new DataObjectArgument(NULL, "");
						} else if(type == "data-property") {
							arg = new DataPropertyArgument(NULL, "");
						} else {
							arg = new Argument();
						}
						child2 = child->xmlChildrenNode;
						argname = ""; best_argname = false; next_best_argname = false;
						while(child2 != NULL) {
							if(!xmlStrcmp(child2->name, (const xmlChar*) "title")) {
								XML_GET_LOCALE_STRING_FROM_TEXT(child2, argname, best_argname, next_best_argname)
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "min")) {
								if(farg) {
									XML_DO_FROM_TEXT(child2, nr.set);
									farg->setMin(&nr);
									XML_GET_FALSE_FROM_PROP(child, "include_equals", b)
									farg->setIncludeEqualsMin(b);
								} else if(iarg) {
									XML_GET_STRING_FROM_TEXT(child2, stmp);
									Number integ(stmp);
									iarg->setMin(&integ);
								}
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "max")) {
								if(farg) {
									XML_DO_FROM_TEXT(child2, nr.set);
									farg->setMax(&nr);
									XML_GET_FALSE_FROM_PROP(child, "include_equals", b)
									farg->setIncludeEqualsMax(b);
								} else if(iarg) {
									XML_GET_STRING_FROM_TEXT(child2, stmp);
									Number integ(stmp);
									iarg->setMax(&integ);
								}
							} else if(farg && !xmlStrcmp(child2->name, (const xmlChar*) "complex_allowed")) {
								XML_GET_FALSE_FROM_TEXT(child2, b);
								farg->setComplexAllowed(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "condition")) {
								XML_DO_FROM_TEXT(child2, arg->setCustomCondition);	
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "matrix_allowed")) {
								XML_GET_TRUE_FROM_TEXT(child2, b);
								arg->setMatrixAllowed(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "zero_forbidden")) {
								XML_GET_TRUE_FROM_TEXT(child2, b);
								arg->setZeroForbidden(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "test")) {
								XML_GET_FALSE_FROM_TEXT(child2, b);
								arg->setTests(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "alert")) {
								XML_GET_FALSE_FROM_TEXT(child2, b);
								arg->setAlerts(b);
							}
							child2 = child2->next;
						}
						if(!argname.empty() && argname[0] == '!') {
							size_t i = argname.find('!', 1);
							if(i == string::npos) {
								arg->setName(argname);
							} else if(i + 1 < argname.length()) {
								arg->setName(argname.substr(i + 1, argname.length() - (i + 1)));
							}
						} else {
							arg->setName(argname);
						}
						itmp = 1;
						XML_GET_INT_FROM_PROP(child, "index", itmp);
						f->setArgumentDefinition(itmp, arg); 
					} else ITEM_READ_NAME(functionNameIsValid)
					 else ITEM_READ_DTH
					 else {
						ITEM_READ_NAMES
					}
					child = child->next;
				}
				if(new_names) {
					ITEM_SET_BEST_NAMES(functionNameIsValid)
					ITEM_SET_REFERENCE_NAMES(functionNameIsValid)
				} else {
					ITEM_SET_NAME_1(functionNameIsValid)
					ITEM_SET_NAME_2
					ITEM_SET_NAME_3
				}
				ITEM_SET_DTH
				if(f->countNames() == 0) {
					f->destroy();
					f = NULL;
				} else {
					f->setChanged(false);
					addFunction(f, true, is_user_defs);
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "dataset") || !xmlStrcmp(cur->name, (const xmlChar*) "builtin_dataset")) {
				bool builtin = !xmlStrcmp(cur->name, (const xmlChar*) "builtin_dataset");
				XML_GET_FALSE_FROM_PROP(cur, "active", active)
				if(builtin) {
					XML_GET_STRING_FROM_PROP(cur, "name", name)
					dc = getDataSet(name);
					if(!dc) {
						goto after_load_object;
					}
					dc->setCategory(category);
				} else {
					dc = new DataSet(category, "", "", "", "", is_user_defs);
				}
				item = dc;
				done_something = true;
				child = cur->xmlChildrenNode;
				ITEM_INIT_DTH
				ITEM_INIT_NAME
				while(child != NULL) {
					if(!xmlStrcmp(child->name, (const xmlChar*) "property")) {
						dp = new DataProperty(dc);
						child2 = child->xmlChildrenNode;
						if(new_names) {
							default_prop_names = ""; best_prop_names = ""; nextbest_prop_names = "";
						} else {
							for(size_t i = 0; i < 10; i++) {
								best_name[i] = false;
								nextbest_name[i] = false;
							}
						}
						proptitle = ""; best_proptitle = false; next_best_proptitle = false;
						propdescr = ""; best_propdescr = false; next_best_propdescr = false;
						while(child2 != NULL) {
							if(!xmlStrcmp(child2->name, (const xmlChar*) "title")) {
								XML_GET_LOCALE_STRING_FROM_TEXT(child2, proptitle, best_proptitle, next_best_proptitle)
							} else if(!new_names && !xmlStrcmp(child2->name, (const xmlChar*) "name")) {
								name_index = 1;
								XML_GET_INT_FROM_PROP(child2, "index", name_index)
								if(name_index > 0 && name_index <= 10) {
									name_index--;
									prop_names[name_index] = "";
									ref_prop_names[name_index] = "";
									value2 = NULL;
									child3 = child2->xmlChildrenNode;
									while(child3 != NULL) {
										if((!best_name[name_index] || (ref_prop_names[name_index].empty() && !locale.empty())) && !xmlStrcmp(child3->name, (const xmlChar*) "name")) {
											lang = xmlNodeGetLang(child3);
											if(!lang) {
												value2 = xmlNodeListGetString(doc, child3->xmlChildrenNode, 1);
												if(locale.empty()) {
													best_name[name_index] = true;
													if(value2) prop_names[name_index] = (char*) value2;
													else prop_names[name_index] = "";
												} else {
													if(!best_name[name_index] && !nextbest_name[name_index]) {
														if(value2) prop_names[name_index] = (char*) value2;
														else prop_names[name_index] = "";
													}
													if(value2) ref_prop_names[name_index] = (char*) value2;
													else ref_prop_names[name_index] = "";
												}
											} else if(!best_name[name_index] && !locale.empty()) {
												if(locale == (char*) lang) {
													value2 = xmlNodeListGetString(doc, child3->xmlChildrenNode, 1);
													best_name[name_index] = true;
													if(value2) prop_names[name_index] = (char*) value2;
													else prop_names[name_index] = "";
												} else if(!nextbest_name[name_index] && strlen((char*) lang) >= 2 && fulfilled_translation == 0 && lang[0] == localebase[0] && lang[1] == localebase[1]) {
													value2 = xmlNodeListGetString(doc, child3->xmlChildrenNode, 1);
													nextbest_name[name_index] = true;
													if(value2) prop_names[name_index] = (char*) value2;
													else prop_names[name_index] = "";
												}
											}
											if(value2) xmlFree(value2);
											if(lang) xmlFree(lang);
											value2 = NULL; lang = NULL;
										}
										child3 = child3->next;
									}
									if(!ref_prop_names[name_index].empty() && ref_prop_names[name_index] == prop_names[name_index]) {
										ref_prop_names[name_index] = "";
									}
								}
							} else if(new_names && !xmlStrcmp(child2->name, (const xmlChar*) "names") && ((best_prop_names.empty() && fulfilled_translation != 2) || default_prop_names.empty())) {
									value2 = xmlNodeListGetString(doc, child2->xmlChildrenNode, 1);
 									lang = xmlNodeGetLang(child2);
									if(!lang) {
										if(default_prop_names.empty()) {
											if(value2) {
												default_prop_names = (char*) value2;
												remove_blank_ends(default_prop_names);
											} else {
												default_prop_names = "";
											}
										}
									} else {
										if(locale == (char*) lang) {
											if(value2) {
												best_prop_names = (char*) value2;
												remove_blank_ends(best_prop_names);
											} else {
												best_prop_names = " ";
											}
									} else if(nextbest_prop_names.empty() && strlen((char*) lang) >= 2 && fulfilled_translation == 0 && lang[0] == localebase[0] && lang[1] == localebase[1]) {
										if(value2) {
											nextbest_prop_names = (char*) value2;
											remove_blank_ends(nextbest_prop_names);
										} else {
											nextbest_prop_names = " ";
										}
									} else if(nextbest_prop_names.empty() && default_prop_names.empty() && value2 && !require_translation) {
										nextbest_prop_names = (char*) value2;
										remove_blank_ends(nextbest_prop_names);
									}
								}
								if(value2) xmlFree(value2);
								if(lang) xmlFree(lang);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "description")) {
								XML_GET_LOCALE_STRING_FROM_TEXT(child2, propdescr, best_propdescr, next_best_propdescr)
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "unit")) {
								XML_DO_FROM_TEXT(child2, dp->setUnit)
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "key")) {
								XML_GET_TRUE_FROM_TEXT(child2, b)
								dp->setKey(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "hidden")) {
								XML_GET_TRUE_FROM_TEXT(child2, b)
								dp->setHidden(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "brackets")) {
								XML_GET_TRUE_FROM_TEXT(child2, b)
								dp->setUsesBrackets(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "approximate")) {
								XML_GET_TRUE_FROM_TEXT(child2, b)
								dp->setApproximate(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "case_sensitive")) {
								XML_GET_TRUE_FROM_TEXT(child2, b)
								dp->setCaseSensitive(b);
							} else if(!xmlStrcmp(child2->name, (const xmlChar*) "type")) {
								XML_GET_STRING_FROM_TEXT(child2, stmp)
								if(stmp == "text") {
									dp->setPropertyType(PROPERTY_STRING);
								} else if(stmp == "number") {
									dp->setPropertyType(PROPERTY_NUMBER);
								} else if(stmp == "expression") {
									dp->setPropertyType(PROPERTY_EXPRESSION);
								}
							}
							child2 = child2->next;
						}
						if(!proptitle.empty() && proptitle[0] == '!') {\
							size_t i = proptitle.find('!', 1);
							if(i == string::npos) {
								dp->setTitle(proptitle);
							} else if(i + 1 < proptitle.length()) {
								dp->setTitle(proptitle.substr(i + 1, proptitle.length() - (i + 1)));
							}
						} else {
							dp->setTitle(proptitle);
						}
						dp->setDescription(propdescr);
						if(new_names) {
							size_t names_i = 0, i2 = 0;
							string *str_names;
							bool had_ref = false;
							if(best_prop_names == "-") {best_prop_names = ""; nextbest_prop_names = "";}
							if(!best_prop_names.empty()) {str_names = &best_prop_names;}
							else if(!nextbest_prop_names.empty()) {str_names = &nextbest_prop_names;}
							else {str_names = &default_prop_names;}
							if(!str_names->empty() && (*str_names)[0] == '!') {
								names_i = str_names->find('!', 1) + 1;
							}
							while(true) {
								size_t i3 = names_i;
								names_i = str_names->find(",", i3);
								if(i2 == 0) {
									i2 = str_names->find(":", i3);
								}
								bool b_prop_ref = false;
								if(i2 < names_i) {
									bool b = true;
									for(; i3 < i2; i3++) {
										switch((*str_names)[i3]) {
											case '-': {b = false; break;}
											case 'r': {b_prop_ref = b; b = true; break;}
										}
									}
									i3++;
									i2 = 0;
								}
								if(names_i == string::npos) {stmp = str_names->substr(i3, str_names->length() - i3);}
								else {stmp = str_names->substr(i3, names_i - i3);}
								remove_blank_ends(stmp);
								if(!stmp.empty()) {
									if(b_prop_ref) had_ref = true;
									dp->addName(stmp, b_prop_ref);
								}
								if(names_i == string::npos) {break;}
								names_i++;
							}
							if(str_names != &default_prop_names && !default_prop_names.empty()) {
								if(default_prop_names[0] == '!') {
									names_i = default_prop_names.find('!', 1) + 1;
								} else {
									names_i = 0;
								}
								i2 = 0;
								while(true) {
									size_t i3 = names_i;
									names_i = default_prop_names.find(",", i3);
									if(i2 == 0) {
										i2 = default_prop_names.find(":", i3);
									}
									bool b_prop_ref = false;
									if(i2 < names_i) {
										bool b = true;
										for(; i3 < i2; i3++) {
											switch(default_prop_names[i3]) {
												case '-': {b = false; break;}
												case 'r': {b_prop_ref = b; b = true; break;}
											}
										}
										i3++;
										i2 = 0;
									}
									if(b_prop_ref || (!had_ref && names_i == string::npos)) {
										had_ref = true;
										if(names_i == string::npos) {stmp = default_prop_names.substr(i3, default_prop_names.length() - i3);}
										else {stmp = default_prop_names.substr(i3, names_i - i3);}
										remove_blank_ends(stmp);
										size_t i4 = dp->hasName(stmp);
										if(i4 > 0) {
											dp->setNameIsReference(i4, true);
										} else if(!stmp.empty()) {
											dp->addName(stmp, true);
										}
									}
									if(names_i == string::npos) {break;}
									names_i++;
								}
							}
							if(!had_ref && dp->countNames() > 0) dp->setNameIsReference(1, true);
						} else {
							bool b = false;
							for(size_t i = 0; i < 10; i++) {
								if(!prop_names[i].empty()) {
									if(!b && ref_prop_names[i].empty()) {
										dp->addName(prop_names[i], true, i + 1);
										b = true;
									} else {
										dp->addName(prop_names[i], false, i + 1);
									}
									prop_names[i] = "";
								}
							}
							for(size_t i = 0; i < 10; i++) {
								if(!ref_prop_names[i].empty()) {
									if(!b) {
										dp->addName(ref_prop_names[i], true);
										b = true;
									} else {
										dp->addName(ref_prop_names[i], false);
									}
									ref_prop_names[i] = "";
								}
							}
						}
						dp->setUserModified(is_user_defs);
						dc->addProperty(dp);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "argument")) {
						child2 = child->xmlChildrenNode;
						argname = ""; best_argname = false; next_best_argname = false;
						while(child2 != NULL) {
							if(!xmlStrcmp(child2->name, (const xmlChar*) "title")) {
								XML_GET_LOCALE_STRING_FROM_TEXT(child2, argname, best_argname, next_best_argname)
							}
							child2 = child2->next;
						}
						itmp = 1;
						XML_GET_INT_FROM_PROP(child, "index", itmp);
						if(dc->getArgumentDefinition(itmp)) {
							dc->getArgumentDefinition(itmp)->setName(argname);
						}
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "object_argument")) {
						child2 = child->xmlChildrenNode;
						argname = ""; best_argname = false; next_best_argname = false;
						while(child2 != NULL) {
							if(!xmlStrcmp(child2->name, (const xmlChar*) "title")) {
								XML_GET_LOCALE_STRING_FROM_TEXT(child2, argname, best_argname, next_best_argname)
							}
							child2 = child2->next;
						}
						itmp = 1;
						if(dc->getArgumentDefinition(itmp)) {
							if(!argname.empty() && argname[0] == '!') {
								size_t i = argname.find('!', 1);
								if(i == string::npos) {
									dc->getArgumentDefinition(itmp)->setName(argname);
								} else if(i + 1 < argname.length()) {
									dc->getArgumentDefinition(itmp)->setName(argname.substr(i + 1, argname.length() - (i + 1)));
								}
							} else {
								dc->getArgumentDefinition(itmp)->setName(argname);
							}
						}
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "property_argument")) {
						child2 = child->xmlChildrenNode;
						argname = ""; best_argname = false; next_best_argname = false;
						while(child2 != NULL) {
							if(!xmlStrcmp(child2->name, (const xmlChar*) "title")) {
								XML_GET_LOCALE_STRING_FROM_TEXT(child2, argname, best_argname, next_best_argname)
							}
							child2 = child2->next;
						}
						itmp = 2;
						if(dc->getArgumentDefinition(itmp)) {
							if(!argname.empty() && argname[0] == '!') {
								size_t i = argname.find('!', 1);
								if(i == string::npos) {
									dc->getArgumentDefinition(itmp)->setName(argname);
								} else if(i + 1 < argname.length()) {
									dc->getArgumentDefinition(itmp)->setName(argname.substr(i + 1, argname.length() - (i + 1)));
								}
							} else {
								dc->getArgumentDefinition(itmp)->setName(argname);
							}
						}
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "default_property")) {
						XML_DO_FROM_TEXT(child, dc->setDefaultProperty)
					} else if(!builtin && !xmlStrcmp(child->name, (const xmlChar*) "copyright")) {
						XML_DO_FROM_TEXT(child, dc->setCopyright)
					} else if(!builtin && !xmlStrcmp(child->name, (const xmlChar*) "datafile")) {
						XML_DO_FROM_TEXT(child, dc->setDefaultDataFile)
					} else ITEM_READ_NAME(functionNameIsValid)
					 else ITEM_READ_DTH
					 else {
						ITEM_READ_NAMES
					}
					child = child->next;
				}
				if(new_names) {
					if(builtin) {
						ITEM_SAVE_BUILTIN_NAMES
					}
					ITEM_SET_BEST_NAMES(functionNameIsValid)
					ITEM_SET_REFERENCE_NAMES(functionNameIsValid)
					if(builtin) {
						ITEM_SET_BUILTIN_NAMES
					}
				} else {
					if(builtin) {
						BUILTIN_NAMES_1
					}
					ITEM_SET_NAME_2
					ITEM_SET_NAME_3
					if(builtin) {
						BUILTIN_NAMES_2
					}
				}
				ITEM_SET_DTH
				if(!builtin && dc->countNames() == 0) {
					dc->destroy();
					dc = NULL;
				} else {
					dc->setChanged(builtin && is_user_defs);
					if(!builtin) addDataSet(dc, true, is_user_defs);
				}
				done_something = true;
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "builtin_function")) {
				XML_GET_STRING_FROM_PROP(cur, "name", name)
				f = getFunction(name);
				if(f) {
					XML_GET_FALSE_FROM_PROP(cur, "active", active)
					f->setLocal(is_user_defs, active);
					f->setCategory(category);
					item = f;
					child = cur->xmlChildrenNode;
					ITEM_INIT_DTH
					ITEM_INIT_NAME
					while(child != NULL) {
						if(!xmlStrcmp(child->name, (const xmlChar*) "argument")) {
							child2 = child->xmlChildrenNode;
							argname = ""; best_argname = false; next_best_argname = false;
							while(child2 != NULL) {
								if(!xmlStrcmp(child2->name, (const xmlChar*) "title")) {
									XML_GET_LOCALE_STRING_FROM_TEXT(child2, argname, best_argname, next_best_argname)
								}
								child2 = child2->next;
							}
							itmp = 1;
							XML_GET_INT_FROM_PROP(child, "index", itmp);
							if(f->getArgumentDefinition(itmp)) {
								if(!argname.empty() && argname[0] == '!') {
									size_t i = argname.find('!', 1);
									if(i == string::npos) {
										f->getArgumentDefinition(itmp)->setName(argname);
									} else if(i + 1 < argname.length()) {
										f->getArgumentDefinition(itmp)->setName(argname.substr(i + 1, argname.length() - (i + 1)));
									}
								} else {
									f->getArgumentDefinition(itmp)->setName(argname);
								}
							} else if(itmp <= f->maxargs() || itmp <= f->minargs()) {
								if(!argname.empty() && argname[0] == '!') {
									size_t i = argname.find('!', 1);
									if(i == string::npos) {
										f->setArgumentDefinition(itmp, new Argument(argname, false));
									} else if(i + 1 < argname.length()) {
										f->setArgumentDefinition(itmp, new Argument(argname.substr(i + 1, argname.length() - (i + 1)), false));
									}
								} else {
									f->setArgumentDefinition(itmp, new Argument(argname, false));
								}
							}
						} else ITEM_READ_NAME(functionNameIsValid)
						 else ITEM_READ_DTH
						 else {
							ITEM_READ_NAMES
						}
						child = child->next;
					}
					if(new_names) {
						ITEM_SAVE_BUILTIN_NAMES
						ITEM_SET_BEST_NAMES(functionNameIsValid)
						ITEM_SET_REFERENCE_NAMES(functionNameIsValid)
						ITEM_SET_BUILTIN_NAMES
					} else {
						BUILTIN_NAMES_1
						ITEM_SET_NAME_2
						ITEM_SET_NAME_3
						BUILTIN_NAMES_2
					}
					ITEM_SET_DTH
					f->setChanged(false);
					done_something = true;
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "unknown")) {
				if(VERSION_BEFORE(0, 6, 3)) {
					XML_GET_STRING_FROM_PROP(cur, "name", name)
				} else {
					name = "";
				}
				XML_GET_FALSE_FROM_PROP(cur, "active", active)
				svalue = "";
				v = new UnknownVariable(category, "", "", is_user_defs, false, active);
				item = v;
				done_something = true;
				child = cur->xmlChildrenNode;
				b = true;
				ITEM_INIT_DTH
				ITEM_INIT_NAME
				while(child != NULL) {
					if(!xmlStrcmp(child->name, (const xmlChar*) "type")) {
						XML_GET_STRING_FROM_TEXT(child, stmp);
						if(!((UnknownVariable*) v)->assumptions()) ((UnknownVariable*) v)->setAssumptions(new Assumptions());
						if(stmp == "integer") ((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_INTEGER);
						else if(stmp == "rational") ((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_RATIONAL);
						else if(stmp == "real") ((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_REAL);
						else if(stmp == "complex") ((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_COMPLEX);
						else if(stmp == "number") ((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_NUMBER);
						else if(stmp == "non-matrix") ((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_NONMATRIX);
						else if(stmp == "none") {
							if(VERSION_BEFORE(0, 9, 1)) {
								((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_NONMATRIX);
							} else {
								((UnknownVariable*) v)->assumptions()->setType(ASSUMPTION_TYPE_NONE);
							}
						}
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "sign")) {
						XML_GET_STRING_FROM_TEXT(child, stmp);
						if(!((UnknownVariable*) v)->assumptions()) ((UnknownVariable*) v)->setAssumptions(new Assumptions());
						if(stmp == "non-zero") ((UnknownVariable*) v)->assumptions()->setSign(ASSUMPTION_SIGN_NONZERO);
						else if(stmp == "non-positive") ((UnknownVariable*) v)->assumptions()->setSign(ASSUMPTION_SIGN_NONPOSITIVE);
						else if(stmp == "negative") ((UnknownVariable*) v)->assumptions()->setSign(ASSUMPTION_SIGN_NEGATIVE);
						else if(stmp == "non-negative") ((UnknownVariable*) v)->assumptions()->setSign(ASSUMPTION_SIGN_NONNEGATIVE);
						else if(stmp == "positive") ((UnknownVariable*) v)->assumptions()->setSign(ASSUMPTION_SIGN_POSITIVE);
						else if(stmp == "unknown") ((UnknownVariable*) v)->assumptions()->setSign(ASSUMPTION_SIGN_UNKNOWN);
					} else ITEM_READ_NAME(variableNameIsValid)
					 else ITEM_READ_DTH
					 else {
						ITEM_READ_NAMES
					}
					child = child->next;
				}
				if(new_names) {
					ITEM_SET_BEST_NAMES(variableNameIsValid)
					ITEM_SET_REFERENCE_NAMES(variableNameIsValid)
				} else {
					ITEM_SET_NAME_1(variableNameIsValid)
					ITEM_SET_NAME_2
					ITEM_SET_NAME_3
				}
				ITEM_SET_DTH
				for(size_t i = 1; i <= v->countNames(); i++) {
					if(v->getName(i).name == "x") {v_x->destroy(); v_x = (UnknownVariable*) v; break;}
					if(v->getName(i).name == "y") {v_y->destroy(); v_y = (UnknownVariable*) v; break;}
					if(v->getName(i).name == "z") {v_z->destroy(); v_z = (UnknownVariable*) v; break;}
				}
				if(v->countNames() == 0) {
					v->destroy();
					v = NULL;
				} else {
					addVariable(v, true, is_user_defs);
					v->setChanged(false);
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "variable")) {
				if(VERSION_BEFORE(0, 6, 3)) {
					XML_GET_STRING_FROM_PROP(cur, "name", name)
				} else {
					name = "";
				}
				XML_GET_FALSE_FROM_PROP(cur, "active", active)
				svalue = "";
				v = new KnownVariable(category, "", "", "", is_user_defs, false, active);
				item = v;
				done_something = true;
				child = cur->xmlChildrenNode;
				b = true;
				ITEM_INIT_DTH
				ITEM_INIT_NAME
				while(child != NULL) {
					if(!xmlStrcmp(child->name, (const xmlChar*) "value")) {
						XML_DO_FROM_TEXT(child, ((KnownVariable*) v)->set);
						XML_GET_PREC_FROM_PROP(child, prec)
						v->setPrecision(prec);
						XML_GET_APPROX_FROM_PROP(child, b);
						v->setApproximate(b);
					} else ITEM_READ_NAME(variableNameIsValid)
					 else ITEM_READ_DTH
					 else {
						ITEM_READ_NAMES
					}
					child = child->next;
				}
				if(new_names) {
					ITEM_SET_BEST_NAMES(variableNameIsValid)
					ITEM_SET_REFERENCE_NAMES(variableNameIsValid)
				} else {
					ITEM_SET_NAME_1(variableNameIsValid)
					ITEM_SET_NAME_2
					ITEM_SET_NAME_3
				}
				ITEM_SET_DTH
				if(v->countNames() == 0) {
					v->destroy();
					v = NULL;
				} else {
					addVariable(v, true, is_user_defs);
					item->setChanged(false);
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "builtin_variable")) {
				XML_GET_STRING_FROM_PROP(cur, "name", name)
				v = getVariable(name);
				if(v) {	
					XML_GET_FALSE_FROM_PROP(cur, "active", active)
					v->setLocal(is_user_defs, active);
					v->setCategory(category);
					item = v;
					child = cur->xmlChildrenNode;
					ITEM_INIT_DTH
					ITEM_INIT_NAME
					while(child != NULL) {
						ITEM_READ_NAME(variableNameIsValid)
						 else ITEM_READ_DTH
						 else {
							ITEM_READ_NAMES
						}
						child = child->next;
					}
					if(new_names) {
						ITEM_SAVE_BUILTIN_NAMES
						ITEM_SET_BEST_NAMES(variableNameIsValid)
						ITEM_SET_REFERENCE_NAMES(variableNameIsValid)
						ITEM_SET_BUILTIN_NAMES
					} else {
						BUILTIN_NAMES_1
						ITEM_SET_NAME_2
						ITEM_SET_NAME_3
						BUILTIN_NAMES_2
					}
					ITEM_SET_DTH
					v->setChanged(false);
					done_something = true;
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "unit")) {
				XML_GET_STRING_FROM_PROP(cur, "type", type)
				if(type == "base") {	
					if(VERSION_BEFORE(0, 6, 3)) {
						XML_GET_STRING_FROM_PROP(cur, "name", name)
					} else {
						name = "";
					}
					XML_GET_FALSE_FROM_PROP(cur, "active", active)
					u = new Unit(category, "", "", "", "", is_user_defs, false, active);
					item = u;
					child = cur->xmlChildrenNode;
					singular = ""; best_singular = false; next_best_singular = false;
					plural = ""; best_plural = false; next_best_plural = false;
					ITEM_INIT_DTH
					ITEM_INIT_NAME
					while(child != NULL) {
						if(!xmlStrcmp(child->name, (const xmlChar*) "system")) {	
							XML_DO_FROM_TEXT(child, u->setSystem)
						} else if((VERSION_BEFORE(0, 6, 3)) && !xmlStrcmp(child->name, (const xmlChar*) "singular")) {
							XML_GET_LOCALE_STRING_FROM_TEXT(child, singular, best_singular, next_best_singular)
							if(!unitNameIsValid(singular, version_numbers, is_user_defs)) {
								singular = "";
							}
						} else if((VERSION_BEFORE(0, 6, 3)) && !xmlStrcmp(child->name, (const xmlChar*) "plural") && !xmlGetProp(child, (xmlChar*) "index")) {
							XML_GET_LOCALE_STRING_FROM_TEXT(child, plural, best_plural, next_best_plural)
							if(!unitNameIsValid(plural, version_numbers, is_user_defs)) {
								plural = "";
							}
						} else ITEM_READ_NAME(unitNameIsValid)
						 else ITEM_READ_DTH
						 else {
							ITEM_READ_NAMES
						}
						child = child->next;
					}
					if(new_names) {
						ITEM_SET_BEST_NAMES(unitNameIsValid)
						ITEM_SET_REFERENCE_NAMES(unitNameIsValid)
					} else {
						ITEM_SET_SHORT_NAME
						ITEM_SET_SINGULAR
						ITEM_SET_PLURAL
						ITEM_SET_NAME_2
						ITEM_SET_NAME_3
					}
					ITEM_SET_DTH
					if(u->countNames() == 0) {
						u->destroy();
						u = NULL;
					} else {
						addUnit(u, true, is_user_defs);
						u->setChanged(false);
					}
					done_something = true;
				} else if(type == "alias") {	
					if(VERSION_BEFORE(0, 6, 3)) {
						XML_GET_STRING_FROM_PROP(cur, "name", name)
					} else {
						name = "";
					}
					XML_GET_FALSE_FROM_PROP(cur, "active", active)
					u = NULL;
					child = cur->xmlChildrenNode;
					singular = ""; best_singular = false; next_best_singular = false;
					plural = ""; best_plural = false; next_best_plural = false;
					usystem = "";
					prec = -1;
					ITEM_INIT_DTH
					ITEM_INIT_NAME
					while(child != NULL) {
						if(!xmlStrcmp(child->name, (const xmlChar*) "base")) {
							child2 = child->xmlChildrenNode;
							exponent = 1;
							svalue = "";
							inverse = "";
							b = true;
							while(child2 != NULL) {
								if(!xmlStrcmp(child2->name, (const xmlChar*) "unit")) {
									XML_GET_STRING_FROM_TEXT(child2, base);
									u = getUnit(base);
									if(!u) {
										u = getCompositeUnit(base);
									}
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "relation")) {
									XML_GET_STRING_FROM_TEXT(child2, svalue);
									XML_GET_APPROX_FROM_PROP(child2, b)
									XML_GET_PREC_FROM_PROP(child, prec)
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "reverse_relation")) {
									XML_GET_STRING_FROM_TEXT(child2, inverse);
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "inverse_relation")) {
									XML_GET_STRING_FROM_TEXT(child2, inverse);
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "exponent")) {
									XML_GET_STRING_FROM_TEXT(child2, stmp);
									if(stmp.empty()) {
										exponent = 1;
									} else {
										exponent = s2i(stmp);
									}
								}
								child2 = child2->next;
							}
						} else if(!xmlStrcmp(child->name, (const xmlChar*) "system")) {
							XML_GET_STRING_FROM_TEXT(child, usystem);
						} else if((VERSION_BEFORE(0, 6, 3)) && !xmlStrcmp(child->name, (const xmlChar*) "singular")) {	
							XML_GET_LOCALE_STRING_FROM_TEXT(child, singular, best_singular, next_best_singular)
							if(!unitNameIsValid(singular, version_numbers, is_user_defs)) {
								singular = "";
							}
						} else if((VERSION_BEFORE(0, 6, 3)) && !xmlStrcmp(child->name, (const xmlChar*) "plural") && !xmlGetProp(child, (xmlChar*) "index")) {	
							XML_GET_LOCALE_STRING_FROM_TEXT(child, plural, best_plural, next_best_plural)
							if(!unitNameIsValid(plural, version_numbers, is_user_defs)) {
								plural = "";
							}
						} else ITEM_READ_NAME(unitNameIsValid)
						 else ITEM_READ_DTH
						 else {
							ITEM_READ_NAMES
						}
						child = child->next;
					}
					if(!u) {
						ITEM_CLEAR_NAMES
						if(!in_unfinished) {
							unfinished_nodes.push_back(cur);
							unfinished_cats.push_back(category);
						}
					} else {
						au = new AliasUnit(category, name, plural, singular, title, u, svalue, exponent, inverse, is_user_defs, false, active);
						au->setDescription(description);
						au->setPrecision(prec);
						au->setApproximate(b);
						au->setHidden(hidden);
						au->setSystem(usystem);
						item = au;
						if(new_names) {
							ITEM_SET_BEST_NAMES(unitNameIsValid)
							ITEM_SET_REFERENCE_NAMES(unitNameIsValid)
						} else {
							ITEM_SET_NAME_2
							ITEM_SET_NAME_3
						}
						if(au->countNames() == 0) {
							au->destroy();
							au = NULL;
						} else {
							addUnit(au, true, is_user_defs);
							au->setChanged(false);
						}
						done_something = true;	
					}
				} else if(type == "composite") {
					if(VERSION_BEFORE(0, 6, 3)) {
						XML_GET_STRING_FROM_PROP(cur, "name", name)
					} else {
						name = "";
					}
					XML_GET_FALSE_FROM_PROP(cur, "active", active)
					child = cur->xmlChildrenNode;
					usystem = "";
					cu = NULL;
					ITEM_INIT_DTH
					ITEM_INIT_NAME
					b = true;
					while(child != NULL) {
						u = NULL;
						if(!xmlStrcmp(child->name, (const xmlChar*) "part")) {
							child2 = child->xmlChildrenNode;
							p = NULL;
							exponent = 1;							
							while(child2 != NULL) {
								if(!xmlStrcmp(child2->name, (const xmlChar*) "unit")) {
									XML_GET_STRING_FROM_TEXT(child2, base);									
									u = getUnit(base);
									if(!u) {
										u = getCompositeUnit(base);
									}
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "prefix")) {
									XML_GET_STRING_FROM_PROP(child2, "type", stmp)
									XML_GET_STRING_FROM_TEXT(child2, svalue);
									p = NULL;
									if(stmp == "binary") {
										litmp = s2i(svalue);
										if(litmp != 0) {
											p = getExactBinaryPrefix(litmp);
											if(!p) b = false;
										}
									} else if(stmp == "number") {
										nr.set(stmp);
										if(!nr.isZero()) {
											p = getExactPrefix(stmp);
											if(!p) b = false;
										}
									} else {
										litmp = s2i(svalue);
										if(litmp != 0) {
											p = getExactDecimalPrefix(litmp);
											if(!p) b = false;
										}
									}
									if(!b) {
										if(cu) {
											delete cu;
										}
										cu = NULL;
										break;
									}												
								} else if(!xmlStrcmp(child2->name, (const xmlChar*) "exponent")) {
									XML_GET_STRING_FROM_TEXT(child2, stmp);
									if(stmp.empty()) {
										exponent = 1;
									} else {
										exponent = s2i(stmp);
									}
								}
								child2 = child2->next;
							}
							if(!b) break;
							if(u) {
								if(!cu) {
									cu = new CompositeUnit("", "", "", "", is_user_defs, false, active);
								}
								cu->add(u, exponent, p);
							} else {
								if(cu) delete cu;
								cu = NULL;
								if(!in_unfinished) {
									unfinished_nodes.push_back(cur);
									unfinished_cats.push_back(category);
								}
								break;
							}
						} else if(!xmlStrcmp(child->name, (const xmlChar*) "system")) {
							XML_GET_STRING_FROM_TEXT(child, usystem);
						} else ITEM_READ_NAME(unitNameIsValid)
						 else ITEM_READ_DTH
						 else {
							ITEM_READ_NAMES
						}
						child = child->next;
					}
					if(cu) {
						item = cu;
						cu->setCategory(category);
						cu->setSystem(usystem);
						if(new_names) {
							ITEM_SET_BEST_NAMES(unitNameIsValid)
							ITEM_SET_REFERENCE_NAMES(unitNameIsValid)
						} else {
							ITEM_SET_NAME_1(unitNameIsValid)
							ITEM_SET_NAME_2
							ITEM_SET_NAME_3
						}
						ITEM_SET_DTH
						if(cu->countNames() == 0) {
							cu->destroy();
							cu = NULL;
						} else {
							addUnit(cu, true, is_user_defs);
							cu->setChanged(false);
						}
						done_something = true;
					} else {
						ITEM_CLEAR_NAMES
					}
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "builtin_unit")) {
				XML_GET_STRING_FROM_PROP(cur, "name", name)
				u = getUnit(name);
				if(!u) {
					u = getCompositeUnit(name);
				}
				if(u) {	
					XML_GET_FALSE_FROM_PROP(cur, "active", active)
					u->setLocal(is_user_defs, active);
					u->setCategory(category);
					item = u;
					child = cur->xmlChildrenNode;
					singular = ""; best_singular = false; next_best_singular = false;
					plural = ""; best_plural = false; next_best_plural = false;
					ITEM_INIT_DTH
					ITEM_INIT_NAME
					while(child != NULL) {
						if(!xmlStrcmp(child->name, (const xmlChar*) "singular")) {	
							XML_GET_LOCALE_STRING_FROM_TEXT(child, singular, best_singular, next_best_singular)
							if(!unitNameIsValid(singular, version_numbers, is_user_defs)) {
								singular = "";
							}
						} else if(!xmlStrcmp(child->name, (const xmlChar*) "plural") && !xmlGetProp(child, (xmlChar*) "index")) {	
							XML_GET_LOCALE_STRING_FROM_TEXT(child, plural, best_plural, next_best_plural)
							if(!unitNameIsValid(plural, version_numbers, is_user_defs)) {
								plural = "";
							}
						} else ITEM_READ_NAME(unitNameIsValid)
						 else ITEM_READ_DTH
						 else {
							ITEM_READ_NAMES
						}
						child = child->next;
					}
					if(new_names) {
						ITEM_SAVE_BUILTIN_NAMES
						ITEM_SET_BEST_NAMES(unitNameIsValid)
						ITEM_SET_REFERENCE_NAMES(unitNameIsValid)
						ITEM_SET_BUILTIN_NAMES
					} else {
						BUILTIN_UNIT_NAMES_1
						ITEM_SET_SINGULAR
						ITEM_SET_PLURAL
						ITEM_SET_NAME_2
						ITEM_SET_NAME_3
						BUILTIN_NAMES_2
					}
					ITEM_SET_DTH
					u->setChanged(false);
					done_something = true;
				}
			} else if(!xmlStrcmp(cur->name, (const xmlChar*) "prefix")) {
				child = cur->xmlChildrenNode;
				XML_GET_STRING_FROM_PROP(cur, "type", type)
				uname = ""; sexp = ""; svalue = "";
				while(child != NULL) {
					if(!xmlStrcmp(child->name, (const xmlChar*) "name")) {
						XML_GET_STRING_FROM_TEXT(child, name);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "abbreviation")) {	
						XML_GET_STRING_FROM_TEXT(child, stmp);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "unicode")) {	
						XML_GET_STRING_FROM_TEXT(child, uname);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "exponent")) {	
						XML_GET_STRING_FROM_TEXT(child, sexp);
					} else if(!xmlStrcmp(child->name, (const xmlChar*) "value")) {	
						XML_GET_STRING_FROM_TEXT(child, svalue);
					}
					child = child->next;
				}
				if(type == "decimal") {
					addPrefix(new DecimalPrefix(s2i(sexp), name, stmp, uname));
				} else if(type == "number") {
					addPrefix(new NumberPrefix(svalue, name, stmp, uname));
				} else if(type == "binary") {
					addPrefix(new BinaryPrefix(s2i(sexp), name, stmp, uname));
				} else {
					if(svalue.empty()) {
						addPrefix(new DecimalPrefix(s2i(sexp), name, stmp, uname));
					} else {
						addPrefix(new NumberPrefix(svalue, name, stmp, uname));
					}
				}
				done_something = true;
			}
			after_load_object:
			cur = NULL;
			if(in_unfinished) {
				if(done_something) {
					in_unfinished--;
					unfinished_nodes.erase(unfinished_nodes.begin() + in_unfinished);
					unfinished_cats.erase(unfinished_cats.begin() + in_unfinished);
				}
				if((int) unfinished_nodes.size() > in_unfinished) {
					cur = unfinished_nodes[in_unfinished];
					category = unfinished_cats[in_unfinished];
				} else if(done_something && unfinished_nodes.size() > 0) {
					cur = unfinished_nodes[0];
					category = unfinished_cats[0];
					in_unfinished = 0;
					done_something = false;
				}
				in_unfinished++;
				done_something = false;
			}
		}
		if(in_unfinished) {
			break;
		}
		while(!nodes.empty() && nodes.back().empty()) {
			size_t cat_i = category.rfind("/");
			if(cat_i == string::npos) {
				category = "";
			} else {
				category = category.substr(0, cat_i);
			}
			nodes.pop_back();
		}
		if(!nodes.empty()) {
			cur = nodes.back().front();
			nodes.back().pop();
			nodes.resize(nodes.size() + 1);
		} else {
			if(unfinished_nodes.size() > 0) {
				cur = unfinished_nodes[0];
				category = unfinished_cats[0];
				in_unfinished = 1;
				done_something = false;
			} else {
				cur = NULL;
			}
		} 
		if(cur == NULL) {
			break;
		} 
	}
	xmlFreeDoc(doc);
	return true;
}
bool Calculator::saveDefinitions() {

	string filename;
	string homedir = getLocalDir();
	mkdir(homedir.c_str(), S_IRWXU);	
	homedir += "definitions/";	
	mkdir(homedir.c_str(), S_IRWXU);
	filename = homedir;
	filename += "functions.xml";
	bool b = true;
	if(!saveFunctions(filename.c_str())) {
		b = false;
	}
	filename = homedir;
	filename += "units.xml";
	if(!saveUnits(filename.c_str())) {
		b = false;
	}
	filename = homedir;
	filename += "variables.xml";
	if(!saveVariables(filename.c_str())) {
		b = false;
	}
	filename = homedir;
	filename += "datasets.xml";
	if(!saveDataSets(filename.c_str())) {
		b = false;
	}
	if(!saveDataObjects()) {
		b = false;
	}
	return b;
}

struct node_tree_item {
	xmlNodePtr node;
	string category;
	vector<node_tree_item> items;
};

int Calculator::saveDataObjects() {
	int returnvalue = 1;
	for(size_t i = 0; i < data_sets.size(); i++) {
		int rv = data_sets[i]->saveObjects(NULL, false);
		if(rv <= 0) returnvalue = rv;
	}
	return returnvalue;
}

int Calculator::savePrefixes(const char* file_name, bool save_global) {
	if(!save_global) {
		return true;
	}
	xmlDocPtr doc = xmlNewDoc((xmlChar*) "1.0");	
	xmlNodePtr cur, newnode;	
	doc->children = xmlNewDocNode(doc, NULL, (xmlChar*) "QALCULATE", NULL);	
	xmlNewProp(doc->children, (xmlChar*) "version", (xmlChar*) VERSION);
	cur = doc->children;
	for(size_t i = 0; i < prefixes.size(); i++) {
		newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "prefix", NULL);
		if(!prefixes[i]->longName(false).empty()) xmlNewTextChild(newnode, NULL, (xmlChar*) "name", (xmlChar*) prefixes[i]->longName(false).c_str());
		if(!prefixes[i]->shortName(false).empty()) xmlNewTextChild(newnode, NULL, (xmlChar*) "abbreviation", (xmlChar*) prefixes[i]->shortName(false).c_str());
		if(!prefixes[i]->unicodeName(false).empty()) xmlNewTextChild(newnode, NULL, (xmlChar*) "unicode", (xmlChar*) prefixes[i]->unicodeName(false).c_str());
		switch(prefixes[i]->type()) {
			case PREFIX_DECIMAL: {
				xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "decimal");
				xmlNewTextChild(newnode, NULL, (xmlChar*) "exponent", (xmlChar*) i2s(((DecimalPrefix*) prefixes[i])->exponent()).c_str());
				break;
			}
			case PREFIX_BINARY: {
				xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "binary");
				xmlNewTextChild(newnode, NULL, (xmlChar*) "exponent", (xmlChar*) i2s(((BinaryPrefix*) prefixes[i])->exponent()).c_str());
				break;
			}
			case PREFIX_NUMBER: {
				xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "number");
				xmlNewTextChild(newnode, NULL, (xmlChar*) "value", (xmlChar*) prefixes[i]->value().print(save_printoptions).c_str());
				break;
			}
		}
	}	
	int returnvalue = xmlSaveFormatFile(file_name, doc, 1);
	xmlFreeDoc(doc);
	return returnvalue;
}

#define SAVE_NAMES(o)\
				str = "";\
				for(size_t i2 = 1;;)  {\
					ename = &o->getName(i2);\
					if(ename->abbreviation) {str += 'a';}\
					bool b_cs = (ename->abbreviation || text_length_is_one(ename->name));\
					if(ename->case_sensitive && !b_cs) {str += 'c';}\
					if(!ename->case_sensitive && b_cs) {str += "-c";}\
					if(ename->avoid_input) {str += 'i';}\
					if(ename->plural) {str += 'p';}\
					if(ename->reference) {str += 'r';}\
					if(ename->suffix) {str += 's';}\
					if(ename->unicode) {str += 'u';}\
					if(str.empty() || str[str.length() - 1] == ',') {\
						if(i2 == 1 && o->countNames() == 1) {\
							if(save_global) {\
								xmlNewTextChild(newnode, NULL, (xmlChar*) "_names", (xmlChar*) ename->name.c_str());\
							} else {\
								xmlNewTextChild(newnode, NULL, (xmlChar*) "names", (xmlChar*) ename->name.c_str());\
							}\
							break;\
						}\
					} else {\
						str += ':';\
					}\
					str += ename->name;\
					i2++;\
					if(i2 > o->countNames()) {\
						if(save_global) {\
							xmlNewTextChild(newnode, NULL, (xmlChar*) "_names", (xmlChar*) str.c_str());\
						} else {\
							xmlNewTextChild(newnode, NULL, (xmlChar*) "names", (xmlChar*) str.c_str());\
						}\
						break;\
					}\
					str += ',';\
				}

int Calculator::saveVariables(const char* file_name, bool save_global) {
	string str;
	const ExpressionName *ename;
	xmlDocPtr doc = xmlNewDoc((xmlChar*) "1.0");	
	xmlNodePtr cur, newnode, newnode2;	
	doc->children = xmlNewDocNode(doc, NULL, (xmlChar*) "QALCULATE", NULL);	
	xmlNewProp(doc->children, (xmlChar*) "version", (xmlChar*) VERSION);
	node_tree_item top;
	top.category = "";
	top.node = doc->children;
	node_tree_item *item;
	string cat, cat_sub;
	for(size_t i = 0; i < variables.size(); i++) {
		if((save_global || variables[i]->isLocal() || variables[i]->hasChanged()) && variables[i]->category() != _("Temporary")) {
			item = &top;
			if(!variables[i]->category().empty()) {
				cat = variables[i]->category();
				size_t cat_i = cat.find("/"); size_t cat_i_prev = 0;
				bool b = false;
				while(true) {
					if(cat_i == string::npos) {
						cat_sub = cat.substr(cat_i_prev, cat.length() - cat_i_prev);
					} else {
						cat_sub = cat.substr(cat_i_prev, cat_i - cat_i_prev);
					}
					b = false;
					for(size_t i2 = 0; i2 < item->items.size(); i2++) {
						if(cat_sub == item->items[i2].category) {
							item = &item->items[i2];
							b = true;
							break;
						}
					}
					if(!b) {
						item->items.resize(item->items.size() + 1);
						item->items[item->items.size() - 1].node = xmlNewTextChild(item->node, NULL, (xmlChar*) "category", NULL);
						item = &item->items[item->items.size() - 1];
						item->category = cat_sub;
						if(save_global) {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "_title", (xmlChar*) item->category.c_str());
						} else {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "title", (xmlChar*) item->category.c_str());
						}
					}
					if(cat_i == string::npos) {
						break;
					}
					cat_i_prev = cat_i + 1;
					cat_i = cat.find("/", cat_i_prev);
				}
			}
			cur = item->node;
			if(!save_global && !variables[i]->isLocal() && variables[i]->hasChanged()) {
				if(variables[i]->isActive()) {
					xmlNewTextChild(cur, NULL, (xmlChar*) "activate", (xmlChar*) variables[i]->referenceName().c_str());
				} else {
					xmlNewTextChild(cur, NULL, (xmlChar*) "deactivate", (xmlChar*) variables[i]->referenceName().c_str());
				}
			} else if(save_global || variables[i]->isLocal()) {
				if(variables[i]->isBuiltin()) {
					if(variables[i]->isKnown()) {
						newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "builtin_variable", NULL);
					} else {
						newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "builtin_unknown", NULL);
					}
					xmlNewProp(newnode, (xmlChar*) "name", (xmlChar*) variables[i]->referenceName().c_str());
				} else {
					if(variables[i]->isKnown()) {
						newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "variable", NULL);
					} else {
						newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "unknown", NULL);
					}
				}
				if(!variables[i]->isActive()) xmlNewProp(newnode, (xmlChar*) "active", (xmlChar*) "false");
				if(variables[i]->isHidden()) xmlNewTextChild(newnode, NULL, (xmlChar*) "hidden", (xmlChar*) "true");
				if(!variables[i]->title(false).empty()) {
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_title", (xmlChar*) variables[i]->title(false).c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "title", (xmlChar*) variables[i]->title(false).c_str());
					}
				}
				SAVE_NAMES(variables[i])
				if(!variables[i]->description().empty()) {
					str = variables[i]->description();
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_description", (xmlChar*) str.c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "description", (xmlChar*) str.c_str());
					}
				}
				if(!variables[i]->isBuiltin()) {
					if(variables[i]->isKnown()) {
						if(((KnownVariable*) variables[i])->isExpression()) {
							newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "value", (xmlChar*) ((KnownVariable*) variables[i])->expression().c_str());
						} else {
							newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "value", (xmlChar*) ((KnownVariable*) variables[i])->get().print(save_printoptions).c_str());
						}
						if(variables[i]->isApproximate()) xmlNewProp(newnode2, (xmlChar*) "approximate", (xmlChar*) "true");
						if(variables[i]->precision() > 0) xmlNewProp(newnode2, (xmlChar*) "precision", (xmlChar*) i2s(variables[i]->precision()).c_str());
					} else {
						if(((UnknownVariable*) variables[i])->assumptions()) {
							switch(((UnknownVariable*) variables[i])->assumptions()->type()) {
								case ASSUMPTION_TYPE_INTEGER: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "type", (xmlChar*) "integer");
									break;
								}
								case ASSUMPTION_TYPE_RATIONAL: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "type", (xmlChar*) "rational");
									break;
								}
								case ASSUMPTION_TYPE_REAL: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "type", (xmlChar*) "real");
									break;
								}
								case ASSUMPTION_TYPE_COMPLEX: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "type", (xmlChar*) "complex");
									break;
								}
								case ASSUMPTION_TYPE_NUMBER: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "type", (xmlChar*) "number");
									break;
								}
								case ASSUMPTION_TYPE_NONMATRIX: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "type", (xmlChar*) "non-matrix");
									break;
								}
								case ASSUMPTION_TYPE_NONE: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "type", (xmlChar*) "none");
									break;
								}
							}
							switch(((UnknownVariable*) variables[i])->assumptions()->sign()) {
								case ASSUMPTION_SIGN_NONZERO: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "sign", (xmlChar*) "non-zero");
									break;
								}
								case ASSUMPTION_SIGN_NONPOSITIVE: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "sign", (xmlChar*) "non-positive");
									break;
								}
								case ASSUMPTION_SIGN_NEGATIVE: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "sign", (xmlChar*) "negative");
									break;
								}
								case ASSUMPTION_SIGN_NONNEGATIVE: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "sign", (xmlChar*) "non-negative");
									break;
								}
								case ASSUMPTION_SIGN_POSITIVE: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "sign", (xmlChar*) "positive");
									break;
								}
								case ASSUMPTION_SIGN_UNKNOWN: {
									newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "sign", (xmlChar*) "unknown");
									break;
								}
							}
						}
					}
				}
			}
		}
	}	
	int returnvalue = xmlSaveFormatFile(file_name, doc, 1);
	xmlFreeDoc(doc);
	return returnvalue;
}

int Calculator::saveUnits(const char* file_name, bool save_global) {
	string str;
	xmlDocPtr doc = xmlNewDoc((xmlChar*) "1.0");	
	xmlNodePtr cur, newnode, newnode2, newnode3;	
	doc->children = xmlNewDocNode(doc, NULL, (xmlChar*) "QALCULATE", NULL);	
	xmlNewProp(doc->children, (xmlChar*) "version", (xmlChar*) VERSION);
	const ExpressionName *ename;
	CompositeUnit *cu = NULL;
	AliasUnit *au = NULL;
	node_tree_item top;
	top.category = "";
	top.node = doc->children;
	node_tree_item *item;
	string cat, cat_sub;
	for(size_t i = 0; i < units.size(); i++) {
		if(save_global || units[i]->isLocal() || units[i]->hasChanged()) {	
			item = &top;
			if(!units[i]->category().empty()) {
				cat = units[i]->category();
				size_t cat_i = cat.find("/"); size_t cat_i_prev = 0;
				bool b = false;
				while(true) {
					if(cat_i == string::npos) {
						cat_sub = cat.substr(cat_i_prev, cat.length() - cat_i_prev);
					} else {
						cat_sub = cat.substr(cat_i_prev, cat_i - cat_i_prev);
					}
					b = false;
					for(size_t i2 = 0; i2 < item->items.size(); i2++) {
						if(cat_sub == item->items[i2].category) {
							item = &item->items[i2];
							b = true;
							break;
						}
					}
					if(!b) {
						item->items.resize(item->items.size() + 1);
						item->items[item->items.size() - 1].node = xmlNewTextChild(item->node, NULL, (xmlChar*) "category", NULL);
						item = &item->items[item->items.size() - 1];
						item->category = cat_sub;
						if(save_global) {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "_title", (xmlChar*) item->category.c_str());
						} else {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "title", (xmlChar*) item->category.c_str());
						}
					}
					if(cat_i == string::npos) {
						break;
					}
					cat_i_prev = cat_i + 1;
					cat_i = cat.find("/", cat_i_prev);
				}
			}
			cur = item->node;	
			if(!save_global && !units[i]->isLocal() && units[i]->hasChanged()) {
				if(units[i]->isActive()) {
					xmlNewTextChild(cur, NULL, (xmlChar*) "activate", (xmlChar*) units[i]->referenceName().c_str());
				} else {
					xmlNewTextChild(cur, NULL, (xmlChar*) "deactivate", (xmlChar*) units[i]->referenceName().c_str());
				}
			} else if(save_global || units[i]->isLocal()) {
				if(units[i]->isBuiltin()) {
					newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "builtin_unit", NULL);
					xmlNewProp(newnode, (xmlChar*) "name", (xmlChar*) units[i]->referenceName().c_str());
				} else {
					newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "unit", NULL);
					switch(units[i]->subtype()) {
						case SUBTYPE_BASE_UNIT: {
							xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "base");
							break;
						}
						case SUBTYPE_ALIAS_UNIT: {
							au = (AliasUnit*) units[i];
							xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "alias");
							break;
						}
						case SUBTYPE_COMPOSITE_UNIT: {
							cu = (CompositeUnit*) units[i];
							xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "composite");
							break;
						}
					}
				}
				if(!units[i]->isActive()) xmlNewProp(newnode, (xmlChar*) "active", (xmlChar*) "false");
				if(units[i]->isHidden()) xmlNewTextChild(newnode, NULL, (xmlChar*) "hidden", (xmlChar*) "true");
				if(!units[i]->system().empty()) xmlNewTextChild(newnode, NULL, (xmlChar*) "system", (xmlChar*) units[i]->system().c_str());
				if(!units[i]->title(false).empty()) {
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_title", (xmlChar*) units[i]->title(false).c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "title", (xmlChar*) units[i]->title(false).c_str());
					}
				}
				if(save_global && units[i]->subtype() == SUBTYPE_COMPOSITE_UNIT) {
					save_global = false;
					SAVE_NAMES(units[i])
					save_global = true;
				} else {
					SAVE_NAMES(units[i])
				}
				if(!units[i]->description().empty()) {
					str = units[i]->description();
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_description", (xmlChar*) str.c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "description", (xmlChar*) str.c_str());
					}
				}
				if(!units[i]->isBuiltin()) {
					if(units[i]->subtype() == SUBTYPE_COMPOSITE_UNIT) {
						for(size_t i2 = 1; i2 <= cu->countUnits(); i2++) {
							newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "part", NULL);
							int exp = 1;
							Prefix *p = NULL;
							Unit *u = cu->get(i2, &exp, &p);
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "unit", (xmlChar*) u->referenceName().c_str());
							if(p) {
								switch(p->type()) {
									case PREFIX_DECIMAL: {
										xmlNewTextChild(newnode2, NULL, (xmlChar*) "prefix", (xmlChar*) i2s(((DecimalPrefix*) p)->exponent()).c_str());
										break;
									}
									case PREFIX_BINARY: {
										newnode3 = xmlNewTextChild(newnode2, NULL, (xmlChar*) "prefix", (xmlChar*) i2s(((BinaryPrefix*) p)->exponent()).c_str());
										xmlNewProp(newnode3, (xmlChar*) "type", (xmlChar*) "binary");
										break;
									}
									case PREFIX_NUMBER: {
										newnode3 = xmlNewTextChild(newnode2, NULL, (xmlChar*) "prefix", (xmlChar*) p->value().print(save_printoptions).c_str());
										xmlNewProp(newnode3, (xmlChar*) "type", (xmlChar*) "number");
										break;
									}
								}
							}
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "exponent", (xmlChar*) i2s(exp).c_str());
						}
					}
					if(units[i]->subtype() == SUBTYPE_ALIAS_UNIT) {
						newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "base", NULL);
						xmlNewTextChild(newnode2, NULL, (xmlChar*) "unit", (xmlChar*) au->firstBaseUnit()->referenceName().c_str());								
						newnode3 = xmlNewTextChild(newnode2, NULL, (xmlChar*) "relation", (xmlChar*) au->expression().c_str());
						if(units[i]->isApproximate()) xmlNewProp(newnode3, (xmlChar*) "approximate", (xmlChar*) "true");
						if(units[i]->precision() > 0) xmlNewProp(newnode2, (xmlChar*) "precision", (xmlChar*) i2s(units[i]->precision()).c_str());
						if(!au->inverseExpression().empty()) {
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "inverse_relation", (xmlChar*) au->inverseExpression().c_str());
						}
						xmlNewTextChild(newnode2, NULL, (xmlChar*) "exponent", (xmlChar*) i2s(au->firstBaseExponent()).c_str());
					}
				}
			}
		}
	}
	int returnvalue = xmlSaveFormatFile(file_name, doc, 1);
	xmlFreeDoc(doc);
	return returnvalue;
}

int Calculator::saveFunctions(const char* file_name, bool save_global) {
	xmlDocPtr doc = xmlNewDoc((xmlChar*) "1.0");	
	xmlNodePtr cur, newnode, newnode2;	
	doc->children = xmlNewDocNode(doc, NULL, (xmlChar*) "QALCULATE", NULL);	
	xmlNewProp(doc->children, (xmlChar*) "version", (xmlChar*) VERSION);
	const ExpressionName *ename;
	node_tree_item top;
	top.category = "";
	top.node = doc->children;
	node_tree_item *item;
	string cat, cat_sub;
	Argument *arg;
	IntegerArgument *iarg;
	NumberArgument *farg;
	string str;
	for(size_t i = 0; i < functions.size(); i++) {
		if(functions[i]->subtype() != SUBTYPE_DATA_SET && (save_global || functions[i]->isLocal() || functions[i]->hasChanged())) {
			item = &top;
			if(!functions[i]->category().empty()) {
				cat = functions[i]->category();
				size_t cat_i = cat.find("/"); size_t cat_i_prev = 0;
				bool b = false;
				while(true) {
					if(cat_i == string::npos) {
						cat_sub = cat.substr(cat_i_prev, cat.length() - cat_i_prev);
					} else {
						cat_sub = cat.substr(cat_i_prev, cat_i - cat_i_prev);
					}
					b = false;
					for(size_t i2 = 0; i2 < item->items.size(); i2++) {
						if(cat_sub == item->items[i2].category) {
							item = &item->items[i2];
							b = true;
							break;
						}
					}
					if(!b) {
						item->items.resize(item->items.size() + 1);
						item->items[item->items.size() - 1].node = xmlNewTextChild(item->node, NULL, (xmlChar*) "category", NULL);
						item = &item->items[item->items.size() - 1];
						item->category = cat_sub;
						if(save_global) {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "_title", (xmlChar*) item->category.c_str());
						} else {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "title", (xmlChar*) item->category.c_str());
						}
					}
					if(cat_i == string::npos) {
						break;
					}
					cat_i_prev = cat_i + 1;
					cat_i = cat.find("/", cat_i_prev);
				}
			}
			cur = item->node;
			if(!save_global && !functions[i]->isLocal() && functions[i]->hasChanged()) {
				if(functions[i]->isActive()) {
					xmlNewTextChild(cur, NULL, (xmlChar*) "activate", (xmlChar*) functions[i]->referenceName().c_str());
				} else {
					xmlNewTextChild(cur, NULL, (xmlChar*) "deactivate", (xmlChar*) functions[i]->referenceName().c_str());
				}
			} else if(save_global || functions[i]->isLocal()) {	
				if(functions[i]->isBuiltin()) {
					newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "builtin_function", NULL);
					xmlNewProp(newnode, (xmlChar*) "name", (xmlChar*) functions[i]->referenceName().c_str());
				} else {
					newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "function", NULL);
				}
				if(!functions[i]->isActive()) xmlNewProp(newnode, (xmlChar*) "active", (xmlChar*) "false");
				if(functions[i]->isHidden()) xmlNewTextChild(newnode, NULL, (xmlChar*) "hidden", (xmlChar*) "true");
				if(!functions[i]->title(false).empty()) {
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_title", (xmlChar*) functions[i]->title(false).c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "title", (xmlChar*) functions[i]->title(false).c_str());
					}
				}
				SAVE_NAMES(functions[i])
				if(!functions[i]->description().empty()) {
					str = functions[i]->description();
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_description", (xmlChar*) str.c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "description", (xmlChar*) str.c_str());
					}
				}
				if(functions[i]->isBuiltin()) {
					cur = newnode;
					for(size_t i2 = 1; i2 <= functions[i]->lastArgumentDefinitionIndex(); i2++) {
						arg = functions[i]->getArgumentDefinition(i2);
						if(arg && !arg->name().empty()) {
							newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "argument", NULL);
							if(save_global) {
								xmlNewTextChild(newnode, NULL, (xmlChar*) "_title", (xmlChar*) arg->name().c_str());
							} else {
								xmlNewTextChild(newnode, NULL, (xmlChar*) "title", (xmlChar*) arg->name().c_str());
							}
							xmlNewProp(newnode, (xmlChar*) "index", (xmlChar*) i2s(i2).c_str());
						}
					}
				} else {
					for(size_t i2 = 1; i2 <= ((UserFunction*) functions[i])->countSubfunctions(); i2++) {
						newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "subfunction", (xmlChar*) ((UserFunction*) functions[i])->getSubfunction(i2).c_str());
						if(((UserFunction*) functions[i])->subfunctionPrecalculated(i2)) xmlNewProp(newnode2, (xmlChar*) "precalculate", (xmlChar*) "true");
						else xmlNewProp(newnode2, (xmlChar*) "precalculate", (xmlChar*) "false");
						
					}
					newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "expression", (xmlChar*) ((UserFunction*) functions[i])->formula().c_str());
					if(functions[i]->isApproximate()) xmlNewProp(newnode2, (xmlChar*) "approximate", (xmlChar*) "true");
					if(functions[i]->precision() > 0) xmlNewProp(newnode2, (xmlChar*) "precision", (xmlChar*) i2s(functions[i]->precision()).c_str());
					if(!functions[i]->condition().empty()) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "condition", (xmlChar*) functions[i]->condition().c_str());
					}
					cur = newnode;
					for(size_t i2 = 1; i2 <= functions[i]->lastArgumentDefinitionIndex(); i2++) {
						arg = functions[i]->getArgumentDefinition(i2);
						if(arg) {
							newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "argument", NULL);
							if(!arg->name().empty()) {
								if(save_global) {
									xmlNewTextChild(newnode, NULL, (xmlChar*) "_title", (xmlChar*) arg->name().c_str());
								} else {
									xmlNewTextChild(newnode, NULL, (xmlChar*) "title", (xmlChar*) arg->name().c_str());
								}
							}
							switch(arg->type()) {
								case ARGUMENT_TYPE_TEXT: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "text"); break;}
								case ARGUMENT_TYPE_SYMBOLIC: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "symbol"); break;}
								case ARGUMENT_TYPE_DATE: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "date"); break;}
								case ARGUMENT_TYPE_INTEGER: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "integer"); break;}
								case ARGUMENT_TYPE_NUMBER: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "number"); break;}
								case ARGUMENT_TYPE_VECTOR: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "vector"); break;}
								case ARGUMENT_TYPE_MATRIX: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "matrix"); break;}
								case ARGUMENT_TYPE_BOOLEAN: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "boolean"); break;}
								case ARGUMENT_TYPE_FUNCTION: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "function"); break;}
								case ARGUMENT_TYPE_UNIT: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "unit"); break;}
								case ARGUMENT_TYPE_VARIABLE: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "variable"); break;}
								case ARGUMENT_TYPE_EXPRESSION_ITEM: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "object"); break;}
								case ARGUMENT_TYPE_ANGLE: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "angle"); break;}
								case ARGUMENT_TYPE_DATA_OBJECT: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "data-object"); break;}
								case ARGUMENT_TYPE_DATA_PROPERTY: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "data-property"); break;}
								default: {xmlNewProp(newnode, (xmlChar*) "type", (xmlChar*) "free");}
							}
							xmlNewProp(newnode, (xmlChar*) "index", (xmlChar*) i2s(i2).c_str());
							if(!arg->tests()) {
								xmlNewTextChild(newnode, NULL, (xmlChar*) "test", (xmlChar*) "false");
							}
							if(!arg->alerts()) {
								xmlNewTextChild(newnode, NULL, (xmlChar*) "alert", (xmlChar*) "false");
							}
							if(arg->zeroForbidden()) {
								xmlNewTextChild(newnode, NULL, (xmlChar*) "zero_forbidden", (xmlChar*) "true");
							}
							if(arg->matrixAllowed()) {
								xmlNewTextChild(newnode, NULL, (xmlChar*) "matrix_allowed", (xmlChar*) "true");
							}
							switch(arg->type()) {
								case ARGUMENT_TYPE_INTEGER: {
									iarg = (IntegerArgument*) arg;
									if(iarg->min()) {
										xmlNewTextChild(newnode, NULL, (xmlChar*) "min", (xmlChar*) iarg->min()->print(save_printoptions).c_str()); 
									}
									if(iarg->max()) {
										xmlNewTextChild(newnode, NULL, (xmlChar*) "max", (xmlChar*) iarg->max()->print(save_printoptions).c_str()); 
									}
									break;
								}
								case ARGUMENT_TYPE_NUMBER: {
									farg = (NumberArgument*) arg;
									if(farg->min()) {
										newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "min", (xmlChar*) farg->min()->print(save_printoptions).c_str()); 
										if(farg->includeEqualsMin()) {
											xmlNewProp(newnode2, (xmlChar*) "include_equals", (xmlChar*) "true");
										} else {
											xmlNewProp(newnode2, (xmlChar*) "include_equals", (xmlChar*) "false");
										}
									}
									if(farg->max()) {
										newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "max", (xmlChar*) farg->max()->print(save_printoptions).c_str()); 
										if(farg->includeEqualsMax()) {
											xmlNewProp(newnode2, (xmlChar*) "include_equals", (xmlChar*) "true");
										} else {
											xmlNewProp(newnode2, (xmlChar*) "include_equals", (xmlChar*) "false");
										}
									}
									if(!farg->complexAllowed()) {
										xmlNewTextChild(newnode, NULL, (xmlChar*) "complex_allowed", (xmlChar*) "false");
									}
									break;						
								}
							}					
							if(!arg->getCustomCondition().empty()) {
								xmlNewTextChild(newnode, NULL, (xmlChar*) "condition", (xmlChar*) arg->getCustomCondition().c_str());
							}
						}
					}
				}
			}
		}
	}
	int returnvalue = xmlSaveFormatFile(file_name, doc, 1);
	xmlFreeDoc(doc);
	return returnvalue;
}
int Calculator::saveDataSets(const char* file_name, bool save_global) {
	xmlDocPtr doc = xmlNewDoc((xmlChar*) "1.0");	
	xmlNodePtr cur, newnode, newnode2;
	doc->children = xmlNewDocNode(doc, NULL, (xmlChar*) "QALCULATE", NULL);	
	xmlNewProp(doc->children, (xmlChar*) "version", (xmlChar*) VERSION);
	const ExpressionName *ename;
	node_tree_item top;
	top.category = "";
	top.node = doc->children;
	node_tree_item *item;
	string cat, cat_sub;
	Argument *arg;
	DataSet *ds;
	DataProperty *dp;
	string str;
	for(size_t i = 0; i < functions.size(); i++) {
		if(functions[i]->subtype() == SUBTYPE_DATA_SET && (save_global || functions[i]->isLocal() || functions[i]->hasChanged())) {
			item = &top;
			ds = (DataSet*) functions[i];
			if(!ds->category().empty()) {
				cat = ds->category();
				size_t cat_i = cat.find("/"); size_t cat_i_prev = 0;
				bool b = false;
				while(true) {
					if(cat_i == string::npos) {
						cat_sub = cat.substr(cat_i_prev, cat.length() - cat_i_prev);
					} else {
						cat_sub = cat.substr(cat_i_prev, cat_i - cat_i_prev);
					}
					b = false;
					for(size_t i2 = 0; i2 < item->items.size(); i2++) {
						if(cat_sub == item->items[i2].category) {
							item = &item->items[i2];
							b = true;
							break;
						}
					}
					if(!b) {
						item->items.resize(item->items.size() + 1);
						item->items[item->items.size() - 1].node = xmlNewTextChild(item->node, NULL, (xmlChar*) "category", NULL);
						item = &item->items[item->items.size() - 1];
						item->category = cat_sub;
						if(save_global) {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "_title", (xmlChar*) item->category.c_str());
						} else {
							xmlNewTextChild(item->node, NULL, (xmlChar*) "title", (xmlChar*) item->category.c_str());
						}
					}
					if(cat_i == string::npos) {
						break;
					}
					cat_i_prev = cat_i + 1;
					cat_i = cat.find("/", cat_i_prev);
				}
			}
			cur = item->node;
			if(save_global || ds->isLocal() || ds->hasChanged()) {	
				if(save_global || ds->isLocal()) {
					newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "dataset", NULL);
				} else {
					newnode = xmlNewTextChild(cur, NULL, (xmlChar*) "builtin_dataset", NULL);
					xmlNewProp(newnode, (xmlChar*) "name", (xmlChar*) ds->referenceName().c_str());
				}
				if(!ds->isActive()) xmlNewProp(newnode, (xmlChar*) "active", (xmlChar*) "false");
				if(ds->isHidden()) xmlNewTextChild(newnode, NULL, (xmlChar*) "hidden", (xmlChar*) "true");
				if(!ds->title(false).empty()) {
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_title", (xmlChar*) ds->title(false).c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "title", (xmlChar*) ds->title(false).c_str());
					}
				}
				if((save_global || ds->isLocal()) && !ds->defaultDataFile().empty()) {
					xmlNewTextChild(newnode, NULL, (xmlChar*) "datafile", (xmlChar*) ds->defaultDataFile().c_str());
				}
				SAVE_NAMES(ds)
				if(!ds->description().empty()) {
					str = ds->description();
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_description", (xmlChar*) str.c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "description", (xmlChar*) str.c_str());
					}
				}
				if((save_global || ds->isLocal()) && !ds->copyright().empty()) {
					if(save_global) {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "_copyright", (xmlChar*) ds->copyright().c_str());
					} else {
						xmlNewTextChild(newnode, NULL, (xmlChar*) "copyright", (xmlChar*) ds->copyright().c_str());
					}
				}
				arg = ds->getArgumentDefinition(1);
				if(arg && ((!save_global && !ds->isLocal()) || arg->name() != _("Object"))) {
					newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "object_argument", NULL);
					if(save_global) {
						xmlNewTextChild(newnode2, NULL, (xmlChar*) "_title", (xmlChar*) arg->name().c_str());
					} else {
						xmlNewTextChild(newnode2, NULL, (xmlChar*) "title", (xmlChar*) arg->name().c_str());
					}
				}
				arg = ds->getArgumentDefinition(2);
				if(arg && ((!save_global && !ds->isLocal()) || arg->name() != _("Property"))) {
					newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "property_argument", NULL);
					if(save_global) {
						xmlNewTextChild(newnode2, NULL, (xmlChar*) "_title", (xmlChar*) arg->name().c_str());
					} else {
						xmlNewTextChild(newnode2, NULL, (xmlChar*) "title", (xmlChar*) arg->name().c_str());
					}
				}
				if((!save_global && !ds->isLocal()) || ds->getDefaultValue(2) != _("info")) {
					xmlNewTextChild(newnode, NULL, (xmlChar*) "default_property", (xmlChar*) ds->getDefaultValue(2).c_str());
				}
				DataPropertyIter it;
				dp = ds->getFirstProperty(&it);
				while(dp) {
					if(save_global || ds->isLocal() || dp->isUserModified()) {
						newnode2 = xmlNewTextChild(newnode, NULL, (xmlChar*) "property", NULL);
						if(!dp->title(false).empty()) {
							if(save_global) {
								xmlNewTextChild(newnode2, NULL, (xmlChar*) "_title", (xmlChar*) dp->title().c_str());
							} else {
								xmlNewTextChild(newnode2, NULL, (xmlChar*) "title", (xmlChar*) dp->title().c_str());
							}
						}
						switch(dp->propertyType()) {
							case PROPERTY_STRING: {
								xmlNewTextChild(newnode2, NULL, (xmlChar*) "type", (xmlChar*) "text");
								break;
							}
							case PROPERTY_NUMBER: {
								xmlNewTextChild(newnode2, NULL, (xmlChar*) "type", (xmlChar*) "number");
								break;
							}
							case PROPERTY_EXPRESSION: {
								xmlNewTextChild(newnode2, NULL, (xmlChar*) "type", (xmlChar*) "expression");
								break;
							}
						}
						if(dp->isHidden()) {
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "hidden", (xmlChar*) "true");
						}
						if(dp->isKey()) {
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "key", (xmlChar*) "true");
						}
						if(dp->isApproximate()) {
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "approximate", (xmlChar*) "true");
						}
						if(dp->usesBrackets()) {
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "brackets", (xmlChar*) "true");
						}
						if(dp->isCaseSensitive()) {
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "case_sensitive", (xmlChar*) "true");
						}
						if(!dp->getUnitString().empty()) {
							xmlNewTextChild(newnode2, NULL, (xmlChar*) "unit", (xmlChar*) dp->getUnitString().c_str());
						}
						str = "";
						for(size_t i2 = 1;;)  {
							if(dp->nameIsReference(i2)) {str += 'r';}
							if(str.empty() || str[str.length() - 1] == ',') {
								if(i2 == 1 && dp->countNames() == 1) {
									if(save_global) {
										xmlNewTextChild(newnode2, NULL, (xmlChar*) "_names", (xmlChar*) dp->getName(i2).c_str());
									} else {
										xmlNewTextChild(newnode2, NULL, (xmlChar*) "names", (xmlChar*) dp->getName(i2).c_str());
									}
									break;
								}
							} else {
								str += ':';
							}
							str += dp->getName(i2);
							i2++;
							if(i2 > dp->countNames()) {
								if(save_global) {
									xmlNewTextChild(newnode2, NULL, (xmlChar*) "_names", (xmlChar*) str.c_str());
								} else {
									xmlNewTextChild(newnode2, NULL, (xmlChar*) "names", (xmlChar*) str.c_str());
								}
								break;
							}
							str += ',';
						}
						if(!dp->description().empty()) {
							str = dp->description();
							if(save_global) {
								xmlNewTextChild(newnode2, NULL, (xmlChar*) "_description", (xmlChar*) str.c_str());
							} else {
								xmlNewTextChild(newnode2, NULL, (xmlChar*) "description", (xmlChar*) str.c_str());
							}
						}
					}
					dp = ds->getNextProperty(&it);
				}
			}
		}
	}
	int returnvalue = xmlSaveFormatFile(file_name, doc, 1);
	xmlFreeDoc(doc);
	return returnvalue;
}

bool Calculator::importCSV(MathStructure &mstruct, const char *file_name, int first_row, string delimiter, vector<string> *headers) {
	FILE *file = fopen(file_name, "r");
	if(file == NULL) {
		return false;
	}
	if(first_row < 1) {
		first_row = 1;
	}
	char line[10000];
	string stmp, str1, str2;
	int row = 0, rows = 1;
	int columns = 1;
	int column;
	mstruct = m_empty_matrix;
	size_t is, is_n;
	bool v_added = false;
	while(fgets(line, 10000, file)) {
		row++;
		if(row >= first_row) {	
			stmp = line;
			remove_blank_ends(stmp);
			if(row == first_row) {
				if(stmp.empty()) {
					row--;
				} else {
					is = 0;
					while((is_n = stmp.find(delimiter, is)) != string::npos) {		
						columns++;
						if(headers) {
							str1 = stmp.substr(is, is_n - is);
							remove_blank_ends(str1);
							headers->push_back(str1);
						}
						is = is_n + delimiter.length();
					}
					if(headers) {
						str1 = stmp.substr(is, stmp.length() - is);
						remove_blank_ends(str1);
						headers->push_back(str1);
					}
					mstruct.resizeMatrix(1, columns, m_undefined);
				}
			}
			if((!headers || row > first_row) && !stmp.empty()) {
				is = 0;
				column = 1;
				if(v_added) {
					mstruct.addRow(m_undefined);
					rows++;
				}
				while(column <= columns) {
					is_n = stmp.find(delimiter, is);
					if(is_n == string::npos) {
						str1 = stmp.substr(is, stmp.length() - is);
					} else {
						str1 = stmp.substr(is, is_n - is);
						is = is_n + delimiter.length();
					}
					CALCULATOR->parse(&mstruct[rows - 1][column - 1], str1);
					column++;
					if(is_n == string::npos) {
						break;
					}
				}
				v_added = true;
			}
		}
	}
	return true;
}

bool Calculator::importCSV(const char *file_name, int first_row, bool headers, string delimiter, bool to_matrix, string name, string title, string category) {
	FILE *file = fopen(file_name, "r");
	if(file == NULL) {
		return false;
	}
	if(first_row < 1) {
		first_row = 1;
	}
	string filestr = file_name;
	size_t i = filestr.find_last_of("/");
	if(i != string::npos) {
		filestr = filestr.substr(i + 1, filestr.length() - (i + 1));
	}
	if(name.empty()) {
		i = filestr.find_last_of(".");
		name = filestr.substr(0, i);
	}
	char line[10000];
	string stmp, str1, str2;
	int row = 0;
	int columns = 1, rows = 1;
	int column;
	vector<string> header;
	vector<MathStructure> vectors;
	MathStructure mstruct = m_empty_matrix;
	size_t is, is_n;
	bool v_added = false;
	while(fgets(line, 10000, file)) {
		row++;
		if(row >= first_row) {	
			stmp = line;
			remove_blank_ends(stmp);
			if(row == first_row) {
				if(stmp.empty()) {
					row--;
				} else {
					is = 0;
					while((is_n = stmp.find(delimiter, is)) != string::npos) {		
						columns++;
						if(headers) {
							str1 = stmp.substr(is, is_n - is);
							remove_blank_ends(str1);
							header.push_back(str1);
						}
						if(!to_matrix) {
							vectors.push_back(m_empty_vector);
						}
						is = is_n + delimiter.length();
					}
					if(headers) {
						str1 = stmp.substr(is, stmp.length() - is);
						remove_blank_ends(str1);
						header.push_back(str1);
					}
					if(to_matrix) {
						mstruct.resizeMatrix(1, columns, m_undefined);
					} else {
						vectors.push_back(m_empty_vector);
					}
				}
			}
			if((!headers || row > first_row) && !stmp.empty()) {
				if(to_matrix && v_added) {
					mstruct.addRow(m_undefined);
					rows++;
				}
				is = 0;
				column = 1;
				while(column <= columns) {
					is_n = stmp.find(delimiter, is);
					if(is_n == string::npos) {
						str1 = stmp.substr(is, stmp.length() - is);
					} else {
						str1 = stmp.substr(is, is_n - is);
						is = is_n + delimiter.length();
					}
					if(to_matrix) {
						CALCULATOR->parse(&mstruct[rows - 1][column - 1], str1);
					} else {
						vectors[column - 1].addChild(CALCULATOR->parse(str1));
					}
					column++;
					if(is_n == string::npos) {
						break;
					}
				}
				for(; column <= columns; column++) {
					if(!to_matrix) {
						vectors[column - 1].addChild(m_undefined);
					}				
				}
				v_added = true;
			}
		}
	}
	if(to_matrix) {
		addVariable(new KnownVariable(category, name, mstruct, title));
	} else {
		if(vectors.size() > 1) {
			if(!category.empty()) {
				category += "/";	
			}
			category += name;
		}
		for(size_t i = 0; i < vectors.size(); i++) {
			str1 = "";
			str2 = "";
			if(vectors.size() > 1) {
				str1 += name;
				str1 += "_";
				if(title.empty()) {
					str2 += name;
					str2 += " ";
				} else {
					str2 += title;
					str2 += " ";
				}		
				if(i < header.size()) {
					str1 += header[i];
					str2 += header[i];
				} else {
					str1 += _("column");
					str1 += "_";
					str1 += i2s(i + 1);
					str2 += _("Column ");
					str2 += i2s(i + 1);				
				}
				gsub(" ", "_", str1);				
			} else {
				str1 = name;
				str2 = title;
				if(i < header.size()) {
					str2 += " (";
					str2 += header[i];
					str2 += ")";
				}
			}
			addVariable(new KnownVariable(category, str1, vectors[i], str2));
		}
	}
	return true;
}
bool Calculator::exportCSV(const MathStructure &mstruct, const char *file_name, string delimiter) {
	FILE *file = fopen(file_name, "w+");
	if(file == NULL) {
		return false;
	}
	MathStructure mcsv(mstruct);
	PrintOptions po;
	po.number_fraction_format = FRACTION_DECIMAL;
	po.decimalpoint_sign = ".";
	po.comma_sign = ",";
	if(mcsv.isMatrix()) {
		for(size_t i = 0; i < mcsv.size(); i++) {
			for(size_t i2 = 0; i2 < mcsv[i].size(); i2++) {
				if(i2 > 0) fputs(delimiter.c_str(), file);
				mcsv[i][i2].format(po);
				fputs(mcsv[i][i2].print(po).c_str(), file);
			}
			fputs("\n", file);
		}
	} else if(mcsv.isVector()) {
		for(size_t i = 0; i < mcsv.size(); i++) {
			mcsv[i].format(po);
			fputs(mcsv[i].print(po).c_str(), file);
			fputs("\n", file);
		}
	} else {
		mcsv.format(po);
		fputs(mcsv.print(po).c_str(), file);
		fputs("\n", file);
	}
	fclose(file);
	return true;
}
int Calculator::testCondition(string expression) {
	MathStructure mstruct = calculate(expression);
	if(mstruct.isNumber()) {
		if(mstruct.number().isPositive()) {
			return 1;
		} else {
			return 0;
		}
	}
	return -1;
}
bool Calculator::loadExchangeRates() {
	xmlDocPtr doc;
	xmlNodePtr cur;
	xmlChar *value;
	string filename, currency, rate;
	string homedir = getLocalDir();
	filename = homedir;
	filename += "eurofxref-daily.xml";
	doc = xmlParseFile(filename.c_str());
	if(doc == NULL) {
		//fetchExchangeRates();
		doc = xmlParseFile(filename.c_str());
		if(doc == NULL) {
			return false;
		}
	}
	cur = xmlDocGetRootElement(doc);
	if(cur == NULL) {
		xmlFreeDoc(doc);
		return false;
	}
	Unit *u;
	while(cur) {
		if(!xmlStrcmp(cur->name, (const xmlChar*) "Cube")) {
			XML_GET_STRING_FROM_PROP(cur, "currency", currency);
			if(!currency.empty()) {
				XML_GET_STRING_FROM_PROP(cur, "rate", rate);
				if(!rate.empty()) {
					rate = "1/" + rate;
					u = getUnit(currency);
					if(!u) {
						addUnit(new AliasUnit(_("Currency"), currency, "", "", "", u_euro, rate, 1, "", false, true));
					} else if(u->subtype() == SUBTYPE_ALIAS_UNIT) {
						((AliasUnit*) u)->setExpression(rate);
					}
				}
			}
		}
		if(cur->children) {
			cur = cur->children;
		} else if(cur->next) {
			cur = cur->next;
		} else {
			cur = cur->parent;
			if(cur) {
				cur = cur->next;
			}
		}
	}
	xmlFreeDoc(doc);
	exchange_rates_warning_issued = false;
	return true;
}
bool Calculator::hasGnomeVFS() {
	if(has_gnomevfs >= 0) return has_gnomevfs > 0;
	gchar *gstr = g_find_program_in_path("gnomevfs-copy");
	if(gstr) {
		g_free(gstr);
		has_gnomevfs = 1;
		return true;
	}
	g_free(gstr);
	has_gnomevfs = 0;
	return has_gnomevfs > 0;
}
bool Calculator::canFetch() {
	if(hasGnomeVFS()) return true;
	gchar *gstr = g_find_program_in_path("wget");
	if(gstr) {
		g_free(gstr);
		return true;
	}
	return false;
	/*if(system("wget --version") == 0) {
		return true;
	}
	return false;*/
}
string Calculator::getExchangeRatesFileName() {
	string homedir = getLocalDir();
	mkdir(homedir.c_str(), S_IRWXU);
	return homedir + "eurofxref-daily.xml";	
}
string Calculator::getExchangeRatesUrl() {
	return "http://www.ecb.int/stats/eurofxref/eurofxref-daily.xml";
}
bool Calculator::fetchExchangeRates(int timeout, string wget_args) {
	int status = 0;
	string homedir = getLocalDir();
	mkdir(homedir.c_str(), S_IRWXU);	
	string cmdline;
	if(hasGnomeVFS()) {
		cmdline = "gnomevfs-copy http://www.ecb.int/stats/eurofxref/eurofxref-daily.xml";
		cmdline += " "; cmdline += homedir; cmdline += "eurofxref-daily.xml";
	} else {	
		cmdline = "wget";
		cmdline += " "; cmdline += "--timeout="; cmdline += i2s(timeout);
		cmdline += " "; cmdline += wget_args;
		cmdline += " "; cmdline +=  "--output-document="; cmdline += homedir; cmdline += "eurofxref-daily.xml";
		cmdline += " "; cmdline += "http://www.ecb.int/stats/eurofxref/eurofxref-daily.xml";
	}
	if(!g_spawn_command_line_sync(cmdline.c_str(), NULL, NULL, NULL, NULL)) status = -1;
	if(status != 0) error(true, _("Failed to download exchange rates from ECB."), NULL);
	return status == 0;	
}
bool Calculator::fetchExchangeRates(int timeout) {
	return fetchExchangeRates(timeout, "--quiet --tries=1");
}
bool Calculator::checkExchangeRatesDate() {
	if(exchange_rates_warning_issued) return true;
	string homedir = getLocalDir();
	homedir += "eurofxref-daily.xml";
	bool up_to_date = false;
	struct stat stats;
	if(stat(homedir.c_str(), &stats) == 0) {
		if(time(NULL) - stats.st_mtime <= 604800) {
			up_to_date = true;
		}
	}
	if(!up_to_date) {
		error(false, _("It has been more than one week since the exchange rates last were updated."), NULL);
		exchange_rates_warning_issued = true;
	}
	return up_to_date;
}

bool Calculator::canPlot() {
	/*FILE *pipe = popen("gnuplot -", "w");
	if(!pipe) {
		return false;
	}
	if(pclose(pipe) != 0) return false;
	pipe = popen("gnuplot -", "w");
	if(!pipe) {
		return false;
	}
	fputs("show version\n", pipe);
	return pclose(pipe) == 0;*/
	gchar *gstr = g_find_program_in_path("gnuplot");
	if(gstr) {
		g_free(gstr);
		return true;
	}
	return false;
}
MathStructure Calculator::expressionToPlotVector(string expression, const MathStructure &min, const MathStructure &max, int steps, MathStructure *x_vector, string x_var, const ParseOptions &po) {
	Variable *v = getActiveVariable(x_var);
	MathStructure x_mstruct;
	if(v) x_mstruct = v;
	else x_mstruct = x_var;
	EvaluationOptions eo;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	ParseOptions po2 = po;
	po2.read_precision = DONT_READ_PRECISION;	
	eo.parse_options = po2;
	MathStructure y_vector(parse(expression, po2).generateVector(x_mstruct, min, max, steps, x_vector, eo));
	if(y_vector.size() == 0) {
		CALCULATOR->error(true, _("Unable to generate plot data with current min, max and sampling rate."), NULL);
	}
	return y_vector;
}
MathStructure Calculator::expressionToPlotVector(string expression, float min, float max, int steps, MathStructure *x_vector, string x_var, const ParseOptions &po) {
	MathStructure min_mstruct(min), max_mstruct(max);
	EvaluationOptions eo;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	ParseOptions po2 = po;
	po2.read_precision = DONT_READ_PRECISION;	
	eo.parse_options = po2;
	MathStructure y_vector(expressionToPlotVector(expression, min_mstruct, max_mstruct, steps, x_vector, x_var, po2));
	y_vector.eval(eo);
	if(y_vector.size() == 0) {
		CALCULATOR->error(true, _("Unable to generate plot data with current min, max and sampling rate."), NULL);
	}
	return y_vector;
}
MathStructure Calculator::expressionToPlotVector(string expression, const MathStructure &min, const MathStructure &max, const MathStructure &step, MathStructure *x_vector, string x_var, const ParseOptions &po) {
	Variable *v = getActiveVariable(x_var);
	MathStructure x_mstruct;
	if(v) x_mstruct = v;
	else x_mstruct = x_var;
	EvaluationOptions eo;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	ParseOptions po2 = po;
	po2.read_precision = DONT_READ_PRECISION;	
	eo.parse_options = po2;
	MathStructure y_vector(parse(expression, po2).generateVector(x_mstruct, min, max, step, x_vector, eo));
	if(y_vector.size() == 0) {
		CALCULATOR->error(true, _("Unable to generate plot data with current min, max and step size."), NULL);
	}
	return y_vector;
}
MathStructure Calculator::expressionToPlotVector(string expression, float min, float max, float step, MathStructure *x_vector, string x_var, const ParseOptions &po) {
	MathStructure min_mstruct(min), max_mstruct(max), step_mstruct(step);
	EvaluationOptions eo;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	ParseOptions po2 = po;
	po2.read_precision = DONT_READ_PRECISION;	
	eo.parse_options = po2;
	MathStructure y_vector(expressionToPlotVector(expression, min_mstruct, max_mstruct, step_mstruct, x_vector, x_var, po2));
	y_vector.eval(eo);
	if(y_vector.size() == 0) {
		CALCULATOR->error(true, _("Unable to generate plot data with current min, max and step size."), NULL);
	}
	return y_vector;
}
MathStructure Calculator::expressionToPlotVector(string expression, const MathStructure &x_vector, string x_var, const ParseOptions &po) {
	Variable *v = getActiveVariable(x_var);
	MathStructure x_mstruct;
	if(v) x_mstruct = v;
	else x_mstruct = x_var;
	EvaluationOptions eo;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	ParseOptions po2 = po;
	po2.read_precision = DONT_READ_PRECISION;	
	eo.parse_options = po2;
	return parse(expression, po2).generateVector(x_mstruct, x_vector, eo).eval(eo);
}

bool Calculator::plotVectors(PlotParameters *param, const vector<MathStructure> &y_vectors, const vector<MathStructure> &x_vectors, vector<PlotDataParameters*> &pdps, bool persistent) {

	string filename;
	string homedir = getLocalDir();
	mkdir(homedir.c_str(), S_IRWXU);	
	homedir += "tmp/";	
	mkdir(homedir.c_str(), S_IRWXU);

	string commandline_extra;
	string title;

	if(!param) {
		PlotParameters pp;
		param = &pp;
	}
	
	string plot;
	
	if(param->filename.empty()) {
		if(!param->color) {
			commandline_extra += " -mono";
		}
		if(param->font.empty()) {
			commandline_extra += " -font \"-*-helvetica-bold-r-*-*-14-*-*-*-*-*-*-*\"";
		}
		plot += "set terminal x11\n";
	} else {
		persistent = true;
		if(param->filetype == PLOT_FILETYPE_AUTO) {
			size_t i = param->filename.find(".");
			if(i == string::npos) {
				param->filetype = PLOT_FILETYPE_PNG;
				error(false, _("No extension in file name. Saving as PNG image."), NULL);
			} else {
				string ext = param->filename.substr(i + 1, param->filename.length() - (i + 1));
				if(ext == "png") {
					param->filetype = PLOT_FILETYPE_PNG;
				} else if(ext == "ps") {
					param->filetype = PLOT_FILETYPE_PS;
				} else if(ext == "eps") {
					param->filetype = PLOT_FILETYPE_EPS;
				} else if(ext == "svg") {
					param->filetype = PLOT_FILETYPE_SVG;
				} else if(ext == "fig") {
					param->filetype = PLOT_FILETYPE_FIG;
				} else if(ext == "tex") {
					param->filetype = PLOT_FILETYPE_LATEX;
				} else {
					param->filetype = PLOT_FILETYPE_PNG;
					error(false, _("Unknown extension in file name. Saving as PNG image."), NULL);
				}
			}
		}
		plot += "set terminal ";
		switch(param->filetype) {
			case PLOT_FILETYPE_FIG: {
				plot += "fig ";
				if(param->color) {
					plot += "color";
				} else {
					plot += "monochrome";
				}
				break;
			}
			case PLOT_FILETYPE_SVG: {
				plot += "svg";
				break;
			}
			case PLOT_FILETYPE_LATEX: {
				plot += "latex ";
				break;
			}
			case PLOT_FILETYPE_PS: {
				plot += "postscript ";
				if(param->color) {
					plot += "color";
				} else {
					plot += "monochrome";
				}
				plot += " \"Times\"";
				break;
			}
			case PLOT_FILETYPE_EPS: {
				plot += "postscript eps ";
				if(param->color) {
					plot += "color";
				} else {
					plot += "monochrome";
				}
				plot += " \"Times\"";
				break;
			}
			default: {
				plot += "png ";
				if(param->color) {
					plot += "color";
				} else {
					plot += "monochrome";
				}
				break;
			}

		}
		plot += "\nset output \"";
		plot += param->filename;
		plot += "\"\n";
	}

	switch(param->legend_placement) {
		case PLOT_LEGEND_NONE: {plot += "set nokey\n"; break;}
		case PLOT_LEGEND_TOP_LEFT: {plot += "set key top left\n"; break;}
		case PLOT_LEGEND_TOP_RIGHT: {plot += "set key top right\n"; break;}
		case PLOT_LEGEND_BOTTOM_LEFT: {plot += "set key bottom left\n"; break;}
		case PLOT_LEGEND_BOTTOM_RIGHT: {plot += "set key bottom right\n"; break;}
		case PLOT_LEGEND_BELOW: {plot += "set key below\n"; break;}
		case PLOT_LEGEND_OUTSIDE: {plot += "set key outside\n"; break;}
	}
	if(!param->x_label.empty()) {
		title = param->x_label;
		gsub("\"", "\\\"", title);
		plot += "set xlabel \"";
		plot += title;
		plot += "\"\n";	
	}
	if(!param->y_label.empty()) {
		string title = param->y_label;
		gsub("\"", "\\\"", title);
		plot += "set ylabel \"";
		plot += title;
		plot += "\"\n";	
	}
	if(!param->title.empty()) {
		title = param->title;
		gsub("\"", "\\\"", title);
		plot += "set title \"";
		plot += title;
		plot += "\"\n";	
	}
	if(param->grid) {
		plot += "set grid\n";
	}
	if(param->y_log) {
		plot += "set logscale y ";
		plot += i2s(param->y_log_base);
		plot += "\n";
	}
	if(param->x_log) {
		plot += "set logscale x ";
		plot += i2s(param->x_log_base);
		plot += "\n";
	}
	if(param->show_all_borders) {
		plot += "set border 15\n";
	} else {
		bool xaxis2 = false, yaxis2 = false;
		for(size_t i = 0; i < pdps.size(); i++) {
			if(pdps[i] && pdps[i]->xaxis2) {
				xaxis2 = true;
			}
			if(pdps[i] && pdps[i]->yaxis2) {
				yaxis2 = true;
			}
		}
		if(xaxis2 && yaxis2) {
			plot += "set border 15\nset x2tics\nset y2tics\n";
		} else if(xaxis2) {
			plot += "set border 7\nset x2tics\n";
		} else if(yaxis2) {
			plot += "set border 11\nset y2tics\n";
		} else {
			plot += "set border 3\n";
		}
		plot += "set xtics nomirror\nset ytics nomirror\n";
	}
	plot += "plot ";
	for(size_t i = 0; i < y_vectors.size(); i++) {
		if(!y_vectors[i].isUndefined()) {
			if(i != 0) {
				plot += ",";
			}
			plot += "\"";
			plot += homedir;
			plot += "gnuplot_data";
			plot += i2s(i + 1);
			plot += "\"";
			if(i < pdps.size()) {
				switch(pdps[i]->smoothing) {
					case PLOT_SMOOTHING_UNIQUE: {plot += " smooth unique"; break;}
					case PLOT_SMOOTHING_CSPLINES: {plot += " smooth csplines"; break;}
					case PLOT_SMOOTHING_BEZIER: {plot += " smooth bezier"; break;}
					case PLOT_SMOOTHING_SBEZIER: {plot += " smooth sbezier"; break;}
					default: {}
				}
				if(pdps[i]->xaxis2 && pdps[i]->yaxis2) {
					plot += " axis x2y2";
				} else if(pdps[i]->xaxis2) {
					plot += " axis x2y1";
				} else if(pdps[i]->yaxis2) {
					plot += " axis x1y2";
				}
				if(!pdps[i]->title.empty()) {
					title = pdps[i]->title;
					gsub("\"", "\\\"", title);
					plot += " title \"";
					plot += title;
					plot += "\"";
				}
				switch(pdps[i]->style) {
					case PLOT_STYLE_LINES: {plot += " with lines"; break;}
					case PLOT_STYLE_POINTS: {plot += " with points"; break;}
					case PLOT_STYLE_POINTS_LINES: {plot += " with linespoints"; break;}
					case PLOT_STYLE_BOXES: {plot += " with boxes"; break;}
					case PLOT_STYLE_HISTOGRAM: {plot += " with histeps"; break;}
					case PLOT_STYLE_STEPS: {plot += " with steps"; break;}
					case PLOT_STYLE_CANDLESTICKS: {plot += " with candlesticks"; break;}
					case PLOT_STYLE_DOTS: {plot += " with dots"; break;}
				}
				if(param->linewidth < 1) {
					plot += " lw 2";
				} else {
					plot += " lw ";
					plot += i2s(param->linewidth);
				}
			}
		}
	}
	plot += "\n";
	
	string filename_data;
	string plot_data;
	PrintOptions po;
	po.number_fraction_format = FRACTION_DECIMAL;
	po.decimalpoint_sign = ".";
	po.comma_sign = ",";
	for(size_t serie = 0; serie < y_vectors.size(); serie++) {
		if(!y_vectors[serie].isUndefined()) {
			filename_data = homedir;
			filename_data += "gnuplot_data";
			filename_data += i2s(serie + 1);
			FILE *fdata = fopen(filename_data.c_str(), "w+");
			if(!fdata) {
				error(true, _("Could not create temporary file %s"), filename_data.c_str(), NULL);
				return false;
			}
			plot_data = "";
			int non_numerical = 0, non_real = 0;
			string str = "";
			for(size_t i = 1; i <= y_vectors[serie].countChildren(); i++) {
				if(serie < x_vectors.size() && !x_vectors[serie].isUndefined() && x_vectors[serie].countChildren() == y_vectors[serie].countChildren()) {
					if(!x_vectors[serie].getChild(i)->isNumber()) {
						non_numerical++;
						if(non_numerical == 1) str = x_vectors[serie].getChild(i)->print(po);
					} else if(!x_vectors[serie].getChild(i)->number().isReal()) {
						non_real++;
						if(non_numerical + non_real == 1) str = x_vectors[serie].getChild(i)->print(po);
					}
					plot_data += x_vectors[serie].getChild(i)->print(po);
					plot_data += " ";
				}
				if(!y_vectors[serie].getChild(i)->isNumber()) {
					non_numerical++;
					if(non_numerical == 1) str = y_vectors[serie].getChild(i)->print(po);
				} else if(!y_vectors[serie].getChild(i)->number().isReal()) {
					non_real++;
					if(non_numerical + non_real == 1) str = y_vectors[serie].getChild(i)->print(po);
				}
				plot_data += y_vectors[serie].getChild(i)->print(po);
				plot_data += "\n";	
			}
			if(non_numerical > 0 || non_real > 0) {
				string stitle;
				if(serie < pdps.size() && !pdps[serie]->title.empty()) {
					stitle = pdps[serie]->title.c_str();
				} else {
					stitle = i2s(serie).c_str();
				}
				if(non_numerical > 0) {
					error(true, _("Series %s contains non-numerical data (\"%s\" first of %s) which can not be properly plotted."), stitle.c_str(), str.c_str(), i2s(non_numerical).c_str(), NULL);
				} else {
					error(true, _("Series %s contains non-real data (\"%s\" first of %s) which can not be properly plotted."), stitle.c_str(), str.c_str(), i2s(non_real).c_str(), NULL);
				}
			}
			fputs(plot_data.c_str(), fdata);
			fflush(fdata);
			fclose(fdata);
		}
	}
	
	return invokeGnuplot(plot, commandline_extra, persistent);
}
bool Calculator::invokeGnuplot(string commands, string commandline_extra, bool persistent) {
	FILE *pipe = NULL;
	if(!b_gnuplot_open || !gnuplot_pipe || persistent || commandline_extra != gnuplot_cmdline) {
		if(!persistent) {
			closeGnuplot();
		}
		string commandline = "gnuplot";
		if(persistent) {
			commandline += " -persist";
		}
		commandline += commandline_extra;
		commandline += " -";
		pipe = popen(commandline.c_str(), "w");
		if(!pipe) {
			error(true, _("Failed to invoke gnuplot. Make sure that you have gnuplot installed in your path."), NULL);
			return false;
		}
		if(!persistent && pipe) {
			gnuplot_pipe = pipe;
			b_gnuplot_open = true;
			gnuplot_cmdline = commandline_extra;
		}
	} else {
		pipe = gnuplot_pipe;
	}
	if(!pipe) {
		return false;
	}
	if(!persistent) {
		fputs("clear\n", pipe);
		fputs("reset\n", pipe);
	}
	fputs(commands.c_str(), pipe);
	fflush(pipe);
	if(persistent) {
		return pclose(pipe) == 0;
	}
	return true;
}
bool Calculator::closeGnuplot() {
	if(gnuplot_pipe) {
		int rv = pclose(gnuplot_pipe);
		gnuplot_pipe = NULL;
		b_gnuplot_open = false;
		return rv == 0;
	}
	gnuplot_pipe = NULL;
	b_gnuplot_open = false;
	return true;
}
bool Calculator::gnuplotOpen() {
	return b_gnuplot_open && gnuplot_pipe;
}

