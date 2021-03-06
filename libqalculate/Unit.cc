/*
    Qalculate    

    Copyright (C) 2003-2007  Niklas Knutsson (nq@altern.org)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "support.h"

#include "Unit.h"
#include "util.h"
#include "Calculator.h"
#include "MathStructure.h"
#include "Prefix.h"

Unit::Unit(string cat_, string name_, string plural_, string singular_, string title_, bool is_local, bool is_builtin, bool is_active) : ExpressionItem(cat_, "", title_, "", is_local, is_builtin, is_active) {
	remove_blank_ends(plural_);
	remove_blank_ends(singular_);
	if(!name_.empty()) {
		names.resize(1);
		names[0].name = name_;
		names[0].unicode = false;
		names[0].abbreviation = true;
		names[0].case_sensitive = true;
		names[0].suffix = false;
		names[0].avoid_input = false;
		names[0].reference = true;
		names[0].plural = false;
	}
	if(!singular_.empty()) {
		names.resize(names.size() + 1);
		names[names.size() - 1].name = singular_;
		names[names.size() - 1].unicode = false;
		names[names.size() - 1].abbreviation = false;
		names[names.size() - 1].case_sensitive = text_length_is_one(names[names.size() - 1].name);
		names[names.size() - 1].suffix = false;
		names[names.size() - 1].avoid_input = false;
		names[names.size() - 1].reference = false;
		names[names.size() - 1].plural = false;
	}
	if(!plural_.empty()) {
		names.resize(names.size() + 1);
		names[names.size() - 1].name = plural_;
		names[names.size() - 1].unicode = false;
		names[names.size() - 1].abbreviation = false;
		names[names.size() - 1].case_sensitive = text_length_is_one(names[names.size() - 1].name);
		names[names.size() - 1].suffix = false;
		names[names.size() - 1].avoid_input = false;
		names[names.size() - 1].reference = false;
		names[names.size() - 1].plural = true;
	}
	b_si = false;
}
Unit::Unit() {
	b_si = false;
}
Unit::Unit(const Unit *unit) {
	set(unit);
}
Unit::~Unit() {}
ExpressionItem *Unit::copy() const {
	return new Unit(this);
}
void Unit::set(const ExpressionItem *item) {
	if(item->type() == TYPE_UNIT) {
		b_si = ((Unit*) item)->isSIUnit();
		ssystem = ((Unit*) item)->system();
	}
	ExpressionItem::set(item);
}
bool Unit::isSIUnit() const {
	return b_si;
}
void Unit::setAsSIUnit() {
	if(!b_si) {
		b_si = true;
		ssystem == "SI";
		setChanged(true);
	}
}
void Unit::setSystem(string s_system) {
	if(s_system != ssystem) {
		ssystem = s_system;
		if(ssystem == "SI" || ssystem == "si" || ssystem == "Si") {
			b_si = true;
		} else {
			b_si = false;
		}
		setChanged(true);
	}
}
const string &Unit::system() const {
	return ssystem;
}
bool Unit::isCurrency() const {
	return baseUnit() == CALCULATOR->u_euro;
}
bool Unit::isUsedByOtherUnits() const {
	return CALCULATOR->unitIsUsedByOtherUnits(this);
}
string Unit::print(bool plural_, bool short_, bool use_unicode, bool (*can_display_unicode_string_function) (const char*, void*), void *can_display_unicode_string_arg) const {
	return preferredName(short_, use_unicode, plural_, false, can_display_unicode_string_function, can_display_unicode_string_arg).name;
}
const string &Unit::plural(bool return_singular_if_no_plural, bool use_unicode, bool (*can_display_unicode_string_function) (const char*, void*), void *can_display_unicode_string_arg) const {
	const ExpressionName *ename = &preferredName(false, use_unicode, true, false, can_display_unicode_string_function, can_display_unicode_string_arg);
	if(!return_singular_if_no_plural && !ename->plural) return empty_string;
	return ename->name;
}
const string &Unit::singular(bool return_abbreviation_if_no_singular, bool use_unicode, bool (*can_display_unicode_string_function) (const char*, void*), void *can_display_unicode_string_arg) const {
	const ExpressionName *ename = &preferredName(false, use_unicode, false, false, can_display_unicode_string_function, can_display_unicode_string_arg);
	if(!return_abbreviation_if_no_singular && ename->abbreviation) return empty_string;
	return ename->name;
}
const string &Unit::abbreviation(bool return_singular_if_no_abbreviation, bool use_unicode, bool (*can_display_unicode_string_function) (const char*, void*), void *can_display_unicode_string_arg) const {
	const ExpressionName *ename = &preferredName(true, use_unicode, false, false, can_display_unicode_string_function, can_display_unicode_string_arg);
	if(!return_singular_if_no_abbreviation && !ename->abbreviation) return empty_string;
	return ename->name;
}
Unit* Unit::baseUnit() const {
	return (Unit*) this;
}
MathStructure &Unit::convertToBaseUnit(MathStructure &mvalue, MathStructure&) const {
	return mvalue;
}
MathStructure &Unit::convertFromBaseUnit(MathStructure &mvalue, MathStructure&) const {
	return mvalue;
}
MathStructure &Unit::convertToBaseUnit(MathStructure &mvalue) const {
	return mvalue;
}
MathStructure &Unit::convertFromBaseUnit(MathStructure &mvalue) const {
	return mvalue;
}
MathStructure Unit::convertToBaseUnit() const {
	return MathStructure(1, 1);
}
MathStructure Unit::convertFromBaseUnit() const {
	return MathStructure(1, 1);
}
int Unit::baseExponent(int exp) const {
	return exp;
}
int Unit::type() const {
	return TYPE_UNIT;
}
int Unit::subtype() const {
	return SUBTYPE_BASE_UNIT;
}
bool Unit::isChildOf(Unit*) const {
	return false;
}
bool Unit::isParentOf(Unit *u) const {
	return u != this && u->baseUnit() == this;
}
bool Unit::hasComplexRelationTo(Unit *u) const {
	if(u == this || u->baseUnit() != this) return false;
	Unit *fbu = u;
	if(fbu->subtype() != SUBTYPE_ALIAS_UNIT) return false;
	while(1) {
		if(fbu == this) return false;
		if(((AliasUnit*) fbu)->hasComplexExpression()) return true;
		if(fbu->subtype() != SUBTYPE_ALIAS_UNIT) return false;
		fbu = (Unit*) ((AliasUnit*) fbu)->firstBaseUnit();
	}
}
MathStructure Unit::convert(Unit *u, bool *converted) const {
	MathStructure mexp(1, 1);
	MathStructure mvalue(1, 1);
	bool b = convert(u, mvalue, mexp);
	if(converted) *converted = b;
	return mvalue;
}
bool Unit::convert(Unit *u, MathStructure &mvalue) const {
	MathStructure mexp(1, 1);
	return convert(u, mvalue, mexp);
}
bool Unit::convert(Unit *u, MathStructure &mvalue, MathStructure &mexp) const {
	if(u == this) {
		return true;
	} else if(u->baseUnit() == baseUnit()) {
		u->convertToBaseUnit(mvalue, mexp);
		convertFromBaseUnit(mvalue, mexp);
		if(isCurrency()) {
			CALCULATOR->checkExchangeRatesDate();
		}
		return true;
	}
	/*} else if(u->subtype() == SUBTYPE_COMPOSITE_UNIT) {
		bool b2 = false;
		CompositeUnit *cu = (CompositeUnit*) u;
		for(size_t i = 1; i <= cu->countUnits(); i++) {
			convert(cu->getInternal(i), mvalue, mexp, &b2);
			if(b2) b = true;
		}
	}*/
	//if(CALCULATOR->alwaysExact() && mvalue->isApproximate()) b = false;
	return false;
}

