/*
    Qalculate    

    Copyright (C) 2003-2007  Niklas Knutsson (nq@altern.org)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "support.h"

#include "util.h"
#include <stdarg.h>
#include "Number.h"

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <chrono>
#include <ctime>


bool eqstr::operator()(const char *s1, const char *s2) const {
	return strcmp(s1, s2) == 0;
}

char buffer[20000];

string date2s(int year, int month, int day) {
	string str = i2s(year);
	str += "-";
	if(month < 10) {
		str += "0";
	}
	str += i2s(month);
	str += "-";
	if(day < 10) {
		str += "0";
	}
	str += i2s(day);
	return str;
}
using chro = std::chrono::system_clock;
bool s2date(string str, chro::time_point *gtime) {
	std::tm time;
	if(strptime(str.c_str(), "%x", &time) || strptime(str.c_str(), "%Ex", &time) || strptime(str.c_str(), "%Y-%m-%d", &time) || strptime(str.c_str(), "%m/%d/%Y", &time) || strptime(str.c_str(), "%m/%d/%y", &time)) {
		*gtime = chro::from_time_t(mktime(&time));
		return true;
	}
	return false;
}

void now(int &hour, int &min, int &sec) {
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);
	hour = lt->tm_hour;
	min = lt->tm_min;
	sec = lt->tm_sec;
}
void tm2ints(tm& lt, int &year, int &month, int &day) {
	year = lt.tm_year + 1900;
	month = lt.tm_mon + 1;
	day = lt.tm_mday;
}
void today(int &year, int &month, int &day) {
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);
	tm2ints(*lt, year, month, day);
}

chro::time_point ints2date(int year, int month, int day) {
	tm time;
	time.tm_year = year - 1900;
	time.tm_mon = month -1;
	time.tm_mday = day;
	return chro::from_time_t(mktime(&time));
}
void date2ints(chro::time_point& d, int& year, int& month, int& day) {
	time_t t = chro::to_time_t(d);
	tm* lt = localtime(&t);
	tm2ints(*lt, year, month, day);
}
bool addDays(chro::time_point *gtime, int days) {
	*gtime += chrono::hours(24) * days;
	return true;
}
bool addDays(int &year, int &month, int &day, int days) {
	chro::time_point p = ints2date(year,month,day);
	p += chrono::hours(days*24);
	date2ints(p, year, month, day);
	return true;
}
string addDays(string str, int days) {
	chro::time_point gtime;
	if(!s2date(str, &gtime) || !addDays(&gtime, days)) {
		return empty_string;
	}
	int y;
	int m;
	int d;
	date2ints(gtime, y, m, d);
	return date2s(y, m, d);
}
//TODO:these
bool addMonths(chro::time_point *gtime, int months) {
	return false;
}
bool addMonths(int &year, int &month, int &day, int months) {
	return false;
	//TODO
}
string addMonths(string str, int months) {
	//TODO
	return empty_string;
}
bool addYears(chro::time_point *gtime, int years) {
	return false;
}
bool addYears(int &year, int &month, int &day, int years) {
	return false;
}
string addYears(string str, int years) {
	return empty_string;
}

int week(string str, bool start_sunday) {
	return 0;
}
int weekday(string str) {
	return 0;
}
int yearday(string str) {
	return 0;
}

bool s2date(string str, int &year, int &month, int &day) {
	chro::time_point t;
	if(!s2date(str, &t)) return false;
	date2ints(t, year, month, day);
	return true;
}
bool isLeapYear(int year) {
	return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
int daysPerYear(int year, int basis) {
	switch(basis) {
		case 0: {
			return 360;
		}
		case 1: {
			if(isLeapYear(year)) {
				return 366;
			} else {
				return 365;
			}
		}
		case 2: {
			return 360;
		}		
		case 3: {
			return 365;
		} 
		case 4: {
			return 360;
		}
	}
	return -1;
}

int daysPerMonth(int month, int year) {
	switch(month) {
		case 1: {} case 3: {} case 5: {} case 7: {} case 8: {} case 10: {} case 12: {
			return 31;
		}
		case 2:	{
			if(isLeapYear(year)) return 29;
			else return 28;
		}				
		default: {
			return 30;
		}
	}
}	

Number yearsBetweenDates(string date1, string date2, int basis, bool date_func) {
	if(basis < 0 || basis > 4) return -1;
	if(basis == 1) {
		int day1, day2, month1, month2, year1, year2;
		if(!s2date(date1, year1, month1, day1)) {
  			return -1;
		}
		if(!s2date(date2, year2, month2, day2)) {
  			return -1;
		}		
		if(year1 > year2 || (year1 == year2 && month1 > month2) || (year1 == year2 && month1 == month2 && day1 > day2)) {
			int year3 = year1, month3 = month1, day3 = day1;
			year1 = year2; month1 = month2; day1 = day2;
			year2 = year3; month2 = month3; day2 = day3;		
		}		
		int days = 0;
		if(year1 == year2) {
			days = daysBetweenDates(year1, month1, day1, year2, month2, day2, basis, date_func);
			if(days < 0) return -1;
			return Number(days, daysPerYear(year1, basis));
		}
		for(int month = 12; month > month1; month--) {
			days += daysPerMonth(month, year1);
		}
		days += daysPerMonth(month1, year1) - day1 + 1;
/*		Number *nr = new Number(days, daysPerYear(year1, basis));
		year1++;
		if(year1 != year2) {
			Number yfr(year2 - year1);
			nr->add(&yfr);
		}
		days = 0;*/
		for(int month = 1; month < month2; month++) {
			days += daysPerMonth(month, year2);
		}
		days += day2 - 1;
		int days_of_years = 0;
		for(int year = year1; year <= year2; year++) {
			days_of_years += daysPerYear(year, basis);
			if(year != year1 && year != year2) {
				days += daysPerYear(year, basis);
			}
		}
		Number year_frac(days_of_years, year2 + 1 - year1);
