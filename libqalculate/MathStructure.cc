/*
    Qalculate    

    Copyright (C) 2004  Niklas Knutsson (nq@altern.org)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "support.h"

#include "MathStructure.h"
#include "Calculator.h"
#include "Number.h"
#include "Function.h"
#include "Variable.h"
#include "Unit.h"
#include "Prefix.h"

#define SWAP_CHILDS(i1, i2)		MathStructure *swap_mstruct = v_subs[v_order[i1]]; v_subs[v_order[i1]] = v_subs[v_order[i2]]; v_subs[v_order[i2]] = swap_mstruct;
#define SET_CHILD_MAP(i)		setToChild(i + 1, true);
#define SET_MAP(o)			set(o, true);
#define SET_MAP_NOCOPY(o)		set_nocopy(o, true);
#define MERGE_APPROX_AND_PREC(o)	if(!b_approx && o.isApproximate()) b_approx = true; if(o.precision() > 0 && (i_precision < 1 || o.precision() < i_precision)) i_precision = o.precision();
#define CHILD_UPDATED(i)		if(!b_approx && CHILD(i).isApproximate()) b_approx = true; if(CHILD(i).precision() > 0 && (i_precision < 1 || CHILD(i).precision() < i_precision)) i_precision = CHILD(i).precision();
#define CHILDREN_UPDATED		for(size_t child_i = 0; child_i < SIZE; child_i++) {if(!b_approx && CHILD(child_i).isApproximate()) b_approx = true; if(CHILD(child_i).precision() > 0 && (i_precision < 1 || CHILD(child_i).precision() < i_precision)) i_precision = CHILD(child_i).precision();}

#define APPEND(o)		v_order.push_back(v_subs.size()); v_subs.push_back(new MathStructure(o)); if(!b_approx && o.isApproximate()) b_approx = true; if(o.precision() > 0 && (i_precision < 1 || o.precision() < i_precision)) i_precision = o.precision();
#define APPEND_NEW(o)		{v_order.push_back(v_subs.size()); MathStructure *m_append_new = new MathStructure(o); v_subs.push_back(m_append_new); if(!b_approx && m_append_new->isApproximate()) b_approx = true; if(m_append_new->precision() > 0 && (i_precision < 1 || m_append_new->precision() < i_precision)) i_precision = m_append_new->precision();}
#define APPEND_COPY(o)		v_order.push_back(v_subs.size()); v_subs.push_back(new MathStructure(*(o))); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define APPEND_POINTER(o)	v_order.push_back(v_subs.size()); v_subs.push_back(o); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define APPEND_REF(o)		v_order.push_back(v_subs.size()); v_subs.push_back(o); (o)->ref(); if(!b_approx && (o)->isApproximate()) b_approx = true; if((o)->precision() > 0 && (i_precision < 1 || (o)->precision() < i_precision)) i_precision = (o)->precision();
#define PREPEND(o)		v_order.insert(v_order.begin(), v_subs.size()); v_subs.push_back(new MathStructure(o)); if(!b_approx && o.isApproximate()) b_approx = true; if(o.precision() > 0 && (i_precision < 1 || o.precision() < i_precision)) i_precision = o.precision();
#define CLEAR			v_order.clear(); for(size_t i = 0; i < v_subs.size(); i++) {v_subs[i]->unref();} v_subs.clear();
#define REDUCE(v_size)		for(size_t v_index = v_size; v_index < v_order.size(); v_index++) {v_subs[v_order[v_index]]->unref(); v_subs.erase(v_subs.begin() + v_order[v_index]);} v_order.resize(v_size);
#define CHILD(v_index)		(*v_subs[v_order[v_index]])
#define SIZE			v_order.size()
#define LAST			(*v_subs[v_order[v_order.size() - 1]])
#define ERASE(v_index)		v_subs[v_order[v_index]]->unref(); v_subs.erase(v_subs.begin() + v_order[v_index]); for(size_t v_index2 = 0; v_index2 < v_order.size(); v_index2++) {if(v_order[v_index2] > v_order[v_index]) v_order[v_index2]--;} v_order.erase(v_order.begin() + v_index);


void MathStructure::mergePrecision(const MathStructure &o) {MERGE_APPROX_AND_PREC(o)}

void idm1(const MathStructure &mnum, bool &bfrac, bool &bint);
void idm2(const MathStructure &mnum, bool &bfrac, bool &bint, Number &nr);
void idm3(MathStructure &mnum, Number &nr);

void MathStructure::ref() {
	i_ref++;
}
void MathStructure::unref() {
	i_ref--;
	if(i_ref == 0) {
		delete this;
	}
}
size_t MathStructure::refcount() const {
	return i_ref;
}


inline void MathStructure::init() {
	m_type = STRUCT_NUMBER;
	b_approx = false;
	i_precision = -1;
	i_ref = 1;
	function_value = NULL;
}

MathStructure::MathStructure() {
	init();
}
MathStructure::MathStructure(const MathStructure &o) {
	init();
	set(o);
}
MathStructure::MathStructure(int num, int den, int exp10) {
	init();
	set(num, den, exp10);
}
MathStructure::MathStructure(string sym) {
	init();
	set(sym);
}
MathStructure::MathStructure(double float_value) {
	init();
	set(float_value);
}
MathStructure::MathStructure(const MathStructure *o, ...) {
	init();
	clear();
	va_list ap;
	va_start(ap, o); 
	if(o) {
		APPEND_COPY(o)
		while(true) {
			o = va_arg(ap, const MathStructure*);
			if(!o) break;
			APPEND_COPY(o)
		}
	}
	va_end(ap);	
	m_type = STRUCT_VECTOR;
}
MathStructure::MathStructure(MathFunction *o, ...) {
	init();
	clear();
	va_list ap;
	va_start(ap, o); 
	o_function = o;
	while(true) {
		const MathStructure *mstruct = va_arg(ap, const MathStructure*);
		if(!mstruct) break;
		APPEND_COPY(mstruct)
	}
	va_end(ap);	
	m_type = STRUCT_FUNCTION;
}
MathStructure::MathStructure(Unit *u, Prefix *p) {
	init();
	set(u, p);
}
MathStructure::MathStructure(Variable *o) {
	init();
	set(o);
}
MathStructure::MathStructure(const Number &o) {
	init();
	set(o);
}
MathStructure::~MathStructure() {
	clear();
}

void MathStructure::set(const MathStructure &o, bool merge_precision) {
	clear(merge_precision);
	switch(o.type()) {
		case STRUCT_NUMBER: {
			o_number.set(o.number());
			break;
		}
		case STRUCT_SYMBOLIC: {
			s_sym = o.symbol();
			break;
		}
		case STRUCT_FUNCTION: {
			o_function = o.function();
			if(o.functionValue()) function_value = new MathStructure(*o.functionValue());
			break;
		}
		case STRUCT_VARIABLE: {
			o_variable = o.variable();
			break;
		}
		case STRUCT_UNIT: {
			o_unit = o.unit();
			o_prefix = o.prefix();
			b_plural = o.isPlural();
			break;
		}
		case STRUCT_COMPARISON: {
			ct_comp = o.comparisonType();
			break;
		}
	}
	for(size_t i = 0; i < o.size(); i++) {
		APPEND_COPY((&o[i]))
	}
	if(merge_precision) {
		MERGE_APPROX_AND_PREC(o);
	} else {
		b_approx = o.isApproximate();
		i_precision = o.precision();
	}
	m_type = o.type();
}
void MathStructure::set_nocopy(MathStructure &o, bool merge_precision) {
	o.ref();
	clear(merge_precision);
	switch(o.type()) {
		case STRUCT_NUMBER: {
			o_number.set(o.number());
			break;
		}
		case STRUCT_SYMBOLIC: {
			s_sym = o.symbol();
			break;
		}
		case STRUCT_FUNCTION: {
			o_function = o.function();
			if(o.functionValue()) function_value = new MathStructure(*o.functionValue());
			break;
		}
		case STRUCT_VARIABLE: {
			o_variable = o.variable();
			break;
		}
		case STRUCT_UNIT: {
			o_unit = o.unit();
			o_prefix = o.prefix();
			b_plural = o.isPlural();
			break;
		}
		case STRUCT_COMPARISON: {
			ct_comp = o.comparisonType();
			break;
		}
	}
	for(size_t i = 0; i < o.size(); i++) {
		APPEND_REF((&o[i]))
	}
	if(merge_precision) {
		MERGE_APPROX_AND_PREC(o);
	} else {
		b_approx = o.isApproximate();
		i_precision = o.precision();
	}
	m_type = o.type();
	o.unref();
}
void MathStructure::setToChild(size_t index, bool preserve_precision) {
	if(index > 0 && index <= SIZE) {
		set_nocopy(CHILD(index - 1), preserve_precision);
	}
}
void MathStructure::set(int num, int den, int exp10, bool preserve_precision) {
	clear(preserve_precision);
	o_number.set(num, den, exp10);
	if(preserve_precision) {
		MERGE_APPROX_AND_PREC(o_number);
	} else {
		b_approx = o_number.isApproximate();
		i_precision = o_number.precision();
	}
	m_type = STRUCT_NUMBER;
}
void MathStructure::set(double float_value, bool preserve_precision) {
	clear(preserve_precision);
	o_number.setFloat(float_value);
	if(preserve_precision) {
		MERGE_APPROX_AND_PREC(o_number);
	} else {
		b_approx = o_number.isApproximate();
		i_precision = o_number.precision();
	}
	m_type = STRUCT_NUMBER;
}
void MathStructure::set(string sym, bool preserve_precision) {
	clear(preserve_precision);
	s_sym = sym;
	m_type = STRUCT_SYMBOLIC;
}
void MathStructure::setVector(const MathStructure *o, ...) {
	clear();
	va_list ap;
	va_start(ap, o); 
	if(o) {
		APPEND_COPY(o)
		while(true) {
			o = va_arg(ap, const MathStructure*);
			if(!o) break;
			APPEND_COPY(o)
		}
	}
	va_end(ap);	
	m_type = STRUCT_VECTOR;
}
void MathStructure::set(MathFunction *o, ...) {
	clear();
	va_list ap;
	va_start(ap, o); 
	o_function = o;
	while(true) {
		const MathStructure *mstruct = va_arg(ap, const MathStructure*);
		if(!mstruct) break;
		APPEND_COPY(mstruct)
	}
	va_end(ap);	
	m_type = STRUCT_FUNCTION;
}
void MathStructure::set(Unit *u, Prefix *p, bool preserve_precision) {
	clear(preserve_precision);
	o_unit = u;
	o_prefix = p;
	m_type = STRUCT_UNIT;
}
void MathStructure::set(Variable *o, bool preserve_precision) {
	clear(preserve_precision);
	o_variable = o;
	m_type = STRUCT_VARIABLE;
}
void MathStructure::set(const Number &o, bool preserve_precision) {
	clear(preserve_precision);
	o_number.set(o);
	if(preserve_precision) {
		MERGE_APPROX_AND_PREC(o_number);
	} else {
		b_approx = o_number.isApproximate();
		i_precision = o_number.precision();
	}
	m_type = STRUCT_NUMBER;
}
void MathStructure::setInfinity(bool preserve_precision) {
	clear(preserve_precision);
	o_number.setInfinity();
	m_type = STRUCT_NUMBER;
}
void MathStructure::setUndefined(bool preserve_precision) {
	clear(preserve_precision);
	m_type = STRUCT_UNDEFINED;
}

void MathStructure::operator = (const MathStructure &o) {set(o);}
void MathStructure::operator = (const Number &o) {set(o);}
void MathStructure::operator = (int i) {set(i, 1);}
void MathStructure::operator = (Unit *u) {set(u);}
void MathStructure::operator = (Variable *v) {set(v);}
void MathStructure::operator = (string sym) {set(sym);}
MathStructure MathStructure::operator - () const {
	MathStructure o2(*this);
	o2.negate();
	return o2;
}
MathStructure MathStructure::operator * (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.multiply(o);
	return o2;
}
MathStructure MathStructure::operator / (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.divide(o);
	return o2;
}
MathStructure MathStructure::operator + (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.add(o);
	return o;
}
MathStructure MathStructure::operator - (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.subtract(o);
	return o2;
}
MathStructure MathStructure::operator ^ (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.raise(o);
	return o2;
}
MathStructure MathStructure::operator && (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.add(o, OPERATION_LOGICAL_AND);
	return o2;
}
MathStructure MathStructure::operator || (const MathStructure &o) const {
	MathStructure o2(*this);
	o2.add(o, OPERATION_LOGICAL_OR);
	return o2;
}
MathStructure MathStructure::operator ! () const {
	MathStructure o2(*this);
	o2.setLogicalNot();
	return o2;
}

void MathStructure::operator *= (const MathStructure &o) {multiply(o);}
void MathStructure::operator /= (const MathStructure &o) {divide(o);}
void MathStructure::operator += (const MathStructure &o) {add(o);}
void MathStructure::operator -= (const MathStructure &o) {subtract(o);}
void MathStructure::operator ^= (const MathStructure &o) {raise(o);}

void MathStructure::operator *= (const Number &o) {multiply(o);}
void MathStructure::operator /= (const Number &o) {divide(o);}
void MathStructure::operator += (const Number &o) {add(o);}
void MathStructure::operator -= (const Number &o) {subtract(o);}
void MathStructure::operator ^= (const Number &o) {raise(o);}
		
void MathStructure::operator *= (int i) {multiply(i);}
void MathStructure::operator /= (int i) {divide(i);}
void MathStructure::operator += (int i) {add(i);}
void MathStructure::operator -= (int i) {subtract(i);}
void MathStructure::operator ^= (int i) {raise(i);}
		
void MathStructure::operator *= (Unit *u) {multiply(u);}
void MathStructure::operator /= (Unit *u) {divide(u);}
void MathStructure::operator += (Unit *u) {add(u);}
void MathStructure::operator -= (Unit *u) {subtract(u);}
void MathStructure::operator ^= (Unit *u) {raise(u);}
		
void MathStructure::operator *= (Variable *v) {multiply(v);}
void MathStructure::operator /= (Variable *v) {divide(v);}
void MathStructure::operator += (Variable *v) {add(v);}
void MathStructure::operator -= (Variable *v) {subtract(v);}
void MathStructure::operator ^= (Variable *v) {raise(v);}
		
void MathStructure::operator *= (string sym) {multiply(sym);}
void MathStructure::operator /= (string sym) {divide(sym);}
void MathStructure::operator += (string sym) {add(sym);}
void MathStructure::operator -= (string sym) {subtract(sym);}
void MathStructure::operator ^= (string sym) {raise(sym);}

bool MathStructure::operator == (const MathStructure &o) const {return equals(o);}
bool MathStructure::operator == (const Number &o) const {return equals(o);}
bool MathStructure::operator == (int i) const {return equals(i);}
bool MathStructure::operator == (Unit *u) const {return equals(u);}
bool MathStructure::operator == (Variable *v) const {return equals(v);}
bool MathStructure::operator == (string sym) const {return equals(sym);}

bool MathStructure::operator != (const MathStructure &o) const {return !equals(o);}

const MathStructure &MathStructure::operator [] (size_t index) const {return CHILD(index);}
MathStructure &MathStructure::operator [] (size_t index) {return CHILD(index);}

void MathStructure::clear(bool preserve_precision) {
	m_type = STRUCT_NUMBER;
	o_number.clear();
	if(function_value) {
		function_value->unref();
		function_value = NULL;
	}
	o_function = NULL;
	o_variable = NULL;
	o_unit = NULL;
	o_prefix = NULL;
	b_plural = false;
	CLEAR;
	if(!preserve_precision) {
		i_precision = -1;
		b_approx = false;
	}
}
void MathStructure::clearVector(bool preserve_precision) {
	clear(preserve_precision);
	m_type = STRUCT_VECTOR;
}
void MathStructure::clearMatrix(bool preserve_precision) {
	clearVector(preserve_precision);
	MathStructure *mstruct = new MathStructure();
	mstruct->clearVector();
	APPEND_POINTER(mstruct);
}

const MathStructure *MathStructure::functionValue() const {
	return function_value;
}

const Number &MathStructure::number() const {
	return o_number;
}
Number &MathStructure::number() {
	return o_number;
}
void MathStructure::numberUpdated() {
	if(m_type != STRUCT_NUMBER) return;
	MERGE_APPROX_AND_PREC(o_number)
}
void MathStructure::childUpdated(size_t index, bool recursive) {
	if(index > SIZE || index < 1) return;
	if(recursive) CHILD(index - 1).childrenUpdated(true);
	MERGE_APPROX_AND_PREC(CHILD(index - 1))
}
void MathStructure::childrenUpdated(bool recursive) {
	for(size_t i = 0; i < SIZE; i++) {
		if(recursive) CHILD(i).childrenUpdated(true);
		MERGE_APPROX_AND_PREC(CHILD(i))
	}
}
const string &MathStructure::symbol() const {
	return s_sym;
}
ComparisonType MathStructure::comparisonType() const {
	return ct_comp;
}
void MathStructure::setComparisonType(ComparisonType comparison_type) {
	ct_comp = comparison_type;
}
void MathStructure::setType(int mtype) {
	m_type = mtype;
}
Unit *MathStructure::unit() const {
	return o_unit;
}
Prefix *MathStructure::prefix() const {
	return o_prefix;
}
void MathStructure::setPrefix(Prefix *p) {
	if(isUnit()) o_prefix = p;
}
bool MathStructure::isPlural() const {
	return b_plural;
}
void MathStructure::setPlural(bool is_plural) {
	if(isUnit()) b_plural = is_plural;
}
void MathStructure::setFunction(MathFunction *f) {
	o_function = f;
}
void MathStructure::setUnit(Unit *u) {
	o_unit = u;
}
void MathStructure::setVariable(Variable *v) {
	o_variable = v;
}
MathFunction *MathStructure::function() const {
	return o_function;
}
Variable *MathStructure::variable() const {
	return o_variable;
}

bool MathStructure::isAddition() const {return m_type == STRUCT_ADDITION;}
bool MathStructure::isMultiplication() const {return m_type == STRUCT_MULTIPLICATION;}
bool MathStructure::isPower() const {return m_type == STRUCT_POWER;}
bool MathStructure::isSymbolic() const {return m_type == STRUCT_SYMBOLIC;}
bool MathStructure::isEmptySymbol() const {return m_type == STRUCT_SYMBOLIC && s_sym.empty();}
bool MathStructure::isVector() const {return m_type == STRUCT_VECTOR;}
bool MathStructure::isMatrix() const {
	if(m_type != STRUCT_VECTOR || SIZE < 1) return false;
	for(size_t i = 0; i < SIZE; i++) {
		if(!CHILD(i).isVector() || (i > 0 && CHILD(i).size() != CHILD(i - 1).size())) return false;
	}
	return true;
}
bool MathStructure::isFunction() const {return m_type == STRUCT_FUNCTION;}
bool MathStructure::isUnit() const {return m_type == STRUCT_UNIT;}
bool MathStructure::isUnit_exp() const {return m_type == STRUCT_UNIT || (m_type == STRUCT_POWER && CHILD(0).isUnit());}
bool MathStructure::isNumber_exp() const {return m_type == STRUCT_NUMBER || (m_type == STRUCT_POWER && CHILD(0).isNumber());}
bool MathStructure::isVariable() const {return m_type == STRUCT_VARIABLE;}
bool MathStructure::isComparison() const {return m_type == STRUCT_COMPARISON;}
bool MathStructure::isLogicalAnd() const {return m_type == STRUCT_LOGICAL_AND;}
bool MathStructure::isLogicalOr() const {return m_type == STRUCT_LOGICAL_OR;}
bool MathStructure::isLogicalXor() const {return m_type == STRUCT_LOGICAL_XOR;}
bool MathStructure::isLogicalNot() const {return m_type == STRUCT_LOGICAL_NOT;}
bool MathStructure::isBitwiseAnd() const {return m_type == STRUCT_BITWISE_AND;}
bool MathStructure::isBitwiseOr() const {return m_type == STRUCT_BITWISE_OR;}
bool MathStructure::isBitwiseXor() const {return m_type == STRUCT_BITWISE_XOR;}
bool MathStructure::isBitwiseNot() const {return m_type == STRUCT_BITWISE_NOT;}
bool MathStructure::isInverse() const {return m_type == STRUCT_INVERSE;}
bool MathStructure::isDivision() const {return m_type == STRUCT_DIVISION;}
bool MathStructure::isNegate() const {return m_type == STRUCT_NEGATE;}
bool MathStructure::isInfinity() const {return m_type == STRUCT_NUMBER && o_number.isInfinite();}
bool MathStructure::isUndefined() const {return m_type == STRUCT_UNDEFINED || (m_type == STRUCT_NUMBER && o_number.isUndefined());}
bool MathStructure::isInteger() const {return m_type == STRUCT_NUMBER && o_number.isInteger();}
bool MathStructure::isNumber() const {return m_type == STRUCT_NUMBER;}
bool MathStructure::isZero() const {return m_type == STRUCT_NUMBER && o_number.isZero();}
bool MathStructure::isOne() const {return m_type == STRUCT_NUMBER && o_number.isOne();}
bool MathStructure::isMinusOne() const {return m_type == STRUCT_NUMBER && o_number.isMinusOne();}

bool MathStructure::hasNegativeSign() const {
	return (m_type == STRUCT_NUMBER && o_number.hasNegativeSign()) || m_type == STRUCT_NEGATE || (m_type == STRUCT_MULTIPLICATION && SIZE > 0 && CHILD(0).hasNegativeSign());
}

bool MathStructure::representsNumber(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return true;}
		case STRUCT_VARIABLE: {return o_variable->representsNumber(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNumber();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNumber(allow_units)) || o_function->representsNumber(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {}
		case STRUCT_POWER: {}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNumber(allow_units)) return false;
			}
			return true;
		}
		default: {return false;}
	}
}
bool MathStructure::representsInteger(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isInteger();}
		case STRUCT_VARIABLE: {return o_variable->representsInteger(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isInteger();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsInteger(allow_units)) || o_function->representsInteger(*this, allow_units);}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsInteger(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_POWER: {
			return CHILD(0).representsInteger(allow_units) && CHILD(1).representsInteger(false) && CHILD(1).representsPositive(false);
		}
		default: {return false;}
	}
}
bool MathStructure::representsPositive(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isPositive();}
		case STRUCT_VARIABLE: {return o_variable->representsPositive(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isPositive();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsPositive(allow_units)) || o_function->representsPositive(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsPositive(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = true;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsPositive(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return (CHILD(0).representsPositive(allow_units) && CHILD(1).representsReal(false)) || (CHILD(0).representsNegative(allow_units) && CHILD(1).representsEven(false) && CHILD(1).representsInteger(false) && CHILD(1).representsPositive(false));
		}
		default: {return false;}
	}
}
bool MathStructure::representsNegative(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isNegative();}
		case STRUCT_VARIABLE: {return o_variable->representsNegative(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNegative();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNegative(allow_units)) || o_function->representsNegative(*this, allow_units);}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNegative(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsPositive(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return CHILD(1).representsInteger(false) && CHILD(1).representsPositive(false) && CHILD(1).representsOdd(false) && CHILD(0).representsNegative(allow_units);
		}
		default: {return false;}
	}
}
bool MathStructure::representsNonNegative(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isNonNegative();}
		case STRUCT_VARIABLE: {return o_variable->representsNonNegative(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonNegative();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonNegative(allow_units)) || o_function->representsNonNegative(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonNegative(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = true;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsNonNegative(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return (CHILD(0).isZero() && CHILD(1).representsNonNegative(false)) || representsPositive(allow_units);
		}
		default: {return false;}
	}
}
bool MathStructure::representsNonPositive(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isNonPositive();}
		case STRUCT_VARIABLE: {return o_variable->representsNonPositive(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonPositive();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonPositive(allow_units)) || o_function->representsNonPositive(*this, allow_units);}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonPositive(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsNegative(allow_units)) {
					b = !b;
				} else if(!CHILD(i).representsPositive(allow_units)) {
					return false;
				}
			}
			return b;
		}
		case STRUCT_POWER: {
			return (CHILD(0).isZero() && CHILD(1).representsPositive(false)) || representsNegative(allow_units);
		}
		default: {return false;}
	}
}
bool MathStructure::representsRational(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isRational();}
		case STRUCT_VARIABLE: {return o_variable->representsRational(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isRational();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsRational(allow_units)) || o_function->representsRational(*this, allow_units);}
		case STRUCT_UNIT: {return false;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsRational(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsRational(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return CHILD(1).representsInteger(false) && CHILD(0).representsRational(allow_units) && (CHILD(0).representsPositive(allow_units) || (CHILD(0).representsNegative(allow_units) && CHILD(1).representsEven(false) && CHILD(1).representsPositive(false)));
		}
		default: {return false;}
	}
}
bool MathStructure::representsReal(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isReal();}
		case STRUCT_VARIABLE: {return o_variable->representsReal(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isReal();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsReal(allow_units)) || o_function->representsReal(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsReal(allow_units)) return false;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsReal(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return (CHILD(0).representsPositive(allow_units) && CHILD(1).representsReal(false)) || (CHILD(0).representsReal(allow_units) && CHILD(1).representsEven(false) && CHILD(1).representsInteger(false) && CHILD(1).representsPositive(false));
		}
		default: {return false;}
	}
}
bool MathStructure::representsComplex(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isComplex();}
		case STRUCT_VARIABLE: {return o_variable->representsComplex(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isComplex();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsComplex(allow_units)) || o_function->representsComplex(*this, allow_units);}
		case STRUCT_ADDITION: {
			bool c = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsComplex(allow_units)) {
					if(c) return false;
					c = true;
				} else if(!CHILD(i).representsReal(allow_units) || !CHILD(i).representsNonZero(allow_units)) {
					return false;
				}
			}
			return c;
		}
		case STRUCT_MULTIPLICATION: {
			bool c = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).representsComplex(allow_units)) {
					if(c) return false;
					c = true;
				} else if(!CHILD(i).representsReal(allow_units)) {
					return false;
				}
			}
			return c;
		}
		case STRUCT_UNIT: {return false;}
		default: {return false;}
	}
}
bool MathStructure::representsNonZero(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return !o_number.isZero();}
		case STRUCT_VARIABLE: {return o_variable->representsNonZero(allow_units);}
		case STRUCT_SYMBOLIC: {return CALCULATOR->defaultAssumptions()->isNonZero();}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsNonZero(allow_units)) || o_function->representsNonZero(*this, allow_units);}
		case STRUCT_UNIT: {return allow_units;}
		case STRUCT_ADDITION: {
			bool neg = false, started = false;
			for(size_t i = 0; i < SIZE; i++) {
				if((!started || neg) && CHILD(i).representsNegative(allow_units)) {
					neg = true;
				} else if(neg || !CHILD(i).representsPositive(allow_units)) {
					return false;
				}
				started = true;
			}
			return true;
		}
		case STRUCT_MULTIPLICATION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(!CHILD(i).representsNonZero(allow_units)) {
					return false;
				}
			}
			return true;
		}
		case STRUCT_POWER: {
			return CHILD(0).representsNonZero(allow_units);
		}
		default: {return false;}
	}
}
bool MathStructure::representsEven(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isEven();}
		case STRUCT_VARIABLE: {return o_variable->representsEven(allow_units);}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsEven(allow_units)) || o_function->representsEven(*this, allow_units);}
		default: {return false;}
	}
}
bool MathStructure::representsOdd(bool allow_units) const {
	switch(m_type) {
		case STRUCT_NUMBER: {return o_number.isOdd();}
		case STRUCT_VARIABLE: {return o_variable->representsOdd(allow_units);}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsOdd(allow_units)) || o_function->representsOdd(*this, allow_units);}
		default: {return false;}
	}
}
bool MathStructure::representsUndefined(bool include_childs, bool include_infinite) const {
	switch(m_type) {
		case STRUCT_NUMBER: {
			if(include_infinite) {
				return o_number.isInfinite();
			}
			return false;
		}
		case STRUCT_UNDEFINED: {return true;}
		case STRUCT_VARIABLE: {return o_variable->representsUndefined(include_childs, include_infinite);}
		case STRUCT_POWER: {return (CHILD(0).isZero() && CHILD(1).representsNegative(false)) || (CHILD(0).isInfinity() && CHILD(1).isZero());}
		case STRUCT_FUNCTION: {return (function_value && function_value->representsUndefined()) || o_function->representsUndefined(*this);}
		default: {
			if(include_childs) {
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).representsUndefined(include_childs, include_infinite)) return true;
				}
			}
			return false;
		}
	}
}


void MathStructure::setApproximate(bool is_approx) {
	b_approx = is_approx;
	if(b_approx) {
		if(i_precision < 1) i_precision = PRECISION;
	} else {
		i_precision = -1;
	}
}
bool MathStructure::isApproximate() const {
	return b_approx;
}

int MathStructure::precision() const {
	return i_precision;
}
void MathStructure::setPrecision(int prec) {
	i_precision = prec;
	if(i_precision > 0) b_approx = true;
}

void MathStructure::transform(int mtype, const MathStructure &o) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND(o);
}
void MathStructure::transform(int mtype, const Number &o) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(o);
}
void MathStructure::transform(int mtype, int i) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_POINTER(new MathStructure(i, 1));
}
void MathStructure::transform(int mtype, Unit *u) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(u);
}
void MathStructure::transform(int mtype, Variable *v) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(v);
}
void MathStructure::transform(int mtype, string sym) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_NEW(sym);
}
void MathStructure::transform_nocopy(int mtype, MathStructure *o) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
	APPEND_POINTER(o);
}
void MathStructure::transform(int mtype) {
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = mtype;
	APPEND_POINTER(struct_this);
}
void MathStructure::add(const MathStructure &o, MathOperation op, bool append) {
	switch(op) {
		case OPERATION_ADD: {
			add(o, append);
			break;
		}
		case OPERATION_SUBTRACT: {
			subtract(o, append);
			break;
		}
		case OPERATION_MULTIPLY: {
			multiply(o, append);
			break;
		}
		case OPERATION_DIVIDE: {
			divide(o, append);
			break;
		}
		case OPERATION_RAISE: {
			raise(o);
			break;
		}
		case OPERATION_EXP10: {
			MathStructure *mstruct = new MathStructure(10, 1);
			mstruct->raise(o);
			multiply_nocopy(mstruct, append);
			break;
		}
		case OPERATION_LOGICAL_AND: {
			if(m_type == STRUCT_LOGICAL_AND && append) {
				APPEND(o);
			} else {
				transform(STRUCT_LOGICAL_AND, o);
			}
			break;
		}
		case OPERATION_LOGICAL_OR: {
			if(m_type == STRUCT_LOGICAL_OR && append) {
				APPEND(o);
			} else {
				transform(STRUCT_LOGICAL_OR, o);
			}
			break;
		}
		case OPERATION_LOGICAL_XOR: {
			transform(STRUCT_LOGICAL_XOR, o);
			break;
		}
		case OPERATION_BITWISE_AND: {
			if(m_type == STRUCT_BITWISE_AND && append) {
				APPEND(o);
			} else {
				transform(STRUCT_BITWISE_AND, o);
			}
			break;
		}
		case OPERATION_BITWISE_OR: {
			if(m_type == STRUCT_BITWISE_OR && append) {
				APPEND(o);
			} else {
				transform(STRUCT_BITWISE_OR, o);
			}
			break;
		}
		case OPERATION_BITWISE_XOR: {
			transform(STRUCT_BITWISE_XOR, o);
			break;
		}
		case OPERATION_EQUALS: {}
		case OPERATION_NOT_EQUALS: {}
		case OPERATION_GREATER: {}
		case OPERATION_LESS: {}
		case OPERATION_EQUALS_GREATER: {}
		case OPERATION_EQUALS_LESS: {
			if(append && m_type == STRUCT_COMPARISON && append) {
				MathStructure *o2 = new MathStructure(CHILD(1));
				o2->add(o, op);
				transform_nocopy(STRUCT_LOGICAL_AND, o2);
			} else if(append && m_type == STRUCT_LOGICAL_AND && LAST.type() == STRUCT_COMPARISON) {
				MathStructure *o2 = new MathStructure(LAST[1]);
				o2->add(o, op);
				APPEND_POINTER(o2);
			} else {
				transform(STRUCT_COMPARISON, o);
				switch(op) {
					case OPERATION_EQUALS: {ct_comp = COMPARISON_EQUALS; break;}
					case OPERATION_NOT_EQUALS: {ct_comp = COMPARISON_NOT_EQUALS; break;}
					case OPERATION_GREATER: {ct_comp = COMPARISON_GREATER; break;}
					case OPERATION_LESS: {ct_comp = COMPARISON_LESS; break;}
					case OPERATION_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
					case OPERATION_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_LESS; break;}
					default: {}
				}
			}
			break;
		}
		default: {
		}
	}
}
void MathStructure::add(const MathStructure &o, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND(o);
	} else {
		transform(STRUCT_ADDITION, o);
	}
}
void MathStructure::subtract(const MathStructure &o, bool append) {
	MathStructure *o2 = new MathStructure(o);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(const MathStructure &o, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND(o);
	} else {
		transform(STRUCT_MULTIPLICATION, o);
	}
}
void MathStructure::divide(const MathStructure &o, bool append) {
//	transform(STRUCT_DIVISION, o);
	MathStructure *o2 = new MathStructure(o);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(const MathStructure &o) {
	transform(STRUCT_POWER, o);
}
void MathStructure::add(const Number &o, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(o);
	} else {
		transform(STRUCT_ADDITION, o);
	}
}
void MathStructure::subtract(const Number &o, bool append) {
	MathStructure *o2 = new MathStructure(o);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(const Number &o, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(o);
	} else {
		transform(STRUCT_MULTIPLICATION, o);
	}
}
void MathStructure::divide(const Number &o, bool append) {
	MathStructure *o2 = new MathStructure(o);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(const Number &o) {
	transform(STRUCT_POWER, o);
}
void MathStructure::add(int i, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_POINTER(new MathStructure(i, 1));
	} else {
		transform(STRUCT_ADDITION, i);
	}
}
void MathStructure::subtract(int i, bool append) {
	MathStructure *o2 = new MathStructure(i, 1);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(int i, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_POINTER(new MathStructure(i, 1));
	} else {
		transform(STRUCT_MULTIPLICATION, i);
	}
}
void MathStructure::divide(int i, bool append) {
	MathStructure *o2 = new MathStructure(i, 1);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(int i) {
	transform(STRUCT_POWER, i);
}
void MathStructure::add(Variable *v, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(v);
	} else {
		transform(STRUCT_ADDITION, v);
	}
}
void MathStructure::subtract(Variable *v, bool append) {
	MathStructure *o2 = new MathStructure(v);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(Variable *v, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(v);
	} else {
		transform(STRUCT_MULTIPLICATION, v);
	}
}
void MathStructure::divide(Variable *v, bool append) {
	MathStructure *o2 = new MathStructure(v);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(Variable *v) {
	transform(STRUCT_POWER, v);
}
void MathStructure::add(Unit *u, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(u);
	} else {
		transform(STRUCT_ADDITION, u);
	}
}
void MathStructure::subtract(Unit *u, bool append) {
	MathStructure *o2 = new MathStructure(u);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(Unit *u, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(u);
	} else {
		transform(STRUCT_MULTIPLICATION, u);
	}
}
void MathStructure::divide(Unit *u, bool append) {
	MathStructure *o2 = new MathStructure(u);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(Unit *u) {
	transform(STRUCT_POWER, u);
}
void MathStructure::add(string sym, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_NEW(sym);
	} else {
		transform(STRUCT_ADDITION, sym);
	}
}
void MathStructure::subtract(string sym, bool append) {
	MathStructure *o2 = new MathStructure(sym);
	o2->negate();
	add_nocopy(o2, append);
}
void MathStructure::multiply(string sym, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_NEW(sym);
	} else {
		transform(STRUCT_MULTIPLICATION, sym);
	}
}
void MathStructure::divide(string sym, bool append) {
	MathStructure *o2 = new MathStructure(sym);
	o2->inverse();
	multiply_nocopy(o2, append);
}
void MathStructure::raise(string sym) {
	transform(STRUCT_POWER, sym);
}
void MathStructure::add_nocopy(MathStructure *o, MathOperation op, bool append) {
	switch(op) {
		case OPERATION_ADD: {
			add_nocopy(o, append);
			break;
		}
		case OPERATION_SUBTRACT: {
			subtract_nocopy(o, append);
			break;
		}
		case OPERATION_MULTIPLY: {
			multiply_nocopy(o, append);
			break;
		}
		case OPERATION_DIVIDE: {
			divide_nocopy(o, append);
			break;
		}
		case OPERATION_RAISE: {
			raise_nocopy(o);
			break;
		}
		case OPERATION_EXP10: {
			MathStructure *mstruct = new MathStructure(10, 1);
			mstruct->raise_nocopy(o);
			multiply_nocopy(mstruct, append);
			break;
		}
		case OPERATION_LOGICAL_AND: {
			if(m_type == STRUCT_LOGICAL_AND && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_LOGICAL_AND, o);
			}
			break;
		}
		case OPERATION_LOGICAL_OR: {
			if(m_type == STRUCT_LOGICAL_OR && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_LOGICAL_OR, o);
			}
			break;
		}
		case OPERATION_LOGICAL_XOR: {
			transform_nocopy(STRUCT_LOGICAL_XOR, o);
			break;
		}
		case OPERATION_BITWISE_AND: {
			if(m_type == STRUCT_BITWISE_AND && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_BITWISE_AND, o);
			}
			break;
		}
		case OPERATION_BITWISE_OR: {
			if(m_type == STRUCT_BITWISE_OR && append) {
				APPEND_POINTER(o);
			} else {
				transform_nocopy(STRUCT_BITWISE_OR, o);
			}
			break;
		}
		case OPERATION_BITWISE_XOR: {
			transform_nocopy(STRUCT_BITWISE_XOR, o);
			break;
		}
		case OPERATION_EQUALS: {}
		case OPERATION_NOT_EQUALS: {}
		case OPERATION_GREATER: {}
		case OPERATION_LESS: {}
		case OPERATION_EQUALS_GREATER: {}
		case OPERATION_EQUALS_LESS: {
			if(append && m_type == STRUCT_COMPARISON && append) {
				MathStructure *o2 = new MathStructure(CHILD(1));
				o2->add_nocopy(o, op);
				transform_nocopy(STRUCT_LOGICAL_AND, o2);
			} else if(append && m_type == STRUCT_LOGICAL_AND && LAST.type() == STRUCT_COMPARISON) {
				MathStructure *o2 = new MathStructure(LAST[1]);
				o2->add_nocopy(o, op);
				APPEND_POINTER(o2);
			} else {
				transform_nocopy(STRUCT_COMPARISON, o);
				switch(op) {
					case OPERATION_EQUALS: {ct_comp = COMPARISON_EQUALS; break;}
					case OPERATION_NOT_EQUALS: {ct_comp = COMPARISON_NOT_EQUALS; break;}
					case OPERATION_GREATER: {ct_comp = COMPARISON_GREATER; break;}
					case OPERATION_LESS: {ct_comp = COMPARISON_LESS; break;}
					case OPERATION_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
					case OPERATION_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_LESS; break;}
					default: {}
				}
			}
			break;
		}
		default: {
		}
	}
}
void MathStructure::add_nocopy(MathStructure *o, bool append) {
	if(m_type == STRUCT_ADDITION && append) {
		APPEND_POINTER(o);
	} else {
		transform_nocopy(STRUCT_ADDITION, o);
	}
}
void MathStructure::subtract_nocopy(MathStructure *o, bool append) {
	o->negate();
	add_nocopy(o, append);
}
void MathStructure::multiply_nocopy(MathStructure *o, bool append) {
	if(m_type == STRUCT_MULTIPLICATION && append) {
		APPEND_POINTER(o);
	} else {
		transform_nocopy(STRUCT_MULTIPLICATION, o);
	}
}
void MathStructure::divide_nocopy(MathStructure *o, bool append) {
	o->inverse();
	multiply_nocopy(o, append);
}
void MathStructure::raise_nocopy(MathStructure *o) {
	transform_nocopy(STRUCT_POWER, o);
}
void MathStructure::negate() {
	//transform(STRUCT_NEGATE);
	MathStructure *struct_this = new MathStructure();
	struct_this->set_nocopy(*this);
	clear(true);
	m_type = STRUCT_MULTIPLICATION;
	APPEND(m_minus_one);
	APPEND_POINTER(struct_this);
}
void MathStructure::inverse() {
	//transform(STRUCT_INVERSE);
	raise(m_minus_one);
}
void MathStructure::setLogicalNot() {
	transform(STRUCT_LOGICAL_NOT);
}		
void MathStructure::setBitwiseNot() {
	transform(STRUCT_BITWISE_NOT);
}		

bool MathStructure::equals(const MathStructure &o) const {
	if(m_type != o.type()) return false;
	if(SIZE != o.size()) return false;
	switch(m_type) {
		case STRUCT_UNDEFINED: {return true;}
		case STRUCT_SYMBOLIC: {return s_sym == o.symbol();}
		case STRUCT_NUMBER: {return o_number.equals(o.number());}
		case STRUCT_VARIABLE: {return o_variable == o.variable();}
		case STRUCT_UNIT: {return o_unit == o.unit() && o_prefix == o.prefix();}
		case STRUCT_COMPARISON: {if(ct_comp != o.comparisonType()) return false; break;}
		case STRUCT_FUNCTION: {if(o_function != o.function()) return false; break;}
	}
	if(SIZE < 1) return false;
	for(size_t i = 0; i < SIZE; i++) {
		if(!CHILD(i).equals(o[i])) return false;
	}
	return true;
}
bool MathStructure::equals(const Number &o) const {
	if(m_type != STRUCT_NUMBER) return false;
	return o_number.equals(o);
}
bool MathStructure::equals(int i) const {
	if(m_type != STRUCT_NUMBER) return false;
	return o_number.equals(Number(i, 1));
}
bool MathStructure::equals(Unit *u) const {
	if(m_type != STRUCT_UNIT) return false;
	return o_unit == u;
}
bool MathStructure::equals(Variable *v) const {
	if(m_type != STRUCT_VARIABLE) return false;
	return o_variable == v;
}
bool MathStructure::equals(string sym) const {
	if(m_type != STRUCT_SYMBOLIC) return false;
	return s_sym == sym;
}
ComparisonResult MathStructure::compare(const MathStructure &o) const {
	if(isNumber() && o.isNumber()) {
		return o_number.compare(o.number());
	}
	if(equals(o)) return COMPARISON_RESULT_EQUAL;
	if(o.representsReal(true) && representsComplex(true)) return COMPARISON_RESULT_NOT_EQUAL;
	if(representsReal(true) && o.representsComplex(true)) return COMPARISON_RESULT_NOT_EQUAL;
	MathStructure mtest(*this);
	mtest -= o;
	EvaluationOptions eo = default_evaluation_options;
	eo.approximation = APPROXIMATION_APPROXIMATE;
	mtest.calculatesub(eo, eo);
	bool incomp = false;
	if(mtest.isAddition()) {
		for(size_t i = 1; i < mtest.size(); i++) {
			if(mtest[i - 1].isUnitCompatible(mtest[i]) == 0) {
				incomp = true;
				break;
			}
		}
	}
	if(mtest.isZero()) return COMPARISON_RESULT_EQUAL;
	else if(mtest.representsPositive(true)) {if(incomp) return COMPARISON_RESULT_NOT_EQUAL; return COMPARISON_RESULT_LESS;}
	else if(mtest.representsNegative(true)) {if(incomp) return COMPARISON_RESULT_NOT_EQUAL; return COMPARISON_RESULT_GREATER;}
	else if(mtest.representsNonZero(true)) return COMPARISON_RESULT_NOT_EQUAL;
	else if(mtest.representsNonPositive(true)) {if(incomp) return COMPARISON_RESULT_NOT_EQUAL; return COMPARISON_RESULT_EQUAL_OR_LESS;}
	else if(mtest.representsNonNegative(true)) {if(incomp) return COMPARISON_RESULT_NOT_EQUAL; return COMPARISON_RESULT_EQUAL_OR_GREATER;}
	return COMPARISON_RESULT_UNKNOWN;
}

int MathStructure::merge_addition(MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.add(mstruct.number()) && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate())) {
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	if(isZero()) {
		set_nocopy(mstruct, true);
		return 1;
	}
	if(mstruct.isZero()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	if(m_type == STRUCT_NUMBER && o_number.isInfinite()) {
		if(mstruct.representsNumber(false)) {
			MERGE_APPROX_AND_PREC(mstruct)
			return 1;
		}
	} else if(mstruct.isNumber() && mstruct.number().isInfinite()) {
		if(representsNumber(false)) {
			clear(true);
			o_number = mstruct.number();
			MERGE_APPROX_AND_PREC(mstruct)
			return 1;
		}
	}
	if(representsUndefined() || mstruct.representsUndefined()) return -1;
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(SIZE == mstruct.size()) {
						for(size_t i = 0; i < SIZE; i++) {
							CHILD(i) += mstruct[i]; 
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}
				default: {
					return -1;
				}
			}
			return -1;
		}
		case STRUCT_ADDITION: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					return -1;
				}
				case STRUCT_ADDITION: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND(mstruct[i]);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					APPEND_REF(&mstruct);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {return -1;}
				case STRUCT_ADDITION: {
					return 0;
				}
				case STRUCT_MULTIPLICATION: {
					size_t i1 = 0, i2 = 0;
					bool b = true;
					if(CHILD(0).isNumber()) i1 = 1;
					if(mstruct[0].isNumber()) i2 = 1;
					if(SIZE - i1 == mstruct.size() - i2) {
						for(size_t i = i1; i < SIZE; i++) {
							if(CHILD(i) != mstruct[i + i2 - i1]) {
								b = false;
								break;
							}
						}
						if(b) {
							if(i1 == 0) {
								PREPEND(m_one);
							}
							if(i2 == 0) {
								CHILD(0).number()++;
							} else {
								CHILD(0).number() += mstruct[0].number();
							}
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
					if(eo.merge_divisions) {
						int pow1 = -1, pow2 = -1;
						for(size_t i = 0; i < SIZE; i++) {
							if(pow1 < 0 && CHILD(i).isPower() && CHILD(i)[0].isAddition() && CHILD(i)[1].isMinusOne()) {
								pow1 = (int) i;
								break;
							}
						}
						if(pow1 >= 0) {
							for(size_t i = 0; i < mstruct.size(); i++) {
								if(mstruct[i].isPower() && mstruct[i][0].isAddition() && mstruct[i][1].isMinusOne()) {
									pow2 = (int) i;
									break;
								}
							}
						}
						if(pow1 >= 0 && pow2 >= 0 && CHILD((size_t) pow1) != mstruct[(size_t) pow2]) {
							const MathStructure *xvar = NULL;
							const MathStructure *mcur = NULL;
							int index = 0;
							while(index <= 3) {
								if(index == 0) {
									mcur = this;
								} else if(index == 1) {
									mcur = &mstruct;									
								} else if(index == 2) {
									mcur = &(CHILD((size_t) pow1)[0]);
								} else if(index == 3) {
									mcur = &(mstruct[(size_t) pow2][0]);
								}
								if(index <= 1) {
									if(mcur->size() == 2) {
										if(pow1 == 0) {
											if((*mcur)[1].isAddition()) {
												mcur = &(*mcur)[1];
											}
										} else {
											if((*mcur)[0].isAddition()) {
												mcur = &(*mcur)[0];
											}
										}
									}
								}
								for(size_t i = 0; i < mcur->size(); i++) {
									if((mcur != this || i != (size_t) pow1) && (mcur != &mstruct || i != (size_t) pow2)) {
										switch((*mcur)[i].type()) {
											case STRUCT_MULTIPLICATION: {
												for(size_t i2 = 0; i2 < SIZE; i2++) {
													switch((*mcur)[i][i2].type()) {
														case STRUCT_NUMBER: {
															break;
														}
														case STRUCT_POWER: {
															if((*mcur)[i][i2][1].isNumber() && (*mcur)[i][1].number().isInteger() && (*mcur)[i][i2][1].number().isPositive()) {
																if(xvar && (*mcur)[i][i2][0] != *xvar) {
																	return -1;
																}
																xvar = &(*mcur)[i][i2][0];
																break;
															}
														}
														default: {
															if(xvar && (*mcur)[i][i2] != *xvar) {
																return -1;
															}
															xvar = &(*mcur)[i][i2];
														}
													}
												}
											}
											case STRUCT_NUMBER: {
												break;
											}
											case STRUCT_POWER: {
												if((*mcur)[i][1].isNumber() && (*mcur)[i][1].number().isInteger() && (*mcur)[i][1].number().isPositive()) {
													if(xvar && (*mcur)[i][0] != *xvar) {
														return -1;
													}
													xvar = &(*mcur)[i][0];
													break;
												}
											}
											default: {
												if(xvar && (*mcur)[i] != *xvar) {
													return -1;
												}
												xvar = &(*mcur)[i];
											}
										}
									}
								}
								index++;
							}
							if(!xvar) return -1;
							MathStructure mtest(*this);
							mtest.mergePrecision(*this);
							MathStructure *mstruct_div = new MathStructure(CHILD((size_t) pow1));
							MathStructure *mstruct_test = new MathStructure(mstruct);
							mstruct_test->mergePrecision(mstruct);
							(*mstruct_div)[0] *= mstruct[(size_t) pow2][0];
							mtest.delChild((size_t) pow1);
							mstruct_test->delChild(pow2 + 1);
							mtest.multiply((*mstruct_div)[0][1]);
							mtest.add_nocopy(mstruct_test, false);
							mtest[1] *= (*mstruct_div)[0][0];
							mtest.multiply_nocopy(mstruct_div, true);
							EvaluationOptions eo2 = eo;
							eo2.merge_divisions = false;
							eo2.do_polynomial_division = false;
							eo2.reduce_divisions = true;
							mtest.calculatesub(eo2, eo2);
							eo2.do_polynomial_division = true;
							eo2.reduce_divisions = false;
							mtest.calculatesub(eo2, eo2);
							if(mtest.countTotalChilds() < countTotalChilds()) {
								set_nocopy(mtest, true);
								return 1;
							}
						}
						break;
					}
					b = true; size_t divs = 0;
					for(; b && i1 < SIZE; i1++) {
						if(CHILD(i1).isPower() && CHILD(i1)[1].hasNegativeSign()) {
							divs++;
							for(; i2 < mstruct.size(); i2++) {
								b = false;
								if(mstruct[i2].isPower() && mstruct[i2][1].hasNegativeSign()) {
									if(mstruct[i2] == CHILD(i1)) {
										b = true;
									}
									i2++;
									break;
								}
							}
						}
					}
					if(b && divs > 0) {
						for(; i2 < mstruct.size(); i2++) {
							if(mstruct[i2].isPower() && mstruct[i2][1].hasNegativeSign()) {
								b = false;
								break;
							}
						}
					}
					if(b && divs > 0) {
						if(SIZE - divs == 0) {
							if(mstruct.size() - divs == 0) {
								PREPEND(MathStructure(2, 1))
							} else if(mstruct.size() - divs == 1) {
								PREPEND(m_one);
								for(size_t i = 0; i < mstruct.size(); i++) {
									if(!mstruct[i].isPower() || !mstruct[i][1].hasNegativeSign()) {
										CHILD(0).add(mstruct[i], true);
										break;
									}
								}
							} else {
								PREPEND(m_one);
								CHILD(0).add(mstruct, true);
								for(size_t i = 0; i < CHILD(0)[CHILD(0).size() - 1].size();) {
									if(CHILD(0)[CHILD(0).size() - 1][i].isPower() && CHILD(0)[CHILD(0).size() - 1][i][1].hasNegativeSign()) {
										CHILD(0)[CHILD(0).size() - 1].delChild(i + 1);
									} else {
										i++;
									}
								}
							}
						} else if(SIZE - divs == 1) {
							size_t index = 0;
							for(; index < SIZE; index++) {
								if(!CHILD(index).isPower() || !CHILD(index)[1].hasNegativeSign()) {
									break;
								}
							}
							if(mstruct.size() - divs == 0) {
								CHILD(index).add(m_one, true);
							} else if(mstruct.size() - divs == 1) {
								for(size_t i = 0; i < mstruct.size(); i++) {
									if(!mstruct[i].isPower() || !mstruct[i][1].hasNegativeSign()) {
										CHILD(index).add(mstruct[i], true);
										break;
									}
								}
							} else {
								CHILD(index).add(mstruct, true);
								for(size_t i = 0; i < CHILD(index)[CHILD(index).size() - 1].size();) {
									if(CHILD(index)[CHILD(index).size() - 1][i].isPower() && CHILD(index)[CHILD(index).size() - 1][i][1].hasNegativeSign()) {
										CHILD(index)[CHILD(index).size() - 1].delChild(i + 1);
									} else {
										i++;
									}
								}
							}
						} else {
							for(size_t i = 0; i < SIZE;) {
								if(CHILD(i).isPower() && CHILD(i)[1].hasNegativeSign()) {
									ERASE(i);
								} else {
									i++;
								}
							}
							if(mstruct.size() - divs == 0) {
								add(m_one);
							} else if(mstruct.size() - divs == 1) {
								for(size_t i = 0; i < mstruct.size(); i++) {
									if(!mstruct[i].isPower() || !mstruct[i][1].hasNegativeSign()) {
										add(mstruct[i]);
										break;
									}
								}
							} else {
								add(mstruct);
								for(size_t i = 0; i < CHILD(1).size();) {
									if(CHILD(1)[i].isPower() && CHILD(1)[i][1].hasNegativeSign()) {
										CHILD(1).delChild(i + 1);
									} else {
										i++;
									}
								}
							}
							for(size_t i = 0; i < mstruct.size(); i++) {
								if(mstruct[i].isPower() && mstruct[i][1].hasNegativeSign()) {
									multiply(mstruct[i], true);
								}
							}
						}
						return 1;
					}
					break;
				}
				case STRUCT_POWER: {
					if(mstruct[1].hasNegativeSign()) {
						bool b = false;
						size_t index = 0;
						for(size_t i = 0; i < SIZE; i++) {
							if(CHILD(i).isPower() && CHILD(i)[1].hasNegativeSign()) {
								if(b) {
									b = false;
									break;
								}
								if(mstruct == CHILD(i)) {
									index = i;
									b = true;
								}
								if(!b) break;
							}
						}
						if(b) {		
							if(SIZE == 2) {
								if(index == 0) {
									CHILD(1).add(m_one, true);
								} else {
									CHILD(0).add(m_one, true);
								}
							} else {
								ERASE(index);
								add(m_one);
								multiply(mstruct, false);
							}
							return 1;
						}
					}
				}
				default: {
					if(SIZE == 2 && CHILD(0).isNumber() && CHILD(1) == mstruct) {
						CHILD(0).number()++;
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}					
			}
			break;
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {return -1;}
				case STRUCT_ADDITION: {}
				case STRUCT_MULTIPLICATION: {
					return 0;
				}
				default: {
					if(equals(mstruct)) {
						multiply(2);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}
			}	
		}		
	}
	return -1;
}

bool reducable(const MathStructure &mnum, const MathStructure &mden, Number &nr) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {}
		case STRUCT_ADDITION: {
			break;
		}
		default: {
			if(mnum.representsNonZero(true)) {
				bool reduce = true;
				for(size_t i = 0; i < mden.size() && reduce; i++) {
					switch(mden[i].type()) {
						case STRUCT_MULTIPLICATION: {
							reduce = false;
							for(size_t i2 = 0; i2 < mden[i].size(); i2++) {
								if(mnum == mden[i][i2]) {
									reduce = true;
									if(!nr.isOne() && !nr.isFraction()) nr.set(1, 1);
									break;
								} else if(mden[i][i2].isPower() && mden[i][i2][1].isNumber() && mden[i][i2][1].number().isReal() && mnum == mden[i][i2][0]) {
									if(!mden[i][i2][1].number().isPositive()) {
										break;
									}
									if(mden[i][i2][1].number().isLessThan(nr)) nr = mden[i][i2][1].number();
									reduce = true;
									break;
								}
							}
							break;
						}
						case STRUCT_POWER: {
							if(mden[i][1].isNumber() && mden[i][1].number().isReal() && mnum == mden[i][0]) {
								if(!mden[i][1].number().isPositive()) {
									reduce = false;
									break;
								}
								if(mden[i][1].number().isLessThan(nr)) nr = mden[i][1].number();
								break;
							}
						}
						default: {
							if(mnum != mden[i]) {
								reduce = false;
								break;
							}
							if(!nr.isOne() && !nr.isFraction()) nr.set(1, 1);
						}
					}
				}
				return reduce;
			}
		}
	}
	return false;
}
void reduce(const MathStructure &mnum, MathStructure &mden, Number &nr) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {}
		case STRUCT_ADDITION: {
			break;
		}
		default: {
			for(size_t i = 0; i < mden.size(); i++) {
				switch(mden[i].type()) {
					case STRUCT_MULTIPLICATION: {
						for(size_t i2 = 0; i2 < mden[i].size(); i2++) {
							if(mden[i][i2] == mnum) {
								if(!nr.isOne()) {
									MathStructure *mexp = new MathStructure(1, 1);
									mexp->number() -= nr;
									mden[i][i2].raise_nocopy(mexp);
								} else {
									if(mden[i].size() == 1) {
										mden[i].set(m_one);
									} else {
										mden[i].delChild(i2 + 1);
										if(mden[i].size() == 1) {
											mden[i].setToChild(1, true);
										}
									}
								}
								break;
							} else if(mden[i][i2].isPower() && mden[i][i2][1].isNumber() && mden[i][i2][1].number().isReal() && mnum.equals(mden[i][i2][0])) {
								mden[i][i2][1].number() -= nr;
								if(mden[i][i2][1].number().isOne()) {
									mden[i][i2].setToChild(1, true);
								}
								break;
							}
						}
						break;
					}
					case STRUCT_POWER: {
						if(mden[i][1].isNumber() && mden[i][1].number().isReal() && mnum.equals(mden[i][0])) {
							mden[i][1].number() -= nr;
							if(mden[i][1].number().isOne()) {
								mden[i].setToChild(1, true);
							}
							break;
						}
					}
					default: {
						if(!nr.isOne()) {
							MathStructure *mexp = new MathStructure(1, 1);
							mexp->number() -= nr;
							mden[i].raise_nocopy(mexp);
						} else {
							mden[i].set(m_one);
						}
					}
				}
			}
		}
	}
}

int MathStructure::merge_multiplication(MathStructure &mstruct, const EvaluationOptions &eo, bool do_append) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.multiply(mstruct.number()) && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.isInfinite() || o_number.isInfinite() || mstruct.number().isInfinite())) {
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	if(mstruct.isOne()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	} else if(isOne()) {
		set_nocopy(mstruct, true);
		return 1;
	}
	if(m_type == STRUCT_NUMBER && o_number.isInfinite()) {
		if(o_number.isMinusInfinity()) {
			if(mstruct.representsPositive(false)) {
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(mstruct.representsNegative(false)) {
				o_number.setPlusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		} else if(o_number.isPlusInfinity()) {
			if(mstruct.representsPositive(false)) {
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(mstruct.representsNegative(false)) {
				o_number.setMinusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		}
		if(mstruct.representsReal(false) && mstruct.representsNonZero(false)) {
			o_number.setInfinity();
			MERGE_APPROX_AND_PREC(mstruct)
			return 1;
		}
	} else if(mstruct.isNumber() && mstruct.number().isInfinite()) {
		if(mstruct.number().isMinusInfinity()) {
			if(representsPositive(false)) {
				clear(true);
				o_number.setMinusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(representsNegative(false)) {
				clear(true);
				o_number.setPlusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		} else if(mstruct.number().isPlusInfinity()) {
			if(representsPositive(false)) {
				clear(true);
				o_number.setPlusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(representsNegative(false)) {
				clear(true);
				o_number.setMinusInfinity();
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		}
		if(representsReal(false) && representsNonZero(false)) {
			clear(true);
			o_number.setInfinity();
			MERGE_APPROX_AND_PREC(mstruct)
			return 1;
		}
	}
	if(representsUndefined() || mstruct.representsUndefined()) return -1;
	const MathStructure *mnum = NULL, *mden = NULL;
	if(eo.do_polynomial_division || eo.reduce_divisions) {
		if(!isNumber() && mstruct.isPower() && mstruct[0].isAddition() && mstruct[0].size() > 1 && mstruct[1].isNumber() && mstruct[1].number().isMinusOne()) {
			if((!isPower() || !CHILD(1).hasNegativeSign()) && representsNumber() && mstruct[0].representsNumber() && (eo.assume_denominators_nonzero || mstruct[0].representsNonZero())) {
				mnum = this;
				mden = &mstruct[0];
			}
		} else if(!mstruct.isNumber() && isPower() && CHILD(0).isAddition() && CHILD(0).size() > 1 && CHILD(1).isNumber() && CHILD(1).number().isMinusOne()) {
			if((!mstruct.isPower() || !mstruct[1].hasNegativeSign()) && mstruct.representsNumber() && CHILD(0).representsNumber() && (eo.assume_denominators_nonzero || CHILD(0).representsNonZero())) {
				mnum = &mstruct;
				mden = &CHILD(0);
			}
		}
	}
	if(mnum && mden && eo.do_polynomial_division && ((mnum->isAddition() && mnum->size() > 1) || (mden->isAddition() && mden->size() > 1))) {
		const MathStructure *mnum2 = mnum;
		const MathStructure *mden2 = mden;
		try_polynomial_division:
		const MathStructure *xvar = NULL;
		const Number *xexp = NULL;
		const MathStructure *mnum_first;
		if(mnum->isAddition() && mnum->size() > 1) {
			mnum_first = &(*mnum)[0];
		} else {
			mnum_first = mnum;
		}
		if(mnum_first->isNumber()) {
		} else if(mnum_first->isPower() && (*mnum_first)[0].size() == 0 && (*mnum_first)[1].isNumber() && (*mnum_first)[1].number().isInteger() && (*mnum_first)[1].number().isPositive()) {
			xvar = &(*mnum_first)[0];
			xexp = &(*mnum_first)[1].number();
		} else if(mnum_first->isMultiplication() && mnum_first->size() == 2 && (*mnum_first)[0].isNumber()) {
			if((*mnum_first)[1].isPower()) {
				if((*mnum_first)[1][0].size() == 0 && (*mnum_first)[1][1].isNumber() && (*mnum_first)[1][1].number().isInteger() && (*mnum_first)[1][1].number().isPositive()) {
					xvar = &(*mnum_first)[1][0];
					xexp = &(*mnum_first)[1][1].number();
				}
			} else if((*mnum_first)[1].size() == 0) {
				xvar = &(*mnum_first)[1];
			}
		} else if(mnum_first->size() == 0) {
			xvar = mnum_first;
		}
		if(xvar) {
			bool b = false;
			const MathStructure *mden_first;
			if(mden->isAddition() && mden->size() > 1) {
				mden_first = &(*mden)[0];
			} else {
				mden_first = mden;
			}
			if(mden_first->isPower() && xexp && (*mden_first)[0].size() == 0 && (*mden_first)[1].isNumber() && xvar->equals((*mden_first)[0]) && (*mden_first)[1].number().isInteger() && (*mden_first)[1].number().isPositive() && !xexp->isLessThan((*mden_first)[1].number())) {
				b = true;
			} else if(mden_first->isMultiplication() && mden_first->size() == 2 && (*mden_first)[0].isNumber()) {
				if((*mden_first)[1].isPower()) {
					if(xexp && (*mden_first)[1][0].size() == 0 && (*mden_first)[1][1].isNumber() && xvar->equals((*mden_first)[1][0]) && (*mden_first)[1][1].number().isInteger() && (*mden_first)[1][1].number().isPositive() && !xexp->isLessThan((*mden_first)[1][1].number())) {
						b = true;
					}
				} else if(xvar->equals((*mden_first)[1])) {
					b = true;
				}
			} else if(mden_first->equals(*xvar)) {
				b = true;
			}
			bool had_num = false;
			for(size_t i = 1; b && mnum->isAddition() && i < mnum->size(); i++) {
				switch((*mnum)[i].type()) {
					case STRUCT_NUMBER: {
						if(had_num) {
							b = false;
						} else {
							had_num = true;
						}
						break;
					}
					case STRUCT_POWER: {
						if(!(*mnum)[i][1].isNumber() || !xvar->equals((*mnum)[i][0]) || !(*mnum)[i][1].number().isInteger() || !(*mnum)[i][1].number().isPositive()) {
							b = false;
						}
						break;
					}
					case STRUCT_MULTIPLICATION: {
						if(!(*mnum)[i].size() == 2 || !(*mnum)[i][0].isNumber()) {
							b = false;
						} else if((*mnum)[i][1].isPower()) {
							if(!(*mnum)[i][1][1].isNumber() || !xvar->equals((*mnum)[i][1][0]) || !(*mnum)[i][1][1].number().isInteger() || !(*mnum)[i][1][1].number().isPositive()) {
								b = false;
							}
						} else if(!xvar->equals((*mnum)[i][1])) {
							b = false;
						}
						break;
					}
					default: {
						if(!xvar->equals((*mnum)[i])) {
							b = false;
						}
					}
				}
			}
			had_num = false;
			for(size_t i = 1; b && mden->isAddition() && i < mden->size(); i++) {
				switch((*mden)[i].type()) {
					case STRUCT_NUMBER: {
						if(had_num) {
							b = false;
						} else {
							had_num = true;
						}
						break;
					}
					case STRUCT_POWER: {
						if(!(*mden)[i][1].isNumber() || !xvar->equals((*mden)[i][0]) || !(*mden)[i][1].number().isInteger() || !(*mden)[i][1].number().isPositive()) {
							b = false;
						}
						break;
					}
					case STRUCT_MULTIPLICATION: {
						if(!(*mden)[i].size() == 2 || !(*mden)[i][0].isNumber()) {
							b = false;
						} else if((*mden)[i][1].isPower()) {
							if(!(*mden)[i][1][1].isNumber() || !xvar->equals((*mden)[i][1][0]) || !(*mden)[i][1][1].number().isInteger() || !(*mden)[i][1][1].number().isPositive()) {
								b = false;
							}
						} else if(!xvar->equals((*mden)[i][1])) {
							b = false;
						}
						break;
					}
					default: {
						if(!xvar->equals((*mden)[i])) {
							b = false;
						}
					}
				}
			}
			if(b) {
				vector<Number> nfactors;
				vector<Number> nexps;
				vector<Number> dfactors;
				vector<Number> dexps;
				for(size_t i = 0; b && i == 0 || (i < mnum->size()); i++) {
					if(i > 0) mnum_first = &(*mnum)[i];
					switch(mnum_first->type()) {
						case STRUCT_NUMBER: {
							nfactors.push_back(mnum_first->number());
							nexps.push_back(Number());
							break;
						}
						case STRUCT_POWER: {
							nfactors.push_back(Number(1, 1));
							nexps.push_back((*mnum_first)[1].number());
							break;
						}
						case STRUCT_MULTIPLICATION: {
							nfactors.push_back((*mnum_first)[0].number());
							if((*mnum_first)[1].isPower()) {
								nexps.push_back((*mnum_first)[1][1].number());
							} else {
								nexps.push_back(Number(1, 1));
							}
							break;
						}
						default: {
							nfactors.push_back(Number(1, 1));
							nexps.push_back(Number(1, 1));
						}
					}
					if(!mnum->isAddition()) break;
				}
				for(size_t i = 0; b && i == 0 || (i < mden->size()); i++) {
					if(i > 0) mden_first = &(*mden)[i];
					switch(mden_first->type()) {
						case STRUCT_NUMBER: {
							dfactors.push_back(mden_first->number());
							dexps.push_back(Number());
							break;
						}
						case STRUCT_POWER: {
							dfactors.push_back(Number(1, 1));
							dexps.push_back((*mden_first)[1].number());
							break;
						}
						case STRUCT_MULTIPLICATION: {
							dfactors.push_back((*mden_first)[0].number());
							if((*mden_first)[1].isPower()) {
								dexps.push_back((*mden_first)[1][1].number());
							} else {
								dexps.push_back(Number(1, 1));
							}
							break;
						}
						default: {
							dfactors.push_back(Number(1, 1));
							dexps.push_back(Number(1, 1));
						}
					}
					if(!mden->isAddition()) break;
				}
				MathStructure xvar2(*xvar);
				MERGE_APPROX_AND_PREC(mstruct);
				MathStructure mtest;
				MathStructure *mtmp;
				Number nftmp, netmp, nfac, nexp;
				bool bdone;
				while(true) {
					if(nexps.empty() || nexps[0].isLessThan(dexps[0])) break;
					nfac = nfactors[0];
					nfac /= dfactors[0];
					nexp = nexps[0];
					nexp -= dexps[0];
					if(mtest.isZero()) {
						mtmp = &mtest;
					} else {
						mtest.add(m_zero, true);
						mtmp = &mtest[mtest.size() - 1];
					}
					if(!nfac.isOne()) {
						mtmp->set(nfac);
						if(!nexp.isZero()) {
							mtmp->multiply(xvar2);
							if(!nexp.isOne()) {
								(*mtmp)[1].raise(nexp);
							}
						}
					} else {
						mtmp->set(xvar2);
						if(!nexp.isOne()) {
							mtmp->raise(nexp);
						}
					}
					nfactors.erase(nfactors.begin());
					nexps.erase(nexps.begin());
					for(size_t i = 1; i < dfactors.size(); i++) {
						nftmp = dfactors[i];
						nftmp *= nfac;
						netmp = dexps[i];
						netmp += nexp;
						if(nfactors.empty()) {
							nftmp.negate();
							nfactors.push_back(nftmp);
							nexps.push_back(netmp);
						} else {
							bdone = false;
							for(size_t i = 0; i < nfactors.size(); i++) {
								if(netmp == nexps[i]) {
									if(nfactors[i] == nftmp) {
										nfactors.erase(nfactors.begin() + i);
										nexps.erase(nexps.begin() + i);
									} else {
										nfactors[i] -= nftmp;
									}
									bdone = true;
									break;
								} else if(netmp.isGreaterThan(nexps[i])) {
									nftmp.negate();
									nfactors.insert(nfactors.begin() + i, nftmp);
									nexps.insert(nexps.begin() + i, netmp);
									bdone = true;
									break;
								}
							}
							if(!bdone) {
								nftmp.negate();
								nfactors.push_back(nftmp);
								nexps.push_back(netmp);
							}
						}
					}
				}
				if(!nexps.empty()) {
					mtest.add(m_zero, true);
					mtest[mtest.size() - 1].transform(STRUCT_MULTIPLICATION);
					for(size_t i = 0; i < nexps.size(); i++) {
						if(mtest[mtest.size() - 1][0].isZero()) {
							mtmp = &mtest[mtest.size() - 1][0];
						} else {
							mtest[mtest.size() - 1][0].add(m_zero, true);
							mtmp = &mtest[mtest.size() - 1][0][mtest[mtest.size() - 1][0].size() - 1];
						}
						if(!nfactors[i].isOne()) {
							mtmp->set(nfactors[i]);
							if(!nexps[i].isZero()) {
								mtmp->multiply(xvar2);
								if(!nexps[i].isOne()) {
									(*mtmp)[1].raise(nexps[i]);
								}
							}
						} else {
							mtmp->set(xvar2);
							if(!nexps[i].isOne()) {
								mtmp->raise(nexps[i]);
							}
						}
					}
					mtest[mtest.size() - 1].multiply(m_zero, true);
					mtest[mtest.size() - 1][1].raise(m_minus_one);
					for(size_t i = 0; i < dexps.size(); i++) {
						if(mtest[mtest.size() - 1][1][0].isZero()) {
							mtmp = &mtest[mtest.size() - 1][1][0];
						} else {
							mtest[mtest.size() - 1][1][0].add(m_zero, true);
							mtmp = &mtest[mtest.size() - 1][1][0][mtest[mtest.size() - 1][1][0].size() - 1];
						}
						if(!dfactors[i].isOne()) {
							mtmp->set(dfactors[i]);
							if(!dexps[i].isZero()) {
								mtmp->multiply(xvar2);
								if(!dexps[i].isOne()) {
									(*mtmp)[1].raise(dexps[i]);
								}
							}
						} else {
							mtmp->set(xvar2);
							if(!dexps[i].isOne()) {
								mtmp->raise(dexps[i]);
							}
						}
					}
				}
				if(mnum != mnum2) {
					mtest.inverse();
				}
				EvaluationOptions eo2 = eo;
				eo2.do_polynomial_division = false;
				eo2.reduce_divisions = true;
				mtest.calculatesub(eo2, eo2);
				if(mtest.countTotalChilds() < countTotalChilds()) {
					set_nocopy(mtest, true);
					return 1;
				}	
			}
		}
		if(mnum == mnum2) {
			mnum = mden;
			mden = mnum2;
			goto try_polynomial_division;
		}
		mnum = mnum2;
		mden = mden2;
	}
	if(mnum && mden && eo.reduce_divisions && mden->isAddition() && mden->size() > 1) {
		switch(mnum->type()) {
			case STRUCT_ADDITION: {
				break;
			}
			case STRUCT_MULTIPLICATION: {
				Number nr;
				vector<Number> nrs;
				vector<size_t> reducables;
				for(size_t i = 0; i < mnum->size(); i++) {
					switch((*mnum)[i].type()) {
						case STRUCT_ADDITION: {break;}
						case STRUCT_POWER: {
							if((*mnum)[i][1].isNumber() && (*mnum)[i][1].number().isReal()) {
								if((*mnum)[i][1].number().isPositive()) {
									nr.set((*mnum)[i][1].number());
									if(reducable((*mnum)[i][0], *mden, nr)) {
										nrs.push_back(nr);
										reducables.push_back(i);
									}
								}
								break;
							}
						}
						default: {
							nr.set(1, 1);
							if(reducable((*mnum)[i], *mden, nr)) {
								nrs.push_back(nr);
								reducables.push_back(i);
							}
						}
					}
				}
				if(reducables.size() > 0) {
					if(mnum == this) {
						transform(STRUCT_MULTIPLICATION, mstruct);
					} else {
						transform(STRUCT_MULTIPLICATION);
						PREPEND(mstruct);
					}
					size_t i_erased = 0;
					for(size_t i = 0; i < reducables.size(); i++) {
						switch(CHILD(0)[reducables[i] - i_erased].type()) {
							case STRUCT_POWER: {
								if(CHILD(0)[reducables[i] - i_erased][1].isNumber() && CHILD(0)[reducables[i] - i_erased][1].number().isReal()) {
									reduce(CHILD(0)[reducables[i] - i_erased][0], CHILD(1)[0], nrs[i]);
									if(nrs[i] == CHILD(0)[reducables[i] - i_erased][1].number()) {
										CHILD(0).delChild(reducables[i] - i_erased + 1);
										i_erased++;
									} else {
										CHILD(0)[reducables[i] - i_erased][1].number() -= nrs[i];
										if(CHILD(0)[reducables[i] - i_erased][1].number().isOne()) {
											CHILD(0)[reducables[i] - i_erased].setToChild(1, true);
										}
									}
									break;
								}
							}
							default: {
								reduce(CHILD(0)[reducables[i] - i_erased], CHILD(1)[0], nrs[i]);
								if(nrs[i].isOne()) {
									CHILD(0).delChild(reducables[i] - i_erased + 1);
									i_erased++;
								} else {
									MathStructure mexp(1, 1);
									mexp.number() -= nrs[i];
									CHILD(0)[reducables[i] - i_erased].raise(mexp);
								}
							}
						}
					}
					if(CHILD(0).size() == 0) {
						setToChild(2, true);
					} else if(CHILD(0).size() == 1) {
						CHILD(0).setToChild(1, true);
					}
					return 1;
				}
				break;
			}
			case STRUCT_POWER: {
				if((*mnum)[1].isNumber() && (*mnum)[1].number().isReal()) {
					if((*mnum)[1].number().isPositive()) {
						Number nr((*mnum)[1].number());
						if(reducable((*mnum)[0], *mden, nr)) {
							if(nr != (*mnum)[1].number()) {
								MathStructure mnum2((*mnum)[0]);
								if(mnum == this) {
									CHILD(1).number() -= nr;
									if(CHILD(1).number().isOne()) {
										set(mnum2);
									}
									transform(STRUCT_MULTIPLICATION, mstruct);
								} else {
									transform(STRUCT_MULTIPLICATION);
									PREPEND(mstruct);
									CHILD(0)[1].number() -= nr;
									if(CHILD(0)[1].number().isOne()) {
										CHILD(0) = mnum2;
									}
								}
								reduce(mnum2, CHILD(1)[0], nr);
							} else {
								if(mnum == this) {
									MathStructure mnum2((*mnum)[0]);
									set_nocopy(mstruct, true);
									reduce(mnum2, CHILD(0), nr);
								} else {
									reduce((*mnum)[0], CHILD(0), nr);
								}
							}
							return 1;
						}
					}
					break;
				}
			}
			default: {
				Number nr(1, 1);
				if(reducable(*mnum, *mden, nr)) {
					if(mnum == this) {
						MathStructure mnum2(*mnum);
						set_nocopy(mstruct, true);
						reduce(mnum2, CHILD(0), nr);
					} else {
						reduce(*mnum, CHILD(0), nr);
					}
					return 1;
				}
			}
		}
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(isMatrix() && mstruct.isMatrix()) {
						if(CHILD(0).size() != mstruct.size()) {
							CALCULATOR->error(true, _("The second matrix must have as many rows (was %s) as the first has columns (was %s) for matrix multiplication."), i2s(mstruct.size()).c_str(), i2s(CHILD(0).size()).c_str(), NULL);
							return -1;
						}
						MathStructure msave(*this);
						clearMatrix(true);
						resizeMatrix(size(), mstruct[0].size(), m_zero);
						MathStructure mtmp;
						for(size_t index_r = 0; index_r < SIZE; index_r++) {
							for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
								for(size_t index = 0; index < msave[0].size(); index++) {
									mtmp = msave[index_r][index];
									mtmp *= mstruct[index][index_c];
									CHILD(index_r)[index_c] += mtmp;
								}			
							}		
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					} else {
						if(SIZE == mstruct.size()) {
							for(size_t i = 0; i < SIZE; i++) {
								CHILD(i) *= mstruct[i];
							}
							m_type = STRUCT_ADDITION;
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
					return -1;
				}
				default: {
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i) *= mstruct;
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
		}
		case STRUCT_ADDITION: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					return 0;
				}
				case STRUCT_ADDITION: {
					MathStructure msave(*this);
					CLEAR;
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND(msave);
						CHILD(i) *= mstruct[i];
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				case STRUCT_POWER: {
					if(mstruct[1].isNumber() && *this == mstruct[0]) {
						if(representsNonNegative(true) || mstruct[1].representsPositive(true)) {
							set_nocopy(mstruct, true);
							CHILD(1) += m_one;
							return 1;
						}
					}
					if(mstruct[1].hasNegativeSign()) {
						int ret;
						vector<bool> merged;
						merged.resize(SIZE, false);
						size_t merges = 0;
						MathStructure mstruct2(mstruct);
						for(size_t i = 0; i < SIZE; i++) {
							if(CHILD(i).isOne()) ret = -1;
							else ret = CHILD(i).merge_multiplication(mstruct2, eo, false);
							if(ret == 1 && mstruct.size() > 0) {
								mstruct2 = mstruct;
							} else if(ret == 0) {
								MathStructure mstruct3(mstruct);
								ret = mstruct3.merge_multiplication(CHILD(i), eo, false);
								if(ret == 1) {
									CHILD(i) = mstruct3;
								}
							}
							if(ret == 1) {
								merged[i] = true;
								merges++;
							} else {								
								merged[i] = false;
							}
						}
						if(merges == 0) {
							return -1;
						} else if(merges == SIZE) {
							return 1;
						} else if(merges == SIZE - 1) {
							for(size_t i = 0; i < SIZE; i++) {
								if(!merged[i]) {
									CHILD(i) *= mstruct;
									break;
								}
							}
						} else {
							MathStructure mdiv;
							merges = 0;
							for(size_t i = 0; i - merges < SIZE; i++) {
								if(!merged[i]) {
									if(merges > 0) {
										mdiv[0].add(CHILD(i - merges), merges > 1);
									} else {
										mdiv *= mstruct;
										mdiv[0] = CHILD(i - merges);
									}
									ERASE(i - merges);
									merges++;
								}
							}
							add(mdiv, SIZE > 1);
						}
						return 1;
					}
				}
				case STRUCT_MULTIPLICATION: {
				}
				default: {
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i).multiply(mstruct);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			return -1;
		}
		case STRUCT_MULTIPLICATION: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {}
				case STRUCT_ADDITION: {
					return 0;
				}
				case STRUCT_MULTIPLICATION: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND_REF(&mstruct[i]);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				case STRUCT_POWER: {
					if(mstruct[1].isNumber()) {
						if(*this == mstruct[0]) {
							if(representsNonNegative(true) || mstruct[1].representsPositive(true)) {
								set_nocopy(mstruct, true);
								CHILD(1) += m_one;
								return 1;
							}
						} else {
							for(size_t i = 0; i < SIZE; i++) {
								int ret = CHILD(i).merge_multiplication(mstruct, eo, false);
								if(ret == 0) {
									ret = mstruct.merge_multiplication(CHILD(i), eo, false);
									if(ret == 1) {
										CHILD(i).set_nocopy(mstruct);
									}
								}
								if(ret == 1) {
									return 1;
								}	
							}
						}
					}
				}
				default: {
					if(do_append) {
						APPEND_REF(&mstruct);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}
			}
			return -1;
		}
		case STRUCT_POWER: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {}
				case STRUCT_ADDITION: {}
				case STRUCT_MULTIPLICATION: {
					return 0;
				}
				case STRUCT_POWER: {
					if(mstruct[0] == CHILD(0)) {
						if(CHILD(0).representsNonZero(true) || (eo.assume_denominators_nonzero && !CHILD(1).isZero()) || (CHILD(1).representsPositive(true) && mstruct[1].representsPositive(true))) {
							MathStructure mstruct2(CHILD(1));
							mstruct2 += mstruct[1];
							if(mstruct2.calculatesub(eo, eo)) {
								CHILD(1) = mstruct2;
								return 1;
							}
						}
					} else if(mstruct[1] == CHILD(1)) {
						MathStructure mstruct2(CHILD(0));
						mstruct2 *= mstruct[0];
						if(mstruct2.calculatesub(eo, eo)) {
							CHILD(0) = mstruct2;
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
					break;
				}
				default: {
					if(!mstruct.isNumber() && CHILD(1).isNumber() && CHILD(0) == mstruct) {
						if(CHILD(0).representsNonZero(true) || (eo.assume_denominators_nonzero && !CHILD(1).isZero()) || CHILD(1).representsPositive(true)) {
							CHILD(1) += m_one;
							MERGE_APPROX_AND_PREC(mstruct)
							return 1;
						}
					}
					if(mstruct.isZero() && !containsType(STRUCT_UNIT, false, true, true)) {
						clear(true);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
					break;
				}
			}
			return -1;
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {}
				case STRUCT_ADDITION: {}
				case STRUCT_MULTIPLICATION: {}
				case STRUCT_POWER: {
					return 0;
				}
				default: {
					if((mstruct.isZero() && !containsType(STRUCT_UNIT, false, true, true) && !representsUndefined(true, true)) || (isZero() && !mstruct.representsUndefined(true, true) && !mstruct.containsType(STRUCT_UNIT, false, true, true))) {
						clear(true); 
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
					if(equals(mstruct)) {
						raise(2);
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
					break;
				}
			}
			break;
		}			
	}
	return -1;
}

int MathStructure::merge_power(MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.raise(mstruct.number()) && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.isInfinite() || o_number.isInfinite() || mstruct.number().isInfinite())) {
			o_number = nr;
			numberUpdated();
			return 1;
		}
		if(mstruct.number().isRational()) {
			if(!mstruct.number().numeratorIsOne()) {
				nr = o_number;
				if(nr.raise(mstruct.number().numerator()) && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.isInfinite() || o_number.isInfinite() || mstruct.number().isInfinite())) {
					o_number = nr;
					numberUpdated();
					nr.set(mstruct.number().denominator());
					nr.recip();
					raise(nr);
					return 1;
				}
			} else if(eo.split_squares && o_number.isInteger() && mstruct.number().denominatorIsTwo()) {
				nr.set(1, 1);
				bool b = true, overflow;
				int val;
				while(b) {
					b = false;
					overflow = false;
					val = o_number.intValue(&overflow);
					if(overflow) {
						cln::cl_I cval = cln::numerator(cln::rational(cln::realpart(o_number.internalNumber())));
						for(size_t i = 0; i < NR_OF_PRIMES; i++) {
							if(cln::zerop(cln::rem(cval, SQUARE_PRIMES[i]))) {
								nr *= PRIMES[i];
								o_number /= SQUARE_PRIMES[i];
								b = true;
								break;
							}
						}
					} else {
						for(size_t i = 0; i < NR_OF_PRIMES; i++) {
							if(SQUARE_PRIMES[i] > val) {
								break;
							} else if(val % SQUARE_PRIMES[i] == 0) {
								nr *= PRIMES[i];
								o_number /= SQUARE_PRIMES[i];
								b = true;
								break;
							}
						}
					}
				}
				if(!nr.isOne()) {
					transform(STRUCT_MULTIPLICATION);
					CHILD(0) ^= mstruct;
					PREPEND(nr);
					return 1;
				}
			}
		}
		return -1;
	}
	if(mstruct.isOne()) {
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	if(m_type == STRUCT_NUMBER && o_number.isInfinite()) {
		if(mstruct.representsNegative(false)) {
			o_number.clear();
			MERGE_APPROX_AND_PREC(mstruct)
			return 1;
		} else if(mstruct.representsNonZero(false) && mstruct.representsNumber(false)) {
			if(o_number.isMinusInfinity()) {
				if(mstruct.representsEven(false)) {
					o_number.setPlusInfinity();
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				} else if(mstruct.representsOdd(false)) {
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				} else if(mstruct.representsInteger(false)) {
					o_number.setInfinity();
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
		}
	} else if(mstruct.isNumber() && mstruct.number().isInfinite()) {
		if(mstruct.number().isInfinity()) {
		} else if(mstruct.number().isMinusInfinity()) {
			if(representsReal(false) && representsNonZero(false)) {
				clear(true);
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			}
		} else if(mstruct.number().isPlusInfinity()) {
			if(representsPositive(false)) {
				set_nocopy(mstruct, true);
				return 1;
			}
		}
	}
	if(representsUndefined() || mstruct.representsUndefined()) return -1;
	if(isZero() && mstruct.representsPositive(true)) {
		return 1;
	}
	if(mstruct.isZero() && !representsUndefined(true, true)) {
		set(m_one);
		MERGE_APPROX_AND_PREC(mstruct)
		return 1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			if(mstruct.isNumber() && mstruct.number().isInteger()) {
				if(isMatrix()) {
					if(matrixIsSymmetric()) {
						Number nr(mstruct.number());
						bool b_neg = false;
						if(nr.isNegative()) {
							nr.setNegative(false);
							b_neg = true;
						}
						if(!nr.isOne()) {
							MathStructure msave(*this);
							nr--;
							while(nr.isPositive()) {
								merge_multiplication(msave, eo);
								nr--;
							}
						}
						if(b_neg) {
							invertMatrix(eo);
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
					return -1;
				} else {
					if(mstruct.number().isMinusOne()) {
						return -1;
					}
					Number nr(mstruct.number());
					if(nr.isNegative()) {
						nr++;
					} else {
						nr--;
					}
					MathStructure msave(*this);
					merge_multiplication(msave, eo);
					raise(nr);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				
			}
			goto default_power_merge;
		}
		case STRUCT_ADDITION: {
			if(mstruct.isNumber() && mstruct.number().isInteger()) {
				if(mstruct.number().isMinusOne()) {
					bool bnum = false;
					for(size_t i = 0; !bnum && i < SIZE; i++) {
						switch(CHILD(i).type()) {
							case STRUCT_NUMBER: {
								if(!CHILD(i).number().isZero() && CHILD(i).number().isReal()) {
									bnum = true;
								}
								break;
							}
							case STRUCT_MULTIPLICATION: {
								if(CHILD(i).size() > 0 && CHILD(i)[0].isNumber()) {
									if(!CHILD(i)[0].number().isZero() && CHILD(i)[0].number().isReal()) {
										bnum = true;
									}
									break;
								}
							}
						}
					}
					if(bnum) {
						Number nr;
						size_t negs = 0;
						for(size_t i = 0; i < SIZE; i++) {
							switch(CHILD(i).type()) {
								case STRUCT_NUMBER: {
									if(!CHILD(i).number().isZero() && CHILD(i).number().isReal()) {
										if(CHILD(i).number().isNegative()) negs++;
										if(nr.isZero()) {
											nr = CHILD(i).number();
										} else {
											if(negs) {
												if(CHILD(i).number().isNegative()) {
													nr.setNegative(true);
													if(CHILD(i).number().isGreaterThan(nr)) {
														nr = CHILD(i).number();
													}
													break;
												} else {
													nr.setNegative(false);
												}
											}
											if(CHILD(i).number().isLessThan(nr)) {
												nr = CHILD(i).number();
											}
										}
									}
									break;
								}
								case STRUCT_MULTIPLICATION: {
									if(CHILD(i).size() > 0 && CHILD(i)[0].isNumber()) {
										if(!CHILD(i)[0].number().isZero() && CHILD(i)[0].number().isReal()) {
											if(CHILD(i)[0].number().isNegative()) negs++;
											if(nr.isZero()) {
												nr = CHILD(i)[0].number();
											} else {
												if(negs) {
													if(CHILD(i)[0].number().isNegative()) {
														nr.setNegative(true);
														if(CHILD(i)[0].number().isGreaterThan(nr)) {
															nr = CHILD(i)[0].number();
														}
														break;
													} else {
														nr.setNegative(false);
													}
												}
												if(CHILD(i)[0].number().isLessThan(nr)) {
													nr = CHILD(i)[0].number();
												}
											}
										}
										break;
									}
								}
								default: {
									if(nr.isZero() || !nr.isFraction()) {
										nr.set(1, 1);
									}
								}
							}
							
						}
						nr.setNegative(negs > SIZE - negs);
						if(bnum && !nr.isOne() && !nr.isZero()) {
							nr.recip();
							for(size_t i = 0; i < SIZE; i++) {
								switch(CHILD(i).type()) {
									case STRUCT_NUMBER: {
										CHILD(i).number() *= nr;
										break;
									}
									case STRUCT_MULTIPLICATION: {
										CHILD(i)[0].number() *= nr;
										break;
									}
									default: {
										CHILD(i) *= nr;
									}
								}
							}
							raise(mstruct);
							multiply(nr);
							return 1;
						}
					}
				} else if(eo.simplify_addition_powers && !mstruct.number().isZero()) {
					bool neg = mstruct.number().isNegative();
					MathStructure mstruct1(CHILD(0));
					MathStructure mstruct2(CHILD(1));
					for(size_t i = 2; i < SIZE; i++) {
						mstruct2.add(CHILD(i), true);
					}
					Number m(mstruct.number());
					m.setNegative(false);
					Number k(1);
					Number p1(m);
					Number p2(1);
					p1--;
					Number bn;
					CLEAR
					APPEND(mstruct1);
					CHILD(0).raise(m);
					while(k.isLessThan(m)) {
						bn.binomial(m, k);
						APPEND_NEW(bn);
						LAST.multiply(mstruct1);
						if(!p1.isOne()) {
							LAST[1].raise_nocopy(new MathStructure(p1));
						}
						LAST.multiply(mstruct2, true);
						if(!p2.isOne()) {
							LAST[2].raise_nocopy(new MathStructure(p2));
						}
						k++;
						p1--;
						p2++;
					}
					APPEND(mstruct2);
					LAST.raise(m);
					if(neg) inverse();
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			goto default_power_merge;
		}
		case STRUCT_MULTIPLICATION: {
			if(mstruct.representsInteger(false)) {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).raise(mstruct);	
				}
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else {
				bool b = true;
				for(size_t i = 0; i < SIZE; i++) {
					if(!CHILD(i).representsNonNegative(true)) {
						b = false;
					}
				}
				if(b) {
					for(size_t i = 0; i < SIZE; i++) {
						CHILD(i).raise(mstruct);	
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			goto default_power_merge;
		}
		case STRUCT_POWER: {
			if(mstruct.representsInteger(false) || CHILD(0).representsReal(true)) {
				if(CHILD(1).isNumber() && CHILD(1).number().isRational()) {
					if(CHILD(1).number().numeratorIsEven() && !mstruct.representsInteger(false)) {
						if(CHILD(0).representsNegative(true)) {
							CHILD(0).negate();							
						} else if(!CHILD(0).representsNonNegative(true)) {
							MathStructure mstruct_base(CHILD(0));
							CHILD(0).set(CALCULATOR->f_abs, &mstruct_base, NULL);
						}
					}
					CHILD(1).multiply(mstruct);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			goto default_power_merge;
		}
		case STRUCT_VARIABLE: {
			if(o_variable == CALCULATOR->v_e) {
				if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi) {
					if(mstruct[0].isNumber()) {
						if(mstruct[0].number().isI() || mstruct[0].number().isMinusI()) {
							set(m_minus_one, true);
							return 1;
						} else if(mstruct[0].number().isComplex() && !mstruct[0].number().hasRealPart()) {
							Number img(mstruct[0].number().imaginaryPart());
							if(img.isInteger()) {
								if(img.isEven()) {
									set(m_one, true);
								} else {
									set(m_minus_one, true);
								}
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							} else if(img == Number(1, 2)) {
								clear(true);
								img.set(1, 1);
								o_number.setImaginaryPart(img);
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							} else if(img == Number(-1, 2)) {
								clear(true);
								img.set(-1, 1);
								o_number.setImaginaryPart(img);
								MERGE_APPROX_AND_PREC(mstruct)
								return 1;
							}
						}
					}
				} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_ln && mstruct.size() == 1) {
					if(mstruct[0].representsPositive(false) || mstruct[0].representsComplex(false)) {
						set(mstruct[0], true);
						break;
					}
				}
			}
			goto default_power_merge;
		}
		default: {
			default_power_merge:
			if(mstruct.isAddition()) {
				MathStructure msave(*this);
				clear(true);
				m_type = STRUCT_MULTIPLICATION;
				for(size_t i = 0; i < mstruct.size(); i++) {
					APPEND(msave);
					LAST.raise(mstruct[i]);
				}
				MERGE_APPROX_AND_PREC(mstruct)
				return 1;
			} else if(mstruct.isMultiplication() && mstruct.size() > 1) {
				MathStructure mthis(*this);
				for(size_t i = 0; i < mstruct.size(); i++) {
					if(i == 0) mthis.raise(mstruct[i]);
					else mthis[1] = mstruct[i];
					if(mthis.calculatesub(eo, eo)) {
						set(mthis);
						if(mstruct.size() == 2) {
							if(i == 0) raise(mstruct[1]);
							else raise(mstruct[0]);
						} else {
							raise(mstruct);
							CHILD(1).delChild(i + 1);
						}
						MERGE_APPROX_AND_PREC(mstruct)
						return 1;
					}
				}
			}
			break;
		}
	}
	return -1;
}

int MathStructure::merge_bitwise_and(MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.bitAnd(mstruct.number()) && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.isInfinite() || o_number.isInfinite() || mstruct.number().isInfinite())) {
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(SIZE < mstruct.size()) return 0;
					for(size_t i = 0; i < mstruct.size(); i++) {
						CHILD(i).add(mstruct[i], OPERATION_LOGICAL_AND);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					return -1;
				}
			}
			return -1;
		}
		case STRUCT_BITWISE_AND: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					return -1;
				}
				case STRUCT_BITWISE_AND: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND(mstruct[i]);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					APPEND_REF(&mstruct);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			break;
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_BITWISE_AND: {
					return 0;
				}
			}	
		}
	}
	return -1;
}
int MathStructure::merge_bitwise_or(MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.bitOr(mstruct.number()) && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.isInfinite() || o_number.isInfinite() || mstruct.number().isInfinite())) {
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(SIZE < mstruct.size()) return 0;
					for(size_t i = 0; i < mstruct.size(); i++) {
						CHILD(i).add(mstruct[i], OPERATION_LOGICAL_OR);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					return -1;
				}
			}
			return -1;
		}
		case STRUCT_BITWISE_OR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					return -1;
				}
				case STRUCT_BITWISE_OR: {
					for(size_t i = 0; i < mstruct.size(); i++) {
						APPEND(mstruct[i]);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					APPEND_REF(&mstruct);
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
			}
			break;
		}
		default: {
			switch(mstruct.type()) {
				case STRUCT_BITWISE_OR: {
					return 0;
				}
			}	
		}
	}
	return -1;
}
int MathStructure::merge_bitwise_xor(MathStructure &mstruct, const EvaluationOptions &eo) {
	if(mstruct.type() == STRUCT_NUMBER && m_type == STRUCT_NUMBER) {
		Number nr(o_number);
		if(nr.bitXor(mstruct.number()) && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || o_number.isApproximate() || mstruct.number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || o_number.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.isInfinite() || o_number.isInfinite() || mstruct.number().isInfinite())) {
			o_number = nr;
			numberUpdated();
			return 1;
		}
		return -1;
	}
	switch(m_type) {
		case STRUCT_VECTOR: {
			switch(mstruct.type()) {
				case STRUCT_VECTOR: {
					if(SIZE < mstruct.size()) return 0;
					for(size_t i = 0; i < mstruct.size(); i++) {
						CHILD(i).add(mstruct[i], OPERATION_LOGICAL_XOR);
					}
					MERGE_APPROX_AND_PREC(mstruct)
					return 1;
				}
				default: {
					return -1;
				}
			}
			return -1;
		}
	}
	return -1;
}

bool MathStructure::calculatesub(const EvaluationOptions &eo, const EvaluationOptions &feo) {
	bool b = true;
	bool c = false;
	while(b) {
		b = false;
		switch(m_type) {
			case STRUCT_VARIABLE: {
				if(eo.calculate_variables && o_variable->isKnown()) {
					if(eo.approximation == APPROXIMATION_APPROXIMATE || !o_variable->isApproximate()) {
						set(((KnownVariable*) o_variable)->get());
						if(eo.calculate_functions) {
							calculateFunctions(feo);
							unformat(feo);
						}
						b = true;
					}
				}
				break;
			}
			case STRUCT_POWER: {
				CHILD(0).calculatesub(eo, feo);
				CHILD(1).calculatesub(eo, feo);
				CHILDREN_UPDATED;
				if(CHILD(0).merge_power(CHILD(1), eo) == 1) {
					setToChild(1);
					b = true;
				}
				break;
			}
			case STRUCT_ADDITION: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}
				CHILDREN_UPDATED;
				evalSort();
				for(size_t i = 0; i < SIZE - 1; i++) {
					size_t i2 = i + 1;
					while(i2 < SIZE) {
						int r = CHILD(i).merge_addition(CHILD(i2), eo);
						if(r == 0) {
							SWAP_CHILDS(i, i2);
							r = CHILD(i).merge_addition(CHILD(i2), eo);
							if(r != 1) {
								SWAP_CHILDS(i, i2);
							}
						}
						if(r == 1) {
							ERASE(i2);
							b = true;
						} else {
							i2++;
						}
					}
				}
				if(SIZE == 1) {
					set_nocopy(CHILD(0));
				} else if(SIZE == 0) {
					clear(true);
				}
				break;
			}
			case STRUCT_MULTIPLICATION: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}				
				CHILDREN_UPDATED;
				evalSort();
				for(size_t i = 0; i < SIZE - 1; i++) {					
					for(size_t i2 = i + 1; i2 < SIZE;) {
						int r = CHILD(i).merge_multiplication(CHILD(i2), eo);
						if(r == 0) {
							SWAP_CHILDS(i, i2);
							r = CHILD(i).merge_multiplication(CHILD(i2), eo);
							if(r != 1) {
								SWAP_CHILDS(i, i2);
							}
						}
						if(r == 1) {
							ERASE(i2);
							b = true;
						} else {
							i2++;
						}
					}
				}
				if(SIZE == 1) {
					set_nocopy(CHILD(0));
				} else if(SIZE == 0) {
					clear(true);
				}
				break;
			}
			case STRUCT_BITWISE_AND: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}				
				CHILDREN_UPDATED;
				evalSort();
				for(size_t i = 0; i < SIZE - 1; i++) {					
					for(size_t i2 = i + 1; i2 < SIZE;) {
						int r = CHILD(i).merge_bitwise_and(CHILD(i2), eo);
						if(r == 0) {
							SWAP_CHILDS(i, i2);
							r = CHILD(i).merge_bitwise_and(CHILD(i2), eo);
							if(r != 1) {
								SWAP_CHILDS(i, i2);
							}
						}
						if(r == 1) {
							ERASE(i2);
							b = true;
						} else {
							i2++;
						}
					}
				}
				if(SIZE == 1) {
					set_nocopy(CHILD(0));
				} else if(SIZE == 0) {
					clear(true);
				}
				break;
			}
			case STRUCT_BITWISE_OR: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}				
				CHILDREN_UPDATED;
				evalSort();
				for(size_t i = 0; i < SIZE - 1; i++) {					
					for(size_t i2 = i + 1; i2 < SIZE;) {
						int r = CHILD(i).merge_bitwise_or(CHILD(i2), eo);
						if(r == 0) {
							SWAP_CHILDS(i, i2);
							r = CHILD(i).merge_bitwise_or(CHILD(i2), eo);
							if(r != 1) {
								SWAP_CHILDS(i, i2);
							}
						}
						if(r == 1) {
							ERASE(i2);
							b = true;
						} else {
							i2++;
						}
					}
				}
				if(SIZE == 1) {
					set_nocopy(CHILD(0));
				} else if(SIZE == 0) {
					clear(true);
				}
				break;
			}
			case STRUCT_BITWISE_XOR: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}				
				CHILDREN_UPDATED;
				evalSort();
				for(size_t i = 0; i < SIZE - 1; i++) {					
					for(size_t i2 = i + 1; i2 < SIZE;) {
						int r = CHILD(i).merge_bitwise_xor(CHILD(i2), eo);
						if(r == 0) {
							SWAP_CHILDS(i, i2);
							r = CHILD(i).merge_bitwise_xor(CHILD(i2), eo);
							if(r != 1) {
								SWAP_CHILDS(i, i2);
							}
						}
						if(r == 1) {
							ERASE(i2);
							b = true;
						} else {
							i2++;
						}
					}
				}
				if(SIZE == 1) {
					set_nocopy(CHILD(0));
				} else if(SIZE == 0) {
					clear(true);
				}
				break;
			}
			case STRUCT_BITWISE_NOT: {
				CHILD(0).calculatesub(eo, feo);
				CHILDREN_UPDATED;
				switch(CHILD(0).type()) {
					case STRUCT_NUMBER: {
						Number nr(CHILD(0).number());
						if(nr.bitNot() && (eo.approximation == APPROXIMATION_APPROXIMATE || !nr.isApproximate() || CHILD(0).number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || CHILD(0).number().isComplex()) && (eo.allow_infinite || !nr.isInfinite() || CHILD(0).number().isInfinite())) {
							set(nr, true);
						}
						break;
					}
					case STRUCT_VECTOR: {
						SET_CHILD_MAP(0);
						for(size_t i = 0; i < SIZE; i++) {
							CHILD(i).setLogicalNot();
						}
						break;
					}
					case STRUCT_BITWISE_NOT: {
						set_nocopy(CHILD(0));
						set_nocopy(CHILD(0));
						break;
					}
				}
				break;
			}
			case STRUCT_LOGICAL_AND: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}
				CHILDREN_UPDATED;
				bool is_true = true;
				size_t i = 0;
				while(i < SIZE) {
					if(CHILD(i).representsNonPositive(true)) {
						clear(true);
						b = true;
						is_true = false;
						break;
					} else if(CHILD(i).representsPositive(true)) {
						b = true;
						ERASE(i);
					} else {
						i++;
						is_true = false;
					}
				}
				if(is_true) {
					set(m_one);
				} else if(m_type == STRUCT_LOGICAL_AND && SIZE == 1) {
					APPEND(m_zero);
					m_type = STRUCT_COMPARISON;
					ct_comp = COMPARISON_GREATER;
				}
				break;
			}
			case STRUCT_LOGICAL_OR: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}
				CHILDREN_UPDATED;
				bool is_false = true;
				for(size_t i = 0; i < SIZE;) {
					if(CHILD(i).representsNonPositive(true)) {
						b = true;
						ERASE(i);
					} else if(CHILD(i).representsPositive(true)) {
						b = true;
						set(m_one);
						is_false = false;
						break;
					} else {
						i++;
						is_false = false;
					}
				}
				if(is_false) {
					clear(true);
				} else if(m_type == STRUCT_LOGICAL_OR && SIZE == 1) {
					APPEND(m_zero);
					m_type = STRUCT_COMPARISON;
					ct_comp = COMPARISON_GREATER;
				}
				break;
			}
			case STRUCT_LOGICAL_XOR: {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).calculatesub(eo, feo);
				}
				CHILDREN_UPDATED;
				bool b0, b1;
				if(CHILD(0).representsNonPositive(true)) {
					b0 = false;
				} else if(CHILD(0).representsPositive(true)) {
					b0 = true;
				} else {
					break;
				}
				if(CHILD(1).representsNonPositive(true)) {
					b1 = false;
				} else if(CHILD(1).representsPositive(true)) {
					b1 = true;
				} else {
					break;
				}
				b = true;
				if((b0 && !b1) || (!b0 && b1)) {
					set(m_one, true);
				} else {
					clear(true);
				}
				break;
			}
			case STRUCT_LOGICAL_NOT: {
				CHILD(0).calculatesub(eo, feo);
				CHILDREN_UPDATED;
				if(CHILD(0).representsPositive(true)) {
					clear(true);
					b = true;
				} else if(CHILD(0).representsNonPositive(true)) {
					set(m_one, true);
					b = true;
				} else if(CHILD(0).isLogicalNot()) {
					set_nocopy(CHILD(0));
					set_nocopy(CHILD(0));
					if(m_type != STRUCT_LOGICAL_NOT && m_type != STRUCT_LOGICAL_AND && m_type != STRUCT_LOGICAL_OR && m_type != STRUCT_LOGICAL_XOR && m_type != STRUCT_COMPARISON) {
						add(m_zero, OPERATION_GREATER);
					}
					b = true;
				}
				break;
			}
			case STRUCT_COMPARISON: {
				if(!eo.test_comparisons) {
					CHILD(0).calculatesub(eo, feo);
					CHILD(1).calculatesub(eo, feo);
					CHILDREN_UPDATED;
					b = false;
					break;
				}
				if((ct_comp == COMPARISON_EQUALS_LESS || ct_comp == COMPARISON_GREATER) && CHILD(1).isZero()) {
					if(CHILD(0).isLogicalNot() || CHILD(0).isLogicalAnd() || CHILD(0).isLogicalOr() || CHILD(0).isLogicalXor() || CHILD(0).isComparison()) {
						if(ct_comp == COMPARISON_EQUALS_LESS) {
							ERASE(1);
							m_type = STRUCT_LOGICAL_NOT;
						} else {
							set_nocopy(CHILD(0));
						}
						b = true;
					}
				} else if((ct_comp == COMPARISON_EQUALS_GREATER || ct_comp == COMPARISON_LESS) && CHILD(0).isZero()) {
					if(CHILD(0).isLogicalNot() || CHILD(1).isLogicalAnd() || CHILD(1).isLogicalOr() || CHILD(1).isLogicalXor() || CHILD(1).isComparison()) {
						if(ct_comp == COMPARISON_EQUALS_GREATER) {
							ERASE(0);
							m_type = STRUCT_LOGICAL_NOT;
						} else {
							set_nocopy(CHILD(1));
						}
						b = true;
					}
				}
				if(b) break;
				if(ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS) {
					if((CHILD(0).representsReal(false) && CHILD(1).representsComplex(false)) || (CHILD(1).representsReal(false) && CHILD(0).representsComplex(false))) {
						if(ct_comp == COMPARISON_EQUALS) {
							clear(true);
						} else {
							clear(true);
							o_number.set(1, 1);
						}
						b = true;
						break;
					}
				}
				if(!CHILD(1).isZero()) {
					MathStructure *mchild = new MathStructure();
					mchild->set_nocopy(CHILD(1));
					CHILD(0).subtract_nocopy(mchild);
					CHILD(0).calculatesub(eo, feo);
					CHILD(1).clear();
					b = true;
				} else {
					CHILD(0).calculatesub(eo, feo);
				}
				CHILDREN_UPDATED;
				bool incomp = false;
				if(CHILD(0).isAddition()) {
					for(size_t i = 1; i < CHILD(0).size(); i++) {
						if(CHILD(0)[i - 1].isUnitCompatible(CHILD(0)[i]) == 0) {
							incomp = true;
							break;
						}
					}
				}
				switch(ct_comp) {
					case COMPARISON_EQUALS: {
						if(incomp) {
							clear(true);
							b = true;
						} else if(CHILD(0).isZero()) {
							clear(true);
							o_number.set(1, 1);
							b = true;	
						} else if(CHILD(0).representsNonZero(true)) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_NOT_EQUALS:  {
						if(incomp) {
							clear(true);
							o_number.set(1, 1);
							b = true;
						} else if(CHILD(0).representsNonZero(true)) {
							clear(true);
							o_number.set(1, 1);
							b = true;	
						} else if(CHILD(0).isZero()) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_LESS:  {
						if(incomp) {
						} else if(CHILD(0).representsNegative(true)) {
							clear(true);
							o_number.set(1, 1);
							b = true;	
						} else if(CHILD(0).representsNonNegative(true)) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_GREATER:  {
						if(incomp) {
						} else if(CHILD(0).representsPositive(true)) {
							clear(true);
							o_number.set(1, 1);
							b = true;	
						} else if(CHILD(0).representsNonPositive(true)) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_EQUALS_LESS:  {
						if(incomp) {
						} else if(CHILD(0).representsNonPositive(true)) {
							clear(true);
							o_number.set(1, 1);
							b = true;	
						} else if(CHILD(0).representsPositive(true)) {
							clear(true);
							b = true;
						}
						break;
					}
					case COMPARISON_EQUALS_GREATER:  {
						if(incomp) {
						} else if(CHILD(0).representsNonNegative(true)) {
							clear(true);
							o_number.set(1, 1);
							b = true;	
						} else if(CHILD(0).representsNegative(true)) {
							clear(true);
							b = true;
						}
						break;
					}
				}
				break;
			}
			case STRUCT_FUNCTION: {
				if(o_function == CALCULATOR->f_abs) {
					b = calculateFunctions(eo, false);
					unformat(eo);
					break;
				}
			}
			default: {
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).calculatesub(eo, feo)) c = true;
				}
				CHILDREN_UPDATED;
			}
		}
		if(b) c = true;
	}
	return c;
}

bool MathStructure::calculateFunctions(const EvaluationOptions &eo, bool recursive) {
	if(m_type == STRUCT_FUNCTION) {
		if(function_value) {
			function_value->unref();
			function_value = NULL;
		}
		if(!o_function->testArgumentCount(SIZE)) {
			return false;
		}
		if(o_function->maxargs() > -1 && (int) SIZE > o_function->maxargs()) {
			REDUCE(o_function->maxargs());
		}
		m_type = STRUCT_VECTOR;
		Argument *arg = NULL, *last_arg = NULL;
		int last_i = 0;

		for(size_t i = 0; i < SIZE; i++) {
			arg = o_function->getArgumentDefinition(i + 1);
			if(arg) {
				last_arg = arg;
				last_i = i;
				if(!arg->test(CHILD(i), i + 1, o_function, eo)) {
					m_type = STRUCT_FUNCTION;
					CHILD_UPDATED(i);
					return false;
				} else {
					CHILD_UPDATED(i);
				}
			}
		}

		if(last_arg && o_function->maxargs() < 0 && last_i >= o_function->minargs()) {
			for(size_t i = last_i + 1; i < SIZE; i++) {
				if(!last_arg->test(CHILD(i), i + 1, o_function, eo)) {
					m_type = STRUCT_FUNCTION;
					CHILD_UPDATED(i);
					return false;
				} else {
					CHILD_UPDATED(i);
				}
			}
		}

		if(!o_function->testCondition(*this)) {
			m_type = STRUCT_FUNCTION;
			return false;
		}

		MathStructure *mstruct = new MathStructure();
		int i = o_function->calculate(*mstruct, *this, eo);
		if(i > 0) {
			set_nocopy(*mstruct, true);
			if(recursive) calculateFunctions(eo);
			mstruct->unref();
			return true;
		} else {
			if(i < 0) {
				i = -i;
				if(i <= (int) SIZE) {
					CHILD(i - 1).set_nocopy(*mstruct);
					CHILD_UPDATED(i - 1);
				}
			}
			/*if(eo.approximation == APPROXIMATION_EXACT) {
				mstruct->clear();
				EvaluationOptions eo2 = eo;
				eo2.approximation = APPROXIMATION_APPROXIMATE;
				CALCULATOR->beginTemporaryStopMessages();
				if(o_function->calculate(*mstruct, *this, eo2) > 0) {
					function_value = mstruct;
					function_value->ref();
					function_value->calculateFunctions(eo2);
				}
				if(CALCULATOR->endTemporaryStopMessages() > 0 && function_value) {
					function_value->unref();
					function_value = NULL;
				}
			}*/
			m_type = STRUCT_FUNCTION;			
			mstruct->unref();
			return false;
		}
	}
	bool b = false;
	if(recursive) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).calculateFunctions(eo)) {
				CHILD_UPDATED(i);
				b = true;
			}
		}
	}
	return b;
}

int evalSortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent);
int evalSortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent) {
	if(parent.isMultiplication()) {
		if(mstruct1.containsType(STRUCT_VECTOR, false, true, true) != 0 && mstruct2.containsType(STRUCT_VECTOR, false, true, true) != 0) {
			return 0;
		}
	}
	if(parent.isAddition()) {
		if(mstruct1.isMultiplication() && mstruct1.size() > 0) {
			size_t start = 0;
			while(mstruct1[start].isNumber() && mstruct1.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct2.isMultiplication()) {
				if(mstruct2.size() < 1) return -1;
				size_t start2 = 0;
				while(mstruct2[start2].isNumber() && mstruct2.size() > start2 + 1) {
					start2++;
				}
				for(size_t i = 0; i + start < mstruct1.size(); i++) {
					if(i + start2 >= mstruct2.size()) return 1;
					i2 = evalSortCompare(mstruct1[i + start], mstruct2[i + start2], parent);
					if(i2 != 0) return i2;
				}
				if(mstruct1.size() - start == mstruct2.size() - start2) return 0;
				return -1;
			} else {
				i2 = evalSortCompare(mstruct1[start], mstruct2, parent);
				if(i2 != 0) return i2;
			}
		} else if(mstruct2.isMultiplication() && mstruct2.size() > 0) {
			size_t start = 0;
			while(mstruct2[start].isNumber() && mstruct2.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct1.isMultiplication()) {
				return 1;
			} else {
				i2 = evalSortCompare(mstruct1, mstruct2[start], parent);
				if(i2 != 0) return i2;
			}
		} 
	}
	if(mstruct1.type() != mstruct2.type()) {
		if(!parent.isMultiplication()) {
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		if(!parent.isMultiplication() || (!mstruct1.isNumber() && !mstruct2.isNumber())) {
			if(mstruct2.isPower()) {
				int i = evalSortCompare(mstruct1, mstruct2[0], parent);
				if(i == 0) {
					return evalSortCompare(m_one, mstruct2[1], parent);
				}
				return i;
			}
			if(mstruct1.isPower()) {
				int i = evalSortCompare(mstruct1[0], mstruct2, parent);
				if(i == 0) {
					return evalSortCompare(mstruct1[1], m_one, parent);
				}
				return i;
			}
		}
		if(mstruct2.isInverse()) return -1;
		if(mstruct1.isInverse()) return 1;
		if(mstruct2.isDivision()) return -1;
		if(mstruct1.isDivision()) return 1;
		if(mstruct2.isNegate()) return -1;
		if(mstruct1.isNegate()) return 1;
		if(mstruct2.type() == STRUCT_ALTERNATIVES) return -1;
		if(mstruct1.type() == STRUCT_ALTERNATIVES) return 1;
		if(mstruct2.isLogicalAnd()) return -1;
		if(mstruct1.isLogicalAnd()) return 1;
		if(mstruct2.isLogicalOr()) return -1;
		if(mstruct1.isLogicalOr()) return 1;
		if(mstruct2.isLogicalXor()) return -1;
		if(mstruct1.isLogicalXor()) return 1;
		if(mstruct2.isLogicalNot()) return -1;
		if(mstruct1.isLogicalNot()) return 1;
		if(mstruct2.isComparison()) return -1;
		if(mstruct1.isComparison()) return 1;
		if(mstruct2.isBitwiseOr()) return -1;
		if(mstruct1.isBitwiseOr()) return 1;
		if(mstruct2.isBitwiseXor()) return -1;
		if(mstruct1.isBitwiseXor()) return 1;
		if(mstruct2.isBitwiseAnd()) return -1;
		if(mstruct1.isBitwiseAnd()) return 1;
		if(mstruct2.isBitwiseNot()) return -1;
		if(mstruct1.isBitwiseNot()) return 1;
		if(mstruct2.isUndefined()) return -1;
		if(mstruct1.isUndefined()) return 1;
		if(mstruct2.isFunction()) return -1;
		if(mstruct1.isFunction()) return 1;
		if(mstruct2.isAddition()) return -1;
		if(mstruct1.isAddition()) return 1;
		if(mstruct2.isMultiplication()) return -1;
		if(mstruct1.isMultiplication()) return 1;
		if(mstruct2.isPower()) return -1;
		if(mstruct1.isPower()) return 1;
		if(mstruct2.isUnit()) return -1;
		if(mstruct1.isUnit()) return 1;
		if(mstruct2.isSymbolic()) return -1;
		if(mstruct1.isSymbolic()) return 1;
		if(mstruct2.isVariable()) return -1;
		if(mstruct1.isVariable()) return 1;
		if(parent.isMultiplication()) {
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		return -1;
	}
	switch(mstruct1.type()) {
		case STRUCT_NUMBER: {
			if(!mstruct1.number().isComplex() && !mstruct2.number().isComplex()) {
				ComparisonResult cmp = mstruct1.number().compare(mstruct2.number());
				if(cmp == COMPARISON_RESULT_LESS) return -1;
				else if(cmp == COMPARISON_RESULT_GREATER) return 1;
				return 0;
			} else {
				if(!mstruct1.number().hasRealPart()) {
					if(mstruct2.number().hasRealPart()) {
						return 1;
					} else {
						ComparisonResult cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
						if(cmp == COMPARISON_RESULT_LESS) return -1;
						else if(cmp == COMPARISON_RESULT_GREATER) return 1;
						return 0;
					}
				} else if(mstruct2.number().hasRealPart()) {
					ComparisonResult cmp = mstruct1.number().compareRealParts(mstruct2.number());
					if(cmp == COMPARISON_RESULT_EQUAL) {
						cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
					} 
					if(cmp == COMPARISON_RESULT_LESS) return -1;
					else if(cmp == COMPARISON_RESULT_GREATER) return 1;
					return 0;
				} else {
					return -1;
				}
			}
			return -1;
		} 
		case STRUCT_UNIT: {
			if(mstruct1.unit() < mstruct2.unit()) return -1;
			if(mstruct1.unit() == mstruct2.unit()) return 0;
			return 1;
		}
		case STRUCT_SYMBOLIC: {
			if(mstruct1.symbol() < mstruct2.symbol()) return -1;
			else if(mstruct1.symbol() == mstruct2.symbol()) return 0;
			return 1;
		}
		case STRUCT_VARIABLE: {
			if(mstruct1.variable() < mstruct2.variable()) return -1;
			else if(mstruct1.variable() == mstruct2.variable()) return 0;
			return 1;
		}
		case STRUCT_FUNCTION: {
			if(mstruct1.function() < mstruct2.function()) return -1;
			if(mstruct1.function() == mstruct2.function()) {
				for(size_t i = 0; i < mstruct2.size(); i++) {
					if(i >= mstruct1.size()) {
						return -1;	
					}
					int i2 = evalSortCompare(mstruct1[i], mstruct2[i], parent);
					if(i2 != 0) return i2;
				}
				return 0;
			}
			return 1;
		}
		case STRUCT_POWER: {
			int i = evalSortCompare(mstruct1[0], mstruct2[0], parent);
			if(i == 0) {
				return evalSortCompare(mstruct1[1], mstruct2[1], parent);
			}
			return i;
		}
		default: {
			if(mstruct2.size() < mstruct1.size()) return -1;
			else if(mstruct2.size() > mstruct1.size()) return 1;
			int ie;
			for(size_t i = 0; i < mstruct1.size(); i++) {
				ie = evalSortCompare(mstruct1[i], mstruct2[i], parent);
				if(ie != 0) {
					return ie;
				}
			}
		}
	}
	return 0;
}

void MathStructure::evalSort(bool recursive) {
	if(recursive) {
		for(size_t i = 0; i < SIZE; i++) {
			CHILD(i).evalSort(true);
		}
	}
	if(m_type != STRUCT_ADDITION && m_type != STRUCT_MULTIPLICATION && m_type != STRUCT_LOGICAL_AND && m_type != STRUCT_LOGICAL_OR && m_type != STRUCT_LOGICAL_XOR && m_type != STRUCT_BITWISE_AND && m_type != STRUCT_BITWISE_OR && m_type != STRUCT_BITWISE_XOR) return;
	vector<size_t> sorted;
	bool b;
	for(size_t i = 0; i < SIZE; i++) {
		b = false;
		for(size_t i2 = 0; i2 < sorted.size(); i2++) {
			if(evalSortCompare(CHILD(i), *v_subs[sorted[i2]], *this) < 0) {	
				sorted.insert(sorted.begin() + i2, v_order[i]);
				b = true;
				break;
			}
		}
		if(!b) sorted.push_back(v_order[i]);
	}
	for(size_t i2 = 0; i2 < sorted.size(); i2++) {
		v_order[i2] = sorted[i2];
	}
}

int sortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent, const PrintOptions &po);
int sortCompare(const MathStructure &mstruct1, const MathStructure &mstruct2, const MathStructure &parent, const PrintOptions &po) {
	if(parent.isMultiplication()) {
		if(mstruct1.containsType(STRUCT_VECTOR) && mstruct2.containsType(STRUCT_VECTOR)) {
			return 0;
		}
	}
	if(parent.isAddition() && po.sort_options.minus_last) {
		bool m1 = mstruct1.hasNegativeSign(), m2 = mstruct2.hasNegativeSign();
		if(m1 && !m2) {
			return 1;
		} else if(m2 && !m1) {
			return -1;
		}
	}
	bool isdiv1 = false, isdiv2 = false;
	if(!po.negative_exponents) {
		if(mstruct1.isMultiplication()) {
			for(size_t i = 0; i < mstruct1.size(); i++) {
				if(mstruct1[i].isPower() && mstruct1[i][1].hasNegativeSign()) {
					isdiv1 = true;
					break;
				}
			}
		} else if(mstruct1.isPower() && mstruct1[1].hasNegativeSign()) {
			isdiv1 = true;
		}
		if(mstruct2.isMultiplication()) {
			for(size_t i = 0; i < mstruct2.size(); i++) {
				if(mstruct2[i].isPower() && mstruct2[i][1].hasNegativeSign()) {
					isdiv2 = true;
					break;
				}
			}
		} else if(mstruct2.isPower() && mstruct2[1].hasNegativeSign()) {
			isdiv2 = true;
		}
	}
	if(parent.isAddition() && isdiv1 == isdiv2) {
		if(mstruct1.isMultiplication() && mstruct1.size() > 0) {
			size_t start = 0;
			while(mstruct1[start].isNumber() && mstruct1.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct2.isMultiplication()) {
				if(mstruct2.size() < 1) return -1;
				size_t start2 = 0;
				while(mstruct2[start2].isNumber() && mstruct2.size() > start2 + 1) {
					start2++;
				}
				for(size_t i = 0; i + start < mstruct1.size(); i++) {
					if(i + start2 >= mstruct2.size()) return 1;
					i2 = sortCompare(mstruct1[i + start], mstruct2[i + start2], parent, po);
					if(i2 != 0) return i2;
				}
				if(mstruct1.size() - start == mstruct2.size() - start2) return 0;
				return -1;
			} else {
				i2 = sortCompare(mstruct1[start], mstruct2, parent, po);
				if(i2 != 0) return i2;
			}
		} else if(mstruct2.isMultiplication() && mstruct2.size() > 0) {
			size_t start = 0;
			while(mstruct2[start].isNumber() && mstruct2.size() > start + 1) {
				start++;
			}
			int i2;
			if(mstruct1.isMultiplication()) {
				return 1;
			} else {
				i2 = sortCompare(mstruct1, mstruct2[start], parent, po);
				if(i2 != 0) return i2;
			}
		} 
	}
	if(mstruct1.type() != mstruct2.type()) {
		if(mstruct1.isVariable() && mstruct2.isSymbolic()) {
			if(parent.isMultiplication()) {
				if(mstruct1.variable()->isKnown()) return -1;
			}
			if(mstruct1.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name < mstruct2.symbol()) return -1;
			else return 1;
		}
		if(mstruct2.isVariable() && mstruct1.isSymbolic()) {
			if(parent.isMultiplication()) {
				if(mstruct2.variable()->isKnown()) return 1;
			}
			if(mstruct1.symbol() < mstruct2.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			else return 1;
		}
		if(!parent.isMultiplication() || (!mstruct1.isNumber() && !mstruct2.isNumber())) {
			if(mstruct2.isPower()) {
				int i = sortCompare(mstruct1, mstruct2[0], parent, po);
				if(i == 0) {
					return sortCompare(m_one, mstruct2[1], parent, po);
				}
				return i;
			}
			if(mstruct1.isPower()) {
				int i = sortCompare(mstruct1[0], mstruct2, parent, po);
				if(i == 0) {
					return sortCompare(mstruct1[1], m_one, parent, po);
				}
				return i;
			}
		}
		if(parent.isMultiplication()) {
			if(mstruct2.isUnit()) return -1;
			if(mstruct1.isUnit()) return 1;
		}
		if(mstruct2.isInverse()) return -1;
		if(mstruct1.isInverse()) return 1;
		if(mstruct2.isDivision()) return -1;
		if(mstruct1.isDivision()) return 1;
		if(mstruct2.isNegate()) return -1;
		if(mstruct1.isNegate()) return 1;
		if(mstruct2.type() == STRUCT_ALTERNATIVES) return -1;
		if(mstruct1.type() == STRUCT_ALTERNATIVES) return 1;
		if(mstruct2.isLogicalAnd()) return -1;
		if(mstruct1.isLogicalAnd()) return 1;
		if(mstruct2.isLogicalOr()) return -1;
		if(mstruct1.isLogicalOr()) return 1;
		if(mstruct2.isLogicalXor()) return -1;
		if(mstruct1.isLogicalXor()) return 1;
		if(mstruct2.isLogicalNot()) return -1;
		if(mstruct1.isLogicalNot()) return 1;
		if(mstruct2.isComparison()) return -1;
		if(mstruct1.isComparison()) return 1;
		if(mstruct2.isBitwiseOr()) return -1;
		if(mstruct1.isBitwiseOr()) return 1;
		if(mstruct2.isBitwiseXor()) return -1;
		if(mstruct1.isBitwiseXor()) return 1;
		if(mstruct2.isBitwiseAnd()) return -1;
		if(mstruct1.isBitwiseAnd()) return 1;
		if(mstruct2.isBitwiseNot()) return -1;
		if(mstruct1.isBitwiseNot()) return 1;
		if(mstruct2.isUndefined()) return -1;
		if(mstruct1.isUndefined()) return 1;
		if(mstruct2.isFunction()) return -1;
		if(mstruct1.isFunction()) return 1;
		if(mstruct2.isAddition()) return -1;
		if(mstruct1.isAddition()) return 1;
		if(!parent.isMultiplication()) {
			if(isdiv2 && mstruct2.isMultiplication()) return -1;
			if(isdiv1 && mstruct1.isMultiplication()) return 1;
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		if(mstruct2.isMultiplication()) return -1;
		if(mstruct1.isMultiplication()) return 1;
		if(mstruct2.isPower()) return -1;
		if(mstruct1.isPower()) return 1;
		if(mstruct2.isUnit()) return -1;
		if(mstruct1.isUnit()) return 1;
		if(mstruct2.isSymbolic()) return -1;
		if(mstruct1.isSymbolic()) return 1;
		if(mstruct2.isVariable()) return -1;
		if(mstruct1.isVariable()) return 1;
		if(parent.isMultiplication()) {
			if(mstruct2.isNumber()) return -1;
			if(mstruct1.isNumber()) return 1;
		}
		return -1;
	}
	switch(mstruct1.type()) {
		case STRUCT_NUMBER: {
			if(!mstruct1.number().isComplex() && !mstruct2.number().isComplex()) {
				ComparisonResult cmp;
				if(parent.isMultiplication() && mstruct2.number().isNegative() != mstruct1.number().isNegative()) cmp = mstruct2.number().compare(mstruct1.number());
				else cmp = mstruct1.number().compare(mstruct2.number());
				if(cmp == COMPARISON_RESULT_LESS) return -1;
				else if(cmp == COMPARISON_RESULT_GREATER) return 1;
				return 0;
			} else {
				if(!mstruct1.number().hasRealPart()) {
					if(mstruct2.number().hasRealPart()) {
						return 1;
					} else {
						ComparisonResult cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
						if(cmp == COMPARISON_RESULT_LESS) return -1;
						else if(cmp == COMPARISON_RESULT_GREATER) return 1;
						return 0;
					}
				} else if(mstruct2.number().hasRealPart()) {
					ComparisonResult cmp = mstruct1.number().compareRealParts(mstruct2.number());
					if(cmp == COMPARISON_RESULT_EQUAL) {
						cmp = mstruct1.number().compareImaginaryParts(mstruct2.number());
					} 
					if(cmp == COMPARISON_RESULT_LESS) return -1;
					else if(cmp == COMPARISON_RESULT_GREATER) return 1;
					return 0;
				} else {
					return -1;
				}
			}
			return -1;
		} 
		case STRUCT_UNIT: {
			if(mstruct1.unit() == mstruct2.unit()) return 0;
			if(mstruct1.unit()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, mstruct1.isPlural(), po.use_reference_names).name < mstruct2.unit()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, mstruct2.isPlural(), po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			return 1;
		}
		case STRUCT_SYMBOLIC: {
			if(mstruct1.symbol() < mstruct2.symbol()) return -1;
			else if(mstruct1.symbol() == mstruct2.symbol()) return 0;
			return 1;
		}
		case STRUCT_VARIABLE: {
			if(mstruct1.variable() == mstruct2.variable()) return 0;
			if(parent.isMultiplication()) {
				if(mstruct1.variable()->isKnown() && !mstruct2.variable()->isKnown()) return -1;
				if(!mstruct1.variable()->isKnown() && mstruct2.variable()->isKnown()) return 1;
			}
			if(mstruct1.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names).name < mstruct2.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			return 1;
		}
		case STRUCT_FUNCTION: {
			if(mstruct1.function() == mstruct2.function()) {
				for(size_t i = 0; i < mstruct2.size(); i++) {
					if(i >= mstruct1.size()) {
						return -1;	
					}
					int i2 = sortCompare(mstruct1[i], mstruct2[i], parent, po);
					if(i2 != 0) return i2;
				}
				return 0;
			}
			if(mstruct1.function()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names).name < mstruct2.function()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg).name) return -1;
			return 1;
		}
		case STRUCT_POWER: {
			int i = sortCompare(mstruct1[0], mstruct2[0], parent, po);
			if(i == 0) {
				return sortCompare(mstruct1[1], mstruct2[1], parent, po);
			}
			return i;
		}
		case STRUCT_MULTIPLICATION: {
			if(isdiv1 != isdiv2) {
				if(isdiv1) return 1;
				return -1;
			}
		}
		default: {
			int ie;
			for(size_t i = 0; i < mstruct1.size(); i++) {
				if(i >= mstruct2.size()) return 1;
				ie = sortCompare(mstruct1[i], mstruct2[i], parent, po);
				if(ie != 0) {
					return ie;
				}
			}
		}
	}
	return 0;
}

void MathStructure::sort(const PrintOptions &po, bool recursive) {
	if(recursive) {
		for(size_t i = 0; i < SIZE; i++) {
			CHILD(i).sort(po);
		}
	}
	if(m_type != STRUCT_ADDITION && m_type != STRUCT_MULTIPLICATION && m_type != STRUCT_LOGICAL_AND && m_type != STRUCT_LOGICAL_OR && m_type != STRUCT_LOGICAL_XOR && m_type != STRUCT_BITWISE_AND && m_type != STRUCT_BITWISE_OR && m_type != STRUCT_BITWISE_XOR) return;
	vector<size_t> sorted;
	bool b;
	PrintOptions po2 = po;
	po2.sort_options.minus_last = po.sort_options.minus_last && !containsUnknowns();
	for(size_t i = 0; i < SIZE; i++) {
		b = false;
		for(size_t i2 = 0; i2 < sorted.size(); i2++) {
			if(sortCompare(CHILD(i), *v_subs[sorted[i2]], *this, po2) < 0) {
				sorted.insert(sorted.begin() + i2, v_order[i]);
				b = true;
				break;
			}
		}
		if(!b) sorted.push_back(v_order[i]);
	}
	for(size_t i2 = 0; i2 < sorted.size(); i2++) {
		v_order[i2] = sorted[i2];
	}
}
bool MathStructure::containsAdditionPower() const {
	if(m_type == STRUCT_POWER && CHILD(0).isAddition()) return true;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).containsAdditionPower()) return true;
	}
	return false;
}