AliasUnit::AliasUnit(string cat_, string name_, string plural_, string short_name_, string title_, Unit *alias, string relation, int exp, string inverse, bool is_local, bool is_builtin, bool is_active) : Unit(cat_, name_, plural_, short_name_, title_, is_local, is_builtin, is_active) {
	o_unit = (Unit*) alias;
	remove_blank_ends(relation);
	remove_blank_ends(inverse);
	svalue = relation;
	sinverse = inverse;
	i_exp = exp;
}
AliasUnit::AliasUnit() {
	o_unit = NULL;
	svalue = "";
	sinverse = "";
	i_exp = 1;
}
AliasUnit::AliasUnit(const AliasUnit *unit) {
	set(unit);
}
AliasUnit::~AliasUnit() {}
ExpressionItem *AliasUnit::copy() const {
	return new AliasUnit(this);
}
void AliasUnit::set(const ExpressionItem *item) {
	if(item->type() == TYPE_UNIT) {
		Unit::set(item);
		if(((Unit*) item)->subtype() == SUBTYPE_ALIAS_UNIT) {
			AliasUnit *u = (AliasUnit*) item;
			o_unit = (Unit*) u->firstBaseUnit();
			i_exp = u->firstBaseExponent();
			svalue = u->expression();
			sinverse = u->inverseExpression();
		}
	} else {
		ExpressionItem::set(item);
	}
}
Unit* AliasUnit::baseUnit() const {
	return o_unit->baseUnit();
}
Unit* AliasUnit::firstBaseUnit() const {
	return o_unit;
}
void AliasUnit::setBaseUnit(Unit *alias) {
	o_unit = (Unit*) alias;
	setChanged(true);
}
string AliasUnit::expression() const {
	return svalue;
}
string AliasUnit::inverseExpression() const {
	return sinverse;
}
void AliasUnit::setExpression(string relation) {
	remove_blank_ends(relation);
	if(relation.empty()) {
		svalue = "1";
	} else {
		svalue = relation;
	}
	setChanged(true);
}
void AliasUnit::setInverseExpression(string inverse) {
	remove_blank_ends(inverse);
	sinverse = inverse;
	setChanged(true);
}
MathStructure &AliasUnit::convertToBaseUnit(MathStructure &mvalue, MathStructure &mexp) const {
	convertToFirstBaseUnit(mvalue, mexp);
	return o_unit->convertToBaseUnit(mvalue, mexp);
}
MathStructure &AliasUnit::convertFromBaseUnit(MathStructure &mvalue, MathStructure &mexp) const {
	Unit *u = (Unit*) baseUnit();
	AliasUnit *u2;
	while(true) {
		u2 = (AliasUnit*) this;
		while(true) {
			if(u2->firstBaseUnit() == u) {
				break;
			} else {
				u2 = (AliasUnit*) u2->firstBaseUnit();
			}
		}
		u = u2;
		u2->convertFromFirstBaseUnit(mvalue, mexp);
		if(u == this) break;
	}	
	return mvalue;
}
MathStructure &AliasUnit::convertToBaseUnit(MathStructure &mvalue) const {
	MathStructure mexp(1, 1);
	return convertToBaseUnit(mvalue, mexp);
}
MathStructure &AliasUnit::convertFromBaseUnit(MathStructure &mvalue) const {
	MathStructure mexp(1, 1);
	return convertFromBaseUnit(mvalue, mexp);
}
MathStructure AliasUnit::convertToBaseUnit() const {
	MathStructure mexp(1, 1);
	MathStructure mvalue(1, 1);
	return convertToBaseUnit(mvalue, mexp);
}
MathStructure AliasUnit::convertFromBaseUnit() const {
	MathStructure mexp(1, 1);
	MathStructure mvalue(1, 1);
	return convertFromBaseUnit(mvalue, mexp);
}