/*		if(days > 0) {
			Number nr2(days, daysPerYear(year2, basis));
			nr->add(&nr2);
		}*/
		Number nr(days);
		nr /= year_frac;
		return nr;
	} else {
		int days = daysBetweenDates(date1, date2, basis, date_func);
		if(days < 0) return -1;
		return Number(days, daysPerYear(0, basis));	
	}
	return -1;
}
int daysBetweenDates(string date1, string date2, int basis, bool date_func) {
	int day1, day2, month1, month2, year1, year2;
	if(!s2date(date1, year1, month1, day1)) {
  		return -1;
	}
	if(!s2date(date2, year2, month2, day2)) {
  		return -1;
	}
	return daysBetweenDates(year1, month1, day1, year2, month2, day2, basis, date_func);	
}
int daysBetweenDates(int year1, int month1, int day1, int year2, int month2, int day2, int basis, bool date_func) {
	if(basis < 0 || basis > 4) return -1;
	bool isleap = false;
	int days, months, years;

	if(year1 > year2 || (year1 == year2 && month1 > month2) || (year1 == year2 && month1 == month2 && day1 > day2)) {
		int year3 = year1, month3 = month1, day3 = day1;
		year1 = year2; month1 = month2; day1 = day2;
		year2 = year3; month2 = month3; day2 = day3;		
	}

	years = year2  - year1;
	months = month2 - month1 + years * 12;
	days = day2 - day1;

	isleap = isLeapYear(year1);

	switch(basis) {
		case 0: {
			if(date_func) {
				if(month1 == 2 && ((day1 == 28 && !isleap) || (day1 == 29 && isleap)) && !(month2 == month1 && day1 == day2 && year1 == year2)) {
					if(isleap) return months * 30 + days - 1;
					else return months * 30 + days - 2;
				}
				if(day1 == 31 && day2 < 31) days++;
			} else {
				if(month1 == 2 && month2 != 2 && year1 == year2) {
					if(isleap) return months * 30 + days - 1;
					else return months * 30 + days - 2;
				}			
			}
			return months * 30 + days;
		}
		case 1: {}
		case 2: {}		
		case 3: {
			int month4 = month2;
			bool b;
			if(years > 0) {
				month4 = 12;
				b = true;
			} else {
				b = false;
			}
			for(; month1 < month4 || b; month1++) {
				if(month1 > month4 && b) {
					b = false;
					month1 = 1;
					month4 = month2;
					if(month1 == month2) break;
				}
				if(!b) {
					days += daysPerMonth(month1, year2);
				} else {
					days += daysPerMonth(month1, year1);
				}
			}
			if(years == 0) return days;
			//if(basis == 1) {
				for(year1 += 1; year1 < year2; year1++) {
					if(isLeapYear(year1)) days += 366;
					else days += 365;
				} 
				return days;
			//}
			//if(basis == 2) return (years - 1) * 360 + days;		
			//if(basis == 3) return (years - 1) * 365 + days;
		} 
		case 4: {
			if(date_func) {
				if(day2 == 31 && day1 < 31) days--;
				if(day1 == 31 && day2 < 31) days++;
			}
			return months * 30 + days;
		}
	}
	return -1;
	
}