size_t MathStructure::countTotalChilds(bool count_function_as_one) const {
	if((m_type == STRUCT_FUNCTION && count_function_as_one) || SIZE == 0) return 1;
	size_t count = 0;
	for(size_t i = 0; i < SIZE; i++) {
		count += CHILD(i).countTotalChilds();
	}
	return count;
}

void try_isolate_x(MathStructure &mstruct, EvaluationOptions &eo3, const EvaluationOptions &eo);
void try_isolate_x(MathStructure &mstruct, EvaluationOptions &eo3, const EvaluationOptions &eo) {
	if(mstruct.isComparison()) {
		if(mstruct.comparisonType() == COMPARISON_EQUALS || mstruct.comparisonType() == COMPARISON_NOT_EQUALS) {
			MathStructure mtest(mstruct);
			eo3.test_comparisons = false;
			mtest.calculatesub(eo3, eo);
			eo3.test_comparisons = eo.test_comparisons;
			if(mtest.isolate_x(eo3)) {
				mstruct = mtest;
			}
		}
	} else {
		for(size_t i = 0; i < mstruct.size(); i++) {
			try_isolate_x(mstruct[i], eo3, eo);
		}
	}
}

MathStructure &MathStructure::eval(const EvaluationOptions &eo) {
	if(eo.structuring == STRUCTURING_NONE) return *this;
	unformat(eo);
	if(eo.sync_units && syncUnits()) {
		unformat(eo);
	}
	if(eo.calculate_functions && calculateFunctions(eo)) {
		unformat(eo);
		if(eo.sync_units && syncUnits()) {
			unformat(eo);
		}
	}
	EvaluationOptions eo2 = eo;
	//eo2.parse_options.angle_unit = ANGLE_UNIT_NONE;
	eo2.simplify_addition_powers = false;
	eo2.do_polynomial_division = false;
	eo2.merge_divisions = false;
	eo2.reduce_divisions = false;
	if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo3 = eo2;
		eo3.approximation = APPROXIMATION_EXACT;
		eo3.split_squares = false;		
		calculatesub(eo3, eo);
		if(eo.sync_units && syncUnits()) {
			unformat(eo);
			calculatesub(eo3, eo);
		}
		eo3.approximation = APPROXIMATION_APPROXIMATE;
		calculatesub(eo3, eo);
		if(eo.sync_units && syncUnits()) {
			unformat(eo);
			calculatesub(eo3, eo);
		}
		if(eo2.isolate_x) {
			isolate_x(eo3);
		}
	} else {
		calculatesub(eo2, eo);
		if(eo.sync_units && syncUnits()) {
			unformat(eo);
			calculatesub(eo2, eo);
		}
		if(eo2.isolate_x) {
			isolate_x(eo2);
		}
	}
	if(eo.simplify_addition_powers || (eo.merge_divisions && eo.do_polynomial_division) || eo.reduce_divisions) {
		eo2.simplify_addition_powers = eo.simplify_addition_powers;
		eo2.reduce_divisions = eo.reduce_divisions;
		//eo2.merge_divisions = eo.merge_divisions && eo.do_polynomial_division;
		if((eo.simplify_addition_powers && containsAdditionPower()) || ((eo.reduce_divisions || eo2.merge_divisions) && containsDivision())) {
			if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
				EvaluationOptions eo3 = eo2;
				eo3.approximation = APPROXIMATION_APPROXIMATE;
				calculatesub(eo3, eo);
				if(eo2.isolate_x) {
					isolate_x(eo3);
				}
			} else {
				calculatesub(eo2, eo);
				if(eo2.isolate_x) {
					isolate_x(eo2);
				}
			}
		}
		eo2.merge_divisions = false;
	}
	if(eo.do_polynomial_division) {
		if(containsDivision()) {
			MathStructure mtest(*this);
			EvaluationOptions eo3 = eo2;
			eo3.do_polynomial_division = true;
			eo3.reduce_divisions = false;
			if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
				eo3.approximation = APPROXIMATION_APPROXIMATE;
			}
			mtest.calculatesub(eo3, eo);
			if(mtest.countTotalChilds() < countTotalChilds()) {
				set_nocopy(mtest);
			}	
			if(eo2.isolate_x) {
				isolate_x(eo3);
			}
			if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
				EvaluationOptions eo3 = eo2;
				eo3.approximation = APPROXIMATION_APPROXIMATE;
				calculatesub(eo3, eo);
			} else {
				calculatesub(eo2, eo);
			}
		}
	}
	if(eo2.isolate_x && containsType(STRUCT_COMPARISON)) {		
		EvaluationOptions eo3 = eo2;
		if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
			eo3.approximation = APPROXIMATION_APPROXIMATE;
		}
		eo3.assume_denominators_nonzero = true;
		try_isolate_x(*this, eo3, eo);
	}
	if(eo2.sync_units && eo2.sync_complex_unit_relations) {
		if(syncUnits(true)) {
			unformat(eo2);
			if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
				EvaluationOptions eo3 = eo2;
				eo3.approximation = APPROXIMATION_EXACT;
				eo3.split_squares = false;
				calculatesub(eo3, eo);
				eo3.approximation = APPROXIMATION_APPROXIMATE;
				calculatesub(eo3, eo);
				if(eo2.isolate_x) {
					isolate_x(eo3);
				}
			} else {
				calculatesub(eo2, eo);
				if(eo2.isolate_x) {
					isolate_x(eo2);
				}
			}
		}
	}
	return *this;
}