int AliasUnit::baseExponent(int exp) const {
	return o_unit->baseExponent(exp * i_exp);
}
MathStructure &AliasUnit::convertFromFirstBaseUnit(MathStructure &mvalue, MathStructure &mexp) const {
	if(i_exp != 1) mexp /= i_exp;
	ParseOptions po;
	if(isApproximate() && precision() < 1) {
		po.read_precision = ALWAYS_READ_PRECISION;
	}
	if(sinverse.empty()) {
		if(svalue.find("\\x") != string::npos) {
			string stmp = svalue;
			string stmp2 = LEFT_PARENTHESIS ID_WRAP_LEFT;
			int x_id = CALCULATOR->addId(new MathStructure(mvalue), true);
			stmp2 += i2s(x_id);
			stmp2 += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
			gsub("\\x", stmp2, stmp);
			stmp2 = LEFT_PARENTHESIS ID_WRAP_LEFT;
			int y_id = CALCULATOR->addId(new MathStructure(mexp), true);
			stmp2 += i2s(y_id);
			stmp2 += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
			gsub("\\y", stmp2, stmp);
			CALCULATOR->parse(&mvalue, stmp, po);
			CALCULATOR->delId(x_id);
			CALCULATOR->delId(y_id);
		} else {
			MathStructure *mstruct = new MathStructure();
			CALCULATOR->parse(mstruct, svalue, po);
			if(!mexp.isOne()) mstruct->raise(mexp);
			mvalue.divide_nocopy(mstruct, true);
		}
	} else {
		if(sinverse.find("\\x") != string::npos) {
			string stmp = sinverse;
			string stmp2 = LEFT_PARENTHESIS ID_WRAP_LEFT;
			int x_id = CALCULATOR->addId(new MathStructure(mvalue), true);
			stmp2 += i2s(x_id);
			stmp2 += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
			gsub("\\x", stmp2, stmp);
			stmp2 = LEFT_PARENTHESIS ID_WRAP_LEFT;
			int y_id = CALCULATOR->addId(new MathStructure(mexp), true);
			stmp2 += i2s(y_id);
			stmp2 += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
			gsub("\\y", stmp2, stmp);
			CALCULATOR->parse(&mvalue, stmp, po);
			CALCULATOR->delId(x_id);
			CALCULATOR->delId(y_id);			
		} else {
			MathStructure *mstruct = new MathStructure();
			CALCULATOR->parse(mstruct, sinverse, po);
			if(!mexp.isOne()) mstruct->raise(mexp);
			mvalue.multiply_nocopy(mstruct, true);
		}
	}
	if(precision() > 0 && (mvalue.precision() < 1 || precision() < mvalue.precision())) mvalue.setPrecision(precision());
	if(isApproximate()) mvalue.setApproximate();
	return mvalue;
}
MathStructure &AliasUnit::convertToFirstBaseUnit(MathStructure &mvalue, MathStructure &mexp) const {
	ParseOptions po;
	if(isApproximate() && precision() < 1) {
		po.read_precision = ALWAYS_READ_PRECISION;
	}
	if(svalue.find("\\x") != string::npos) {
		string stmp = svalue;
		string stmp2 = LEFT_PARENTHESIS ID_WRAP_LEFT;
		int x_id = CALCULATOR->addId(new MathStructure(mvalue), true);
		stmp2 += i2s(x_id);
		stmp2 += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
		gsub("\\x", stmp2, stmp);
		stmp2 = LEFT_PARENTHESIS ID_WRAP_LEFT;
		int y_id = CALCULATOR->addId(new MathStructure(mexp), true);
		stmp2 += i2s(y_id);
		stmp2 += ID_WRAP_RIGHT RIGHT_PARENTHESIS;
		gsub("\\y", stmp2, stmp);
		CALCULATOR->parse(&mvalue, stmp, po);
		CALCULATOR->delId(x_id);
		CALCULATOR->delId(y_id);
	} else {
		MathStructure *mstruct = new MathStructure();
		CALCULATOR->parse(mstruct, svalue, po);
		if(!mexp.isOne()) mstruct->raise(mexp);
		mvalue.multiply_nocopy(mstruct, true);
	}
	if(precision() > 0 && (mvalue.precision() < 1 || precision() < mvalue.precision())) mvalue.setPrecision(precision());
	if(isApproximate()) mvalue.setApproximate();
	if(i_exp != 1) mexp.multiply(i_exp);
	return mvalue;
}
void AliasUnit::setExponent(int exp) {
	i_exp = exp;
	setChanged(true);
}
int AliasUnit::firstBaseExponent() const {
	return i_exp;
}
int AliasUnit::subtype() const {
	return SUBTYPE_ALIAS_UNIT;
}
bool AliasUnit::isChildOf(Unit *u) const {
	if(u == this) return false;
	if(baseUnit() == u) return true;
	if(u->baseUnit() != baseUnit()) return false;
	Unit *u2 = (Unit*) this;
	while(1) {
		u2 = (Unit*) ((AliasUnit*) u2)->firstBaseUnit();
		if(u == u2) return true;
		if(u2->subtype() != SUBTYPE_ALIAS_UNIT) return false;
	}
	return false;
}
bool AliasUnit::isParentOf(Unit *u) const {
	if(u == this) return false;
	if(u->baseUnit() != baseUnit()) return false;
	while(1) {
		if(u->subtype() != SUBTYPE_ALIAS_UNIT) return false;
		u = ((AliasUnit*) u)->firstBaseUnit();
		if(u == this) return true;
	}
	return false;
}
bool AliasUnit::hasComplexExpression() const {
	return svalue.find("\\x") != string::npos;
}
bool AliasUnit::hasComplexRelationTo(Unit *u) const {
	if(u == this || u->baseUnit() != baseUnit()) return false;
	if(isParentOf(u)) {
		Unit *fbu = u;
		while(true) {
			if((const Unit*) fbu == this) return false;
			if(((AliasUnit*) fbu)->hasComplexExpression()) return true;
			if(fbu->subtype() != SUBTYPE_ALIAS_UNIT) return false;
			fbu = (Unit*) ((AliasUnit*) fbu)->firstBaseUnit();			
		}	
	} else if(isChildOf(u)) {
		Unit *fbu = (Unit*) this;
		if(fbu->subtype() != SUBTYPE_ALIAS_UNIT) return false;
		while(true) {
			if((const Unit*) fbu == u) return false;
			if(((AliasUnit*) fbu)->hasComplexExpression()) return true;
			if(fbu->subtype() != SUBTYPE_ALIAS_UNIT) return false;
			fbu = (Unit*) ((AliasUnit*) fbu)->firstBaseUnit();
		}			
	} else {
		return hasComplexRelationTo(baseUnit()) || u->hasComplexRelationTo(u->baseUnit());
	}
}

