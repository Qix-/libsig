#define LIBSIG_MAIN
#define LIBSIG_RUNAWAYTHRESH 200
#include <sig.hh>

#include "./test.inc"

#include <sstream>

using namespace libsig;
using namespace std;

static libsig::detail::system_state *libsigstate;

BEFORE_ALL() {
	libsig::detail::system = libsig::detail::system_state();
	libsigstate = &libsig::detail::system;
	(void) libsigstate;
}

TEST(signal_basic) {
	sig<int> s;
	CHECK(s == 0);
	s = 10;
	CHECK(s == 10);
	CHECK(s == 10); /* intentional */
	s = 10; /* intentional */
	CHECK(s == 10); /* intentional */
	s = 15;
	CHECK(s == 15);
}

TEST(value_basic) {
	val<int> s;
	CHECK(s == 0);
	s = 10;
	CHECK(s == 10);
	CHECK(s == 10); /* intentional */
	s = 10; /* intentional */
	CHECK(s == 10); /* intentional */
	s = 15;
	CHECK(s == 15);
}

TEST(signal_basic_init) {
	sig<int> s(42);
	CHECK(s == 42);
	s = 10;
	CHECK(s == 10);
	CHECK(s == 10); /* intentional */
	s = 10; /* intentional */
	CHECK(s == 10); /* intentional */
	s = 15;
	CHECK(s == 15);
}

TEST(value_basic_init) {
	val<int> s(42);
	CHECK(s == 42);
	s = 10;
	CHECK(s == 10);
	CHECK(s == 10); /* intentional */
	s = 10; /* intentional */
	CHECK(s == 10); /* intentional */
	s = 15;
	CHECK(s == 15);
}

TEST(root_basic) {
	bool called = false;

	sig_root root([&] {
		called = true;
	});

	ASSERT(called);
}

TEST(basic_computation) {
	sig_root([] {
		sig<int> a, b;
		int invocations = 0;

		CHECK(a == 0);
		CHECK(b == 0);

		S([=, &invocations]() mutable {
			++invocations;
			b = a;
		});

		CHECK(a == 0);
		CHECK(b == 0);
		CHECK(invocations == 1);

		a = 10;
		CHECK(a == 10);
		CHECK(b == 10);
		CHECK(invocations == 2);

		b = 50;
		CHECK(a == 10);
		CHECK(b == 50);
		CHECK(invocations == 2);

		a = 75;
		CHECK(a == 75);
		CHECK(b == 75);
		CHECK(invocations == 3);
	});
}

TEST(computation_empty) {
	int invocations = 0;

	sig_root root([&]() {
		S([&] {++invocations;});
		S([&] {++invocations;}); /* intentional */
		S([&] {++invocations;}); /* intentional */

		for (int i = 0; i < 100; i++) {
			S([&] {++invocations;});
		}
	});

	ASSERT(invocations == 103);
}

TEST(computation_no_deps) {
	bool called = false;
	sig_root root([&] {
		S([&] {
			called = true;
		});
	});
	ASSERT(called == true);
}

TEST(computation_no_deps_signal) {
	sig<bool> called(false);
	sig_root root([=]() mutable {
		called = true;
	});
	ASSERT(called == true);
}

TEST(signal_sample) {
	sig<int> i(10);
	sig<int> res_a, res_b;

	CHECK(i == 10);
	CHECK(res_a == 0);
	CHECK(res_b == 0);

	sig_root root([=]() mutable {
		S([=]() mutable {
			res_a = i;
		});

		S([=]() mutable {
			res_b = i.sample();
		});
	});

	CHECK(i == 10); /* intentional */
	CHECK(res_a == 10);
	CHECK(res_b == 10);

	i = 15;

	CHECK(i == 15);
	CHECK(res_a == 15);
	CHECK(res_b == 10);
}

TEST(value_sample) {
	val<int> i(10);
	val<int> res_a, res_b;

	CHECK(i == 10);
	CHECK(res_a == 0);
	CHECK(res_b == 0);

	sig_root root([=]() mutable {
		S([=]() mutable {
			res_a = i;
		});

		S([=]() mutable {
			res_b = i.sample();
		});
	});

	CHECK(i == 10); /* intentional */
	CHECK(res_a == 10);
	CHECK(res_b == 10);

	i = 15;

	CHECK(i == 15);
	CHECK(res_a == 15);
	CHECK(res_b == 10);
}