bool factorize_find_multiplier(const MathStructure &mstruct, MathStructure &mnew, MathStructure &factor_mstruct) {
	factor_mstruct.set(m_one);
	switch(mstruct.type()) {
		case STRUCT_ADDITION: {
			bool bfrac = false, bint = true;
			idm1(mstruct, bfrac, bint);
			if(bfrac || bint) {
				Number gcd(1, 1);
				idm2(mstruct, bfrac, bint, gcd);
				if((bint || bfrac) && !gcd.isOne()) {		
					if(bfrac) gcd.recip();
					factor_mstruct.set(gcd);
				}
			}
			if(mstruct.size() > 0) {
				size_t i = 0;
				const MathStructure *cur_mstruct;
				while(true) {
					if(mstruct[0].isMultiplication()) {
						if(i >= mstruct[0].size()) {
							break;
						}
						cur_mstruct = &mstruct[0][i];
					} else {
						cur_mstruct = &mstruct[0];
					}
					if(!cur_mstruct->isNumber() && (!cur_mstruct->isPower() || cur_mstruct->exponent()->isNumber())) {
						const MathStructure *exp = NULL;
						const MathStructure *bas;
						if(cur_mstruct->isPower()) {
							exp = cur_mstruct->exponent();
							bas = cur_mstruct->base();
						} else {
							bas = cur_mstruct;
						}
						bool b = true;
						for(size_t i2 = 1; i2 < mstruct.size(); i2++) {
							b = false;
							size_t i3 = 0;
							const MathStructure *cmp_mstruct;
							while(true) {
								if(mstruct[i2].isMultiplication()) {
									if(i3 >= mstruct[i2].size()) {
										break;
									}
									cmp_mstruct = &mstruct[i2][i3];
								} else {
									cmp_mstruct = &mstruct[i2];
								}
								if(cmp_mstruct->equals(*bas)) {
									if(exp) {
										exp = NULL;
									}
									b = true;
									break;
								} else if(cmp_mstruct->isPower() && cmp_mstruct->base()->equals(*bas)) {
									if(exp) {
										if(cmp_mstruct->exponent()->isNumber()) {
											if(cmp_mstruct->exponent()->number().isLessThan(exp->number())) {
												exp = cmp_mstruct->exponent();
											}
											b = true;
											break;
										} else {
											exp = NULL;
										}
									} else {
										b = true;
										break;
									}
								}
								if(!mstruct[i2].isMultiplication()) {
									break;
								}
								i3++;
							}
							if(!b) break;
						}
						if(b) {
							if(exp) {
								MathStructure *mstruct = new MathStructure(*bas);
								mstruct->raise(*exp);
								if(factor_mstruct.isOne()) {
									factor_mstruct.set_nocopy(*mstruct);
									mstruct->unref();
								} else {
									factor_mstruct.multiply_nocopy(mstruct, true);
								}
							} else {
								if(factor_mstruct.isOne()) factor_mstruct.set(*bas);
								else factor_mstruct.multiply(*bas, true);
							}
						}
					}
					if(!mstruct[0].isMultiplication()) {
						break;
					}
					i++;
				}
			}
			if(!factor_mstruct.isOne()) {
				if(&mstruct != &mnew) mnew.set(mstruct);
				MathStructure *mfactor;
				size_t i = 0;
				while(true) {
					if(factor_mstruct.isMultiplication()) {
						if(i >= factor_mstruct.size()) break;
						mfactor = &factor_mstruct[i];
					} else {
						mfactor = &factor_mstruct;
					}
					for(size_t i2 = 0; i2 < mnew.size(); i2++) {
						switch(mnew[i2].type()) {
							case STRUCT_NUMBER: {
								if(mfactor->isNumber()) {
									mnew[i2].number() /= mfactor->number();
								}
								break;
							}
							case STRUCT_POWER: {
								if(mfactor->isNumber()) {
									mnew[i2].transform(STRUCT_MULTIPLICATION);
									mnew[i2].insertChild(MathStructure(1, 1), 1);
									mnew[i2][0].number() /= mfactor->number();
								} else if(mfactor->isPower()) {
									if(mfactor->equals(mnew[i2])) {
										mnew[i2].set(m_one);
									} else {
										mnew[i2][1].number() -= mfactor->exponent()->number();
										if(mnew[i2][1].number().isOne()) {
											mnew[i2].setToChild(1, true);
										}
									}
								} else {
									mnew[i2][1].number() -= 1;
									if(mnew[i2][1].number().isOne()) {
										MathStructure mstruct2(mnew[i2][0]);
										mnew[i2] = mstruct2;
									} else if(mnew[i2][1].number().isZero()) {
										mnew[i2].set(m_one);
									}
								}
								break;
							}
							case STRUCT_MULTIPLICATION: {
								bool b = true;
								if(mfactor->isNumber() && (mnew[i2].size() < 1 || !mnew[i2][0].isNumber())) {
									mnew[i2].insertChild(MathStructure(1, 1), 1);
								}
								for(size_t i3 = 0; i3 < mnew[i2].size() && b; i3++) {
									switch(mnew[i2][i3].type()) {
										case STRUCT_NUMBER: {
											if(mfactor->isNumber()) {
												if(mfactor->equals(mnew[i2][i3])) {
													mnew[i2].delChild(i3 + 1);
												} else {
													mnew[i2][i3].number() /= mfactor->number();
												}
												b = false;
											}
											break;
										}
										case STRUCT_POWER: {
											if(mfactor->isPower() && mfactor->base()->equals(mnew[i2][i3][0])) {
												if(mfactor->equals(mnew[i2][i3])) {
													mnew[i2].delChild(i3 + 1);
												} else {
													mnew[i2][i3][1].number() -= mfactor->exponent()->number();
													if(mnew[i2][i3][1].number().isOne()) {
														MathStructure mstruct2(mnew[i2][i3][0]);
														mnew[i2][i3] = mstruct2;
													} else if(mnew[i2][i3][1].number().isZero()) {
														mnew[i2].delChild(i3 + 1);
													}
												}
												b = false;
											} else if(mfactor->equals(mnew[i2][i3][0])) {
												if(mnew[i2][i3][1].number() == 2) {
													MathStructure mstruct2(mnew[i2][i3][0]);
													mnew[i2][i3] = mstruct2;
												} else if(mnew[i2][i3][1].number().isOne()) {
													mnew[i2].delChild(i3 + 1);
												} else {
													mnew[i2][i3][1].number() -= 1;
												}
												b = false;
											}
											break;
										}
										default: {
											if(mfactor->equals(mnew[i2][i3])) {
												mnew[i2].delChild(i3 + 1);
												b = false;
											}
										}
									}
								}
								if(mnew[i2].size() == 1) {
									MathStructure mstruct2(mnew[i2][0]);
									mnew[i2] = mstruct2;
								}
								break;
							}
							default: {
								if(mfactor->isNumber()) {
									mnew[i2].transform(STRUCT_MULTIPLICATION);
									mnew[i2].insertChild(MathStructure(1, 1), 1);
									mnew[i2][0].number() /= mfactor->number();
								} else {
									mnew[i2].set(m_one);
								}
							}
						}
					}
					if(factor_mstruct.isMultiplication()) {
						i++;
					} else {
						break;
					}
				}
				return true;
			}
		}
	}
	return false;
}