AliasUnit_Composite::AliasUnit_Composite(Unit *alias, int exp, Prefix *prefix_) : AliasUnit("", alias->name(), alias->plural(false), alias->singular(false), "", alias, "", exp, "") {
	prefixv = (Prefix*) prefix_;
}
AliasUnit_Composite::AliasUnit_Composite(const AliasUnit_Composite *unit) {
	set(unit);
}
AliasUnit_Composite::~AliasUnit_Composite() {}
ExpressionItem *AliasUnit_Composite::copy() const {
	return new AliasUnit_Composite(this);
}
void AliasUnit_Composite::set(const ExpressionItem *item) {
	if(item->type() == TYPE_UNIT) {
		if(((Unit*) item)->subtype() == SUBTYPE_ALIAS_UNIT) {
			AliasUnit::set(item);
			prefixv = (Prefix*) ((AliasUnit_Composite*) item)->prefix();
		} else {
			Unit::set(item);
		}
	} else {
		ExpressionItem::set(item);
	}
}
string AliasUnit_Composite::print(bool plural_, bool short_, bool use_unicode, bool (*can_display_unicode_string_function) (const char*, void*), void *can_display_unicode_string_arg) const {
	string str = "";
	if(prefixv) {
		str += prefixv->name(short_, use_unicode, can_display_unicode_string_function, can_display_unicode_string_arg);
	}
	str += preferredName(short_, use_unicode, plural_, false, can_display_unicode_string_function, can_display_unicode_string_arg).name;
	return str;
}
Prefix *AliasUnit_Composite::prefix() const {
	return prefixv;
}
int AliasUnit_Composite::prefixExponent() const {
	if(prefixv && prefixv->type() == PREFIX_DECIMAL) return ((DecimalPrefix*) prefixv)->exponent();
	if(prefixv && prefixv->type() == PREFIX_BINARY) return ((BinaryPrefix*) prefixv)->exponent();
	return 0;
}
void AliasUnit_Composite::set(Unit *u, int exp, Prefix *prefix_) {
	setBaseUnit(u);
	setExponent(exp);
	prefixv = (Prefix*) prefix_;
}
MathStructure &AliasUnit_Composite::convertToFirstBaseUnit(MathStructure &mvalue, MathStructure &mexp) const {
	if(prefixv) {
		MathStructure *mstruct = new MathStructure(prefixv->value());
		if(!mexp.isOne()) mstruct->raise(mexp);
		mvalue.multiply_nocopy(mstruct, true);
	}
	if(i_exp != 1) mexp.multiply(i_exp);
	return mvalue;
}
MathStructure &AliasUnit_Composite::convertFromFirstBaseUnit(MathStructure &mvalue, MathStructure &mexp) const {
	if(i_exp != 1) mexp /= i_exp;
	if(prefixv) {
		MathStructure *mstruct = new MathStructure(prefixv->value());
		if(!mexp.isOne()) mstruct->raise(mexp);
		mvalue.divide_nocopy(mstruct, true);
	}
	return mvalue;
}

