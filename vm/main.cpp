// #define MEW_NOTUSE_THROWS
#include <iostream>
#include "mewall"
#include "virtual.hpp"
#include "mewcolors"
#ifdef _WIN32

#include <windows.h>
#include <libloaderapi.h>

#endif

// #include <stdafx.h>
#include <iostream>
using namespace std;
#if       _WIN32_WINNT < 0x0500
  #undef  _WIN32_WINNT
  #define _WIN32_WINNT   0x0500
#endif
#ifdef _WIN32
	#include <windows.h>
	#include "Wincon.h"
#endif

#define HELP_PAGE \
	"Usage:\n" \
	Italic BRIGHT("> ./nanvm <path/to/file>\n") \
	"Flags:\n" \
	Italic BRIGHT(\
		"-h, --help\tShow this help page\n" \
		"--version\tDisplay current vm version\n"\
		"--get_test\tGenerate hellow word file\n"\
	) 

int main(int argc, char** argv) {
	mew::args __args(argc, argv);
	__args.normalize();

	if (
		!__args.has_needs(1)   ||
		__args.has("-h")      ||
		__args.has("--help")
	) {
		printf(HELP_PAGE); exit(0);
	}

	if (__args.has("--version") || __args.has("--versions")) {
		printf(
			"NAN VIRTUAL MACHINE version '%ld'\n"
			Italic BRIGHT(
				"  prod by 'nansotu studio'(C) dev so2u\n"
			)
			, Virtual::GetVersion());
		exit(0);
	}

	if (__args.has("--get_test")) {
		return !Tests::test_Virtual();
	}

	const char* path = __args.getNextPath();
	MewUserAssert(mew::is_exists(path),"path is not exsist");
	Virtual::VirtualMachine vm;
	Virtual::Code* code = Virtual::Code_LoadFromFile(path);
	// vm.hdlls = hdlls;
	int exit_code = Virtual::Execute(vm, *code);

#ifdef _WIN32
	HWND consoleWnd = GetConsoleWindow();
	DWORD dwProcessId;
	GetWindowThreadProcessId(consoleWnd, &dwProcessId);
	if (GetCurrentProcessId()==dwProcessId) {
		printf("\n");
		system("pause");
	}	
#endif
	return exit_code;
}