bool MathStructure::factorize(const EvaluationOptions &eo) {
	switch(type()) {
		case STRUCT_ADDITION: {
			if(SIZE <= 3 && SIZE > 1) {
				MathStructure *xvar = NULL;
				Number nr2(1, 1);
				if(CHILD(0).isPower() && CHILD(0)[0].size() == 0 && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isTwo()) {
					xvar = &CHILD(0)[0];
				} else if(CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0].isNumber()) {
					if(CHILD(0)[1].isPower()) {
						if(CHILD(0)[1][0].size() == 0 && CHILD(0)[1][1].isNumber() && CHILD(0)[1][1].number().isTwo()) {
							xvar = &CHILD(0)[1][0];
							nr2.set(CHILD(0)[0].number());
						}
					}
				}
				if(xvar) {
					bool factorable = false;
					Number nr1, nr0;
					if(SIZE == 2 && CHILD(1).isNumber()) {
						factorable = true;
						nr0 = CHILD(1).number();
					} else if(SIZE == 3 && CHILD(2).isNumber()) {
						nr0 = CHILD(2).number();
						if(CHILD(1).isMultiplication()) {
							if(CHILD(1).size() == 2 && CHILD(1)[0].isNumber() && xvar->equals(CHILD(1)[1])) {
								nr1 = CHILD(1)[0].number();
								factorable = true;
							}
						} else if(xvar->equals(CHILD(1))) {
							nr1.set(1, 1);
							factorable = true;
						}
					}
					if(factorable) {
						Number nr4ac(4, 1);
						nr4ac *= nr2;
						nr4ac *= nr0;
						Number nrtwo(2, 1);
						Number nr2a(2, 1);
						nr2a *= nr2;
						Number sqrtb24ac(nr1);
						sqrtb24ac.raise(nrtwo);
						sqrtb24ac -= nr4ac;
						if(!sqrtb24ac.isNegative()) {
							nrtwo.set(1, 2);
							MathStructure mstructb24(sqrtb24ac);
							if(eo.approximation == APPROXIMATION_EXACT && !sqrtb24ac.isApproximate()) {
								sqrtb24ac.raise(nrtwo);
								if(sqrtb24ac.isApproximate()) {
									mstructb24.raise(nrtwo);
								} else {
									mstructb24.set(sqrtb24ac);
								}
							} else {
								mstructb24.number().raise(nrtwo);
							}
							MathStructure m1(nr1), m2(nr1);
							Number mul1(1, 1), mul2(1, 1);
							if(mstructb24.isNumber()) {
								m1.number() += mstructb24.number();
								m1.number() /= nr2a;
								if(m1.number().isRational() && !m1.number().isInteger()) {
									mul1 = m1.number().denominator();
									m1.number() *= mul1;
								}
								m2.number() -= mstructb24.number();
								m2.number() /= nr2a;
								if(m2.number().isRational() && !m2.number().isInteger()) {
									mul2 = m2.number().denominator();
									m2.number() *= mul2;
								}
							} else {
								EvaluationOptions eo2 = eo;
								eo2.calculate_functions = false;
								eo2.sync_units = false;
								m1 += mstructb24;
								m1 /= nr2a;
								m1.eval(eo2);
								if(m1.isNumber()) {
									if(m1.number().isRational() && !m1.number().isInteger()) {
										mul1 = m1.number().denominator();
										m1.number() *= mul1;
									}
								} else {
									bool bint = false, bfrac = false;
									idm1(m1, bfrac, bint);
									if(bfrac) {
										idm2(m1, bfrac, bint, mul1);
										idm3(m1, mul1);
									}
								}
								m2 -= mstructb24;
								m2 /= nr2a;
								m2.eval(eo2);
								if(m2.isNumber()) {
									if(m2.number().isRational() && !m2.number().isInteger()) {
										mul2 = m2.number().denominator();
										m2.number() *= mul2;
									}
								} else {
									bool bint = false, bfrac = false;
									idm1(m2, bfrac, bint);
									if(bfrac) {
										idm2(m2, bfrac, bint, mul2);
										idm3(m2, mul2);
									}
								}
							}
							nr2 /= mul1;
							nr2 /= mul2;
							if(m1 == m2 && mul1 == mul2) {
								MathStructure xvar2(*xvar);
								if(!mul1.isOne()) xvar2 *= mul1;
								set(m1);
								add(xvar2, true);
								raise(MathStructure(2, 1));
								if(!nr2.isOne()) {
									multiply(nr2);
								}
							} else {
								m1.add(*xvar, true);
								if(!mul1.isOne()) m1[m1.size() - 1] *= mul1;
								m2.add(*xvar, true);
								if(!mul2.isOne()) m2[m2.size() - 1] *= mul2;
								clear(true);
								m_type = STRUCT_MULTIPLICATION;
								if(!nr2.isOne()) {
									APPEND_NEW(nr2);
								}
								APPEND(m1);
								APPEND(m2);
							}							
							return true;
						}
					}
				}
			}
			MathStructure factor_mstruct(1, 1);
			MathStructure mnew;
			if(factorize_find_multiplier(*this, mnew, factor_mstruct)) {
				mnew.factorize(eo);
				if(mnew.isMultiplication()) {
					set(mnew, true);
					bool b = false;
					for(size_t i = 0; i < SIZE; i++) {
						if(!CHILD(i).isAddition()) {
							int ret = CHILD(i).merge_multiplication(factor_mstruct, eo);
							if(ret == 0) {
								ret = factor_mstruct.merge_multiplication(CHILD(i), eo);
								if(ret > 0) {
									CHILD(i) = factor_mstruct;
								}
							}
							if(ret > 0) b = true;
							break;
						}
					}
					if(!b) {
						PREPEND(factor_mstruct);
					}
				} else {
					clear(true);
					m_type = STRUCT_MULTIPLICATION;
					APPEND(factor_mstruct);
					APPEND(mnew);
				}
				return true;
			}
			if(SIZE > 1 && CHILD(SIZE - 1).isNumber() && CHILD(SIZE - 1).number().isInteger()) {
				MathStructure *xvar = NULL;
				Number qnr(1, 1);
				int degree = 1;
				bool overflow = false;
				int qcof = 1;
				if(CHILD(0).isPower() && CHILD(0)[0].size() == 0 && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isInteger() && CHILD(0)[1].number().isPositive()) {
					xvar = &CHILD(0)[0];
					degree = CHILD(0)[1].number().intValue(&overflow);
				} else if(CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0].isNumber() && CHILD(0)[0].number().isInteger()) {
					if(CHILD(0)[1].isPower()) {
						if(CHILD(0)[1][0].size() == 0 && CHILD(0)[1][1].isNumber() && CHILD(0)[1][1].number().isInteger() && CHILD(0)[1][1].number().isPositive()) {
							xvar = &CHILD(0)[1][0];
							qcof = CHILD(0)[0].number().intValue(&overflow);
							if(!overflow) {
								if(qcof < 0) qcof = -qcof;
								degree = CHILD(0)[1][1].number().intValue(&overflow);
							}
						}
					}
				}
				int pcof = 1;
				if(!overflow) {
					pcof = CHILD(SIZE - 1).number().intValue(&overflow);
					if(pcof < 0) pcof = -pcof;
				}
				if(xvar && !overflow && degree <= 1000 && degree > 2) {
					bool b = true;
					for(size_t i = 1; b && i < SIZE - 1; i++) {
						switch(CHILD(i).type()) {
							case STRUCT_NUMBER: {
								b = false;
								break;
							}
							case STRUCT_POWER: {
								if(!CHILD(i)[1].isNumber() || !xvar->equals(CHILD(i)[0]) || !CHILD(i)[1].number().isInteger() || !CHILD(i)[1].number().isPositive()) {
									b = false;
								}
								break;
							}
							case STRUCT_MULTIPLICATION: {
								if(!CHILD(i).size() == 2 || !CHILD(i)[0].isNumber()) {
									b = false;
								} else if(CHILD(i)[1].isPower()) {
									if(!CHILD(i)[1][1].isNumber() || !xvar->equals(CHILD(i)[1][0]) || !CHILD(i)[1][1].number().isInteger() || !CHILD(i)[1][1].number().isPositive()) {
										b = false;
									}
								} else if(!xvar->equals(CHILD(i)[1])) {
									b = false;
								}
								break;
							}
							default: {
								if(!xvar->equals(CHILD(i))) {
									b = false;
								}
							}
						}
					}
					if(b) {
						vector<Number> factors;
						factors.resize(degree + 1, Number());
						factors[0] = CHILD(SIZE - 1).number();
						vector<int> ps;
						vector<int> qs;
						vector<Number> zeroes;
						int curdeg = 1, prevdeg = 0;
						for(size_t i = 0; b && i < SIZE - 1; i++) {
							switch(CHILD(i).type()) {
								case STRUCT_POWER: {
									curdeg = CHILD(i)[1].number().intValue(&overflow);
									if(curdeg == prevdeg || curdeg > degree || prevdeg > 0 && curdeg > prevdeg || overflow) {
										b = false;
									} else {
										factors[curdeg].set(1, 1);
									}
									break;
								}
								case STRUCT_MULTIPLICATION: {
									if(CHILD(i)[1].isPower()) {
										curdeg = CHILD(i)[1][1].number().intValue(&overflow);
									} else {
										curdeg = 1;
									}
									if(curdeg == prevdeg || curdeg > degree || prevdeg > 0 && curdeg > prevdeg || overflow) {
										b = false;
									} else {
										factors[curdeg] = CHILD(i)[0].number();
									}
									break;
								}
								default: {
									curdeg = 1;
									factors[curdeg].set(1, 1);
								}
							}
							prevdeg = curdeg;
						}
						while(b && degree > 2) {
							for(int i = 1; i <= 1000; i++) {
								if(i > pcof) break;
								if(pcof % i == 0) ps.push_back(i);
							}
							for(int i = 1; i <= 1000; i++) {
								if(i > qcof) break;
								if(qcof % i == 0) qs.push_back(i);
							}
							Number itest;
							int i2;
							size_t pi = 0, qi = 0;
							Number nrtest(ps[0], qs[0]);
							while(true) {
								itest.clear(); i2 = degree;
								while(true) {
									itest += factors[i2];
									if(i2 == 0) break;
									itest *= nrtest;
									i2--;
								}
								if(itest.isZero()) {
									break;
								}
								if(nrtest.isPositive()) {
									nrtest.negate();
								} else {
									qi++;
									if(qi == qs.size()) {
										qi = 0;
										pi++;
										if(pi == ps.size()) {
											break;
										}
									}
									nrtest.set(ps[pi], qs[qi]);
								}
							}
							if(itest.isZero()) {
								itest.clear(); i2 = degree;
								Number ntmp(factors[i2]);
								for(; i2 > 0; i2--) {
									itest += ntmp;
									ntmp = factors[i2 - 1];
									factors[i2 - 1] = itest;
									itest *= nrtest;
								}
								degree--;
								nrtest.negate();
								zeroes.push_back(nrtest);
								if(degree == 2) {
									break;
								}
								qcof = factors[degree].intValue(&overflow);
								if(!overflow) {
									if(qcof < 0) qcof = -qcof;
									pcof = factors[0].intValue(&overflow);
									if(!overflow) {
										if(pcof < 0) pcof = -pcof;
									}
								}
								if(overflow) {
									break;
								}
							} else {
								break;
							}
							ps.clear();
							qs.clear();
						}
						if(zeroes.size() > 0) {
							MathStructure mleft;
							MathStructure mtmp;
							MathStructure *mcur;
							for(int i = degree; i >= 0; i--) {
								if(!factors[i].isZero()) {
									if(mleft.isZero()) {
										mcur = &mleft;
									} else {
										mleft.add(m_zero, true);
										mcur = &mleft[mleft.size() - 1];
									}
									if(i > 1) {
										if(!factors[i].isOne()) {
											mcur->multiply(*xvar);
											(*mcur)[0].set(factors[i]);
											mcur = &(*mcur)[1];
										} else {
											mcur->set(*xvar);
										}
										mtmp.set(i, 1);
										mcur->raise(mtmp);
									} else if(i == 1) {
										if(!factors[i].isOne()) {
											mcur->multiply(*xvar);
											(*mcur)[0].set(factors[i]);
										} else {
											mcur->set(*xvar);
										}
									} else {
										mcur->set(factors[i]);
									}
								}
							}
							mleft.factorize(eo);
							vector<int> powers;
							vector<size_t> powers_i;
							int dupsfound = 0;
							for(size_t i = 0; i < zeroes.size() - 1; i++) {
								while(i + 1 < zeroes.size() && zeroes[i] == zeroes[i + 1]) {
									dupsfound++;
									zeroes.erase(zeroes.begin() + (i + 1));
								}
								if(dupsfound > 0) {
									powers_i.push_back(i);
									powers.push_back(dupsfound + 1);
									dupsfound = 0;
								}
							}
							MathStructure xvar2(*xvar);
							Number *nrmul;
							if(mleft.isMultiplication()) {
								set(mleft);
								evalSort();
								if(CHILD(0).isNumber()) {
									nrmul = &CHILD(0).number();
								} else if(CHILD(0).isMultiplication() && CHILD(0).size() > 0 && CHILD(0)[0].isNumber()) {
									nrmul = &CHILD(0)[0].number();
								} else {
									PREPEND(m_one);
									nrmul = &CHILD(0).number();
								}
							} else {
								clear(true);
								m_type = STRUCT_MULTIPLICATION;
								APPEND(m_one);
								APPEND(mleft);
								nrmul = &CHILD(0).number();
							}
							size_t pi = 0;
							for(size_t i = 0; i < zeroes.size(); i++) {
								if(zeroes[i].isInteger()) {
									APPEND(xvar2);
								} else {
									APPEND(m_zero);
								}
								mcur = &CHILD(SIZE - 1);
								if(pi < powers_i.size() && powers_i[pi] == i) {
									mcur->raise(MathStructure(powers[pi], 1));
									mcur = &(*mcur)[0];
									if(zeroes[i].isInteger()) {
										mcur->add(zeroes[i]);
									} else {
										Number nr(zeroes[i].denominator());
										mcur->add(zeroes[i].numerator());
										(*mcur)[0] *= xvar2;
										(*mcur)[0][0].number() = nr;
										nr.raise(powers[pi]);
										nrmul->divide(nr);										
									}
									pi++;
								} else {
									if(zeroes[i].isInteger()) {
										mcur->add(zeroes[i]);
									} else {
										nrmul->divide(zeroes[i].denominator());
										mcur->add(zeroes[i].numerator());
										(*mcur)[0] *= xvar2;
										(*mcur)[0][0].number() = zeroes[i].denominator();
									}
								}
							}
							if(CHILD(0).isNumber() && CHILD(0).number().isOne()) {
								ERASE(0);
							} else if(CHILD(0).isMultiplication() && CHILD(0).size() > 0 && CHILD(0)[0].isNumber() && CHILD(0)[0].number().isOne()) {
								if(CHILD(0).size() == 1) {
									ERASE(0);
								} else if(CHILD(0).size() == 2) {
									CHILD(0).setToChild(2, true);
								} else {
									CHILD(0).delChild(1);
								}
							}
							evalSort(true);
							Number dupspow;
							for(size_t i = 0; i < SIZE - 1; i++) {
								mcur = NULL;
								if(CHILD(i).isPower()) {
									if(CHILD(i)[0].isAddition() && CHILD(i)[1].isNumber()) {
										mcur = &CHILD(i)[0];
									}
								} else if(CHILD(i).isAddition()) {
									mcur = &CHILD(i);
								}
								while(mcur && i + 1 < SIZE) {
									if(CHILD(i + 1).isPower()) {
										if(CHILD(i + 1)[0].isAddition() && CHILD(i + 1)[1].isNumber() && mcur->equals(CHILD(i + 1)[0])) {
											dupspow += CHILD(i + 1)[1].number();
										} else {
											mcur = NULL;
										}
									} else if(CHILD(i + 1).isAddition() && mcur->equals(CHILD(i + 1))) {
										dupspow++;
									} else {
										mcur = NULL;
									}
									if(mcur) {
										ERASE(i + 1);
									}
								}
								if(!dupspow.isZero()) {
									if(CHILD(i).isPower()) {
										CHILD(i)[1].number() += dupspow;
									} else {
										dupspow++;
										CHILD(i) ^= dupspow;
									}
									dupspow.clear();
								}
							}
							if(SIZE == 1) {
								setToChild(1, true);
							}
							return true;
						}
					}
				}
			}
		}
		default: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).factorize(eo)) {
					CHILD_UPDATED(i);
				}
			}
		}
	}
	return false;
}

void MathStructure::addChild(const MathStructure &o) {
	APPEND(o);
}
void MathStructure::addChild_nocopy(MathStructure *o) {
	APPEND_POINTER(o);
}
void MathStructure::delChild(size_t index) {
	if(index > 0 && index <= SIZE) {
		ERASE(index - 1);
	}
}
void MathStructure::insertChild(const MathStructure &o, size_t index) {
	if(index > 0 && index <= v_subs.size()) {
		v_order.insert(v_order.begin() + (index - 1), v_subs.size());
		v_subs.push_back(new MathStructure(o));
		CHILD_UPDATED(index - 1);
	} else {
		addChild(o);
	}
}
void MathStructure::insertChild_nocopy(MathStructure *o, size_t index) {
	if(index > 0 && index <= v_subs.size()) {
		v_order.insert(v_order.begin() + (index - 1), v_subs.size());
		v_subs.push_back(o);
		CHILD_UPDATED(index - 1);
	} else {
		addChild_nocopy(o);
	}
}
void MathStructure::setChild(const MathStructure &o, size_t index) {
	if(index > 0 && index <= SIZE) {
		CHILD(index - 1) = o;
		CHILD_UPDATED(index - 1);
	}
}
void MathStructure::setChild_nocopy(MathStructure *o, size_t index) {
	if(index > 0 && index <= SIZE) {
		v_subs[v_order[index - 1]]->unref();
		v_subs[v_order[index - 1]] = o;
		CHILD_UPDATED(index - 1);
	}
}
const MathStructure *MathStructure::getChild(size_t index) const {
	if(index > 0 && index <= v_order.size()) {
		return &CHILD(index - 1);
	}
	return NULL;
}
MathStructure *MathStructure::getChild(size_t index) {
	if(index > 0 && index <= v_order.size()) {
		return &CHILD(index - 1);
	}
	return NULL;
}
size_t MathStructure::countChilds() const {
	return SIZE;
}
size_t MathStructure::size() const {
	return SIZE;
}
const MathStructure *MathStructure::base() const {
	if(m_type == STRUCT_POWER && SIZE >= 1) {
		return &CHILD(0);
	}
	return NULL;
}
const MathStructure *MathStructure::exponent() const {
	if(m_type == STRUCT_POWER && SIZE >= 2) {
		return &CHILD(1);
	}
	return NULL;
}
MathStructure *MathStructure::base() {
	if(m_type == STRUCT_POWER && SIZE >= 1) {
		return &CHILD(0);
	}
	return NULL;
}
MathStructure *MathStructure::exponent() {
	if(m_type == STRUCT_POWER && SIZE >= 2) {
		return &CHILD(1);
	}
	return NULL;
}
void MathStructure::addAlternative(const MathStructure &o) {
	if(m_type != STRUCT_ALTERNATIVES) {
		MathStructure copy_this(*this);
		clear(true);
		m_type = STRUCT_ALTERNATIVES;
		APPEND(copy_this);
	}
	APPEND(o);
}

int MathStructure::type() const {
	return m_type;
}
void MathStructure::unformat(const EvaluationOptions &eo) {
	for(size_t i = 0; i < SIZE; i++) {
		CHILD(i).unformat(eo);
	}
	switch(m_type) {
		case STRUCT_INVERSE: {
			APPEND(m_minus_one);
			m_type = STRUCT_POWER;	
		}
		case STRUCT_NEGATE: {
			PREPEND(m_minus_one);
			m_type = STRUCT_MULTIPLICATION;
		}
		case STRUCT_DIVISION: {
			CHILD(1).raise(m_minus_one);
			m_type = STRUCT_MULTIPLICATION;
		}
		case STRUCT_UNIT: {
			if(o_prefix && !eo.keep_prefixes) {
				if(o_prefix == CALCULATOR->decimal_null_prefix || o_prefix == CALCULATOR->binary_null_prefix) {
					o_prefix = NULL;
				} else {
					Unit *u = o_unit;
					Prefix *p = o_prefix;
					//set(10);
					//raise(p->exponent());
					set(p->value());
					multiply(u);
				}
			}
			b_plural = false;
		}
	}
}

void idm1(const MathStructure &mnum, bool &bfrac, bool &bint) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {
			if((!bfrac || bint) && mnum.number().isRational()) {
				if(!mnum.number().isInteger()) {
					bint = false;
					bfrac = true;
				}
			} else {
				bint = false;
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if((!bfrac || bint) && mnum.size() > 0 && mnum[0].isNumber() && mnum[0].number().isRational()) {
				if(!mnum[0].number().isInteger()) {
					bint = false;
					bfrac = true;
				}
				
			} else {
				bint = false;
			}
			break;
		}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < mnum.size() && (!bfrac || bint); i++) {
				idm1(mnum[i], bfrac, bint);
			}
			break;
		}
		default: {
			bint = false;
		}
	}
}
void idm2(const MathStructure &mnum, bool &bfrac, bool &bint, Number &nr) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {
			if(mnum.number().isRational()) {
				if(mnum.number().isInteger()) {
					if(bint) {
						if(mnum.number().isOne()) {
							bint = false;
						} else if(nr.isOne()) {
							nr = mnum.number();
						} else if(nr != mnum.number()) {
							nr.gcd(mnum.number());
							if(nr.isOne()) bint = false;
						}
					}
				} else {
					if(nr.isOne()) {
						nr = mnum.number().denominator();
					} else {
						Number nden(mnum.number().denominator());
						if(nr != nden) {
							Number ngcd(nden);
							ngcd.gcd(nr);
							nden /= ngcd;
							nr *= nden;
						}
					}
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(mnum.size() > 0 && mnum[0].isNumber() && mnum[0].number().isRational()) {
				if(mnum[0].number().isInteger()) {
					if(bint) {
						if(mnum[0].number().isOne()) {
							bint = false;
						} else if(nr.isOne()) {
							nr = mnum[0].number();
						} else if(nr != mnum[0].number()) {
							nr.gcd(mnum[0].number());
							if(nr.isOne()) bint = false;
						}
					}
				} else {
					if(nr.isOne()) {
						nr = mnum[0].number().denominator();
					} else {
						Number nden(mnum[0].number().denominator());
						if(nr != nden) {
							Number ngcd(nden);
							ngcd.gcd(nr);
							nden /= ngcd;
							nr *= nden;
						}
					}
				}
			}
			break;
		}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < mnum.size() && (bfrac || bint); i++) {
				idm2(mnum[i], bfrac, bint, nr);
			}
			break;
		}
	}
}
void idm3(MathStructure &mnum, Number &nr) {
	switch(mnum.type()) {
		case STRUCT_NUMBER: {
			mnum.number() *= nr;
			mnum.numberUpdated();
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(mnum.size() > 0 && mnum[0].isNumber()) {
				mnum[0].number() *= nr;
			} else {
				mnum.insertChild(nr, 1);
			}
			break;
		}
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < mnum.size(); i++) {
				idm3(mnum[i], nr);
			}
			break;
		}
		default: {
			mnum.transform(STRUCT_MULTIPLICATION);
			mnum.insertChild(nr, 1);
		}
	}
}

bool MathStructure::improve_division_multipliers(const PrintOptions &po) {
	switch(m_type) {
		case STRUCT_MULTIPLICATION: {
			size_t inum = 0, iden = 0;
			bool bfrac = false, bint = true, bdiv = false, bnonunitdiv = false;
			size_t index1, index2;
			bool dofrac = !po.negative_exponents;
			for(size_t i2 = 0; i2 < SIZE; i2++) {
				if(CHILD(i2).isPower() && CHILD(i2)[1].isMinusOne()) {
					if(!po.place_units_separately || !CHILD(i2)[0].isUnit()) {
						if(iden == 0) index1 = i2;
						iden++;
						bdiv = true;
						if(!CHILD(i2)[0].isUnit()) bnonunitdiv = true;
						if(CHILD(i2)[0].contains(STRUCT_ADDITION)) {
							dofrac = true;
						}
					}
				} else if(!bdiv && !po.negative_exponents && CHILD(i2).isPower() && CHILD(i2)[1].hasNegativeSign()) {
					if(!po.place_units_separately || !CHILD(i2)[0].isUnit()) {
						if(!bdiv) index1 = i2;
						bdiv = true;
						if(!CHILD(i2)[0].isUnit()) bnonunitdiv = true;
					}
				} else {
					if(!po.place_units_separately || !CHILD(i2).isUnit_exp()) {
						if(inum == 0) index2 = i2;
						inum++;
					}
				}
			}
			if(!bdiv || !bnonunitdiv) break;
			if(iden > 1 && !po.negative_exponents) {
				size_t i2 = index1 + 1;
				while(i2 < SIZE) {
					if(CHILD(i2).isPower() && CHILD(i2)[1].isMinusOne()) {
						CHILD(index1)[0].multiply(CHILD(i2)[0], true);
						ERASE(i2);
					} else {
						i2++;
					}
				}
				iden = 1;
			}
			if(bint) bint = inum > 0 && iden == 1;
			if(inum > 0) idm1(CHILD(index2), bfrac, bint);
			if(iden > 0) idm1(CHILD(index1)[0], bfrac, bint);
			bool b = false;
			if(!dofrac) bfrac = false;
			if(bint || bfrac) {
				Number nr(1, 1);
				if(inum > 0) idm2(CHILD(index2), bfrac, bint, nr);
				if(iden > 0) idm2(CHILD(index1)[0], bfrac, bint, nr);
				if((bint || bfrac) && !nr.isOne()) {
					if(bint) {
						nr.recip();
					}
					if(inum == 0) {
						PREPEND(MathStructure(nr));
					} else if(inum > 1 && !CHILD(index2).isNumber()) {
						idm3(*this, nr);
					} else {
						idm3(CHILD(index2), nr);
					}
					if(iden > 0) {
						idm3(CHILD(index1)[0], nr);
					} else {
						MathStructure mstruct(nr);
						mstruct.raise(m_minus_one);
						insertChild(mstruct, index1);
					}
					b = true;
				}
			}
			/*if(!po.negative_exponents && SIZE == 2 && CHILD(1).isAddition()) {
				MathStructure factor_mstruct(1, 1);
				if(factorize_find_multiplier(CHILD(1), CHILD(1), factor_mstruct)) {
					transform(STRUCT_MULTIPLICATION);
					PREPEND(factor_mstruct);
				}
			}*/
			return b;
		}
		case STRUCT_DIVISION: {
			bool bint = true, bfrac = false;
			idm1(CHILD(0), bfrac, bint);
			idm1(CHILD(1), bfrac, bint);
			if(bint || bfrac) {
				Number nr(1, 1);
				idm2(CHILD(0), bfrac, bint, nr);
				idm2(CHILD(1), bfrac, bint, nr);
				if((bint || bfrac) && !nr.isOne()) {
					if(bint) {
						nr.recip();
					}
					idm3(CHILD(0), nr);
					idm3(CHILD(1), nr);
					return true;
				}
			}
			break;
		}
		case STRUCT_INVERSE: {
			bool bint = false, bfrac = false;
			idm1(CHILD(0), bfrac, bint);
			if(bint || bfrac) {
				Number nr(1, 1);
				idm2(CHILD(0), bfrac, bint, nr);
				if((bint || bfrac) && !nr.isOne()) {
					setToChild(1, true);
					idm3(*this, nr);
					transform_nocopy(STRUCT_DIVISION, new MathStructure(nr));
					SWAP_CHILDS(0, 1);
					return true;
				}
			}
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(1).isMinusOne()) {
				bool bint = false, bfrac = false;
				idm1(CHILD(0), bfrac, bint);
				if(bint || bfrac) {
					Number nr(1, 1);
					idm2(CHILD(0), bfrac, bint, nr);
					if((bint || bfrac) && !nr.isOne()) {
						idm3(CHILD(0), nr);
						transform(STRUCT_MULTIPLICATION);
						PREPEND(MathStructure(nr));
						return true;
					}
				}
				break;
			}
		}
		default: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).improve_division_multipliers()) b = true;
			}
			return b;
		}
	}
	return false;
}