TEST(no_redundant_value_schedule) {
	sig<int> s(0);
	val<int> v(0);
	int sig_count = 0;
	int val_count = 0;

	CHECK(s == 0);
	CHECK(v == 0);

	sig_root root([=, &sig_count, &val_count]() mutable {
		S([=, &sig_count]() mutable {
			s.depend();
			++sig_count;
		});

		S([=, &val_count]() mutable {
			v.depend();
			++val_count;
		});
	});

	CHECK(sig_count == 1);
	CHECK(val_count == 1);

	s = 1;
	CHECK(sig_count == 2);
	CHECK(s == 1);
	CHECK(val_count == 1);
	CHECK(v == 0);

	/* intentional */
	s = 1;
	CHECK(sig_count == 3);
	CHECK(s == 1);
	CHECK(val_count == 1);
	CHECK(v == 0);

	s = 2;
	CHECK(sig_count == 4);
	CHECK(s == 2);
	CHECK(val_count == 1);
	CHECK(v == 0);

	v = 1;
	CHECK(sig_count == 4);
	CHECK(s == 2);
	CHECK(val_count == 2);
	CHECK(v == 1);

	v = 2;
	CHECK(sig_count == 4);
	CHECK(s == 2);
	CHECK(val_count == 3);
	CHECK(v == 2);

	/* intentional */
	v = 2;
	CHECK(sig_count == 4);
	CHECK(s == 2);
	CHECK(val_count == 3); /* no execution */
	CHECK(v == 2);

	v = 3;
	CHECK(sig_count == 4);
	CHECK(s == 2);
	CHECK(val_count == 4);
	CHECK(v == 3);
}

TEST(chained_condition) {
	sig<int> i, i_times_10;
	sig<string> i_result;

	sig_root root([=]() mutable {
		CHECK(i == 0);
		CHECK(i_times_10 == 0);
		CHECK(i_result == "");

		S([=]() mutable {
			i_times_10 = i * 10;
		});

		CHECK(i == 0);
		CHECK(i_times_10 == 0);
		CHECK(i_result == "");

		i = 14;

		CHECK(i == 14);
		CHECK(i_times_10 == 140);
		CHECK(i_result == "");

		S([=]() mutable {
			i_result = "result: " + to_string(i_times_10);
		});

		CHECK(i == 14);
		CHECK(i_times_10 == 140);
		CHECK(i_result == "result: 140");
	});

	i = 62;
	CHECK(i == 62);
	CHECK(i_times_10 == 620);
	CHECK(i_result == "result: 620");

	/* intentional */
	i = 62;
	CHECK(i == 62);
	CHECK(i_times_10 == 620);
	CHECK(i_result == "result: 620");

	i = -150;
	CHECK(i == -150);
	CHECK(i_times_10 == -1500);
	CHECK(i_result == "result: -1500");
}

TEST(two_roots_interop) {
	sig<int> i, times_2, times_4;

	sig_root root1([=]() mutable {
		S([=]() mutable { times_2 = i * 2; });
	});

	sig_root root2([=]() mutable {
		S([=]() mutable { times_4 = i * 4; });
	});

	CHECK(i == 0);
	CHECK(times_2 == 0);
	CHECK(times_4 == 0);

	i = 100;
	CHECK(i == 100);
	CHECK(times_2 == 200);
	CHECK(times_4 == 400);
}

TEST(downstream_update) {
	sig<int> i, times_2, times_2_times_2;

	sig_root root([=]() mutable {
		S([=]() mutable {times_2 = i * 2;});
		S([=]() mutable {times_2_times_2 = times_2 * 2;});
	});

	CHECK(i == 0);
	CHECK(times_2 == 0);
	CHECK(times_2_times_2 == 0);

	i = 4;
	CHECK(i == 4);
	CHECK(times_2 == 8);
	CHECK(times_2_times_2 == 16);

	times_2 = 15;
	CHECK(i == 4);
	CHECK(times_2 == 15);
	CHECK(times_2_times_2 == 30);

	/* nothing scheduled */
	times_2_times_2 = 80;
	CHECK(i == 4);
	CHECK(times_2 == 15);
	CHECK(times_2_times_2 == 80);

	i = 13;
	CHECK(i == 13);
	CHECK(times_2 == 26);
	CHECK(times_2_times_2 == 52);
}

