#include "mewargs"
#include "asm.hpp"

#ifdef _WIN32

#include <windows.h>
#include <libloaderapi.h>

#endif

#define HELP_PAGE \
	"Usage: nanasm <input> -o <output>\n" \
	"Flags:\n"\
	"-rn -- run no output file"



int main(int argc, char** argv) {
	mew::args __args(argc, argv);
	__args.normalize();
	
	if (
		!__args.has_needs(1) ||
		__args.has("-h") ||
		__args.has("--help")
	) {
		printf(HELP_PAGE); exit(0);
	}

#include "mewpack"
struct _Flags {
	bytepartf(RunOnly)
	bytepartf(Debug)
} _flags;
#include "mewpop"
	_flags.RunOnly = __args.has("-rn");
	_flags.Debug = __args.has("-d") || __args.has("--debug");

	const char* output_name = __args.getNextOr("-o", "./temp.nb");
	// const char* input_name  = "D:/so2u/GITHUB/cuba/nan/nuna/nanbase/test-asm/test.ns";
	const char* input_name  = __args.getNextPath();

	if (!input_name) {
		printf(HELP_PAGE); exit(1);
	}

	auto comp = nanasm::Compiler::compile(input_name);
	if (_flags.RunOnly) {
		Virtual::VirtualMachine vm;
		Virtual::Code* code = *comp;
		int exit_code = Virtual::Execute(vm, *code);
		return exit_code; 
	} else {
		comp.save(output_name);
	}
	// if (flags.debug) {
	// 	comp.debugSave(output_name);
	return 0;	
}