void MathStructure::setPrefixes(const PrintOptions &po, MathStructure *parent, size_t pindex) {
	switch(m_type) {
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			size_t i = SIZE;
			for(size_t i2 = 0; i2 < SIZE; i2++) {
				if(CHILD(i2).isUnit_exp()) {
					if((CHILD(i2).isUnit() && CHILD(i2).prefix()) || (CHILD(i2).isPower() && CHILD(i2)[0].prefix())) {
						b = false;
						return;
					}
					if(po.use_prefixes_for_currencies || (CHILD(i2).isPower() && !CHILD(i2)[0].unit()->isCurrency()) || (CHILD(i2).isUnit() && !CHILD(i2).unit()->isCurrency())) {
						b = true;
						if(i > i2) i = i2;
						break;
					} else if(i < i2) {
						i = i2;
					}
				}
			}
			if(b) {
				Number exp(1, 1);
				Number exp2(1, 1);
				bool b2 = false;
				size_t i2 = i;
				if(CHILD(i).isPower()) {
					if(CHILD(i)[1].isNumber() && CHILD(i)[1].number().isInteger()) {
						exp = CHILD(i)[1].number();
					} else {
						b = false;
					}
				}
				if(po.use_denominator_prefix && !exp.isNegative()) {
					for(; i2 < SIZE; i2++) {
						if(CHILD(i2).isPower() && CHILD(i2)[0].isUnit() && CHILD(i2)[1].isNumber() && CHILD(i2)[1].number().isNegative()) {
							if(CHILD(i2)[1].prefix() || !CHILD(i2)[1].number().isInteger()) {
								break;
							}
							if(!b) {
								b = true;
								exp = CHILD(i2)[1].number();
								i = i2;
							} else {
								b2 = true;
								exp2 = CHILD(i2)[1].number();
							}
							break;
						}
					}
				} else if(exp.isNegative() && b) {
					for(; i2 < SIZE; i2++) {
						if(CHILD(i2).isPower() && CHILD(i2)[0].isUnit() && !(CHILD(i2)[1].isNumber() && CHILD(i2)[1].number().isNegative())) {
							if(CHILD(i2)[1].prefix() || !CHILD(i2)[1].number().isInteger()) {
								if(!po.use_denominator_prefix) {
									b = false;
								}
								break;
							}
							if(po.use_denominator_prefix) {
								b2 = true;
								exp2 = exp;
								exp = CHILD(i2)[1].number();
								size_t i3 = i;
								i = i2;
								i2 = i3;
							} else {
								i = i2;
								exp = CHILD(i2)[1].number();
							}
							break;
						} else if(CHILD(i2).isUnit()) {
							if(po.use_denominator_prefix) {
								b2 = true;
								exp2 = exp;
								exp.set(1, 1);
								size_t i3 = i;
								i = i2;
								i2 = i3;
							} else {
								i = i2;
								exp.set(1, 1);
							}
							break;
						}
					}
				}
				Number exp10;
				if(b) {
					if(po.prefix) {
						if(po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix) {
							if(CHILD(i).isUnit()) CHILD(i).setPrefix(po.prefix);
							else CHILD(i)[0].setPrefix(po.prefix);
							if(CHILD(0).isNumber()) {
								CHILD(0).number() /= po.prefix->value(exp);
							} else {
								PREPEND(po.prefix->value(exp));
								CHILD(0).number().recip();
							}
							if(b2) {
								exp10 = CHILD(0).number();
								exp10.log(10);
								exp10.floor();
							}
						}
					} else if(po.use_unit_prefixes && CHILD(0).isNumber() && exp.isInteger()) {
						exp10 = CHILD(0).number();
						exp10.log(10);
						exp10.floor();
						if(b2) {	
							Number tmp_exp(exp10);
							tmp_exp.setNegative(false);
							Number e1(3, 1);
							e1 *= exp;
							Number e2(3, 1);
							e2 *= exp2;
							e2.setNegative(false);
							int i = 0;
							while(true) {
								tmp_exp -= e1;
								if(!tmp_exp.isPositive()) {
									break;
								}
								if(exp10.isNegative()) i++;
								tmp_exp -= e2;
								if(tmp_exp.isNegative()) {
									break;
								}
								if(!exp10.isNegative())	i++;
							}
							e2.setNegative(exp10.isNegative());
							e2 *= i;
							exp10 -= e2;
						}
						DecimalPrefix *p = CALCULATOR->getBestDecimalPrefix(exp10, exp, po.use_all_prefixes);
						if(p) {
							Number test_exp(exp10);
							test_exp -= p->exponent(exp);
							if(test_exp.isInteger()) {
								if((exp10.isPositive() && exp10.compare(test_exp) == COMPARISON_RESULT_LESS) || (exp10.isNegative() && exp10.compare(test_exp) == COMPARISON_RESULT_GREATER)) {
									CHILD(0).number() /= p->value(exp);
									if(CHILD(i).isUnit()) CHILD(i).setPrefix(p);
									else CHILD(i)[0].setPrefix(p);
								}
							}
						}
					}
					if(b2 && CHILD(0).isNumber() && ((po.prefix && po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix) || po.use_unit_prefixes)) {
						exp10 = CHILD(0).number();
						exp10.log(10);
						exp10.floor();
						DecimalPrefix *p = CALCULATOR->getBestDecimalPrefix(exp10, exp2, po.use_all_prefixes);
						if(p) {
							Number test_exp(exp10);
							test_exp -= p->exponent(exp2);
							if(test_exp.isInteger()) {
								if((exp10.isPositive() && exp10.compare(test_exp) == COMPARISON_RESULT_LESS) || (exp10.isNegative() && exp10.compare(test_exp) == COMPARISON_RESULT_GREATER)) {
									CHILD(0).number() /= p->value(exp2);
									CHILD(i2)[0].setPrefix(p);
								}
							}
						}
					}	
				}
				break;
			}
		}
		case STRUCT_UNIT: {
			if(!o_prefix && (po.prefix && po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix)) {
				transform(STRUCT_MULTIPLICATION, m_one);
				SWAP_CHILDS(0, 1);
				setPrefixes(po, parent, pindex);
			}
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(0).isUnit()) {
				if(CHILD(1).isNumber() && CHILD(1).number().isReal() && !o_prefix && (po.prefix && po.prefix != CALCULATOR->decimal_null_prefix && po.prefix != CALCULATOR->binary_null_prefix)) {
					transform(STRUCT_MULTIPLICATION, m_one);
					SWAP_CHILDS(0, 1);
					setPrefixes(po, parent, pindex);
				}
				break;
			}
		}
		default: {
			for(size_t i = 0; i < SIZE; i++) {
				CHILD(i).setPrefixes(po, this, i + 1);
			}
		}
	}
}
void MathStructure::postFormatUnits(const PrintOptions &po, MathStructure*, size_t) {
	switch(m_type) {
		case STRUCT_DIVISION: {
			if(po.place_units_separately) {
				vector<size_t> nums;
				bool b1 = false, b2 = false;
				if(CHILD(0).isMultiplication()) {
					for(size_t i = 0; i < CHILD(0).size(); i++) {
						if(CHILD(0)[i].isUnit_exp()) {
							nums.push_back(i);
						} else {
							b1 = true;
						}
					}
					b1 = b1 && !nums.empty();
				} else if(CHILD(0).isUnit_exp()) {
					b1 = true;
				}
				vector<size_t> dens;
				if(CHILD(1).isMultiplication()) {
					for(size_t i = 0; i < CHILD(1).size(); i++) {
						if(CHILD(1)[i].isUnit_exp()) {
							dens.push_back(i);
						} else {
							b2 = true;
						}
					}
					b2 = b2 && !dens.empty();
				} else if(CHILD(1).isUnit_exp()) {
					if(CHILD(0).isUnit_exp()) {
						b1 = false;
					} else {
						b2 = true;
					}
				}
				if(b2 && !b1) b1 = true;
				if(b1) {
					MathStructure num = m_undefined;
					if(CHILD(0).isUnit_exp()) {
						num = CHILD(0);
						CHILD(0).set(m_one);
					} else if(nums.size() > 0) {
						num = CHILD(0)[nums[0]];
						for(size_t i = 1; i < nums.size(); i++) {
							num.multiply(CHILD(0)[nums[i]], i > 1);
						}
						for(size_t i = 0; i < nums.size(); i++) {
							CHILD(0).delChild(nums[i] + 1 - i);
						}
						if(CHILD(0).size() == 1) {
							CHILD(0).setToChild(1, true);
						}
					}
					MathStructure den = m_undefined;
					if(CHILD(1).isUnit_exp()) {
						den = CHILD(1);
						setToChild(1, true);
					} else if(dens.size() > 0) {
						den = CHILD(1)[dens[0]];
						for(size_t i = 1; i < dens.size(); i++) {
							den.multiply(CHILD(1)[dens[i]], i > 1);
						}
						for(size_t i = 0; i < dens.size(); i++) {
							CHILD(1).delChild(dens[i] + 1 - i);
						}
						if(CHILD(1).size() == 1) {
							CHILD(1).setToChild(1, true);
						}
					}
					if(num.isUndefined()) {
						transform(STRUCT_DIVISION, den);
					} else {
						if(!den.isUndefined()) {
							num.transform(STRUCT_DIVISION, den);
						}
						multiply(num, false);
					}
					if(CHILD(0).isDivision()) {
						if(CHILD(0)[0].isMultiplication()) {
							if(CHILD(0)[0].size() == 1) {
								CHILD(0)[0].setToChild(1, true);
							} else if(CHILD(0)[0].size() == 0) {
								CHILD(0)[0] = 1;
							}
						}
						if(CHILD(0)[1].isMultiplication()) {
							if(CHILD(0)[1].size() == 1) {
								CHILD(0)[1].setToChild(1, true);
							} else if(CHILD(0)[1].size() == 0) {
								CHILD(0).setToChild(1, true);
							}
						} else if(CHILD(0)[1].isOne()) {
							CHILD(0).setToChild(1, true);
						}
						if(CHILD(0).isDivision() && CHILD(0)[1].isNumber() && CHILD(0)[0].isMultiplication() && CHILD(0)[0].size() > 1 && CHILD(0)[0][0].isNumber()) {
							MathStructure *msave = new MathStructure;
							if(CHILD(0)[0].size() == 2) {
								msave->set(CHILD(0)[0][1]);
								CHILD(0)[0].setToChild(1, true);
							} else {
								msave->set(CHILD(0)[0]);
								CHILD(0)[0].setToChild(1, true);
								msave->delChild(1);
							}
							if(isMultiplication()) {
								insertChild_nocopy(msave, 2);
							} else {
								CHILD(0).multiply_nocopy(msave);
							}
						}
					}
					bool do_plural = po.short_multiplication;
					switch(CHILD(0).type()) {
						case STRUCT_NUMBER: {
							if(CHILD(0).isZero() || CHILD(0).number().isOne() || CHILD(0).number().isMinusOne() || CHILD(0).number().isFraction()) {
								do_plural = false;
							}
							break;
						}
						case STRUCT_DIVISION: {
							if(CHILD(0)[0].isNumber() && CHILD(0)[1].isNumber()) {
								if(CHILD(0)[0].number().isLessThanOrEqualTo(CHILD(0)[1].number())) {
									do_plural = false;
								}
							}
							break;
						}
						case STRUCT_INVERSE: {
							if(CHILD(0)[0].isNumber() && CHILD(0)[0].number().isGreaterThanOrEqualTo(1)) {
								do_plural = false;
							}
							break;
						}
					}
					switch(CHILD(1).type()) {
						case STRUCT_UNIT: {
							CHILD(1).setPlural(do_plural);
							break;
						}
						case STRUCT_POWER: {
							CHILD(1)[0].setPlural(do_plural);
							break;
						}
						case STRUCT_MULTIPLICATION: {
							if(po.limit_implicit_multiplication) CHILD(1)[0].setPlural(do_plural);
							else CHILD(1)[CHILD(1).size() - 1].setPlural(do_plural);
							break;
						}
						case STRUCT_DIVISION: {
							switch(CHILD(1)[0].type()) {
								case STRUCT_UNIT: {
									CHILD(1)[0].setPlural(do_plural);
									break;
								}
								case STRUCT_POWER: {
									CHILD(1)[0][0].setPlural(do_plural);
									break;
								}
								case STRUCT_MULTIPLICATION: {
									if(po.limit_implicit_multiplication) CHILD(1)[0][0].setPlural(do_plural);
									else CHILD(1)[0][CHILD(1)[0].size() - 1].setPlural(do_plural);
									break;
								}
							}
							break;
						}
						
					}
				}
			} else {
				for(size_t i = 0; i < SIZE; i++) {
					CHILD(i).postFormatUnits(po, this, i + 1);
					CHILD_UPDATED(i);
				}
			}
			break;
		}
		case STRUCT_UNIT: {
			b_plural = false;
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(SIZE > 1 && CHILD(1).isUnit_exp() && CHILD(0).isNumber()) {
				bool do_plural = po.short_multiplication && !(CHILD(0).isZero() || CHILD(0).number().isOne() || CHILD(0).number().isMinusOne() || CHILD(0).number().isFraction());
				size_t i = 2;
				for(; i < SIZE; i++) {
					if(CHILD(i).isUnit()) {
						CHILD(i).setPlural(false);
					} else if(CHILD(i).isPower() && CHILD(i)[0].isUnit()) {
						CHILD(i)[0].setPlural(false);
					} else {
						break;
					}
				}
				if(do_plural) {
					if(po.limit_implicit_multiplication) i = 1;
					else i--;
					if(CHILD(i).isUnit()) {
						CHILD(i).setPlural(true);
					} else {
						CHILD(i)[0].setPlural(true);
					}
				}
			} else if(SIZE > 0) {
				int last_unit = -1;
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).isUnit()) {
						CHILD(i).setPlural(false);
						if(!po.limit_implicit_multiplication || last_unit < 0) {
							last_unit = i;
						}
					} else if(CHILD(i).isPower() && CHILD(i)[0].isUnit()) {
						CHILD(i)[0].setPlural(false);
						if(!po.limit_implicit_multiplication || last_unit < 0) {
							last_unit = i;
						}
					} else if(last_unit >= 0) {
						break;
					}
				}
				if(po.short_multiplication && last_unit > 0) {
					if(CHILD(last_unit).isUnit()) {
						CHILD(last_unit).setPlural(true);
					} else {
						CHILD(last_unit)[0].setPlural(true);
					}
				}
			}
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(0).isUnit()) {
				CHILD(0).setPlural(false);
				break;
			}
		}
		default: {
			for(size_t i = 0; i < SIZE; i++) {
				CHILD(i).postFormatUnits(po, this, i + 1);
				CHILD_UPDATED(i);
			}
		}
	}
}
void MathStructure::prefixCurrencies() {
	if(isMultiplication()) {
		int index = -1;
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).isUnit_exp()) {
				if(CHILD(i).isUnit() && CHILD(i).unit()->isCurrency()) {
					if(index >= 0) {
						index = -1;
						break;
					}
					index = i;
				} else {
					index = -1;
					break;
				}
			}
		}
		if(index >= 0) {
			v_order.insert(v_order.begin(), v_order[index]);
			v_order.erase(v_order.begin() + (index + 1));
		}
	} else {
		for(size_t i = 0; i < SIZE; i++) {
			CHILD(i).prefixCurrencies();
		}
	}
}
void MathStructure::format(const PrintOptions &po) {
	if(!po.preserve_format) {
		sort(po);
		if(po.improve_division_multipliers) {
			if(improve_division_multipliers(po)) sort(po);
		}
		setPrefixes(po);
	}
	formatsub(po);
	if(!po.preserve_format) {
		postFormatUnits(po);
		if(po.sort_options.prefix_currencies && po.abbreviate_names && CALCULATOR->place_currency_code_before) {
			prefixCurrencies();
		}
	}
}
void MathStructure::formatsub(const PrintOptions &po, MathStructure *parent, size_t pindex) {

	for(size_t i = 0; i < SIZE; i++) {
		CHILD(i).formatsub(po, this, i + 1);
		CHILD_UPDATED(i);
	}
	switch(m_type) {
		case STRUCT_ADDITION: {
			break;
		}
		case STRUCT_DIVISION: {
			if(po.preserve_format) break;
			if(CHILD(0).isAddition() && CHILD(0).size() > 0) {
				bool b = true;
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(!CHILD(0)[i].isNegate()) {
						b = false;
						break;
					}
				}
				if(b) {
					for(size_t i = 0; i < CHILD(0).size(); i++) {
						CHILD(0)[i].setToChild(1, true);
					}
					transform(STRUCT_NEGATE);
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			if(po.preserve_format) break;
			if(CHILD(0).isNegate()) {
				CHILD(0).number().negate();
				if(CHILD(0)[0].isOne()) {
					ERASE(0);
					if(SIZE == 1) {
						setToChild(1, true);
					}
				} else {
					CHILD(0).setToChild(1, true);
				}
				transform(STRUCT_NEGATE);
				formatsub(po, parent, pindex);
				break;
			}
			
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).isInverse()) {
					if(!po.negative_exponents || !CHILD(i)[0].isNumber()) {
						b = true;
						break;
					}
				} else if(CHILD(i).isDivision()) {
					if(!CHILD(i)[0].isNumber() || !CHILD(i)[1].isNumber() || (!po.negative_exponents && CHILD(i)[0].number().isOne())) {
						b = true;
						break;
					}
				}
			}

			if(b) {
				MathStructure *den = new MathStructure();
				MathStructure *num = new MathStructure();
				num->setUndefined();
				short ds = 0, ns = 0;
				MathStructure *mnum = NULL, *mden = NULL;
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).isInverse()) {
						mden = &CHILD(i)[0];
					} else if(CHILD(i).isDivision()) {
						mnum = &CHILD(i)[0];
						mden = &CHILD(i)[1];
					} else {
						mnum = &CHILD(i);
					}
					if(mnum) {
						if(ns > 0) {
							if(mnum->isMultiplication() && num->isNumber()) {
								for(size_t i2 = 0; i2 < mnum->size(); i2++) {
									num->multiply((*mnum)[i2], true);
								}
							} else {
								num->multiply(*mnum, ns > 1);
							}
						} else {
							num->set(*mnum);
						}						
						ns++;
						mnum = NULL;
					}
					if(mden) {
						if(ds > 0) {	
							if(mden->isMultiplication() && den->isNumber()) {
								for(size_t i2 = 0; i2 < mden->size(); i2++) {
									den->multiply((*mden)[i2], true);
								}
							} else {
								den->multiply(*mden, ds > 1);
							}							
						} else {
							den->set(*mden);
						}
						ds++;
						mden = NULL;
					}
				}
				clear(true);
				m_type = STRUCT_DIVISION;
				if(num->isUndefined()) num->set(m_one);
				APPEND_POINTER(num);
				APPEND_POINTER(den);
				formatsub(po, parent, pindex);
				break;
			}

			size_t index = 0;
			if(CHILD(0).isOne()) {
				index = 1;
			}
			switch(CHILD(index).type()) {
				case STRUCT_POWER: {
					if(!CHILD(index)[0].isUnit_exp()) {
						break;
					}
				}
				case STRUCT_UNIT: {
					if(index == 0) {
						if(!parent || (!parent->isDivision() || pindex != 2)) {
							PREPEND(m_one);
						}
					}
					break;
				}
				default: {
					if(index == 1) {
						ERASE(0);
						if(SIZE == 1) {
							setToChild(1, true);
						}
					}
				}
			}

			break;
		}
		case STRUCT_UNIT: {
			if(po.preserve_format) break;
			if(!parent || (!parent->isPower() && !parent->isMultiplication() && !parent->isInverse() && !(parent->isDivision() && pindex == 2))) {				
				multiply(m_one);
				SWAP_CHILDS(0, 1);
			}
			break;
		}
		case STRUCT_POWER: {
			if(po.preserve_format) break;
			if((!po.negative_exponents || CHILD(0).isAddition()) && CHILD(1).isNegate() && (!CHILD(0).isVector() || !CHILD(1).isMinusOne())) {
				if(CHILD(1)[0].isOne()) {
					m_type = STRUCT_INVERSE;
					ERASE(1);
				} else {
					CHILD(1).setToChild(1, true);
					transform(STRUCT_INVERSE);
				}
				formatsub(po, parent, pindex);
			} else if(po.halfexp_to_sqrt && ((CHILD(1).isDivision() && CHILD(1)[0].isNumber() && CHILD(1)[0].number().isInteger() && CHILD(1)[1].isNumber() && CHILD(1)[1].number().isTwo()) || (CHILD(1).isNumber() && CHILD(1).number().denominatorIsTwo()) || (CHILD(1).isInverse() && CHILD(1)[0].isNumber() && CHILD(1)[0].number() == 2))) {
				if(CHILD(1).isInverse() || (CHILD(1).isDivision() && CHILD(1)[0].number().isOne()) || (CHILD(1).isNumber() && CHILD(1).number().numeratorIsOne())) {
					m_type = STRUCT_FUNCTION;
					ERASE(1)
					o_function = CALCULATOR->f_sqrt;
				} else {
					if(CHILD(1).isNumber()) {
						CHILD(1).number() -= Number(1, 2);
					} else {
						Number nr = CHILD(1)[0].number();
						nr /= CHILD(1)[1].number();
						nr.floor();
						CHILD(1).set(nr);
					}
					if(CHILD(1).number().isOne()) {
						setToChild(1, true);
						if(parent && parent->isMultiplication()) {
							parent->insertChild(MathStructure(CALCULATOR->f_sqrt, this, NULL), pindex + 1);
						} else {
							multiply(MathStructure(CALCULATOR->f_sqrt, this, NULL));
						}
					} else {
						if(parent && parent->isMultiplication()) {
							parent->insertChild(MathStructure(CALCULATOR->f_sqrt, &CHILD(0), NULL), pindex + 1);
						} else {
							multiply(MathStructure(CALCULATOR->f_sqrt, &CHILD(0), NULL));
						}
					}
				}
				formatsub(po, parent, pindex);
			} else if(CHILD(0).isUnit_exp() && (!parent || (!parent->isPower() && !parent->isMultiplication() && !parent->isInverse() && !(parent->isDivision() && pindex == 2)))) {
				multiply(m_one);
				SWAP_CHILDS(0, 1);
			}
			break;
		}
		case STRUCT_NUMBER: {
			if(o_number.isNegative()) {
				o_number.negate();
				transform(STRUCT_NEGATE);
				formatsub(po, parent, pindex);
			} else if(po.number_fraction_format == FRACTION_COMBINED && po.base != BASE_SEXAGESIMAL && po.base != BASE_TIME && o_number.isRational() && !o_number.isInteger()) {
				if(o_number.isFraction()) {
					Number num(o_number.numerator());
					Number den(o_number.denominator());
					clear(true);
					if(num.isOne()) {
						m_type = STRUCT_INVERSE;
					} else {
						m_type = STRUCT_DIVISION;
						APPEND_NEW(num);
					}
					APPEND_NEW(den);
				} else {
					Number frac(o_number);
					frac.frac();
					MathStructure *num = new MathStructure(frac.numerator());
					num->transform(STRUCT_DIVISION, frac.denominator());
					o_number.trunc();
					add_nocopy(num);
				}
			} else if((po.number_fraction_format == FRACTION_FRACTIONAL || po.base == BASE_ROMAN_NUMERALS) && po.base != BASE_SEXAGESIMAL && po.base != BASE_TIME && o_number.isRational() && !o_number.isInteger()) {
				Number num(o_number.numerator());
				Number den(o_number.denominator());
				clear(true);
				if(num.isOne()) {
					m_type = STRUCT_INVERSE;
				} else {
					m_type = STRUCT_DIVISION;
					APPEND_NEW(num);
				}
				APPEND_NEW(den);
			} else if(o_number.isComplex()) {
				if(o_number.hasRealPart()) {
					Number re(o_number.realPart());
					Number im(o_number.imaginaryPart());
					MathStructure *mstruct = new MathStructure(im);
					if(im.isOne()) {
						mstruct->set(CALCULATOR->v_i);
					} else {
						mstruct->multiply_nocopy(new MathStructure(CALCULATOR->v_i));
					}
					o_number = re;
					add_nocopy(mstruct);
					formatsub(po, parent, pindex);
				} else {
					Number im(o_number.imaginaryPart());
					if(im.isOne()) {
						set(CALCULATOR->v_i, true);
					} else if(im.isMinusOne()) {
						set(CALCULATOR->v_i, true);
						transform(STRUCT_NEGATE);
					} else {
						o_number = im;
						multiply_nocopy(new MathStructure(CALCULATOR->v_i));
					}
					formatsub(po, parent, pindex);
				}
			}
			break;
		}
	}
}

int namelen(const MathStructure &mstruct, const PrintOptions &po, const InternalPrintStruct&, bool *abbreviated = NULL) {
	const string *str;
	switch(mstruct.type()) {
		case STRUCT_FUNCTION: {
			const ExpressionName *ename = &mstruct.function()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			str = &ename->name;
			if(abbreviated) *abbreviated = ename->abbreviation;
			break;
		}
		case STRUCT_VARIABLE:  {
			const ExpressionName *ename = &mstruct.variable()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			str = &ename->name;
			if(abbreviated) *abbreviated = ename->abbreviation;
			break;
		}
		case STRUCT_SYMBOLIC:  {
			str = &mstruct.symbol();
			if(abbreviated) *abbreviated = false;
			break;
		}
		case STRUCT_UNIT:  {
			const ExpressionName *ename = &mstruct.unit()->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, mstruct.isPlural(), po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			str = &ename->name;
			if(abbreviated) *abbreviated = ename->abbreviation;
			break;
		}
		default: {if(abbreviated) *abbreviated = false; return 0;}
	}
	if(text_length_is_one(*str)) return 1;
	return str->length();
}

bool MathStructure::needsParenthesis(const PrintOptions &po, const InternalPrintStruct&, const MathStructure &parent, size_t index, bool flat_division, bool) const {
	switch(parent.type()) {
		case STRUCT_MULTIPLICATION: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return flat_division;}
				case STRUCT_INVERSE: {return flat_division;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return false;}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return po.excessive_parenthesis;}
				default: {return true;}
			}
		}
		case STRUCT_INVERSE: {}
		case STRUCT_DIVISION: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_DIVISION: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_POWER: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_BITWISE_OR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_BITWISE_XOR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_BITWISE_NOT: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_OR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_XOR: {return flat_division || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_NOT: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return flat_division || po.excessive_parenthesis;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return false;}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_ADDITION: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return po.excessive_parenthesis;}
				case STRUCT_DIVISION: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return index > 1 || po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return false;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return false;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return false;}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_POWER: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return index == 1 || flat_division || po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return index == 1 || flat_division || po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return true;}
				case STRUCT_NEGATE: {return index == 1 || po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return index == 1 || po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return index == 1 || po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return false;}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_NEGATE: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return false;}
				case STRUCT_DIVISION: {return po.excessive_parenthesis;}
				case STRUCT_INVERSE: {return flat_division && po.excessive_parenthesis;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return true;}
				case STRUCT_NEGATE: {return true;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return po.excessive_parenthesis;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return po.excessive_parenthesis;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return false;}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_BITWISE_AND: {}
		case STRUCT_BITWISE_OR: {}
		case STRUCT_BITWISE_XOR: {}
		case STRUCT_LOGICAL_AND: {}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_XOR: {}
		case STRUCT_COMPARISON: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return flat_division;}
				case STRUCT_INVERSE: {return flat_division;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return false;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return false;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return false;}
				case STRUCT_VECTOR: {return false;}
				case STRUCT_NUMBER: {return false;}
				case STRUCT_VARIABLE: {return false;}
				case STRUCT_SYMBOLIC: {return false;}
				case STRUCT_UNIT: {return false;}
				case STRUCT_UNDEFINED: {return false;}
				default: {return true;}
			}
		}
		case STRUCT_LOGICAL_NOT: {}
		case STRUCT_BITWISE_NOT: {
			switch(m_type) {
				case STRUCT_MULTIPLICATION: {return true;}
				case STRUCT_DIVISION: {return true;}
				case STRUCT_INVERSE: {return true;}
				case STRUCT_ADDITION: {return true;}
				case STRUCT_POWER: {return po.excessive_parenthesis;}
				case STRUCT_NEGATE: {return po.excessive_parenthesis;}
				case STRUCT_BITWISE_AND: {return true;}
				case STRUCT_BITWISE_OR: {return true;}
				case STRUCT_BITWISE_XOR: {return true;}
				case STRUCT_BITWISE_NOT: {return true;}
				case STRUCT_LOGICAL_AND: {return true;}
				case STRUCT_LOGICAL_OR: {return true;}
				case STRUCT_LOGICAL_XOR: {return true;}
				case STRUCT_LOGICAL_NOT: {return true;}
				case STRUCT_COMPARISON: {return true;}				
				case STRUCT_FUNCTION: {return po.excessive_parenthesis;}
				case STRUCT_VECTOR: {return po.excessive_parenthesis;}
				case STRUCT_NUMBER: {return po.excessive_parenthesis;}
				case STRUCT_VARIABLE: {return po.excessive_parenthesis;}
				case STRUCT_SYMBOLIC: {return po.excessive_parenthesis;}
				case STRUCT_UNIT: {return po.excessive_parenthesis;}
				case STRUCT_UNDEFINED: {return po.excessive_parenthesis;}
				default: {return true;}
			}
		}
		case STRUCT_FUNCTION: {
			return false;
		}
		case STRUCT_VECTOR: {
			return false;
		}
		default: {
			return true;
		}
	}
}

int MathStructure::neededMultiplicationSign(const PrintOptions &po, const InternalPrintStruct &ips, const MathStructure &parent, size_t index, bool par, bool par_prev, bool flat_division, bool flat_power) const {
	if(!po.short_multiplication) return MULTIPLICATION_SIGN_OPERATOR;
	if(index <= 1) return MULTIPLICATION_SIGN_NONE;
	if(par_prev && par) return MULTIPLICATION_SIGN_NONE;
	if(par_prev) return MULTIPLICATION_SIGN_OPERATOR;
	int t = parent[index - 2].type();
	if(flat_power && t == STRUCT_POWER) return MULTIPLICATION_SIGN_OPERATOR;
	if(par && t == STRUCT_POWER) return MULTIPLICATION_SIGN_SPACE;
	if(par) return MULTIPLICATION_SIGN_NONE;
	bool abbr_prev = false, abbr_this = false;
	int namelen_prev = namelen(parent[index - 2], po, ips, &abbr_prev);
	int namelen_this = namelen(*this, po, ips, &abbr_this);	
	switch(t) {
		case STRUCT_MULTIPLICATION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_INVERSE: {}
		case STRUCT_DIVISION: {if(flat_division) return MULTIPLICATION_SIGN_OPERATOR; return MULTIPLICATION_SIGN_SPACE;}
		case STRUCT_ADDITION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_POWER: {if(flat_power) return MULTIPLICATION_SIGN_OPERATOR; break;}
		case STRUCT_NEGATE: {break;}
		case STRUCT_BITWISE_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_COMPARISON: {return MULTIPLICATION_SIGN_OPERATOR;}		
		case STRUCT_FUNCTION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_VECTOR: {break;}
		case STRUCT_NUMBER: {break;}
		case STRUCT_VARIABLE: {break;}
		case STRUCT_SYMBOLIC: {
			break;
		}
		case STRUCT_UNIT: {
			if(m_type == STRUCT_UNIT) {
				if(!po.limit_implicit_multiplication && !abbr_prev && !abbr_this) {
					return MULTIPLICATION_SIGN_SPACE;
				} 
				if(po.place_units_separately) {
					return MULTIPLICATION_SIGN_OPERATOR_SHORT;
				} else {
					return MULTIPLICATION_SIGN_OPERATOR;
				}
			} else if(m_type == STRUCT_NUMBER) {
				if(namelen_prev > 1) {
					return MULTIPLICATION_SIGN_SPACE;
				} else {
					return MULTIPLICATION_SIGN_NONE;
				}
			}
			//return MULTIPLICATION_SIGN_SPACE;
		}
		case STRUCT_UNDEFINED: {break;}
		default: {return MULTIPLICATION_SIGN_OPERATOR;}
	}
	switch(m_type) {
		case STRUCT_MULTIPLICATION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_INVERSE: {}
		case STRUCT_DIVISION: {if(flat_division) return MULTIPLICATION_SIGN_OPERATOR; return MULTIPLICATION_SIGN_SPACE;}
		case STRUCT_ADDITION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_POWER: {return CHILD(0).neededMultiplicationSign(po, ips, parent, index, CHILD(0).needsParenthesis(po, ips, *this, 1, flat_division, flat_power), par_prev, flat_division, flat_power);}
		case STRUCT_NEGATE: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_BITWISE_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_AND: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_OR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_XOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_LOGICAL_NOT: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_COMPARISON: {return MULTIPLICATION_SIGN_OPERATOR;}		
		case STRUCT_FUNCTION: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_VECTOR: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_NUMBER: {return MULTIPLICATION_SIGN_OPERATOR;}
		case STRUCT_VARIABLE: {}
		case STRUCT_SYMBOLIC: {
			if(po.limit_implicit_multiplication && t != STRUCT_NUMBER) return MULTIPLICATION_SIGN_OPERATOR;
			if(t != STRUCT_NUMBER && ((namelen_prev > 1 || namelen_this > 1) || equals(parent[index - 2]))) return MULTIPLICATION_SIGN_OPERATOR;
			if(namelen_this > 1 || (m_type == STRUCT_SYMBOLIC && !po.allow_non_usable)) return MULTIPLICATION_SIGN_SPACE;
			return MULTIPLICATION_SIGN_NONE;
		}
		case STRUCT_UNIT: {
			if(t == STRUCT_POWER && parent[index - 2][0].isUnit_exp()) {
				return MULTIPLICATION_SIGN_NONE;
			}			
			return MULTIPLICATION_SIGN_SPACE;
		}
		case STRUCT_UNDEFINED: {return MULTIPLICATION_SIGN_OPERATOR;}
		default: {return MULTIPLICATION_SIGN_OPERATOR;}
	}
}

string MathStructure::print(const PrintOptions &po, const InternalPrintStruct &ips) const {
	if(ips.depth == 0 && po.is_approximate) *po.is_approximate = false;
	string print_str;
	InternalPrintStruct ips_n = ips;
	if(isApproximate()) ips_n.parent_approximate = true;
	if(precision() > 0 && (ips_n.parent_precision < 1 || precision() < ips_n.parent_precision)) ips_n.parent_precision = precision();
	switch(m_type) {
		case STRUCT_NUMBER: {
			print_str = o_number.print(po, ips_n);
			break;
		}
		case STRUCT_SYMBOLIC: {
			if(po.allow_non_usable) {
				print_str = s_sym;
			} else {
				print_str = "\"";
				print_str += s_sym;
				print_str += "\"";
			}
			break;
		}
		case STRUCT_ADDITION: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					if(CHILD(i).type() == STRUCT_NEGATE) {
						if(po.spacious) print_str += " ";
						if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MINUS, po.can_display_unicode_string_arg))) print_str += SIGN_MINUS;
						else print_str += "-";
						if(po.spacious) print_str += " ";
						ips_n.wrap = CHILD(i)[0].needsParenthesis(po, ips_n, *this, i + 1, true, true);
						print_str += CHILD(i)[0].print(po, ips_n);
					} else {
						if(po.spacious) print_str += " ";
						print_str += "+";
						if(po.spacious) print_str += " ";
						ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
						print_str += CHILD(i).print(po, ips_n);
					}
				} else {
					ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
					print_str += CHILD(i).print(po, ips_n);
				}
			}
			break;
		}
		case STRUCT_NEGATE: {
			if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MINUS, po.can_display_unicode_string_arg))) print_str += SIGN_MINUS;
			else print_str = "-";
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_MULTIPLICATION: {
			ips_n.depth++;
			bool par_prev = false;
			for(size_t i = 0; i < SIZE; i++) {
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				if(!po.short_multiplication && i > 0) {
					if(po.spacious) print_str += " ";
					if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIDOT, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIDOT;
					else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_SMALLCIRCLE, po.can_display_unicode_string_arg))) print_str += SIGN_SMALLCIRCLE;
					else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_X && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIPLICATION, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIPLICATION;
					else print_str += "*";
					if(po.spacious) print_str += " ";
				} else if(i > 0) {
					switch(CHILD(i).neededMultiplicationSign(po, ips_n, *this, i + 1, ips_n.wrap, par_prev, true, true)) {
						case MULTIPLICATION_SIGN_SPACE: {print_str += " "; break;}
						case MULTIPLICATION_SIGN_OPERATOR: {
							if(po.spacious) {
								if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIDOT, po.can_display_unicode_string_arg))) print_str += " " SIGN_MULTIDOT " ";
								else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_SMALLCIRCLE, po.can_display_unicode_string_arg))) print_str += " " SIGN_SMALLCIRCLE " ";
								else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_X && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIPLICATION, po.can_display_unicode_string_arg))) print_str += " " SIGN_MULTIPLICATION " ";
								else print_str += " * ";
								break;
							}
						}
						case MULTIPLICATION_SIGN_OPERATOR_SHORT: {
							if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIDOT, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIDOT;
							else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_DOT && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_SMALLCIRCLE, po.can_display_unicode_string_arg))) print_str += SIGN_SMALLCIRCLE;
							else if(po.use_unicode_signs && po.multiplication_sign == MULTIPLICATION_SIGN_X && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_MULTIPLICATION, po.can_display_unicode_string_arg))) print_str += SIGN_MULTIPLICATION;
							else print_str += "*";
							break;
						}
					}
				}
				print_str += CHILD(i).print(po, ips_n);
				par_prev = ips_n.wrap;
			}
			break;
		}
		case STRUCT_INVERSE: {
			print_str = "1";
			if(po.spacious) print_str += " ";
			if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION;
			} else if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION_SLASH && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION_SLASH, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION_SLASH;
			} else {
				print_str += "/";
			}
			if(po.spacious) print_str += " ";
			ips_n.depth++;
			ips_n.division_depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_DIVISION: {
			ips_n.depth++;
			ips_n.division_depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str = CHILD(0).print(po, ips_n);
			if(po.spacious) print_str += " ";
			if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION;
			} else if(po.use_unicode_signs && po.division_sign == DIVISION_SIGN_DIVISION_SLASH && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_DIVISION_SLASH, po.can_display_unicode_string_arg))) {
				print_str += SIGN_DIVISION_SLASH;
			} else {
				print_str += "/";
			}
			if(po.spacious) print_str += " ";
			ips_n.wrap = CHILD(1).needsParenthesis(po, ips_n, *this, 2, true, true);
			print_str += CHILD(1).print(po, ips_n);
			break;
		}
		case STRUCT_POWER: {
			ips_n.depth++;
			ips_n.power_depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str = CHILD(0).print(po, ips_n);
			print_str += "^";
			ips_n.wrap = CHILD(1).needsParenthesis(po, ips_n, *this, 2, true, true);
			PrintOptions po2 = po;
			po2.show_ending_zeroes = false;
			print_str += CHILD(1).print(po2, ips_n);
			break;
		}
		case STRUCT_COMPARISON: {
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str = CHILD(0).print(po, ips_n);
			if(po.spacious) print_str += " ";
			switch(ct_comp) {
				case COMPARISON_EQUALS: {print_str += "="; break;}
				case COMPARISON_NOT_EQUALS: {
					if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_NOT_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_NOT_EQUAL;
					else print_str += "!="; 
					break;
				}
				case COMPARISON_GREATER: {print_str += ">"; break;}
				case COMPARISON_LESS: {print_str += "<"; break;}
				case COMPARISON_EQUALS_GREATER: {
					if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_GREATER_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_GREATER_OR_EQUAL;
					else print_str += ">="; 
					break;
				}
				case COMPARISON_EQUALS_LESS: {
					if(po.use_unicode_signs && (!po.can_display_unicode_string_function || (*po.can_display_unicode_string_function) (SIGN_LESS_OR_EQUAL, po.can_display_unicode_string_arg))) print_str += SIGN_LESS_OR_EQUAL;
					else print_str += "<="; 
					break;
				}
			}
			if(po.spacious) print_str += " ";
			ips_n.wrap = CHILD(1).needsParenthesis(po, ips_n, *this, 2, true, true);
			print_str += CHILD(1).print(po, ips_n);
			break;
		}
		case STRUCT_BITWISE_AND: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "&";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_BITWISE_OR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "|";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_BITWISE_XOR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "XOR";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_BITWISE_NOT: {
			print_str = "~";
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_LOGICAL_AND: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "&&";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_LOGICAL_OR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "||";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_LOGICAL_XOR: {
			ips_n.depth++;
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					if(po.spacious) print_str += " ";
					print_str += "XOR";
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			break;
		}
		case STRUCT_LOGICAL_NOT: {
			print_str = "!";
			ips_n.depth++;
			ips_n.wrap = CHILD(0).needsParenthesis(po, ips_n, *this, 1, true, true);
			print_str += CHILD(0).print(po, ips_n);
			break;
		}
		case STRUCT_VECTOR: {
			ips_n.depth++;
			print_str = "[";
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					print_str += po.comma();
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			print_str += "]";
			break;
		}
		case STRUCT_UNIT: {
			const ExpressionName *ename = &o_unit->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, b_plural, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			if(o_prefix) print_str += o_prefix->name(po.abbreviate_names && ename->abbreviation, po.use_unicode_signs, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			print_str += ename->name;
			if(po.hide_underscore_spaces && !ename->suffix) {
				gsub("_", " ", print_str);
			}
			break;
		}
		case STRUCT_VARIABLE: {
			const ExpressionName *ename = &o_variable->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			print_str += ename->name;
			if(po.hide_underscore_spaces && !ename->suffix) {
				gsub("_", " ", print_str);
			}
			break;
		}
		case STRUCT_FUNCTION: {
			ips_n.depth++;
			const ExpressionName *ename = &o_function->preferredDisplayName(po.abbreviate_names, po.use_unicode_signs, false, po.use_reference_names, po.can_display_unicode_string_function, po.can_display_unicode_string_arg);
			print_str += ename->name;
			if(po.hide_underscore_spaces && !ename->suffix) {
				gsub("_", " ", print_str);
			}
			print_str += "(";
			for(size_t i = 0; i < SIZE; i++) {
				if(i > 0) {
					print_str += po.comma();
					if(po.spacious) print_str += " ";
				}
				ips_n.wrap = CHILD(i).needsParenthesis(po, ips_n, *this, i + 1, true, true);
				print_str += CHILD(i).print(po, ips_n);
			}
			print_str += ")";
			break;
		}
		case STRUCT_UNDEFINED: {
			print_str = _("undefined");
			break;
		}
	}
	if(ips.wrap) {
		print_str.insert(0, "(");
		print_str += ")";
	}
	return print_str;
}

MathStructure &MathStructure::flattenVector(MathStructure &mstruct) const {
	if(!isVector()) {
		mstruct = *this;
		return mstruct;
	}
	MathStructure mstruct2;
	mstruct.clearVector();
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).isVector()) {
			CHILD(i).flattenVector(mstruct2);
			for(size_t i2 = 0; i2 < mstruct2.size(); i2++) {
				mstruct.addItem(mstruct2[i2]);
			}
		} else {
			mstruct.addItem(CHILD(i));
		}
	}
	return mstruct;
}
bool MathStructure::rankVector(bool ascending) {
	vector<int> ranked;
	vector<bool> ranked_equals_prev;	
	bool b;
	for(size_t index = 0; index < SIZE; index++) {
		b = false;
		for(size_t i = 0; i < ranked.size(); i++) {
			ComparisonResult cmp = CHILD(index).compare(CHILD(ranked[i]));
			if(COMPARISON_NOT_FULLY_KNOWN(cmp)) {
				CALCULATOR->error(true, _("Unsolvable comparison at component %s when trying to rank vector."), i2s(index).c_str(), NULL);
				return false;
			}
			if((ascending && cmp == COMPARISON_RESULT_GREATER) || cmp == COMPARISON_RESULT_EQUAL || (!ascending && cmp == COMPARISON_RESULT_LESS)) {
				if(cmp == COMPARISON_RESULT_EQUAL) {
					ranked.insert(ranked.begin() + i + 1, index);
					ranked_equals_prev.insert(ranked_equals_prev.begin() + i + 1, true);
				} else {
					ranked.insert(ranked.begin() + i, index);
					ranked_equals_prev.insert(ranked_equals_prev.begin() + i, false);
				}
				b = true;
				break;
			}
		}
		if(!b) {
			ranked.push_back(index);
			ranked_equals_prev.push_back(false);
		}
	}	
	int n_rep = 0;
	for(int i = (int) ranked.size() - 1; i >= 0; i--) {
		if(ranked_equals_prev[i]) {
			n_rep++;
		} else {
			if(n_rep) {
				MathStructure v(i + 1 + n_rep, 1);
				v += i + 1;
				v *= MathStructure(1, 2);
				for(; n_rep >= 0; n_rep--) {
					CHILD(ranked[i + n_rep]) = v;
				}
			} else {
				CHILD(ranked[i]).set(i + 1, 1);
			}
			n_rep = 0;
		}
	}
	return true;
}
bool MathStructure::sortVector(bool ascending) {
	vector<size_t> ranked_mstructs;
	bool b;
	for(size_t index = 0; index < SIZE; index++) {
		b = false;
		for(size_t i = 0; i < ranked_mstructs.size(); i++) {
			ComparisonResult cmp = CHILD(index).compare(*v_subs[ranked_mstructs[i]]);
			if(COMPARISON_MIGHT_BE_LESS_OR_GREATER(cmp)) {
				CALCULATOR->error(true, _("Unsolvable comparison at component %s when trying to sort vector."), i2s(index).c_str(), NULL);
				return false;
			}
			if((ascending && COMPARISON_IS_EQUAL_OR_GREATER(cmp)) || (!ascending && COMPARISON_IS_EQUAL_OR_LESS(cmp))) {
				ranked_mstructs.insert(ranked_mstructs.begin() + i, v_order[index]);
				b = true;
				break;
			}
		}
		if(!b) {
			ranked_mstructs.push_back(v_order[index]);
		}
	}	
	v_order = ranked_mstructs;
	return true;
}
MathStructure &MathStructure::getRange(int start, int end, MathStructure &mstruct) const {
	if(!isVector()) {
		if(start > 1) {
			mstruct.clear();
			return mstruct;
		} else {
			mstruct = *this;
			return mstruct;
		}
	}
	if(start < 1) start = 1;
	else if(start > (int) SIZE) {
		mstruct.clear();
		return mstruct;
	}
	if(end < (int) 1 || end > (int) SIZE) end = SIZE;
	else if(end < start) end = start;	
	mstruct.clearVector();
	for(; start <= end; start++) {
		mstruct.addItem(CHILD(start - 1));
	}
	return mstruct;
}

