/* A very, very hacked together test framework. */

#include <csetjmp>
#include <csignal>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <cstring>
#include <climits>
#include <vector>
#ifdef _WIN32
#	include <Windows.h>
#else
#	include <sys/ioctl.h>
#endif

#ifndef _TESTS_WIDTH
#	define _TESTS_WIDTH 40
#endif

namespace _tests {
static int _total_assertions = 0;
static int _successful_assertions = 0;
static bool _failure = false;
static bool _total_failure = false;
static jmp_buf _assert_buf;
static const char *_last_file;
static unsigned long _last_line;
static bool _needs_nl = false;
static bool _selective_tests = false;
static std::set<std::string> _enabled_tests;

template< typename T >
inline T imax(T a, T b) {
	/* https://stackoverflow.com/questions/514435 */
	T mask = a - b;
	mask = mask >> ((sizeof(T) * CHAR_BIT) - 1);
	return a + ((b - a) & mask);
}

static bool _show_all() {
	static int show = -1;
	if (show == -1) {
		const char *v = getenv("VERBOSE");
		show = v && std::string(v) != "0";
	}

	return show == 1;
}

static long _term_width() {
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &csbi);
	return csbi.dwSize.X <= 0 ? 80 : csbi.dwSize.X;
#else
	struct winsize size;
	ioctl(2, TIOCGWINSZ, &size);
	return size.ws_col <= 0 ? 80 : size.ws_col;
#endif
}

static void _clear_line() {
	std::cerr << "\r" << std::string(_term_width(), ' ') << "\r";
}

static void _assert(bool cond, std::string msg, const char *file, unsigned long line, bool cont) {
	++_total_assertions;
	_last_file = file;
	_last_line = line;

	char spacebuf[_TESTS_WIDTH];
	memset(spacebuf, ' ', sizeof(spacebuf));
	spacebuf[imax(((signed)sizeof(spacebuf) - 1) - (signed)msg.length(), 0)] = 0;

	if (cond) {
		if (!_show_all()) _clear_line();
		++_successful_assertions;
		std::cerr << "\x1b[32;1m│      ok:\x1b[m " << msg;
		std::cerr << spacebuf;
		std::cerr << "\x1b[2m " << file << ":" << line << "\x1b[m";
		if (_show_all()) {
			std::cerr << std::endl;
		} else {
			_needs_nl = true;
		}
	} else {
		if (_needs_nl) _clear_line();
		std::cerr << "\x1b[31;1m│   fault:\x1b[m " << msg;
		std::cerr << spacebuf;
		std::cerr << "\x1b[2;91m " << file << ":" << line << "\x1b[m" << std::endl;
		_needs_nl = false;
		_failure = true;
		if (!cont) {
			longjmp(_assert_buf, 1);
		}
	}
}

static void _on_signal(int sig) {
	std::string msg(strsignal(sig));
	msg = "caught signal: " + msg;

	char spacebuf[_TESTS_WIDTH];
	memset(spacebuf, ' ', sizeof(spacebuf));
	spacebuf[imax(((signed)sizeof(spacebuf) - 1) - (signed)msg.length(), 0)] = 0;

	if (_needs_nl) _clear_line();
	if (_last_file) {
		std::cerr << "\x1b[31;1m│   fault:\x1b[m " << msg;
		std::cerr << spacebuf;
		std::cerr << "\x1b[2;91m " << _last_file << ":" << _last_line;
		std::cerr << " \x1b[33mNEARBY\x1b[39m" << "\x1b[m" << std::endl;
	} else {
		std::cerr << "\x1b[31;1m│   fault:\x1b[m " << msg;
		std::cerr << spacebuf;
		std::cerr << "\x1b[2;91m <unknown location>\x1b[m" << std::endl;
	}
	_failure = true;
	_needs_nl = false;
	longjmp(_assert_buf, 1);
}

class _test_prep {
	friend class _test_harness;
	static std::deque<void(*)()> preps;
public:
	_test_prep(void (*fn)()) {
		preps.push_back(fn);
	}
};