string& gsub(const string &pattern, const string &sub, string &str) {
	size_t i = str.find(pattern);
	while(i != string::npos) {
		str.replace(i, pattern.length(), sub);
		i = str.find(pattern, i + sub.length());
	}
	return str;
}
string& gsub(const char *pattern, const char *sub, string &str) {
	size_t i = str.find(pattern);
	while(i != string::npos) {
		str.replace(i, strlen(pattern), string(sub));
		i = str.find(pattern, i + strlen(sub));
	}
	return str;
}

string& remove_blanks(string &str) {
	size_t i = str.find_first_of(SPACES, 0);
	while(i != string::npos) {
		str.erase(i, 1);
		i = str.find_first_of(SPACES, i);
	}
	return str;
}

string& remove_duplicate_blanks(string &str) {
	size_t i = str.find_first_of(SPACES, 0);
	while(i != string::npos) {
		if(i == 0 || is_in(SPACES, str[i - 1])) {
			str.erase(i, 1);
		} else {
			i++;
		}
		i = str.find_first_of(SPACES, i);
	}
	return str;
}

string& remove_blank_ends(string &str) {
	size_t i = str.find_first_not_of(SPACES);
	size_t i2 = str.find_last_not_of(SPACES);
	if(i != string::npos && i2 != string::npos) {
		if(i > 0 || i2 < str.length() - 1) {
			str = str.substr(i, i2 - i + 1);
		}
	} else {
		str.resize(0);
	}
	return str;
}
string& remove_parenthesis(string &str) {
	if(str[0] == LEFT_PARENTHESIS_CH && str[str.length() - 1] == RIGHT_PARENTHESIS_CH) {
		str = str.substr(1, str.length() - 2);
		return remove_parenthesis(str);
	}
	return str;
}

string d2s(double value, int precision) {
	//	  qgcvt(value, precision, buffer);
	sprintf(buffer, "%.*G", precision, value);
	string stmp = buffer;
	//	  gsub("e", "E", stmp);
	return stmp;
}

string p2s(void *o) {
	sprintf(buffer, "%p", o);
	string stmp = buffer;
	return stmp;
}
string i2s(int value) {
	//	  char buffer[10];
	sprintf(buffer, "%i", value);
	string stmp = buffer;
	return stmp;
}
string i2s(long int value) {
	sprintf(buffer, "%li", value);
	string stmp = buffer;
	return stmp;
}
string i2s(unsigned int value) {
	sprintf(buffer, "%u", value);
	string stmp = buffer;
	return stmp;
}
string i2s(unsigned long int value) {
	sprintf(buffer, "%lu", value);
	string stmp = buffer;
	return stmp;
}
const char *b2yn(bool b, bool capital) {
	if(capital) {
		if(b) return _("Yes");
		return _("No");
	}
	if(b) return _("yes");
	return _("no");
}
const char *b2tf(bool b, bool capital) {
	if(capital) {
		if(b) return _("True");
		return _("False");
	}
	if(b) return _("true");
	return _("false");
}
const char *b2oo(bool b, bool capital) {
	if(capital) {
		if(b) return _("On");
		return _("Off");
	}
	if(b) return _("on");
	return _("off");
}
int s2i(const string& str) {
	return strtol(str.c_str(), NULL, 10);
}
int s2i(const char *str) {
	return strtol(str, NULL, 10);
}
void *s2p(const string& str) {
	void *p;
	sscanf(str.c_str(), "%p", &p);
	return p;
}
void *s2p(const char *str) {
	void *p;
	sscanf(str, "%p", &p);
	return p;
}