TEST(nested_signal_creation) {
	sig<int> input;
	sig<int> result;

	sig_root root([=]() mutable {
		S([=]() mutable {
			sig<int> intermediate(input * 10);

			S([=]() mutable {
				result = 50 * intermediate;
			});
		});
	});

	ASSERT(input == 0);
	ASSERT(result == 0);

	input = 10;
	CHECK(result == 5000);
}

TEST(nested_computation) {
	sig<int> i, j, k, l;
	int outer = 0;
	int middle = 0;
	int inner = 0;

	sig_root root([=, &outer, &middle, &inner]() mutable {
		S([=, &outer, &middle, &inner]() mutable {
			j = i;
			++outer;

			S([=, &middle, &inner]() mutable {
				++middle;
				k = j;

				S([=, &inner]() mutable {
					l = k;
					++inner;
				});
			});
		});
	});

	CHECK(i == 0);
	CHECK(j == 0);
	CHECK(k == 0);
	CHECK(l == 0);
	CHECK(outer == 1);
	CHECK(middle == 1);
	CHECK(inner == 1);

	i = 10;
	CHECK(i == 10);
	CHECK(j == 10);
	CHECK(k == 10);
	CHECK(l == 10);
	CHECK(outer == 2);
	CHECK(middle == 2);
	CHECK(inner == 2);

	j = 15;
	CHECK(i == 10);
	CHECK(j == 15);
	CHECK(k == 15);
	CHECK(l == 15);
	CHECK(outer == 2);
	CHECK(middle == 3);
	CHECK(inner == 3);

	i = 30;
	CHECK(i == 30);
	CHECK(j == 30);
	CHECK(k == 30);
	CHECK(l == 30);
	CHECK(outer == 3);
	CHECK(middle == 4);
	CHECK(inner == 4);

	k = 42;
	CHECK(i == 30);
	CHECK(j == 30);
	CHECK(k == 42);
	CHECK(l == 42);
	CHECK(outer == 3);
	CHECK(middle == 4);
	CHECK(inner == 5);

	S.freeze([&] {
		/*
			k and j should be overwritten here
		*/
		i = 100;
		k = 200;
		j = 300;
		CHECK(i == 30);
		CHECK(j == 30);
		CHECK(k == 42);
		CHECK(l == 42);
	});

	CHECK(i == 100);
	CHECK(j == 100);
	CHECK(k == 100);
	CHECK(l == 100);
	CHECK(outer == 4);
	CHECK(middle == 5);
	CHECK(inner == 6);
}

TEST(freeze) {
	sig<int> i, j, k;

	CHECK(i == 0);
	CHECK(j == 0);
	CHECK(k == 0);

	sig_root root([=]() mutable {
		S([=]() mutable {
			j = i;
		});

		S([=]() mutable {
			k = j;
		});
	});

	i = 5;
	CHECK(i == 5);
	CHECK(j == 5);
	CHECK(k == 5);

	S.freeze([=]() mutable {
		i = 10;

		CHECK(i == 5);
		CHECK(k == 5);
		CHECK(j == 5);
	});

	CHECK(i == 10);
	CHECK(j == 10);
	CHECK(k == 10);

	j = 20;
	CHECK(i == 10);
	CHECK(j == 20);
	CHECK(k == 20);

	S.freeze([=]() mutable {
		/*
			Since all swaps happen after freeze,
			prior to execution, the j=30 below
			is innocuous and will result in an
			overwrite.
		*/

		i = 42;
		CHECK(i == 10);
		CHECK(j == 20);
		CHECK(k == 20);

		j = 30;
		CHECK(i == 10);
		CHECK(j == 20);
		CHECK(k == 20);
	});

	CHECK(i == 42);
	CHECK(j == 42);
	CHECK(k == 42);
}

