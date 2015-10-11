var qalc_load_file = cwrap("qalc_load_file", null, ["string"]);
var _qalc = cwrap("qalc", "number", ["string"]);
var qalc_toload = ["data/prefixes.xml",
		"data/currencies.xml",
		"data/units.xml",
		"data/functions.xml",
		"data/datasets.xml",
		"data/variables.xml"];

function qalc_load_next() {
	var x = qalc_toload.shift();
	console.log(x);
	qalc_load_file(x);
}

function qalc_load_all_files() {
	while(qalc_toload.length > 0) qalc_load_next();
}

function qalc(inp) {
	var addr = _qalc(inp);
	var oup = Pointer_stringify(addr);
	_free(addr);
	return oup;
}

Module['noExitRuntime'] = true;
Module['_main'] = function() {
	qalc_load_file("data/units.xml");
	return 0;
}