size_t find_ending_bracket(const string &str, size_t start, int *missing) {
	int i_l = 1;
	while(true) {
		start = str.find_first_of(LEFT_PARENTHESIS RIGHT_PARENTHESIS, start);
		if(start == string::npos) {
			if(missing) *missing = i_l;
			return string::npos;
		}
		if(str[start] == LEFT_PARENTHESIS_CH) {
			i_l++;
		} else {
			i_l--;
			if(!i_l) {
				if(missing) *missing = i_l;
				return start;
			}
		}
		start++;
	}
}

char op2ch(MathOperation op) {
	switch(op) {
		case OPERATION_ADD: return PLUS_CH;
		case OPERATION_SUBTRACT: return MINUS_CH;		
		case OPERATION_MULTIPLY: return MULTIPLICATION_CH;		
		case OPERATION_DIVIDE: return DIVISION_CH;		
		case OPERATION_RAISE: return POWER_CH;		
		case OPERATION_EXP10: return EXP_CH;
		default: return ' ';		
	}
}

string& wrap_p(string &str) {
	str.insert(str.begin(), 1, LEFT_PARENTHESIS_CH);
	str += RIGHT_PARENTHESIS_CH;
	return str;
}

bool is_in(const char *str, char c) {
	for(size_t i = 0; i < strlen(str); i++) {
		if(str[i] == c)
			return true;
	}
	return false;
}
bool is_not_in(const char *str, char c) {
	for(size_t i = 0; i < strlen(str); i++) {
		if(str[i] == c)
			return false;
	}
	return true;
}
bool is_in(const string &str, char c) {
	for(size_t i = 0; i < str.length(); i++) {
		if(str[i] == c)
			return true;
	}
	return false;
}
bool is_not_in(const string &str, char c) {
	for(size_t i = 0; i < str.length(); i++) {
		if(str[i] == c)
			return false;
	}
	return true;
}

int sign_place(string *str, size_t start) {
	size_t i = str->find_first_of(OPERATORS, start);
	if(i != string::npos)
		return i;
	else
		return -1;
}

int gcd(int i1, int i2) {
	if(i1 < 0) i1 = -i1;
	if(i2 < 0) i2 = -i2;
	if(i1 == i2) return i2;
	int i3;
	if(i2 > i1) {
		i3 = i2;
		i2 = i1;
		i1 = i3;
	}
	while((i3 = i1 % i2) != 0) {
		i1 = i2;
		i2 = i3;
	}
	return i2;
}

size_t unicode_length(const string &str) {
	size_t l = str.length(), l2 = 0;
	for(size_t i = 0; i < l; i++) {
		if(str[i] > 0 || (unsigned char) str[i] >= 0xC2) {
			l2++;
		}
	}
	return l2;
}
size_t unicode_length(const char *str) {
	size_t l = strlen(str), l2 = 0;
	for(size_t i = 0; i < l; i++) {
		if(str[i] > 0 || (unsigned char) str[i] >= 0xC2) {
			l2++;
		}
	}
	return l2;
}

bool text_length_is_one(const string &str) {
	if(str.empty()) return false;
	if(str.length() == 1) return true;
	if(str[0] >= 0) return false;
	for(size_t i = 1; i < str.length(); i++) {
		if(str[i] > 0 || (unsigned char) str[i] >= 0xC2) {
			return false;
		}
	}
	return true;
}

bool insens_comp_char (char c1, char c2) { return std::tolower(c1)<std::tolower(c2);}
bool equalsIgnoreCase(const string &str1, const string &str2) {
	return std::lexicographical_compare(str1.begin(), str1.end(), str2.begin(), str2.end(), insens_comp_char);
}

bool equalsIgnoreCase(const string &str1, const char *str2) {
	return equalsIgnoreCase(str1, string(str2));
}

void parse_qalculate_version(string qalculate_version, int *qalculate_version_numbers) {
	for(size_t i = 0; i < 3; i++) {
		size_t dot_i = qalculate_version.find(".");
		if(dot_i == string::npos) {
			qalculate_version_numbers[i] = s2i(qalculate_version);
			break;
		}
		qalculate_version_numbers[i] = s2i(qalculate_version.substr(0, dot_i));
		qalculate_version = qalculate_version.substr(dot_i + 1, qalculate_version.length() - (dot_i + 1));
	}
}

string getLocalDir() {
	string homedir = "";
	struct passwd *pw = getpwuid(getuid());
	if(pw) {
		homedir = pw->pw_dir;
		homedir += "/";
	}
	homedir += ".qalculate/";
	return homedir;
}