CompositeUnit::CompositeUnit(string cat_, string name_, string title_, string base_expression_, bool is_local, bool is_builtin, bool is_active) : Unit(cat_, name_, "", "", title_, is_local, is_builtin, is_active) {
	setBaseExpression(base_expression_);
	setChanged(false);
}
CompositeUnit::CompositeUnit(const CompositeUnit *unit) {
	set(unit);
}
CompositeUnit::~CompositeUnit() {
	clear();
}
ExpressionItem *CompositeUnit::copy() const {
	return new CompositeUnit(this);
}
void CompositeUnit::set(const ExpressionItem *item) {
	if(item->type() == TYPE_UNIT) {
		Unit::set(item);
		if(((Unit*) item)->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			CompositeUnit *cu = (CompositeUnit*) item;
			for(size_t i = 1; i <= cu->countUnits(); i++) {
				int exp = 1; Prefix *p = NULL;
				Unit *u = cu->get(i, &exp, &p);
				units.push_back(new AliasUnit_Composite(u, exp, p));
			}
		}
	} else {
		ExpressionItem::set(item);
	}
}
void CompositeUnit::add(Unit *u, int exp, Prefix *prefix) {
	bool b = false;
	for(size_t i = 0; i < units.size(); i++) {
		if(exp > units[i]->firstBaseExponent()) {
			units.insert(units.begin() + i, new AliasUnit_Composite(u, exp, prefix));
			b = true;
			break;
		}
	}
	if(!b) {
		units.push_back(new AliasUnit_Composite(u, exp, prefix));
	}
}
Unit *CompositeUnit::get(size_t index, int *exp, Prefix **prefix) const {
	if(index > 0 && index <= units.size()) {
		if(exp) *exp = units[index - 1]->firstBaseExponent();
		if(prefix) *prefix = (Prefix*) units[index - 1]->prefix();
		return (Unit*) units[index - 1]->firstBaseUnit();
	}
	return NULL;
}
void CompositeUnit::setExponent(size_t index, int exp) {
	if(index > 0 && index <= units.size()) {
		bool b = exp > units[index - 1]->firstBaseExponent();
		units[index - 1]->setExponent(exp);
		if(b) {
			for(size_t i = 0; i < index - 1; i++) {
				if(exp > units[i]->firstBaseExponent()) {
					AliasUnit_Composite *u = units[index - 1];
					units.erase(units.begin() + (index - 1));
					units.insert(units.begin() + i, u);
					break;
				}
			}
		} else {
			for(size_t i = units.size() - 1; i > index - 1; i--) {
				if(exp < units[i]->firstBaseExponent()) {
					AliasUnit_Composite *u = units[index - 1];
					units.insert(units.begin() + i, u);
					units.erase(units.begin() + (index - 1));
					break;
				}
			}
		}
	}
}
void CompositeUnit::setPrefix(size_t index, Prefix *prefix) {
	if(index > 0 && index <= units.size()) {
		units[index - 1]->set(units[index - 1]->firstBaseUnit(), units[index - 1]->firstBaseExponent(), prefix);
	}
}
size_t CompositeUnit::countUnits() const {
	return units.size();
}
size_t CompositeUnit::find(Unit *u) const {
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->firstBaseUnit() == u) {
			return i + 1;
		}
	}
	return 0;
}
void CompositeUnit::del(size_t index) {
	if(index > 0 && index <= units.size()) {
		delete units[index - 1];
		units.erase(units.begin() + (index - 1));
	}
}
string CompositeUnit::print(bool plural_, bool short_, bool use_unicode, bool (*can_display_unicode_string_function) (const char*, void*), void *can_display_unicode_string_arg) const {
	string str = "";
	bool b = false, b2 = false;
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->firstBaseExponent() != 0) {
			if(!b && units[i]->firstBaseExponent() < 0 && i > 0) {
				str += "/";
				b = true;
				if(i < units.size() - 1) {
					b2 = true;
					str += "(";
				}				
			} else {
//				if(i > 0) str += "*";
				if(i > 0) str += " ";
			}
			if(plural_ && i == 0 && units[i]->firstBaseExponent() > 0) {
				str += units[i]->print(true, short_, use_unicode, can_display_unicode_string_function, can_display_unicode_string_arg);
			} else {
				str += units[i]->print(false, short_, use_unicode, can_display_unicode_string_function, can_display_unicode_string_arg);
			}
			if(b) {
				if(units[i]->firstBaseExponent() != -1) {
					str += "^";
					str += i2s(-units[i]->firstBaseExponent());
				}
			} else {
				if(units[i]->firstBaseExponent() != 1) {
					str += "^";
					str += i2s(units[i]->firstBaseExponent());
				}
			}
		}
	}
	if(b2) str += ")";
	return str;
}
int CompositeUnit::subtype() const {
	return SUBTYPE_COMPOSITE_UNIT;
}
bool CompositeUnit::containsRelativeTo(Unit *u) const {
	if(u == this) return false;
	CompositeUnit *cu;
	for(size_t i = 0; i < units.size(); i++) {
		if(u == units[i] || u->baseUnit() == units[i]->baseUnit()) return true;
		if(units[i]->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
			cu = (CompositeUnit*) units[i]->baseUnit();
			if(cu->containsRelativeTo(u)) return true;
		}
	}
	if(u->baseUnit()->subtype() == SUBTYPE_COMPOSITE_UNIT) {
		cu = (CompositeUnit*) u->baseUnit();
		for(size_t i = 1; i <= cu->countUnits(); i++) {
			if(containsRelativeTo(cu->get(i)->baseUnit())) return true;
		}
		return false;
	}	
	return false;
}
MathStructure CompositeUnit::generateMathStructure(bool make_division) const {
	MathStructure mstruct;
	bool has_p = false;
	for(size_t i = 0; i < units.size(); i++) {
		if(units[i]->prefix()) {
			has_p = true;
			break;
		}
	}
	MathStructure mden;
	for(size_t i = 0; i < units.size(); i++) {
		MathStructure mstruct2;
		if(!has_p || units[i]->prefix()) {
			mstruct2.set(units[i]->firstBaseUnit(), units[i]->prefix());
		} else {				
			mstruct2.set(units[i]->firstBaseUnit(), CALCULATOR->decimal_null_prefix);
		}
		if(make_division && units[i]->firstBaseExponent() < 0) {
			if(units[i]->firstBaseExponent() != -1) {
				mstruct2 ^= -units[i]->firstBaseExponent();
			}
		} else if(units[i]->firstBaseExponent() != 1) {
			mstruct2 ^= units[i]->firstBaseExponent();
		}
		if(i == 0) {
			if(make_division && units[i]->firstBaseExponent() < 0) {
				mstruct = 1;
				mden = mstruct2;
			} else {
				mstruct = mstruct2;
			}
		} else if(make_division && units[i]->firstBaseExponent() < 0) {
			if(mden.isZero()) {
				mden = mstruct2;
			} else {
				mden *= mstruct2;
			}
		} else {
			mstruct *= mstruct2;
		}
	}
	if(make_division && !mden.isZero()) {
		mstruct.transform(STRUCT_DIVISION, mden);
	}
	return mstruct;
}
void CompositeUnit::setBaseExpression(string base_expression_) {
	clear();
	if(base_expression_.empty()) {
		setChanged(true);
		return;
	}
	EvaluationOptions eo;
	eo.approximation = APPROXIMATION_EXACT;
	eo.sync_units = false;
	eo.keep_prefixes = true;
	eo.structuring = STRUCTURING_NONE;
	eo.reduce_divisions = false;
	ParseOptions po;
	po.variables_enabled = false;
	po.functions_enabled = false;
	po.unknowns_enabled = false;
	MathStructure mstruct;
	bool had_errors = false;
	CALCULATOR->beginTemporaryStopMessages();
	CALCULATOR->parse(&mstruct, base_expression_, po);
	mstruct.eval(eo);
	if(CALCULATOR->endTemporaryStopMessages() > 0) had_errors = true;
	if(mstruct.isUnit()) {
		add(mstruct.unit(), 1, mstruct.prefix());
	} else if(mstruct.isPower() && mstruct[0].isUnit() && mstruct[1].isInteger()) {
		add(mstruct[0].unit(), mstruct[1].number().intValue(), mstruct[0].prefix());
	} else if(mstruct.isMultiplication()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isUnit()) {
				add(mstruct[i].unit(), 1, mstruct[i].prefix());
			} else if(mstruct[i].isPower() && mstruct[i][0].isUnit() && mstruct[i][1].isInteger()) {
				add(mstruct[i][0].unit(), mstruct[i][1].number().intValue(), mstruct[i][0].prefix());
			} else {
				had_errors = true;
			}
		}
	} else {
		had_errors = true;
	}
	if(had_errors) CALCULATOR->error(false, _("Error(s) in unitexpression."), NULL);
	setChanged(true);
}
void CompositeUnit::clear() {
	for(size_t i = 0; i < units.size(); i++) {
		delete units[i];
	}
	units.clear();
}