TEST(scheduled_value) {
	val<int> i(10), j(0);

	ASSERT(i != j);

	sig_root root([=]() mutable {
		S([=]() mutable {
			int prev = j.sample();
			j = i;
			CHECK(j.sample() == prev);
		});

		S([=]() mutable {
			CHECK(j == i.sample());
		});
	});

	i = 10;
	CHECK(i == 10);
	CHECK(j == 10);
	i = 20;
	CHECK(i == 20);
	CHECK(j == 20);
	i = 30;
	CHECK(i == 30);
	CHECK(j == 30);
}

TEST(signal_equality) {
	sig<int> i, j;
	CHECK(i == j);
	i = 10;
	CHECK(i != j);
	j = 10;
	CHECK(i == j);
}

TEST(value_equality) {
	val<int> i, j;
	CHECK(i == j);
	i = 10;
	CHECK(i != j);
	j = 10;
	CHECK(i == j);
}

TEST(mixed_equality) {
	val<int> i;
	sig<int> j;
	CHECK(i == j);
	CHECK(j == i);
	i = 10;
	CHECK(i != j);
	CHECK(j != i);
	j = 10;
	CHECK(i == j);
	CHECK(j == i);
}

TEST(conflicting_assignment) {
	sig<int> i;
	bool caught = false;

	sig_root root([=, &caught]() mutable {
		S([=, &caught]() mutable {
			try {
				i = 10;
				i = 10; /* OK */
				i = 40; /* should throw */
				FAIL();
			} catch (const std::logic_error &ex) {
				CHECK(string(ex.what()) == "new value conflicts with scheduled value");
				caught = true;
			}
		});
	});

	ASSERT(caught);
}

TEST(freeze_is_not_computation) {
	sig<int> i(10), j;

	sig_root([=]() mutable {
		CHECK(i == 10);
		CHECK(j == 0);

		S.freeze([=]() mutable {
			j = i;
		});

		CHECK(i == 10);
		CHECK(j == 10);

		i = 14;
		CHECK(i == 14);
		CHECK(j == 10);
	});
}

TEST(root_is_not_a_computation) {
	sig<int> i, j;

	CHECK(i == 0);
	CHECK(j == 0);

	sig_root root([=]() mutable {
		j = i;
	});

	CHECK(i == 0);
	CHECK(j == 0);

	i = 15;
	CHECK(i == 15);
	CHECK(j == 0);
}

TEST_FAIL(runaway_clock) {
	sig<int> i, j;
	sig_root root([=]() mutable {
		S([=]() mutable { i = j; });
		S([=]() mutable { j = i; });
	});
}

TEST(computation_must_be_in_root) {
	bool thrown = false;
	try {
		ASSERT(!libsig::detail::system.current_owner);
		S([] {});
		FAIL();
	} catch (const std::logic_error &ex) {
		CHECK(string(ex.what()) == "computations must be created from within a sig_root context");
		thrown = true;
	}
	ASSERT(thrown);
}

TEST(signal_can_be_ostreamed) {
	stringstream ss;
	sig<string> str("hello");
	sig<int> i(1240);

	ASSERT(str == "hello");
	ASSERT(i == 1240);
	ss << str << "=" << i;

	ASSERT(ss.str() == "hello=1240");
}

TEST(value_can_be_ostreamed) {
	stringstream ss;
	val<string> str("hello");
	val<int> i(1240);

	ASSERT(str == "hello");
	ASSERT(i == 1240);
	ss << str << "=" << i;

	ASSERT(ss.str() == "hello=1240");
}

struct testdcns {
	testdcns(int &_i) : i(_i) {}
	~testdcns() { ++i; }
	int &i;
};

TEST(destructor_called_on_refresh) {
	int dcount = 0;
	sig<int> foo;

	sig_root root([=, &dcount]() mutable {
		S([=, &dcount]() mutable {
			volatile int foo_i = foo;
			(void) foo_i;

			auto dcount_ptr = make_shared<testdcns>(dcount);

			S([=]() mutable {
				volatile auto a = dcount_ptr.get(); // try to convince the compiler not to optimize it out.
				(void) a;
			});
		});
	});

	ASSERT(dcount == 0);
	foo = 10;
	ASSERT(dcount == 1);
	foo = 10;
	ASSERT(dcount == 2);
	foo = 15;
	ASSERT(dcount == 3);
}