void MathStructure::resizeVector(size_t i, const MathStructure &mfill) {
	if(i > SIZE) {
		while(i > SIZE) {
			APPEND(mfill);
		}
	} else if(i > SIZE) {
		REDUCE(i)
	}
}

size_t MathStructure::rows() const {
	if(m_type != STRUCT_VECTOR || SIZE == 0 || (SIZE == 1 && (!CHILD(0).isVector() || CHILD(0).size() == 0))) return 0;
	return SIZE;
}
size_t MathStructure::columns() const {
	if(m_type != STRUCT_VECTOR || SIZE == 0 || !CHILD(0).isVector()) return 0;
	return CHILD(0).size();
}
const MathStructure *MathStructure::getElement(size_t row, size_t column) const {
	if(row == 0 || column == 0 || row > rows() || column > columns()) return NULL;
	if(CHILD(row - 1).size() < column) return NULL;
	return &CHILD(row - 1)[column - 1];
}
MathStructure *MathStructure::getElement(size_t row, size_t column) {
	if(row == 0 || column == 0 || row > rows() || column > columns()) return NULL;
	if(CHILD(row - 1).size() < column) return NULL;
	return &CHILD(row - 1)[column - 1];
}
MathStructure &MathStructure::getArea(size_t r1, size_t c1, size_t r2, size_t c2, MathStructure &mstruct) const {
	size_t r = rows();
	size_t c = columns();
	if(r1 < 1) r1 = 1;
	else if(r1 > r) r1 = r;
	if(c1 < 1) c1 = 1;
	else if(c1 > c) c1 = c;
	if(r2 < 1 || r2 > r) r2 = r;
	else if(r2 < r1) r2 = r1;
	if(c2 < 1 || c2 > c) c2 = c;
	else if(c2 < c1) c2 = c1;
	mstruct.clearMatrix(); mstruct.resizeMatrix(r2 - r1 + 1, c2 - c1 + 1, m_undefined);
	for(size_t index_r = r1; index_r <= r2; index_r++) {
		for(size_t index_c = c1; index_c <= c2; index_c++) {
			mstruct[index_r - r1][index_c - c1] = CHILD(index_r - 1)[index_c - 1];
		}			
	}
	return mstruct;
}
MathStructure &MathStructure::rowToVector(size_t r, MathStructure &mstruct) const {
	if(r > rows()) {
		mstruct = m_undefined;
		return mstruct;
	}
	if(r < 1) r = 1;
	mstruct = CHILD(r - 1);
	return mstruct;
}
MathStructure &MathStructure::columnToVector(size_t c, MathStructure &mstruct) const {
	if(c > columns()) {
		mstruct = m_undefined;
		return mstruct;
	}
	if(c < 1) c = 1;
	mstruct.clearVector();
	for(size_t i = 0; i < SIZE; i++) {
		mstruct.addItem(CHILD(i)[c - 1]);
	}
	return mstruct;
}
MathStructure &MathStructure::matrixToVector(MathStructure &mstruct) const {
	if(!isVector()) {
		mstruct = *this;
		return mstruct;
	}
	mstruct.clearVector();
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).isVector()) {
			for(size_t i2 = 0; i2 < CHILD(i).size(); i2++) {
				mstruct.addItem(CHILD(i)[i2]);
			}
		} else {
			mstruct.addItem(CHILD(i));
		}
	}
	return mstruct;
}
void MathStructure::setElement(const MathStructure &mstruct, size_t row, size_t column) {
	if(row > rows() || column > columns() || row < 1 || column < 1) return;
	CHILD(row - 1)[column - 1] = mstruct;
	CHILD(row - 1).childUpdated(column);
	CHILD_UPDATED(row - 1);
}
void MathStructure::addRows(size_t r, const MathStructure &mfill) {
	if(r == 0) return;
	size_t cols = columns();
	MathStructure mstruct; mstruct.clearVector();
	mstruct.resizeVector(cols, mfill);
	for(size_t i = 0; i < r; i++) {
		APPEND(mstruct);
	}
}
void MathStructure::addColumns(size_t c, const MathStructure &mfill) {
	if(c == 0) return;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).isVector()) {
			for(size_t i2 = 0; i2 < c; i2++) {
				CHILD(i).addItem(mfill);
			}
		}
	}
	CHILDREN_UPDATED;
}
void MathStructure::addRow(const MathStructure &mfill) {
	addRows(1, mfill);
}
void MathStructure::addColumn(const MathStructure &mfill) {
	addColumns(1, mfill);
}
void MathStructure::resizeMatrix(size_t r, size_t c, const MathStructure &mfill) {
	if(r > SIZE) {
		addRows(r - SIZE, mfill);
	} else if(r != SIZE) {
		REDUCE(r);
	}
	size_t cols = columns();
	if(c > cols) {
		addColumns(c - cols, mfill);
	} else if(c != cols) {
		for(unsigned i = 0; i < SIZE; i++) {
			CHILD(i).resizeVector(c, mfill);
		}
	}
}
bool MathStructure::matrixIsSymmetric() const {
	return rows() == columns();
}
MathStructure &MathStructure::determinant(MathStructure &mstruct, const EvaluationOptions &eo) const {
	if(!matrixIsSymmetric()) {
		CALCULATOR->error(true, _("The determinant can only be calculated for symmetric matrices."), NULL);
		mstruct = m_undefined;
		return mstruct;
	}
	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	eo2.sync_units = false;
	if(b_approx) mstruct.setApproximate();
	mstruct.setPrecision(i_precision);
	if(SIZE == 1) {
		if(CHILD(0).size() >= 1) {	
			mstruct = CHILD(0)[0];
		}
		mstruct.eval(eo2);
	} else if(SIZE == 2) {
		mstruct = CHILD(0)[0];
		mstruct.multiply(CHILD(1)[1], true);
		MathStructure *mtmp = new MathStructure(CHILD(1)[0]);
		mtmp->multiply(CHILD(0)[1], true);
		mstruct.subtract_nocopy(mtmp, true);
		mstruct.eval(eo2);
	} else {
		MathStructure mtrx;
		mtrx.clearMatrix();
		mtrx.resizeMatrix(SIZE - 1, CHILD(0).size() - 1, m_undefined);
		for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
			for(size_t index_r2 = 1; index_r2 < SIZE; index_r2++) {
				for(size_t index_c2 = 0; index_c2 < CHILD(index_r2).size(); index_c2++) {
					if(index_c2 > index_c) {
						mtrx.setElement(CHILD(index_r2)[index_c2], index_r2, index_c2);
					} else if(index_c2 < index_c) {
						mtrx.setElement(CHILD(index_r2)[index_c2], index_r2, index_c2 + 1);
					}
				}
			}
			
			MathStructure *mdet = new MathStructure();
			mtrx.determinant(*mdet, eo);
			
			if(index_c % 2 == 1) {
				mdet->negate();
			}
			
			mdet->multiply(CHILD(0)[index_c], true);

			mstruct.add_nocopy(mdet, true);
			mstruct.eval(eo2);

		}
	}
	return mstruct;
}
MathStructure &MathStructure::permanent(MathStructure &mstruct, const EvaluationOptions &eo) const {
	if(!matrixIsSymmetric()) {
		CALCULATOR->error(true, _("The permanent can only be calculated for symmetric matrices."), NULL);
		mstruct = m_undefined;
		return mstruct;
	}
	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	eo2.sync_units = false;
	if(b_approx) mstruct.setApproximate();
	mstruct.setPrecision(i_precision);
	if(SIZE == 1) {
		if(CHILD(0).size() >= 1) {	
			mstruct == CHILD(0)[0];
		}
		mstruct.eval(eo2);
	} else if(SIZE == 2) {
		mstruct = CHILD(0)[0];
		mstruct *= CHILD(1)[1];
		MathStructure mtmp = CHILD(1)[0];
		mtmp *= CHILD(0)[1];
		mstruct += mtmp;
		mstruct.eval(eo2);
	} else {
		MathStructure mtrx;
		mtrx.clearMatrix();
		mtrx.resizeMatrix(SIZE - 1, CHILD(0).size() - 1, m_undefined);
		for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
			for(size_t index_r2 = 1; index_r2 < SIZE; index_r2++) {
				for(size_t index_c2 = 0; index_c2 < CHILD(index_r2).size(); index_c2++) {
					if(index_c2 > index_c) {
						mtrx.setElement(CHILD(index_r2)[index_c2], index_r2, index_c2);
					} else if(index_c2 < index_c) {
						mtrx.setElement(CHILD(index_r2)[index_c2], index_r2, index_c2 + 1);
					}
				}
			}
			MathStructure mdet;
			mtrx.permanent(mdet, eo);
			
			mdet *= CHILD(0)[index_c];
			mstruct += mdet;
		}
		mstruct.eval(eo2);
	}
	return mstruct;
}
void MathStructure::setToIdentityMatrix(size_t n) {
	clearMatrix();
	resizeMatrix(n, n, m_zero);
	for(size_t i = 0; i < n; i++) {
		CHILD(i)[i] = m_one;
	}
}
MathStructure &MathStructure::getIdentityMatrix(MathStructure &mstruct) const {
	mstruct.setToIdentityMatrix(columns());
	return mstruct;
}
bool MathStructure::invertMatrix(const EvaluationOptions &eo) {
	if(!matrixIsSymmetric()) return false;
	MathStructure mstruct;
	determinant(mstruct, eo);
	mstruct.raise(m_minus_one);
	adjointMatrix(eo);
	multiply(mstruct);
	eval(eo);
	return true;
}
bool MathStructure::adjointMatrix(const EvaluationOptions &eo) {
	if(!matrixIsSymmetric()) return false;
	MathStructure msave(*this);
	for(size_t index_r = 0; index_r < SIZE; index_r++) {
		for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
			msave.cofactor(index_r + 1, index_c + 1, CHILD(index_r)[index_c], eo);
		}
	}
	transposeMatrix();
	eval(eo);
	return true;
}
bool MathStructure::transposeMatrix() {
	MathStructure msave(*this);
	resizeMatrix(CHILD(0).size(), SIZE, m_undefined);
	for(size_t index_r = 0; index_r < SIZE; index_r++) {
		for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
			CHILD(index_r)[index_c] = msave[index_c][index_r];
		}
	}
	return true;
}	
MathStructure &MathStructure::cofactor(size_t r, size_t c, MathStructure &mstruct, const EvaluationOptions &eo) const {
	if(r < 1) r = 1;
	if(c < 1) c = 1;
	if(r > SIZE || c > CHILD(0).size()) {
		mstruct = m_undefined;
		return mstruct;
	}
	r--; c--;
	mstruct.clearMatrix(); mstruct.resizeMatrix(SIZE - 1, CHILD(0).size() - 1, m_undefined);
	for(size_t index_r = 0; index_r < SIZE; index_r++) {
		if(index_r != r) {
			for(size_t index_c = 0; index_c < CHILD(0).size(); index_c++) {
				if(index_c > c) {
					if(index_r > r) {
						mstruct[index_r - 1][index_c - 1] = CHILD(index_r)[index_c];
					} else {
						mstruct[index_r][index_c - 1] = CHILD(index_r)[index_c];
					}
				} else if(index_c < c) {
					if(index_r > r) {
						mstruct[index_r - 1][index_c] = CHILD(index_r)[index_c];
					} else {
						mstruct[index_r][index_c] = CHILD(index_r)[index_c];
					}
				}
			}
		}
	}	
	MathStructure mstruct2;
	mstruct = mstruct.determinant(mstruct2, eo);
	if((r + c) % 2 == 1) {
		mstruct.negate();
	}
	mstruct.eval(eo);
	return mstruct;
}

void gatherInformation(const MathStructure &mstruct, vector<Unit*> &base_units, vector<AliasUnit*> &alias_units) {
	switch(mstruct.type()) {
		case STRUCT_UNIT: {
			switch(mstruct.unit()->subtype()) {
				case SUBTYPE_BASE_UNIT: {
					for(size_t i = 0; i < base_units.size(); i++) {
						if(base_units[i] == mstruct.unit()) {
							return;
						}
					}
					base_units.push_back(mstruct.unit());
					break;
				}
				case SUBTYPE_ALIAS_UNIT: {
					for(size_t i = 0; i < alias_units.size(); i++) {
						if(alias_units[i] == mstruct.unit()) {
							return;
						}
					}
					alias_units.push_back((AliasUnit*) (mstruct.unit()));
					break;
				}
				case SUBTYPE_COMPOSITE_UNIT: {
					gatherInformation(((CompositeUnit*) (mstruct.unit()))->generateMathStructure(), base_units, alias_units);
					break;
				}				
			}
			break;
		}
		default: {
			for(size_t i = 0; i < mstruct.size(); i++) {
				gatherInformation(mstruct[i], base_units, alias_units);
			}
			break;
		}
	}

}

int MathStructure::isUnitCompatible(const MathStructure &mstruct) {
	int b1 = mstruct.containsRepresentativeOfType(STRUCT_UNIT, true, true);
	int b2 = containsRepresentativeOfType(STRUCT_UNIT, true, true);
	if(b1 < 0 || b2 < 0) return -1;
	if(b1 != b2) return false;
	if(!b1) return true;
	if(mstruct.isMultiplication()) {
		size_t i2 = 0;
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).containsType(STRUCT_UNIT)) {
				bool b = false;
				for(; i2 < mstruct.size(); i2++) {
					if(mstruct[i2].containsType(STRUCT_UNIT)) {
						if(!CHILD(i).isUnitCompatible(mstruct[i2])) {
							return false;
						}
						i2++;
						b = true;
						break;
					}
				}
				if(!b) return false;
			}
		}
		for(; i2 < mstruct.size(); i2++) {
			if(mstruct[i2].containsType(STRUCT_UNIT)) {
				return false;
			}	
		}
	}
	if(isUnit()) {
		return equals(mstruct);
	} else if(isPower()) {
		return equals(mstruct);
	}
	return true;
}

bool MathStructure::syncUnits(bool sync_complex_relations) {
	vector<Unit*> base_units;
	vector<AliasUnit*> alias_units;
	vector<CompositeUnit*> composite_units;	
	gatherInformation(*this, base_units, alias_units);
	CompositeUnit *cu;
	bool b = false, do_erase = false;
	for(size_t i = 0; i < alias_units.size(); ) {
		do_erase = false;
		if(alias_units[i]->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			b = false;
			cu = (CompositeUnit*) alias_units[i]->baseUnit();
			for(size_t i2 = 0; i2 < base_units.size(); i2++) {
				if(cu->containsRelativeTo(base_units[i2])) {
					for(size_t i3 = 0; i3 < composite_units.size(); i3++) {
						if(composite_units[i3] == cu) {
							b = true;
							break;
						}
					}
					if(!b) composite_units.push_back(cu);					
					do_erase = true;
					break;
				}
			}
			for(size_t i2 = 0; !do_erase && i2 < alias_units.size(); i2++) {
				if(i != i2 && cu->containsRelativeTo(alias_units[i2])) {
					for(size_t i3 = 0; i3 < composite_units.size(); i3++) {
						if(composite_units[i3] == cu) {
							b = true;
							break;
						}
					}
					if(!b) composite_units.push_back(cu);
					do_erase = true;
					break;
				}
			}					
		}
		if(do_erase) {
			alias_units.erase(alias_units.begin() + i);
			for(int i2 = 0; i2 < (int) cu->units.size(); i2++) {
				b = false;
				switch(cu->units[i2]->firstBaseUnit()->subtype()) {
					case SUBTYPE_BASE_UNIT: {
						for(size_t i3 = 0; i3 < base_units.size(); i3++) {
							if(base_units[i3] == cu->units[i2]->firstBaseUnit()) {
								b = true;
								break;
							}
						}
						if(!b) base_units.push_back((Unit*) cu->units[i2]->firstBaseUnit());
						break;
					}
					case SUBTYPE_ALIAS_UNIT: {
						for(size_t i3 = 0; i3 < alias_units.size(); i3++) {
							if(alias_units[i3] == cu->units[i2]->firstBaseUnit()) {
								b = true;
								break;
							}
						}
						if(!b) alias_units.push_back((AliasUnit*) cu->units[i2]->firstBaseUnit());				
						break;
					}
					case SUBTYPE_COMPOSITE_UNIT: {
						gatherInformation(((CompositeUnit*) cu->units[i2]->firstBaseUnit())->generateMathStructure(), base_units, alias_units);
						break;
					}
				}
			}
			i = 0;
		} else {
			i++;
		}
	}
	for(size_t i = 0; i < alias_units.size();) {
		do_erase = false;
		for(size_t i2 = 0; i2 < alias_units.size(); i2++) {
			if(i != i2 && alias_units[i]->baseUnit() == alias_units[i2]->baseUnit()) { 
				if(alias_units[i2]->isParentOf(alias_units[i])) {
					do_erase = true;
					break;
				} else if(!alias_units[i]->isParentOf(alias_units[i2])) {
					b = false;
					for(size_t i3 = 0; i3 < base_units.size(); i3++) {
						if(base_units[i3] == alias_units[i2]->firstBaseUnit()) {
							b = true;
							break;
						}
					}
					if(!b) base_units.push_back((Unit*) alias_units[i]->baseUnit());
					do_erase = true;
					break;
				}
			}
		} 
		if(do_erase) {
			alias_units.erase(alias_units.begin() + i);
			i = 0;
		} else {
			i++;
		}
	}	
	for(size_t i = 0; i < alias_units.size();) {
		do_erase = false;
		if(alias_units[i]->baseUnit()->subtype() == SUBTYPE_BASE_UNIT) {
			for(size_t i2 = 0; i2 < base_units.size(); i2++) {
				if(alias_units[i]->baseUnit() == base_units[i2]) {
					do_erase = true;
					break;
				}
			}
		} 
		if(do_erase) {
			alias_units.erase(alias_units.begin() + i);
			i = 0;
		} else {
			i++;
		}
	}
	b = false;
	for(size_t i = 0; i < composite_units.size(); i++) {	
		if(convert(composite_units[i], sync_complex_relations)) b = true;
	}	
	if(dissolveAllCompositeUnits()) b = true;
	for(size_t i = 0; i < base_units.size(); i++) {	
		if(convert(base_units[i], sync_complex_relations)) b = true;
	}
	for(size_t i = 0; i < alias_units.size(); i++) {	
		if(convert(alias_units[i], sync_complex_relations)) b = true;
	}
	return b;
}
bool MathStructure::testDissolveCompositeUnit(Unit *u) {
	if(m_type == STRUCT_UNIT) {
		if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) o_unit)->containsRelativeTo(u)) {
				set(((CompositeUnit*) o_unit)->generateMathStructure());
				return true;
			}
		} else if(o_unit->subtype() == SUBTYPE_ALIAS_UNIT && o_unit->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) (o_unit->baseUnit()))->containsRelativeTo(u)) {
				convert(o_unit->baseUnit());
				convert(u);
				return true;
			}		
		}
	}
	return false; 
}
bool MathStructure::testCompositeUnit(Unit *u) {
	if(m_type == STRUCT_UNIT) {
		if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) o_unit)->containsRelativeTo(u)) {
				return true;
			}
		} else if(o_unit->subtype() == SUBTYPE_ALIAS_UNIT && o_unit->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			if(((CompositeUnit*) (o_unit->baseUnit()))->containsRelativeTo(u)) {
				return true;
			}		
		}
	}
	return false; 
}
bool MathStructure::dissolveAllCompositeUnits() {
	switch(m_type) {
		case STRUCT_UNIT: {
			if(o_unit->subtype() == SUBTYPE_COMPOSITE_UNIT) {
				set(((CompositeUnit*) o_unit)->generateMathStructure());
				return true;
			}
			break;
		}
		default: {
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).dissolveAllCompositeUnits()) {
					CHILD_UPDATED(i);
					b = true;
				}
			}
			return b;
		}		
	}		
	return false;
}
bool MathStructure::convert(Unit *u, bool convert_complex_relations) {
	bool b = false;	
	if(m_type == STRUCT_UNIT && o_unit == u) return false;
	if(u->subtype() == SUBTYPE_COMPOSITE_UNIT && !(m_type == STRUCT_UNIT && o_unit->baseUnit() == u)) {
		return convert(((CompositeUnit*) u)->generateMathStructure());
	}
	if(m_type == STRUCT_UNIT) {
		if(!convert_complex_relations && u->hasComplexRelationTo(o_unit)) return false;
		if(testDissolveCompositeUnit(u)) {
			convert(u, convert_complex_relations);
			return true;
		}
		MathStructure *exp = new MathStructure(1, 1);
		MathStructure *mstruct = new MathStructure(1, 1);
		u->convert(o_unit, *mstruct, *exp, &b);
		if(b) {
			o_unit = u;
			if(!exp->isOne()) {
				raise_nocopy(exp);
			} else {
				exp->unref();
			}
			if(!mstruct->isOne()) {
				multiply_nocopy(mstruct);
			} else {
				mstruct->unref();
			}
		} else {
			exp->unref();
			mstruct->unref();
		}
		return b;
	} else {
		if(convert_complex_relations) {
			if(m_type == STRUCT_MULTIPLICATION) {
				int b_c = -1;
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).isUnit_exp()) {
						if((CHILD(i).isUnit() && u->hasComplexRelationTo(CHILD(i).unit())) || (CHILD(i).isPower() && u->hasComplexRelationTo(CHILD(i)[0].unit()))) {
							if(b_c >= 0) {
								b_c = -1;
								break;
							}
							b_c = i;
						}
					}
				}
				if(b_c >= 0) {
					MathStructure mstruct(1, 1);
					if(SIZE == 2) {
						if(b_c == 0) mstruct = CHILD(1);
						else mstruct = CHILD(0);
					} else if(SIZE > 2) {
						mstruct = *this;
						mstruct.delChild(b_c + 1);
					}
					MathStructure exp(1, 1);
					Unit *u2;
					if(CHILD(b_c).isPower()) {
						if(CHILD(b_c)[0].testDissolveCompositeUnit(u)) {
							convert(u, convert_complex_relations);
							return true;
						}	
						exp = CHILD(b_c)[1];
						u2 = CHILD(b_c)[0].unit();
					} else {
						if(CHILD(b_c).testDissolveCompositeUnit(u)) {
							convert(u, convert_complex_relations);
							return true;
						}
						u2 = CHILD(b_c).unit();
					}
					u->convert(u2, mstruct, exp, &b);
					if(b) {
						set(u);
						if(!exp.isOne()) {
							raise(exp);
						}
						if(!mstruct.isOne()) {
							multiply(mstruct);
						}
					}
					return b;
				}
			} else if(m_type == STRUCT_POWER) {
				if(CHILD(0).isUnit() && u->hasComplexRelationTo(CHILD(0).unit())) {
					if(CHILD(0).testDissolveCompositeUnit(u)) {
						convert(u, convert_complex_relations);
						return true;
					}	
					MathStructure exp(CHILD(1));
					MathStructure mstruct(1, 1);
					u->convert(CHILD(0).unit(), mstruct, exp, &b);
					if(b) {
						set(u);
						if(!exp.isOne()) {
							raise(exp);
						}
						if(!mstruct.isOne()) {
							multiply(mstruct);
						}
					}
					return b;
				}
			}
			if(m_type == STRUCT_MULTIPLICATION || m_type == STRUCT_POWER) {
				for(size_t i = 0; i < SIZE; i++) {
					if(CHILD(i).convert(u, false)) {
						CHILD_UPDATED(i);
						b = true;
					}
				}
				return b;
			}
		}
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).convert(u, convert_complex_relations)) {
				CHILD_UPDATED(i);
				b = true;
			}
		}
		return b;		
	}	
	return b;
}
bool MathStructure::convert(const MathStructure unit_mstruct, bool convert_complex_relations) {
	bool b = false;
	if(unit_mstruct.type() == STRUCT_UNIT) {
		if(convert(unit_mstruct.unit(), convert_complex_relations)) b = true;
	} else {
		for(size_t i = 0; i < unit_mstruct.size(); i++) {
			if(convert(unit_mstruct[i], convert_complex_relations)) b = true;
		}
	}	
	return b;
}

int MathStructure::contains(const MathStructure &mstruct, bool structural_only, bool check_variables, bool check_functions) const {
	if(equals(mstruct)) return 1;
	if(structural_only) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).contains(mstruct, false, check_variables, check_functions)) return 1;
		}
	} else {
		int ret = 0;
		if(m_type != STRUCT_FUNCTION) {
			for(size_t i = 0; i < SIZE; i++) {
				int retval = CHILD(i).contains(mstruct, structural_only, check_variables, check_functions);
				if(retval == 1) return 1;
				else if(retval < 0) ret = retval;
			}
		}
		if(m_type == STRUCT_VARIABLE && check_variables && o_variable->isKnown()) {
			if(((KnownVariable*) o_variable)->get().contains(mstruct, structural_only, check_variables, check_functions) == 0) return 0;
		} else if(m_type == STRUCT_FUNCTION && check_functions) {
			if(function_value) {
				if(function_value->contains(mstruct, structural_only, check_variables, check_functions) == 0) return 0;
			}
		}
		if((m_type == STRUCT_VARIABLE && o_variable->isKnown()) || m_type == STRUCT_FUNCTION) {
			if(representsNumber(false)) {
				if(mstruct.isNumber()) return -1;
				return 0;
			} else {
				return -1;
			}
		}
		return ret;
	}
	return 0;
}
int MathStructure::containsRepresentativeOf(const MathStructure &mstruct, bool check_variables, bool check_functions) const {
	if(equals(mstruct)) return 1;
	int ret = 0;
	if(m_type != STRUCT_FUNCTION) {
		for(size_t i = 0; i < SIZE; i++) {
			int retval = CHILD(i).containsRepresentativeOf(mstruct, check_variables, check_functions);
			if(retval == 1) return 1;
			else if(retval < 0) ret = retval;
		}
	}
	if(m_type == STRUCT_VARIABLE && check_variables && o_variable->isKnown()) {
		if(((KnownVariable*) o_variable)->get().containsRepresentativeOf(mstruct, check_variables, check_functions) == 0) return 0;
	} else if(m_type == STRUCT_FUNCTION && check_functions) {
		if(function_value) {
			if(function_value->containsRepresentativeOf(mstruct, check_variables, check_functions) == 0) return 0;
		}
	}
	if(m_type == STRUCT_SYMBOLIC || m_type == STRUCT_VARIABLE || m_type == STRUCT_FUNCTION) {
		if(representsNumber(false)) {
			if(mstruct.isNumber()) return -1;
			return 0;
		} else {
			return -1;
		}
	}
	return ret;
}

int MathStructure::containsType(int mtype, bool structural_only, bool check_variables, bool check_functions) const {
	if(m_type == mtype) return 1;
	if(structural_only) {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).containsType(mtype, true, check_variables, check_functions)) return 1;
		}
		return 0;
	} else {
		int ret = 0;
		if(m_type != STRUCT_FUNCTION) {
			for(size_t i = 0; i < SIZE; i++) {
				int retval = CHILD(i).containsType(mtype, false, check_variables, check_functions);
				if(retval == 1) return 1;
				else if(retval < 0) ret = retval;
			}
		}
		if(check_variables && m_type == STRUCT_VARIABLE && o_variable->isKnown()) {
			if(((KnownVariable*) o_variable)->get().containsType(mtype, false, check_variables, check_functions) == 0) return 0;
		} else if(check_functions && m_type == STRUCT_FUNCTION) {
			if(function_value) {
				if(function_value->containsType(mtype, false, check_variables, check_functions) == 0) return 0;
			}
		}
		if((m_type == STRUCT_VARIABLE && o_variable->isKnown()) || m_type == STRUCT_FUNCTION) {
			if(representsNumber(false)) {
				return mtype == STRUCT_NUMBER;
			} else {
				return -1;
			}
		}
		return ret;
	}
}
int MathStructure::containsRepresentativeOfType(int mtype, bool check_variables, bool check_functions) const {
	if(m_type == mtype) return 1;
	int ret = 0;
	if(m_type != STRUCT_FUNCTION) {
		for(size_t i = 0; i < SIZE; i++) {
			int retval = CHILD(i).containsRepresentativeOfType(mtype, check_variables, check_functions);
			if(retval == 1) return 1;
			else if(retval < 0) ret = retval;
		}
	}
	if(check_variables && m_type == STRUCT_VARIABLE && o_variable->isKnown()) {
		if(((KnownVariable*) o_variable)->get().containsRepresentativeOfType(mtype, check_variables, check_functions) == 0) return 0;
	} else if(check_functions && m_type == STRUCT_FUNCTION) {
		if(function_value) {
			if(function_value->containsRepresentativeOfType(mtype, check_variables, check_functions) == 0) return 0;
		}
	}
	if(m_type == STRUCT_SYMBOLIC || m_type == STRUCT_VARIABLE || m_type == STRUCT_FUNCTION) {
		if(representsNumber(false)) {
			return mtype == STRUCT_NUMBER;
		} else {
			return -1;
		}
	}
	return ret;
}
bool MathStructure::containsUnknowns() const {
	if(m_type == STRUCT_SYMBOLIC || (m_type == STRUCT_VARIABLE && !o_variable->isKnown())) return true;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).containsUnknowns()) return true;
	}
	return false;
}
bool MathStructure::containsDivision() const {
	if(m_type == STRUCT_DIVISION || m_type == STRUCT_INVERSE || (m_type == STRUCT_POWER && CHILD(1).hasNegativeSign())) return true;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).containsDivision()) return true;
	}
	return false;
}
void MathStructure::findAllUnknowns(MathStructure &unknowns_vector) {
	if(!unknowns_vector.isVector()) unknowns_vector.clearVector();
	switch(m_type) {
		case STRUCT_VARIABLE: {
			if(o_variable->isKnown()) {
				break;
			}
		}
		case STRUCT_SYMBOLIC: {
			bool b = false;
			for(size_t i = 0; i < unknowns_vector.size(); i++) {
				if(equals(unknowns_vector[i])) {
					b = true;
					break;
				}
			}
			if(!b) unknowns_vector.addItem(*this);
			break;
		}
		default: {
			for(size_t i = 0; i < SIZE; i++) {
				CHILD(i).findAllUnknowns(unknowns_vector);
			}
		}
	}
}
bool MathStructure::replace(const MathStructure &mfrom, const MathStructure &mto) {
	if(equals(mfrom)) {
		set(mto);
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).replace(mfrom, mto)) {
			b = true;
			CHILD_UPDATED(i);
		}
	}
	return b;
}
bool MathStructure::replace(const MathStructure &mfrom1, const MathStructure &mto1, const MathStructure &mfrom2, const MathStructure &mto2) {
	if(equals(mfrom1)) {
		set(mto1);
		return true;
	}
	if(equals(mfrom2)) {
		set(mto2);
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < SIZE; i++) {
		if(CHILD(i).replace(mfrom1, mto1, mfrom2, mto2)) {
			b = true;
			CHILD_UPDATED(i);
		}
	}
	return b;
}
bool MathStructure::removeType(int mtype) {
	if(m_type == mtype || (m_type == STRUCT_POWER && CHILD(0).type() == mtype)) {
		set(1);
		return true;
	}
	bool b = false;
	if(m_type == STRUCT_MULTIPLICATION) {
		for(int i = 0; i < (int) SIZE; i++) {
			if(CHILD(i).removeType(mtype)) {
				if(CHILD(i).isOne()) {
					ERASE(i);
					i--;
				} else {
					CHILD_UPDATED(i);
				}
				b = true;
			}
		}
		if(SIZE == 0) {
			set(1);
		}
	} else {
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).removeType(mtype)) {
				b = true;
				CHILD_UPDATED(i);
			}
		}
	}
	return b;
}

MathStructure MathStructure::generateVector(MathStructure x_mstruct, const MathStructure &min, const MathStructure &max, int steps, MathStructure *x_vector, const EvaluationOptions &eo) const {
	if(steps < 1) {
		steps = 1;
	}
	MathStructure x_value(min);
	MathStructure y_value;
	MathStructure y_vector;
	y_vector.clearVector();
	MathStructure step(max);
	step -= min;
	step /= steps;
	step.eval(eo);
	if(!step.isNumber() || step.number().isNegative()) {
		return y_vector;
	}
	for(int i = 0; i <= steps; i++) {
		if(x_vector) {
			x_vector->addComponent(x_value);
		}
		y_value = *this;
		y_value.replace(x_mstruct, x_value);
		y_value.eval(eo);
		y_vector.addComponent(y_value);
		x_value += step;
		x_value.eval(eo);
	}
	return y_vector;
}
MathStructure MathStructure::generateVector(MathStructure x_mstruct, const MathStructure &min, const MathStructure &max, const MathStructure &step, MathStructure *x_vector, const EvaluationOptions &eo) const {
	MathStructure x_value(min);
	MathStructure y_value;
	MathStructure y_vector;
	y_vector.clearVector();
	if(min != max) {
		MathStructure mtest(max);
		mtest -= min;
		mtest /= step;
		mtest.eval(eo);
		if(!mtest.isNumber() || mtest.number().isNegative()) {
			return y_vector;
		}
	}
	ComparisonResult cr = max.compare(x_value);
	while(COMPARISON_IS_EQUAL_OR_LESS(cr)) {
		if(x_vector) {
			x_vector->addComponent(x_value);
		}
		y_value = *this;
		y_value.replace(x_mstruct, x_value);
		y_value.eval(eo);
		y_vector.addComponent(y_value);
		x_value += step;
		x_value.eval(eo);
		if(cr == COMPARISON_RESULT_EQUAL) break;
		cr = max.compare(x_value);
	}
	return y_vector;
}
MathStructure MathStructure::generateVector(MathStructure x_mstruct, const MathStructure &x_vector, const EvaluationOptions &eo) const {
	MathStructure y_value;
	MathStructure y_vector;
	y_vector.clearVector();
	for(size_t i = 1; i <= x_vector.components(); i++) {
		y_value = *this;
		y_value.replace(x_mstruct, x_vector.getComponent(i));
		y_value.eval(eo);
		y_vector.addComponent(y_value);
	}
	return y_vector;
}

