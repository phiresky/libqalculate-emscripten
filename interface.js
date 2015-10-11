var qalc_load_file = cwrap("qalc_load_file", null, ["string"]);
var _qalc = cwrap("qalc", "number", ["string"]);

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