class _test_harness {
	typedef void (*testfn)();
	static std::vector<_test_harness*> tests;
	testfn fn;
	const char *name;
	const char *file;
	unsigned long line;
	bool should_fail;
public:
	_test_harness(testfn _fn, const char *_name, bool _should_fail, const char *_file, unsigned long _line)
	: fn(_fn), name(_name), file(_file), line(_line), should_fail(_should_fail) {
		(void) _assert;
		tests.push_back(this);
	}

	static void run_all() {
		for (auto th : tests) {
			if (_selective_tests) {
				if (_enabled_tests.size() == 0) break;
				auto itr = _enabled_tests.find(th->name);
				if (itr == _enabled_tests.end()) {
					continue;
				} else {
					_enabled_tests.erase(itr);
				}
			}

			++_total_assertions;
			std::cerr << "\x1b[1m" << th->name << "\x1b[m";
			if (th->should_fail) std::cerr << " \x1b[2m(must fail)\x1b[m";
			std::cerr << std::endl;

			_failure = false;
			_needs_nl = false;
			if (!setjmp(_assert_buf)) {
				_last_file = th->file;
				_last_line = th->line;
				for (auto prep : _test_prep::preps) prep();
				th->fn();
			}

			_failure = _failure && !th->should_fail;
			std::string ex_reason;
			if (th->should_fail) {
				ex_reason = _failure
					? "\x1b[0;2m (completed successfully; expected failure)"
					: "\x1b[0;2m (failed as expected)";
			}

			if (_needs_nl) _clear_line();
			if (_failure) {
				std::cerr << "\x1b[31;1m┕ FAIL" << ex_reason << "\x1b[m" << std::endl;
			} else {
				std::cerr << "\x1b[32;1m┕ PASS" << ex_reason << "\x1b[m" << std::endl;
			}

			_total_failure = _total_failure || _failure;
			if (!_failure) {
				++_successful_assertions;
			}
		}
	}
};

std::vector<_test_harness*> _test_harness::tests;
std::deque<void(*)()> _test_prep::preps;

}

#define _ASSERT(x, xstr, cont) do { _tests::_last_file = __FILE__; _tests::_last_line = __LINE__; _tests::_assert((x), (xstr), __FILE__, __LINE__, (cont)); } while (0)
#define ASSERT(x) _ASSERT((x), (#x), false)
#define CHECK(x) _ASSERT((x), (#x), true)
#define FAIL() ASSERT(false && "force failure")

#define _TEST(name, should_fail) static void _test_##name(); \
	_tests::_test_harness _test_harness_##name(_test_##name, #name, (should_fail), __FILE__, __LINE__); \
	static void _test_##name()

#define TEST(name) _TEST(name, false)
#define TEST_FAIL(name) _TEST(name, true)

#define _BEFORE_ALL(uid) static void _test_prep_##uid(); \
	_tests::_test_prep _test_prep_harness_##uid(&_test_prep_##uid); \
	static void _test_prep_##uid()
#define BEFORE_ALL() _BEFORE_ALL(__LINE__)

int main(int argc, const char **argv) {
	signal(SIGSEGV, _tests::_on_signal);
	signal(SIGABRT, _tests::_on_signal);

	if (argc > 1) {
		_tests::_selective_tests = true;
		for (int i = 1; i < argc; i++) {
			_tests::_enabled_tests.insert(argv[i]);
		}
	}

	_tests::_test_harness::run_all();
	if (_tests::_total_assertions > 0) {
		if (_tests::_total_failure) {
			std::cerr.precision(2);
			std::cerr << std::endl << "\x1b[31;1mFAILURE ("
				<< (_tests::_total_assertions == 0
					? 0.0f
					: (((float) _tests::_successful_assertions / (float) _tests::_total_assertions)) * 100.0f)
				<< "% success rate)\x1b[m" << std::endl;
		} else {
			std::cerr << std::endl << "\x1b[32;1mOK\x1b[m" << std::endl;
		}

		if (_tests::_enabled_tests.size()) std::cerr << std::endl;
	} else {
		_tests::_total_failure = true;
		std::cerr << "\x1b[31;1merror:\x1b[m no tests were executed" << std::endl;
	}

	for (const auto &name : _tests::_enabled_tests) {
		std::cerr << "\x1b[33;1mwarning:\x1b[m test not found: \x1b[1m" << name << "\x1b[m" << std::endl;
		_tests::_total_failure = true;
	}

	return _tests::_total_failure ? 1 : 0;
}