bool MathStructure::differentiate(const MathStructure &x_var, const EvaluationOptions &eo) {
	if(equals(x_var)) {
		set(m_one);
		return true;
	}
	switch(m_type) {
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).differentiate(x_var, eo)) {
					CHILD_UPDATED(i);
				}
			}
			break;
		}
		case STRUCT_ALTERNATIVES: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).differentiate(x_var, eo)) {
					CHILD_UPDATED(i);
				}
			}
			break;
		}
		case STRUCT_BITWISE_AND: {}
		case STRUCT_BITWISE_OR: {}
		case STRUCT_BITWISE_XOR: {}
		case STRUCT_LOGICAL_AND: {}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_XOR: {}
		case STRUCT_COMPARISON: {}
		case STRUCT_UNIT: {}
		case STRUCT_NUMBER: {
			clear(true);
			break;
		}
		case STRUCT_POWER: {
			if(SIZE < 1) {
				clear(true);
				break;
			} else if(SIZE < 2) {
				setToChild(1, true);
				return differentiate(x_var, eo);
			}
			bool x_in_base = CHILD(0).containsRepresentativeOf(x_var, true, true) != 0;
			bool x_in_exp = CHILD(1).containsRepresentativeOf(x_var, true, true) != 0;
			if(x_in_base && !x_in_exp) {
				MathStructure exp_mstruct(CHILD(1));
				MathStructure base_mstruct(CHILD(0));
				CHILD(1) += m_minus_one;
				multiply(exp_mstruct);
				base_mstruct.differentiate(x_var, eo);
				multiply(base_mstruct);
			} else if(!x_in_base && x_in_exp) {
				MathStructure exp_mstruct(CHILD(1));
				MathStructure mstruct(CALCULATOR->f_ln, &CHILD(0), NULL);
				multiply(mstruct);
				exp_mstruct.differentiate(x_var, eo);
				multiply(exp_mstruct);
			} else if(x_in_base && x_in_exp) {
				MathStructure exp_mstruct(CHILD(1));
				MathStructure base_mstruct(CHILD(0));
				exp_mstruct.differentiate(x_var, eo);
				base_mstruct.differentiate(x_var, eo);
				base_mstruct /= CHILD(0);
				base_mstruct *= CHILD(1);
				MathStructure mstruct(CALCULATOR->f_ln, &CHILD(0), NULL);
				mstruct *= exp_mstruct;
				mstruct += base_mstruct;
				multiply(mstruct);
			} else {
				clear(true);
			}
			break;
		}
		case STRUCT_FUNCTION: {
			if(o_function == CALCULATOR->f_ln && SIZE == 1) {
				MathStructure mstruct(CHILD(0));
				set(mstruct, true);
				inverse();
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
			} else if(o_function == CALCULATOR->f_sin && SIZE == 1) {
				o_function = CALCULATOR->f_cos;
				MathStructure mstruct(CHILD(0));
				mstruct.differentiate(x_var, eo);
				multiply(mstruct);
				if(CALCULATOR->getRadUnit()) divide(CALCULATOR->getRadUnit());
			} else if(o_function == CALCULATOR->f_cos && SIZE == 1) {
				o_function = CALCULATOR->f_sin;
				MathStructure mstruct(CHILD(0));
				negate();
				mstruct.differentiate(x_var, eo);
				multiply(mstruct, true);
				if(CALCULATOR->getRadUnit()) divide(CALCULATOR->getRadUnit());
			} else if(o_function == CALCULATOR->f_integrate && SIZE == 2 && CHILD(1) == x_var) {
				setToChild(1, true);
			} else if(o_function == CALCULATOR->f_diff && SIZE == 3 && CHILD(1) == x_var) {
				CHILD(2) += m_one;
			} else if(o_function == CALCULATOR->f_diff && SIZE == 2 && CHILD(1) == x_var) {
				APPEND(MathStructure(2, 1));
			} else if(o_function == CALCULATOR->f_diff && SIZE == 1 && x_var == (Variable*) CALCULATOR->v_x) {
				APPEND(x_var);
				APPEND(MathStructure(2, 1));
			} else {
				if(!eo.calculate_functions || !calculateFunctions(eo)) {
					MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
					set(mstruct);
					return false;
				} else {
					EvaluationOptions eo2 = eo;
					eo2.calculate_functions = false;
					return differentiate(x_var, eo2);
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			MathStructure mstruct;
			int idiv = -1;
			if(SIZE > 2) {
				mstruct.setType(STRUCT_MULTIPLICATION);
				for(size_t i = 0; i < SIZE; i++) {
					if(idiv < 0 && CHILD(i).isPower() && CHILD(i)[1].isNumber() && CHILD(i)[1].number().isNegative()) {
						idiv = (int) i;
					}
					if((idiv >= 0 && (size_t) idiv != i) || i < SIZE - 1) {
						mstruct.addChild(CHILD(i));
					}
				}
				if(idiv < 0) {
					MathStructure mstruct2(LAST);
					MathStructure mstruct3(mstruct);
					mstruct.differentiate(x_var, eo);
					mstruct2.differentiate(x_var, eo);			
					mstruct *= LAST;
					mstruct2 *= mstruct3;
					set(mstruct, true);
					add(mstruct2);
				}
			} else {
				if(CHILD(1).isPower() && CHILD(1)[1].isNumber() && CHILD(1)[1].number().isNegative()) {
					idiv = 1;
					mstruct = CHILD(0);
				} else if(CHILD(0).isPower() && CHILD(0)[1].isNumber() && CHILD(0)[1].number().isNegative()) {
					idiv = 0;
					mstruct = CHILD(1);
				}
				if(idiv < 0) {
					mstruct = CHILD(0);
					MathStructure mstruct2(CHILD(1));
					mstruct.differentiate(x_var, eo);
					mstruct2.differentiate(x_var, eo);	
					mstruct *= CHILD(1);
					mstruct2 *= CHILD(0);
					set(mstruct, true);
					add(mstruct2);
				}
			}
			if(idiv >= 0) {
				MathStructure vstruct;
				if(CHILD((size_t) idiv)[1].isMinusOne()) {
					vstruct = CHILD((size_t) idiv)[0];
				} else {
					vstruct = CHILD((size_t) idiv);
					vstruct[1].number().negate();
				}
				MathStructure mdiv(vstruct);
				mdiv ^= MathStructure(2, 1);
				MathStructure diff_v(vstruct);
				diff_v.differentiate(x_var, eo);
				MathStructure diff_u(mstruct);
				diff_u.differentiate(x_var, eo);
				vstruct *= diff_u;
				mstruct *= diff_v;
				set_nocopy(vstruct, true);
				subtract(mstruct);
				divide(mdiv);
			}
			break;
		}
		case STRUCT_SYMBOLIC: {
			if(representsNumber()) {
				clear(true);
			} else {
				MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
				set(mstruct);
				return false;
			}
			break;
		}
		case STRUCT_VARIABLE: {
			if(eo.calculate_variables && o_variable->isKnown()) {
				if(eo.approximation != APPROXIMATION_EXACT || !o_variable->isApproximate()) {
					set(((KnownVariable*) o_variable)->get(), true);
					return differentiate(x_var, eo);
				} else if(containsRepresentativeOf(x_var, true, true) != 0) {
					MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
					set(mstruct);
					return false;
				}
			}
			if(representsNumber(true)) {
				clear(true);
				break;
			}
		}		
		default: {
			MathStructure mstruct(CALCULATOR->f_diff, this, &x_var, &m_one, NULL);
			set(mstruct);
			return false;
		}	
	}
	return true;
}

bool MathStructure::integrate(const MathStructure &x_var, const EvaluationOptions &eo) {
	if(equals(x_var)) {
		raise(2);
		multiply(MathStructure(1, 2));
		return true;
	}
	if(containsRepresentativeOf(x_var, true, true) == 0) {
		multiply(x_var);
		return true;
	}
	switch(m_type) {
		case STRUCT_ADDITION: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).integrate(x_var, eo)) {
					CHILD_UPDATED(i);
				}
			}
			break;
		}
		case STRUCT_ALTERNATIVES: {
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).integrate(x_var, eo)) {
					CHILD_UPDATED(i);
				}
			}
			break;
		}
		case STRUCT_BITWISE_AND: {}
		case STRUCT_BITWISE_OR: {}
		case STRUCT_BITWISE_XOR: {}
		case STRUCT_LOGICAL_AND: {}
		case STRUCT_LOGICAL_OR: {}
		case STRUCT_LOGICAL_XOR: {}
		case STRUCT_COMPARISON: {}
		case STRUCT_UNIT: {}
		case STRUCT_NUMBER: {
			multiply(x_var);
			break;
		}
		case STRUCT_POWER: {
			if(CHILD(0).equals(x_var)) {
				if(CHILD(1).isNumber() && CHILD(1).number().isMinusOne()) {
					MathStructure mstruct(CALCULATOR->f_abs, &x_var, NULL);
					set(CALCULATOR->f_ln, &mstruct, NULL);
					break;
				} else if(CHILD(1).isNumber() || (CHILD(1).containsRepresentativeOf(x_var, true, true) == 0 && CHILD(1).representsNonNegative(false))) {
					CHILD(1) += m_one;
					MathStructure mstruct(CHILD(1));
					divide(mstruct);
					break;
				}
			} else if(CHILD(0).isVariable() && CHILD(0).variable() == CALCULATOR->v_e) {
				if(CHILD(1).equals(x_var)) {
					break;
				} else if(CHILD(1).isMultiplication()) {
					bool b = false;
					size_t i = 0;
					for(; i < CHILD(1).size(); i++) {
						if(CHILD(1)[i].equals(x_var)) {
							b = true;
							break;
						}
					}
					if(b) {
						MathStructure mstruct;
						if(CHILD(1).size() == 2) {
							if(i == 0) {
								mstruct = CHILD(1)[1];
							} else {
								mstruct = CHILD(1)[0];
							}
						} else {
							mstruct = CHILD(1);
							mstruct.delChild(i + 1);
						}
						if(mstruct.representsNonZero(false) && mstruct.containsRepresentativeOf(x_var, true, true) == 0) {
							divide(mstruct);
							break;
						}
					}
				}
			} else if(CHILD(1).equals(x_var) && CHILD(0).containsRepresentativeOf(x_var, true, true) == 0 && CHILD(0).representsPositive(false)) {
				MathStructure mstruct(CALCULATOR->f_ln, &CHILD(0), NULL);
				CHILD(1) *= mstruct;
				CHILD(0) = CALCULATOR->v_e;
				CHILDREN_UPDATED;
				return integrate(x_var, eo);
			}
			MathStructure mstruct(CALCULATOR->f_integrate, this, &x_var, NULL);
			set(mstruct);
			return false;
		}
		case STRUCT_FUNCTION: {
			if(o_function == CALCULATOR->f_ln && SIZE == 1 && CHILD(0) == x_var) {
				multiply(x_var);
				subtract(x_var);
			} else if(o_function == CALCULATOR->f_sin && SIZE == 1 && CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0] == x_var && CHILD(0)[1] == CALCULATOR->getRadUnit()) {
				o_function = CALCULATOR->f_cos;
				multiply(m_minus_one);
			} else if(o_function == CALCULATOR->f_cos && SIZE == 1 && CHILD(0).isMultiplication() && CHILD(0).size() == 2 && CHILD(0)[0] == x_var && CHILD(0)[1] == CALCULATOR->getRadUnit()) {
				o_function = CALCULATOR->f_sin;
			} else if(o_function == CALCULATOR->f_diff && SIZE == 3 && CHILD(1) == x_var) {
				if(CHILD(2).isOne()) {
					setToChild(1, true);
				} else {
					CHILD(2) += m_minus_one;
				}
			} else if(o_function == CALCULATOR->f_diff && SIZE == 2 && CHILD(1) == x_var) {
				setToChild(1, true);
			} else {
				if(!eo.calculate_functions || !calculateFunctions(eo)) {
					MathStructure mstruct(CALCULATOR->f_integrate, this, &x_var, NULL);
					set(mstruct);
					return false;
				} else {
					EvaluationOptions eo2 = eo;
					eo2.calculate_functions = false;
					return integrate(x_var, eo2);
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			MathStructure mstruct;
			bool b = false;
			for(size_t i = 0; i < SIZE; i++) {
				if(CHILD(i).containsRepresentativeOf(x_var, true, true) == 0) {
					if(b) {
						mstruct *= CHILD(i);
					} else {
						mstruct = CHILD(i);
						b = true;
					}
					ERASE(i);
				}
			}
			if(b) {
				if(SIZE == 1) {
					setToChild(1, true);
				} else if(SIZE == 0) {
					set(mstruct, true);
					break;	
				}
				integrate(x_var, eo);
				multiply(mstruct);
			} else {
				MathStructure mstruct(CALCULATOR->f_integrate, this, &x_var, NULL);
				set(mstruct);
				return false;
			}
			break;
		}
		case STRUCT_SYMBOLIC: {
			if(representsNumber()) {
				multiply(x_var);
			} else {
				MathStructure mstruct(CALCULATOR->f_integrate, this, &x_var, NULL);
				set(mstruct);
				return false;
			}
			break;
		}
		case STRUCT_VARIABLE: {
			if(eo.calculate_variables && o_variable->isKnown()) {
				if(eo.approximation != APPROXIMATION_EXACT || !o_variable->isApproximate()) {
					set(((KnownVariable*) o_variable)->get(), true);
					return integrate(x_var, eo);
				} else if(containsRepresentativeOf(x_var, true, true) != 0) {
					MathStructure mstruct3(1);
					MathStructure mstruct(CALCULATOR->f_integrate, this, &x_var, &mstruct3, NULL);
					set(mstruct);
					return false;
				}
			}
			if(representsNumber()) {
				multiply(x_var);
				break;
			}
		}		
		default: {
			MathStructure mstruct(CALCULATOR->f_integrate, this, &x_var, NULL);
			set(mstruct);
			return false;
		}	
	}
	return true;
}


const MathStructure &MathStructure::find_x_var() const {
	if(isSymbolic()) {
		return *this;
	} else if(isVariable()) {
		if(o_variable->isKnown()) return m_undefined;
		return *this;
	}
	const MathStructure *mstruct;
	const MathStructure *x_mstruct = &m_undefined;
	for(size_t i = 0; i < SIZE; i++) {
		mstruct = &CHILD(i).find_x_var();
		if(mstruct->isVariable()) {
			if(mstruct->variable() == CALCULATOR->v_x) {
				return *mstruct;
			} else if(!x_mstruct->isVariable()) {
				x_mstruct = mstruct;
			} else if(mstruct->variable() == CALCULATOR->v_y) {
				x_mstruct = mstruct;
			} else if(mstruct->variable() == CALCULATOR->v_z && x_mstruct->variable() != CALCULATOR->v_y) {
				x_mstruct = mstruct;
			}
		} else if(mstruct->isSymbolic()) {
			if(!x_mstruct->isVariable() && !x_mstruct->isSymbolic()) {
				x_mstruct = mstruct;
			}
		}
	}
	return *x_mstruct;
}
bool MathStructure::isolate_x(const EvaluationOptions &eo, const MathStructure &x_varp) {
	if(!isComparison()) {
		bool b = false;
		for(size_t i = 0; i < SIZE; i++) {
			if(CHILD(i).isolate_x(eo, x_varp)) {
				CHILD_UPDATED(i);
				b = true;
			}
		}
		return b;
	}
	if(x_varp.isUndefined()) {
		const MathStructure *x_var2;
		if(eo.isolate_var) x_var2 = eo.isolate_var;
		else x_var2 = &find_x_var();
		if(x_var2->isUndefined()) return false;
		if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
			return isolate_x(eo, *x_var2);
		}
		if(CHILD(1).isZero() && CHILD(0).isAddition()) {
			bool found_1x = false;
			for(size_t i = 0; i < CHILD(0).size(); i++) {
				if(CHILD(0)[i] == *x_var2) {
					found_1x = true;
				} else if(CHILD(0)[i].contains(*x_var2)) {
					found_1x = false;
					break;
				}
			}
			if(found_1x) return isolate_x(eo, *x_var2);
		}
		if(containsType(STRUCT_VECTOR)) return false;
		MathStructure x_var(*x_var2);
		MathStructure msave(*this);
		if(isolate_x(eo, x_var)) {	
			if(isComparison() && CHILD(0) == x_var) {
				bool b = false;
				MathStructure mtest;
				EvaluationOptions eo2 = eo;
				eo2.calculate_functions = false;
				eo2.isolate_x = false;
				eo2.test_comparisons = true;
				if(eo2.approximation == APPROXIMATION_EXACT) eo2.approximation = APPROXIMATION_TRY_EXACT;
				if(CHILD(1).isVector()) {
					MathStructure mtest2;
					for(size_t i2 = 0; i2 < CHILD(1).size();) {
						mtest = x_var;
						mtest.transform(STRUCT_COMPARISON, CHILD(1)[i2]);
						mtest.setComparisonType(comparisonType());
						mtest.eval(eo2);
						if(mtest.isComparison()) {
							mtest = msave;
							mtest.replace(x_var, CHILD(1)[i2]);
							CALCULATOR->beginTemporaryStopMessages();
							mtest.eval(eo2);
							if(CALCULATOR->endTemporaryStopMessages() > 0 || !mtest.isNumber() || (ct_comp == COMPARISON_EQUALS && mtest.number().getBoolean() < 1) || (ct_comp == COMPARISON_NOT_EQUALS && mtest.number().getBoolean() > 0)) {
								CHILD(1).delChild(i2 + 1);
							} else {
								i2++;
							}
						} else {
							CHILD(1).delChild(i2 + 1);
							mtest2 = mtest;
							b = true;
						}
					}
					if(CHILD(1).isVector() && CHILD(1).size() > 0) {
						if(CHILD(1).size() == 1) {
							msave = CHILD(1)[0];
							CHILD(1) = msave;
						}
						return true;
					} else if(b) {
						set(mtest2);
						return true;
					}
				} else {
					mtest = *this;
					mtest.eval(eo2);
					if(mtest.isComparison()) {
						mtest = msave;
						mtest.replace(x_var, CHILD(1));
						CALCULATOR->beginTemporaryStopMessages();
						mtest.eval(eo2);
						if(CALCULATOR->endTemporaryStopMessages() == 0 && mtest.isNumber() && ((ct_comp == COMPARISON_EQUALS && mtest.number().getBoolean() > 0) || (ct_comp == COMPARISON_NOT_EQUALS && mtest.number().getBoolean() < 1))) {
							return true;
						}
					} else {
						set(mtest);
						return true;
					}
					set(msave);
					return false;
				}
				set(msave);
			}
		}
		return false;
	}
	MathStructure x_var(x_varp);
	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	eo2.test_comparisons = false;
	eo2.isolate_x = false;
	switch(CHILD(0).type()) {
		case STRUCT_ADDITION: {
			bool b = false;
			for(size_t i = 0; i < CHILD(0).size(); i++) {
				if(!CHILD(0)[i].contains(x_var)) {
					CHILD(1).subtract(CHILD(0)[i], true);
					CHILD(0).delChild(i + 1);
					b = true;
				}
			}
			if(b) {
				CHILD(1).eval(eo2);
				CHILD_UPDATED(1);
				if(CHILD(0).size() == 1) {
					MathStructure msave(CHILD(0)[0]);
					CHILD(0) = msave;
				}
				isolate_x(eo, x_var);
				return true;
			}
			if(CHILD(0).size() == 2) {
				bool sqpow = false;
				bool nopow = false;
				MathStructure mstruct_a, mstruct_b;
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(!sqpow && CHILD(0)[i].isPower() && CHILD(0)[i][0] == x_var && CHILD(0)[i][1].isNumber() && CHILD(0)[i][1].number() == 2) {
						sqpow = true;
						mstruct_a.set(m_one);
					} else if(!nopow && CHILD(0)[i] == x_var) {
						nopow = true;
						mstruct_b.set(m_one);
					} else if(CHILD(0)[i].isMultiplication()) {
						for(size_t i2 = 0; i2 < CHILD(0)[i].size(); i2++) {
							if(CHILD(0)[i][i2] == x_var) {
								if(nopow) break;
								nopow = true;
								mstruct_b = CHILD(0)[i];
								mstruct_b.delChild(i2 + 1);
							} else if(CHILD(0)[i][i2].isPower() && CHILD(0)[i][i2][0] == x_var && CHILD(0)[i][i2][1].isNumber() && CHILD(0)[i][i2][1].number() == 2) {
								if(sqpow) break;
								sqpow = true;
								mstruct_a = CHILD(0)[i];
								mstruct_a.delChild(i2 + 1);
							} else if(CHILD(0)[i][i2].contains(x_var)) {
								sqpow = false;
								nopow = false;
								break;
							}
						}
					}
				}	
				if(sqpow && nopow) {
					MathStructure b2(mstruct_b);
					b2 ^= 2;
					MathStructure ac(4, 1);
					ac *= mstruct_a;
					ac *= CHILD(1);
					b2 += ac;
					b2 ^= MathStructure(1, 2);
					mstruct_b.negate();
					MathStructure mstruct_1(mstruct_b);
					mstruct_1 += b2;
					MathStructure mstruct_2(mstruct_b);
					mstruct_2 -= b2;
					mstruct_a *= 2;
					mstruct_1 /= mstruct_a;
					mstruct_2 /= mstruct_a;
					mstruct_1.eval(eo2);
					mstruct_2.eval(eo2);
					CHILD(0) = x_var;
					if(mstruct_1 != mstruct_2) {
						CHILD(1).clearVector();
						CHILD(1).addItem(mstruct_1);
						CHILD(1).addItem(mstruct_2);
					} else {
						CHILD(1) = mstruct_1;
					}
					CHILDREN_UPDATED;
					return true;
				} 
			}
			for(size_t i2 = 0; i2 < CHILD(0).size(); i2++) {
				if(CHILD(0)[i2].isMultiplication()) {
					int index = -1;
					for(size_t i = 0; i < CHILD(0)[i2].size(); i++) {
						if(CHILD(0)[i2][i].contains(x_var)) {
  							if(CHILD(0)[i2][i].isPower() && CHILD(0)[i2][i][1].isNumber() && CHILD(0)[i2][i][1].number().isMinusOne() && (eo.assume_denominators_nonzero || CHILD(0)[i2][i][0].representsNonZero(true))) {
								index = i;
								break;
							}
						}
					}
					if(index >= 0) {
						MathStructure msave(CHILD(0)[i2]);
						CHILD(0).delChild(i2 + 1);
						CHILD(0) -= CHILD(1);
						CHILD(1).clear();
						CHILD(0) *= msave[index][0];
						msave.delChild(index + 1);
						if(msave.size() == 1) {
							MathStructure msave2(msave[0]);
							msave = msave2;
						}
						CHILD(0) += msave;
						CHILD(0).eval(eo2);
						CHILDREN_UPDATED;
						isolate_x(eo, x_var);
						return true;
					}
				}
			}
			if(CHILD(1).isNumber()) {
				MathStructure mtest(*this);
				if(!CHILD(1).isZero()) {
					mtest[0].add(CHILD(1), true);
					mtest[0][mtest[0].size() - 1].number().negate();
					mtest[1].clear();
					mtest.childrenUpdated();
				}
				if(mtest[0].factorize(eo)) {
					mtest.childUpdated(1);
					if(mtest.isolate_x(eo, x_var)) {
						set(mtest);
						return true;
					}
				}
			}
			break;
		}
		case STRUCT_MULTIPLICATION: {
			bool b = false;
			for(size_t i = 0; i < CHILD(0).size(); i++) {
				if(!CHILD(0)[i].contains(x_var)) {
					if(eo.assume_denominators_nonzero || CHILD(0)[i].representsNonZero(true)) {
						bool b2 = false;
						if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
							if(CHILD(0)[i].representsNegative(true)) {
								switch(ct_comp) {
									case COMPARISON_LESS: {ct_comp = COMPARISON_GREATER; break;}
									case COMPARISON_GREATER: {ct_comp = COMPARISON_LESS; break;}
									case COMPARISON_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
									case COMPARISON_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_LESS; break;}
									default: {}
								}
								b2 = true;
							} else if(CHILD(0)[i].representsNonNegative(true)) {
								b2 = true;
							}
						} else {
							b2 = true;
						}
						if(b2) {
							CHILD(1).divide(CHILD(0)[i], true);
							CHILD(0).delChild(i + 1);
							b = true;
						}
					}
				}
			}
			CHILD(1).eval(eo2);
			CHILD_UPDATED(1);
			if(b) {
				if(CHILD(0).size() == 1) {
					MathStructure msave(CHILD(0)[0]);
					CHILD(0) = msave;
				}
				if(CHILD(1).contains(x_var)) {
					CHILD(0) -= CHILD(1);
					CHILD(1).clear();
				}
				isolate_x(eo, x_var);				
				return true;
			} else if(CHILD(1).isZero() && (ct_comp == COMPARISON_EQUALS || ct_comp == COMPARISON_NOT_EQUALS)) {
				MathStructure mtest, mtest2, msol;
				msol.clearVector();
				for(size_t i = 0; i < CHILD(0).size(); i++) {
					if(CHILD(0)[i].contains(x_var)) {
						mtest.clear();
						mtest.transform(STRUCT_COMPARISON, CHILD(1));
						mtest.setComparisonType(ct_comp);
						mtest[0] = CHILD(0)[i];
						mtest.childUpdated(1);
						mtest.isolate_x(eo, x_var);
						if(mtest[0] == x_var) {
							eo2.test_comparisons = true;
							if(eo2.approximation == APPROXIMATION_EXACT) eo2.approximation = APPROXIMATION_TRY_EXACT;eo2.approximation = APPROXIMATION_TRY_EXACT;
							if(mtest[1].isVector()) {
								for(size_t i2 = 0; i2 < mtest[1].size(); i2++) {
									mtest2 = x_var;
									mtest2.transform(STRUCT_COMPARISON, mtest[1][i2]);
									mtest2.setComparisonType(mtest.comparisonType());
									mtest2.eval(eo2);
									if(mtest2.isComparison()) {
										mtest2 = *this;
										mtest2[0].replace(x_var, mtest[1][i2]);
										mtest2.childUpdated(1);
										CALCULATOR->beginTemporaryStopMessages();
										mtest2.eval(eo2);
										if(CALCULATOR->endTemporaryStopMessages() == 0 && mtest2.isNumber() && ((ct_comp == COMPARISON_EQUALS && mtest2.number().getBoolean() > 0) || (ct_comp == COMPARISON_NOT_EQUALS && mtest2.number().getBoolean() < 1))) {
											msol.addChild(mtest[1][i2]);
										}
									}
								}
							} else {
								mtest2 = mtest;
								mtest2.eval(eo2);
								if(mtest2.isComparison()) {
									mtest2 = *this;
									mtest2[0].replace(x_var, mtest[1]);
									mtest2.childUpdated(1);
									CALCULATOR->beginTemporaryStopMessages();
									mtest2.eval(eo2);
									if(CALCULATOR->endTemporaryStopMessages() == 0 && mtest2.isNumber() && ((ct_comp == COMPARISON_EQUALS && mtest2.number().getBoolean() > 0) || (ct_comp == COMPARISON_NOT_EQUALS && mtest2.number().getBoolean() < 1))) {
										msol.addChild(mtest[1]);
									}
								}
							}
						}
					}
				}
				if(msol.size() > 0) {
					CHILD(0) = x_var;
					if(msol.size() == 1) {
						CHILD(1) = msol[0];
					} else {
						CHILD(1) = msol;
					}
					CHILDREN_UPDATED;
					return true;
				}
			}
			break;
		} 
		case STRUCT_POWER: {
			if(CHILD(0)[0].contains(x_var)) {
				if(!CHILD(0)[1].contains(x_var)) {
					MathStructure mnonzero, mbefore;
					bool test_nonzero = false;
					if(!CHILD(0)[1].representsNonNegative(true) && !CHILD(0)[0].representsNonZero(true)) {
						if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) return false;
						test_nonzero = true;
						if(CHILD(0)[1].isNumber()) {
							if(CHILD(0)[1].isMinusOne()) {
								mnonzero = CHILD(0)[0];
							} else {
								mnonzero = CHILD(0);
								mnonzero[1].number().setNegative(false);
							}
						} else {
							mnonzero = CHILD(0);
							mnonzero[1].negate();
						}
					}
					if(CHILD(0)[1].representsEven(true)) {

						MathStructure exp(1, 1);
						exp /= CHILD(0)[1];
						
						bool b1 = true, b2 = true;
						
						MathStructure mstruct(*this);
						mstruct[1] ^= exp;
						mstruct[1].eval(eo2);
						mstruct[0] = CHILD(0)[0];
						mstruct.childrenUpdated();
						
						MathStructure mtest1(mstruct);
						mtest1.isolate_x(eo, x_var);
						MathStructure mtestC1(mtest1);
						eo2.test_comparisons = true;
						mtestC1.eval(eo2);
						b1 = mtestC1.isComparison();
						eo2.test_comparisons = false;
						
						if(test_nonzero && b1) {
							if(mtestC1[0] != x_var) {
								return false;
							}
							mnonzero.replace(x_var, mtestC1[1]);
							CALCULATOR->beginTemporaryStopMessages();
							mnonzero.eval(eo2);
							if(CALCULATOR->endTemporaryStopMessages() > 0) {
								return false;
							} else if(!((eo.assume_denominators_nonzero && !mnonzero.isZero()) || mnonzero.representsNonZero())) {
								if(eo.assume_denominators_nonzero) {
									b1 = false;
								} else {
									return false;
								}
							}	
						}
						
						MathStructure mtest2(mstruct);
						mtest2[1].negate();
						mtest2[1].eval(eo2);
						mtest2.childUpdated(2);
						switch(ct_comp) {
							case COMPARISON_LESS: {mtest2.setComparisonType(COMPARISON_GREATER); break;}
							case COMPARISON_GREATER: {mtest2.setComparisonType(COMPARISON_LESS); break;}
							case COMPARISON_EQUALS_LESS: {mtest2.setComparisonType(COMPARISON_EQUALS_GREATER); break;}
							case COMPARISON_EQUALS_GREATER: {mtest2.setComparisonType(COMPARISON_EQUALS_LESS); break;}
							default: {}
						}
						mtest2.isolate_x(eo, x_var);						
						MathStructure mtestC2(mtest2);
						if(mtest2 == mtest1) {
							b2 = false;
							mtestC2 = mtestC1;	
						}
						if(b2) {
							eo2.test_comparisons = true;
							mtestC2.eval(eo2);
							b2 = mtestC2.isComparison();
							eo2.test_comparisons = false;
						}
						if(test_nonzero && b2) {
							if(mtestC2[0] != x_var) {
								return false;
							}
							mnonzero.replace(x_var, mtestC2[1]);
							CALCULATOR->beginTemporaryStopMessages();
							mnonzero.eval(eo2);
							if(CALCULATOR->endTemporaryStopMessages() > 0) {
								return false;
							} else if(!((eo.assume_denominators_nonzero && !mnonzero.isZero()) || mnonzero.representsNonZero())) {
								if(b1 && eo.assume_denominators_nonzero) {
									b2 = false;
								} else {
									return false;
								}
							}	
						}
					
						if(b1 && !b2) {
							set(mtest1);	
						} else if(b2 && !b1) {
							set(mtest2);	
						} else if(!b1 && !b2) {
							if(mtestC1 == mtestC2) {
								set(mtestC1);
							} else {
								clearVector();
								APPEND(mtestC1);
								APPEND(mtestC2);
							}
						} else if(mtest1[0] == mtest2[0] && mtest1.comparisonType() == mtest2.comparisonType()) {
							CHILD(0) = mtest1[0];
							CHILD(1).clearVector();
							CHILD(1).addItem(mtest1[1]);
							CHILD(1).addItem(mtest2[1]);
							CHILDREN_UPDATED;
						} else {
							clearVector();
							APPEND(mtest1);
							APPEND(mtest2);
						}						
					} else {
						if(test_nonzero) mbefore = *this;
						MathStructure exp(1, 1);
						exp /= CHILD(0)[1];
						CHILD(1) ^= exp;
						CHILD(1).eval(eo2);
						MathStructure msave(CHILD(0)[0]);
						CHILD(0) = msave;
						CHILDREN_UPDATED;
						isolate_x(eo, x_var);
						if(test_nonzero) {
							if(isComparison() && CHILD(0) == x_var) {
								mnonzero.replace(x_var, CHILD(1));
								CALCULATOR->beginTemporaryStopMessages();
								mnonzero.eval(eo2);
								if(CALCULATOR->endTemporaryStopMessages() == 0 && ((eo.assume_denominators_nonzero && !mnonzero.isZero()) || mnonzero.representsNonZero())) {
									return true;
								}	
							}
							set(mbefore);
							return false;
						}
					}
					return true;
				}
			} else if(CHILD(0)[1].contains(x_var)) {
				if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
					if(CHILD(0)[0].isNumber() && CHILD(0)[0].number().isReal()) {
						if(CHILD(0)[0].number().isFraction()) {
							switch(ct_comp) {
								case COMPARISON_LESS: {ct_comp = COMPARISON_GREATER; break;}
								case COMPARISON_GREATER: {ct_comp = COMPARISON_LESS; break;}
								case COMPARISON_EQUALS_LESS: {ct_comp = COMPARISON_EQUALS_GREATER; break;}
								case COMPARISON_EQUALS_GREATER: {ct_comp = COMPARISON_EQUALS_LESS; break;}
								default: {}
							}
						}
					} else {
						return false;
					}
				}
				MathStructure msave(CHILD(1));
				CHILD(1).set(CALCULATOR->f_logn, &msave, &CHILD(0)[0], NULL);
				eo2.calculate_functions = true;
				CHILD(1).eval(eo2);
				CHILD(0).setToChild(2, true);
				CHILDREN_UPDATED;
				isolate_x(eo, x_var);
				return true;
			}
			break;
		}
		case STRUCT_FUNCTION: {
			/*if(CHILD(0).function() == CALCULATOR->f_sin && CHILD(0).size() == 1) {
				if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
					break;
				}
				if(CHILD(0)[0].contains(x_var)) {
					MathStructure msave(CHILD(1));
					CHILD(1).set(CALCULATOR->f_asin, &msave, NULL);
					EvaluationOptions eo3 = eo2;
					eo3.parse_options.angle_unit = ANGLE_UNIT_RADIANS;
					CHILD(1).calculateFunctions(eo3, false);
					CHILD(0).setToChild(1, true);
					if(CALCULATOR->getRadUnit()) CHILD(0) /= CALCULATOR->getRadUnit();
					CHILD(0).eval(eo2);
					CHILDREN_UPDATED;
					isolate_x(eo, x_var);
					return true;
				}
			} else */
			if(CHILD(0).function() == CALCULATOR->f_ln && CHILD(0).size() == 1) {
				if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
					break;
				}
				if(CHILD(0)[0].contains(x_var)) {
					MathStructure msave(CHILD(1));
					CHILD(1).set(CALCULATOR->v_e);
					CHILD(1) ^= msave;
					CHILD(1).eval(eo2);
					CHILD(0).setToChild(1, true);
					CHILDREN_UPDATED;
					isolate_x(eo, x_var);
					return true;
				}
			} else if(CHILD(0).function() == CALCULATOR->f_logn && CHILD(0).size() == 2) {
				if(ct_comp != COMPARISON_EQUALS && ct_comp != COMPARISON_NOT_EQUALS) {
					break;
				}
				if(CHILD(0)[0].contains(x_var)) {
					MathStructure msave(CHILD(1));
					CHILD(1) = CHILD(0)[1];
					CHILD(1) ^= msave;
					CHILD(1).eval(eo2);
					MathStructure msave2(CHILD(0)[0]);
					CHILD(0) = msave2;
					CHILDREN_UPDATED;
					isolate_x(eo, x_var);					
					return true;
				}
			} else if(CHILD(0).function() == CALCULATOR->f_abs && CHILD(0).size() == 1) {
				if(CHILD(0)[0].contains(x_var)) {
					
					bool b1 = true, b2 = true;
									
					MathStructure mtest1(*this);
					mtest1[0] = CHILD(0)[0];
					mtest1.childUpdated(1);
					mtest1.isolate_x(eo, x_var);					
					MathStructure mtestC1(mtest1);
					eo2.test_comparisons = true;
					mtestC1.eval(eo2);
					b1 = mtestC1.isComparison();
					eo2.test_comparisons = false;
						
					MathStructure mtest2(*this);
					mtest2[0] = CHILD(0)[0];
					mtest2[1].negate();
					mtest2[1].eval(eo2);
					mtest2.childrenUpdated();
					switch(ct_comp) {
						case COMPARISON_LESS: {mtest2.setComparisonType(COMPARISON_GREATER); break;}
						case COMPARISON_GREATER: {mtest2.setComparisonType(COMPARISON_LESS); break;}
						case COMPARISON_EQUALS_LESS: {mtest2.setComparisonType(COMPARISON_EQUALS_GREATER); break;}
						case COMPARISON_EQUALS_GREATER: {mtest2.setComparisonType(COMPARISON_EQUALS_LESS); break;}
						default: {}
					}
					mtest2.isolate_x(eo, x_var);
					MathStructure mtestC2(mtest2);
					if(mtest2 == mtest1) {
						b2 = false;
						mtestC2 = mtestC1;	
					}
					if(b2) {
						eo2.test_comparisons = true;
						mtestC2.eval(eo2);
						b2 = mtestC2.isComparison();
						eo2.test_comparisons = false;
					}
					
					
					if(b1 && !b2) {
						set(mtest1);	
					} else if(b2 && !b1) {
						set(mtest2);	
					} else if(!b1 && !b2) {
						if(mtestC1 == mtestC2) {
							set(mtestC1);
						} else {
							clearVector();
							APPEND(mtestC1);
							APPEND(mtestC2);
						}
					} else if(mtest1[0] == mtest2[0] && mtest1.comparisonType() == mtest2.comparisonType()) {
						CHILD(0) = mtest1[0];
						CHILD(1).clearVector();
						CHILD(1).addItem(mtest1[1]);
						CHILD(1).addItem(mtest2[1]);
						CHILDREN_UPDATED;
					} else {
						clearVector();
						APPEND(mtest1);
						APPEND(mtest2);
					}
					return true;
					
				}
			}
			break;
		}
	}
	return false;
}

