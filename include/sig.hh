#ifndef LIBSIG__HH
#define LIBSIG__HH
#pragma once
/*
	        _           _          _
	       / /\        /\ \       /\ \
	      / /  \       \ \ \     /  \ \
	     / / /\ \__    /\ \_\   / /\ \_\
	    / / /\ \___\  / /\/_/  / / /\/_/
	    \ \ \ \/___/ / / /    / / / ______
	     \ \ \      / / /    / / / /\_____\
	 _    \ \ \    / / /    / / /  \/____ /
	/_/\__/ / /___/ / /__  / / /_____/ / /
	\ \/___/ //\__\/_/___\/ / /______\/ /
	 \_____\/ \/_________/\/___________/

	              libsig
	           by Josh Junon
	            MIT License
*/

#include <cassert>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>

#ifndef LIBSIG_RUNAWAYTHRESH
#	define LIBSIG_RUNAWAYTHRESH 1000
#endif

namespace libsig {
namespace detail {

	typedef unsigned long long age_t;

	struct node {
		bool stale;

		std::function<void()> update;

		node()
		: stale(true)
		{}

		node(const node &) = delete;
		node(node &&) = delete;
	};

	struct owner {
		std::set<std::shared_ptr<node>> children;

		owner() = default;
		owner(const owner &) = delete;
		owner(owner &&) = delete;
	};

	class clock {
		friend struct freeze_guard;

		age_t current_time;
		int frozen;
		std::list<std::weak_ptr<node>> scheduled;

		inline void event() {
			if (frozen) return;
			auto fg = freeze<false>();

			age_t start_time = current_time;

			while (scheduled.size()) {
				auto next = scheduled;
				scheduled.clear();

				if ((++current_time) - start_time > LIBSIG_RUNAWAYTHRESH) {
					throw std::logic_error("runaway clock detected");
				}

				for (auto &n : next) {
					if (auto np = n.lock()) {
						np->update();
					}
				}
			}
		}

	public:
		template <bool RaiseEvent = true>
		struct freeze_guard {
			clock *c;

			freeze_guard(clock *_c)
			: c(_c)
			{ ++c->frozen; }

			freeze_guard(const freeze_guard &other)
			: c(other.c)
			{ ++c->frozen; }

			~freeze_guard() {
				if (RaiseEvent) { /* optimized out */
					if (--c->frozen == 0) {
						c->event();
					}
				} else {
					--c->frozen;
				}
			}
		};

		clock()
		: current_time(1ull) /* must start at 1 since all computations start at 0 */ /* XXX this might not be the case after all */
		, frozen(0)
		{}

		template <bool RaiseEvent>
		inline freeze_guard<RaiseEvent> freeze()
		{ return freeze_guard<RaiseEvent>(this); }

		inline age_t time() const
			{ return current_time; }

		/*
			WARNING: like the name suggests, this consumes (clears) all elements
			         from the `observers` collection.
		*/
		template <typename Coll>
		inline void consume_and_schedule_all(Coll &observers) {
			for (auto &observer : observers) {
				if (auto op = observer.lock()) {
					op->stale = true;
				}
			}
			scheduled.splice(scheduled.end(), observers);
			event();
		}

		inline void schedule_one(std::weak_ptr<node> n) {
			scheduled.push_back(n);
			event();
		}
	};

	struct system_state {
		clock root_clock;
		std::shared_ptr<owner> current_owner;
		std::shared_ptr<node> observer;
	};

#ifdef LIBSIG_MAIN
	thread_local system_state system;
#else
	extern thread_local system_state system;
#endif

	struct owner_guard {
		std::shared_ptr<owner> prev;

		owner_guard(std::shared_ptr<owner> p)
		: prev(system.current_owner)
		{
			system.current_owner = p;
		}

		~owner_guard() {
			system.current_owner = prev;
		}

		owner_guard(const owner_guard &) = delete;
		owner_guard(owner_guard &&) = delete;
	};

	struct observer_guard {
		std::shared_ptr<node> prev;

		observer_guard(std::shared_ptr<node> p)
		: prev(system.observer)
		{
			system.observer = p;
		}

		~observer_guard() {
			system.observer = prev;
		}

		observer_guard(const observer_guard &) = delete;
		observer_guard(observer_guard &&) = delete;
	};

	template <typename T, bool Value = false>
	class signal {
		template <typename U, bool V>
		friend class signal;

		struct data : public node {
			std::weak_ptr<data> self;
			T current_value;
			T scheduled_value;
			bool value_is_scheduled;
			std::list<std::weak_ptr<node>> observers;

			data()
			: current_value(T())
			, value_is_scheduled(false)
			{}

			data(const T &v)
			: current_value(v)
			, value_is_scheduled(false)
			{}

			data(const data &) = delete;
			data(data &&) = delete;

			inline void set_self(std::weak_ptr<data> _self) {
				self = _self;

				this->update = [_self] {
					if (auto self_p = _self.lock()) {
						self_p->swap();
					}
				};
			}

			inline void swap() {
				if (value_is_scheduled) {
					value_is_scheduled = false;
					current_value = std::move(scheduled_value);
					schedule_all_observers();
				}
			}

			inline void depend() {
				if (system.current_owner) {
					if (auto self_p = self.lock()) {
						system.current_owner->children.insert(self_p);
					}
				}

				if (system.observer) {
					observers.push_back(system.observer);
				}
			}

			inline T* operator ->()
				{ depend(); return &current_value; }

			inline operator T&()
				{ depend(); return current_value; }

			inline void schedule_self() {
				assert(!value_is_scheduled);
				value_is_scheduled = true;
				system.root_clock.schedule_one(self);
			}

			inline void schedule_all_observers()
				{ system.root_clock.consume_and_schedule_all(observers); }

			inline void schedule(const T &v) {
				if (Value) { /* optimized out */
					if (value_is_scheduled) {
							if (v != scheduled_value) {
								throw std::logic_error("new value conflicts with scheduled value");
							}
					} else if (current_value != v) {
						scheduled_value = v;
						schedule_self();
					}
				} else {
					if (value_is_scheduled) {
						if (v != scheduled_value) {
							throw std::logic_error("new value conflicts with scheduled value");
						}
					} else {
						scheduled_value = v;
						schedule_self();
					}
				}
			}

			inline T& sample()
				{ return current_value; }

			inline const T& sample() const
				{ return current_value; }

#			define LIBSIG_SIG_OP(op) \
				template <typename U> \
				inline auto operator op(U &other) \
					-> decltype(current_value op other) \
					{ depend(); return current_value op other; }

			LIBSIG_SIG_OP(==)
			LIBSIG_SIG_OP(!=)
			LIBSIG_SIG_OP(*)
			LIBSIG_SIG_OP(/)
			LIBSIG_SIG_OP(+)
			LIBSIG_SIG_OP(-)
			LIBSIG_SIG_OP(%)
			LIBSIG_SIG_OP(^)
			LIBSIG_SIG_OP(&)
			LIBSIG_SIG_OP(|)

#			undef LIBSIG_SIG_OP
		};

		std::shared_ptr<data> d;

		signal(std::shared_ptr<data> _d)
		: d(_d)
		{}

	public:
		typedef T signal_type;

		signal()
		: d(new data())
		{
			d->set_self(d);
		}

		explicit signal(const T &v)
		: d(new data(v))
		{
			d->set_self(d);
		}

		explicit signal(const signal<T, Value> &other)
		: d(other.d)
		{}

		inline void depend()
			{ d->depend(); }

		inline operator T&()
			{ return d->operator T&(); }

		inline T* operator ->()
			{ return d->operator ->(); }

		template <typename U, bool V>
		inline signal<T, Value> & operator =(signal<U, V> &sig)
			{ d->schedule(sig.operator U&()); return *this; }

		inline signal<T, Value> & operator =(const T &v)
			{ d->schedule(v); return *this; }

		inline T& sample()
			{ return d->sample(); }

		inline const T& sample() const
			{ return d->sample(); }

#		define LIBSIG_SIG_OP(op) \
			template <typename U> \
			inline auto operator op(const U &other) \
				-> decltype(d->operator op(other)) \
				{ return d->operator op(other); } \
			template <typename U, bool V> \
			inline auto operator op(signal<U, V> &other) \
				-> decltype(d->operator op(other.operator U&())) \
				{ return d->operator op(other.operator U&()); }

		LIBSIG_SIG_OP(==)
		LIBSIG_SIG_OP(!=)
		LIBSIG_SIG_OP(*)
		LIBSIG_SIG_OP(/)
		LIBSIG_SIG_OP(+)
		LIBSIG_SIG_OP(-)
		LIBSIG_SIG_OP(%)
		LIBSIG_SIG_OP(^)
		LIBSIG_SIG_OP(&)
		LIBSIG_SIG_OP(|)

#		undef LIBSIG_SIG_OP

		friend std::ostream & operator<<(std::ostream &os, signal<T, Value> &sig) {
			os << sig.operator T&();
			return os;
		}
	};

	class computation {
		friend class api;

		struct data : public node, public owner {
			std::weak_ptr<data> self;
			std::function<void()> fn;
			std::list<std::weak_ptr<node>> observers;

			data(std::function<void()> _fn)
			: fn(_fn)
			{}

			inline void depend() {
				if (system.observer) {
					observers.push_back(system.observer);
				}
			}

			inline void schedule_all_observers()
				{ system.root_clock.consume_and_schedule_all(observers); }

			inline void set_self(std::weak_ptr<data> _self) {
				self = _self;

				this->update = [_self] {
					if (auto self_p = _self.lock()) {
						self_p->recompute();
					}
				};
			}

			void recompute() {
				if (auto self_p = self.lock()) {
					if (stale) {
						stale = false;
						children.clear();
						owner_guard og(self_p);
						observer_guard obg(self_p);
						fn();
						schedule_all_observers();
					}
				}
			}

			inline void schedule_self()
				{ system.root_clock.schedule_one(self); }
		};

		std::shared_ptr<data> d;

		computation(std::function<void()> fn)
		: d(new data(fn))
		{
			d->set_self(d);

			if (system.current_owner) {
				system.current_owner->children.insert(d);
			} else {
				throw std::logic_error("computations must be created from within a sig_root context");
			}

			d->schedule_self();
		}

	public:
		computation(const computation &other)
		: d(other.d)
		{}
	};

	class signal_root {
		struct data : public owner {};
		std::shared_ptr<data> d;

	public:
		signal_root(std::function<void()> fn)
		: d(new data())
		{
			owner_guard og(d);
			fn();
		}

		signal_root(const signal_root &other)
		: d(other.d)
		{}

		signal_root(signal_root &&other)
		: d(std::move(other.d))
		{}
	};

	class api {
		template<typename T, bool Value>
		struct extract_signal {
			typedef T value_type;
		};

		template<typename T, bool Value>
		struct extract_signal <signal<T, Value>, Value> {
			typedef T value_type;
		};

	public:
		auto operator()(std::function<void()> fn) -> computation {
			return computation(fn);
		}

		void freeze(std::function<void()> fn) {
			auto fg = system.root_clock.freeze<true>();
			fn();
		}
	};
}}

namespace libsig {
	template <typename T>
	using sig = detail::signal<T>;
	template <typename T>
	using val = detail::signal<T, true>;
	using computation = detail::computation;
	using sig_root = detail::signal_root;
	static detail::api S;
}

#endif
