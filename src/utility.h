#pragma once

namespace mob
{

#define MOB_ENUM_OPERATORS(E) \
	inline E operator|(E e1, E e2) { return (E)((int)e1 | (int)e2); } \
	inline E operator&(E e1, E e2) { return (E)((int)e1 & (int)e2); } \
	inline E operator|=(E& e1, E e2) { e1 = e1 | e2; return e1; }

template <class E>
bool is_set(E e, E v)
{
	static_assert(std::is_enum_v<E>, "this is for enums");
	return (e & v) == v;
}

template <class E>
bool is_any_set(E e, E v)
{
	static_assert(std::is_enum_v<E>, "this is for enums");
	return (e & v) != E(0);
}


#define MOB_WIDEN2(x) L ## x
#define MOB_WIDEN(x) MOB_WIDEN2(x)
#define MOB_FILE_UTF16 MOB_WIDEN(__FILE__)

#define MOB_ASSERT(x, ...) \
	mob_assert(x, __VA_ARGS__, #x, MOB_FILE_UTF16, __LINE__, __FUNCSIG__);

void mob_assertion_failed(
	const char* message,
	const char* exp, const wchar_t* file, int line, const char* func);

template <class X>
inline void mob_assert(
	X&& x, const char* message,
	const char* exp, const wchar_t* file, int line, const char* func)
{
	if (!(x))
		mob_assertion_failed(message, exp, file, line, func);
}

template <class X>
inline void mob_assert(
	X&& x, const char* exp, const wchar_t* file, int line, const char* func)
{
	if (!(x))
		mob_assertion_failed(nullptr, exp, file, line, func);
}

void set_thread_exception_handlers();

template <class F>
std::thread start_thread(F&& f)
{
	return std::thread([f]
	{
		set_thread_exception_handlers();
		f();
	});
}


enum class encodings
{
	dont_know = 0,
	utf8,
	utf16,
	acp,
	oem
};


class context;
class url;

class bailed
{
public:
	bailed(std::string s={})
		: s_(std::move(s))
	{
	}

	const char* what() const
	{
		return s_.c_str();
	}

private:
	std::string s_;
};


struct handle_closer
{
	using pointer = HANDLE;

	void operator()(HANDLE h)
	{
		if (h != INVALID_HANDLE_VALUE)
			::CloseHandle(h);
	}
};

using handle_ptr = std::unique_ptr<HANDLE, handle_closer>;


struct file_closer
{
	void operator()(std::FILE* f)
	{
		if (f)
			std::fclose(f);
	}
};

using file_ptr = std::unique_ptr<FILE, file_closer>;

// time since mob started
//
std::chrono::nanoseconds timestamp();


template <std::size_t N>
class instrumentable
{
public:
	struct time_pair
	{
		std::chrono::nanoseconds start{}, end{};
	};

	struct task
	{
		std::string name;
		std::vector<time_pair> tps;
	};

	struct timing_ender
	{
		timing_ender(time_pair& tp)
			: tp(tp)
		{
		}

		~timing_ender()
		{
			tp.end = timestamp();
		}

		time_pair& tp;
	};

	instrumentable(std::string name, std::array<std::string, N> names)
		: name_(std::move(name))
	{
		for (std::size_t i=0; i<N; ++i)
			tasks_[i].name = names[i];
	}

	const std::string& instrumentable_name() const
	{
		return name_;
	}

	template <auto E, class F>
	auto instrument(F&& f)
	{
		auto& t = std::get<static_cast<std::size_t>(E)>(tasks_);
		t.tps.push_back({});
		t.tps.back().start = timestamp();
		timing_ender te(t.tps.back());
		return f();
	}

	const std::array<task, N>& instrumented_tasks() const
	{
		return tasks_;
	}

private:
	std::string name_;
	std::array<task, N> tasks_;
};


template <class F>
class guard
{
public:
	guard(F f)
		: f_(f)
	{
	}

	~guard()
	{
		f_();
	}

private:
	F f_;
};


class file_deleter
{
public:
	file_deleter(const context& cx, fs::path p);
	file_deleter(const file_deleter&) = delete;
	file_deleter& operator=(const file_deleter&) = delete;
	~file_deleter();

	void delete_now();
	void cancel();

private:
	const context& cx_;
	fs::path p_;
	bool delete_;
};


class directory_deleter
{
public:
	directory_deleter(const context& cx, fs::path p);
	directory_deleter(const directory_deleter&) = delete;
	directory_deleter& operator=(const directory_deleter&) = delete;
	~directory_deleter();

	void delete_now();
	void cancel();

private:
	const context& cx_;
	fs::path p_;
	bool delete_;
};


class interruption_file
{
public:
	interruption_file(const context& cx, fs::path dir, std::string name);

	fs::path file() const;
	bool exists() const;

	void create();
	void remove();

private:
	const context& cx_;
	fs::path dir_;
	std::string name_;
};


class bypass_file
{
public:
	bypass_file(const context& cx, fs::path dir, std::string name);

	bool exists() const;
	void create();

private:
	const context& cx_;
	fs::path file_;
};


class console_color
{
public:
	enum colors
	{
		white,
		grey,
		yellow,
		red
	};

	console_color();
	console_color(colors c);
	~console_color();

private:
	bool reset_;
	WORD old_atts_;
};


enum class arch
{
	x86 = 1,
	x64,
	dont_care,

	def = x64
};


url make_prebuilt_url(const std::string& filename);
url make_appveyor_artifact_url(
	arch a, const std::string& project, const std::string& filename);

// case insensitive, underscores and dashes are equivalent; gets converted to
// a regex where * becomes .*
//
bool glob_match(const std::string& pattern, const std::string& s);

std::string replace_all(
	std::string s, const std::string& from, const std::string& to);

template <class T, class Sep>
T join(const std::vector<T>& v, const Sep& sep)
{
	T s;
	bool first = true;

	for (auto&& e : v)
	{
		if (!first)
			s += sep;

		s += e;
		first = false;
	}

	return s;
}

std::vector<std::string> split(const std::string& s, const std::string& seps);
std::vector<std::string> split_quoted(const std::string& s, const std::string& seps);

std::string pad_right(std::string s, std::size_t n, char c=' ');
std::string pad_left(std::string s, std::size_t n, char c=' ');

void trim(std::string& s, std::string_view what=" \t\r\n");
void trim(std::wstring& s, std::wstring_view what=L" \t\r\n");

std::string table(
	const std::vector<std::pair<std::string, std::string>>& v,
	std::size_t indent, std::size_t spacing);

std::string trim_copy(std::string_view s, std::string_view what=" \t\r\n");
std::wstring trim_copy(std::wstring_view s, std::wstring_view what=L" \t\r\n");

std::wstring utf8_to_utf16(std::string_view s);
std::string utf16_to_utf8(std::wstring_view ws);
std::string bytes_to_utf8(encodings e, std::string_view bytes);
std::string utf8_to_bytes(encodings e, std::string_view utf8);

template <class T>
std::string path_to_utf8(T&&) = delete;

std::string path_to_utf8(fs::path p);


class u8stream
{
public:
	u8stream(bool err)
		: err_(err)
	{
	}

	template <class... Args>
	u8stream& operator<<(Args&&... args)
	{
		std::ostringstream oss;
		((oss << std::forward<Args>(args)), ...);

		do_output(oss.str());

		return *this;
	}

	void write_ln(std::string_view utf8);

private:
	bool err_;

	void do_output(const std::string& s);
};


extern u8stream u8cout;
extern u8stream u8cerr;

void set_std_streams();


template <class F>
void for_each_line(std::string_view s, F&& f)
{
	if (s.empty())
		return;

	const char* const begin = s.data();
	const char* const end = s.data() + s.size();

	const char* start = begin;
	const char* p = begin;

	for (;;)
	{
		MOB_ASSERT(p && p >= begin && p <= end);
		MOB_ASSERT(start && start >= begin && start <= end);

		if (p == end || *p == '\n' || *p == '\r')
		{
			if (p != start)
			{
				MOB_ASSERT(p >= start);

				const auto n = static_cast<std::size_t>(p - start);
				MOB_ASSERT(n <= s.size());

				f(std::string_view(start, n));
			}

			while (p != end && (*p == '\n' || *p == '\r'))
				++p;

			MOB_ASSERT(p && p >= begin && p <= end);

			if (p == end)
				break;

			start = p;
		}
		else
		{
			++p;
		}
	}
}



template <class T>
class repeat_iterator
{
public:
	repeat_iterator(const T& s)
		: s_(s)
	{
	}

	bool operator==(const repeat_iterator&) const
	{
		return false;
	}

	const T& operator*() const
	{
		return s_;
	}

	repeat_iterator& operator++()
	{
		// no-op
		return *this;
	}

private:
	const T& s_;
};


template <class T>
class repeat_range
{
public:
	using value_type = T;
	using const_iterator = repeat_iterator<value_type>;

	repeat_range(const T& s)
		: s_(s)
	{
	}

	const_iterator begin() const
	{
		return const_iterator(s_);
	}

	const_iterator end() const
	{
		return const_iterator(s_);
	}

private:
	T s_;
};


template <class T>
repeat_iterator<T> begin(const repeat_range<T>& r)
{
	return r.begin();
}

template <class T>
repeat_iterator<T> end(const repeat_range<T>& r)
{
	return r.end();
}


template <class T>
struct repeat_converter
{
	using value_type = T;
};

template <>
struct repeat_converter<const char*>
{
	using value_type = std::string;
};

template <std::size_t N>
struct repeat_converter<const char[N]>
{
	using value_type = std::string;
};

template <std::size_t N>
struct repeat_converter<char[N]>
{
	using value_type = std::string;
};


template <class T>
auto repeat(const T& s)
{
	return repeat_range<typename repeat_converter<T>::value_type>(s);
}


template <
	class Range1,
	class Range2,
	class Container=std::vector<std::pair<
	typename Range1::value_type,
	typename Range2::value_type>>>
Container zip(const Range1& range1, const Range2& range2)
{
	Container out;

	auto itor1 = begin(range1);
	auto end1 = end(range1);

	auto itor2 = begin(range2);
	auto end2 = end(range2);

	for (;;)
	{
		if (itor1 == end1 || itor2 == end2)
			break;

		out.push_back({*itor1, *itor2});
		++itor1;
		++itor2;
	}

	return out;
}


template <class F, class T>
auto map(const std::vector<T>& v, F&& f)
{
	using mapped_type = decltype(f(std::declval<T>()));
	std::vector<mapped_type> out;

	for (auto&& e : v)
		out.push_back(f(e));

	return out;
}


class thread_pool
{
public:
	typedef std::function<void ()> fun;

	thread_pool(std::size_t count=std::thread::hardware_concurrency());
	~thread_pool();

	// non-copyable
	thread_pool(const thread_pool&) = delete;
	thread_pool& operator=(const thread_pool&) = delete;

	void add(fun f);
	void join();

private:
	struct thread_info
	{
		std::atomic<bool> running = false;
		fun thread_fun;
		std::thread thread;
	};


	const std::size_t count_;
	std::vector<std::unique_ptr<thread_info>> threads_;

	bool try_add(fun thread_fun);
};


// see https://github.com/isanae/mob/issues/4
//
// this restores the original console font if it changed
//
class font_restorer
{
public:
	font_restorer()
		: restore_(false)
	{
		std::memset(&old_, 0, sizeof(old_));
		old_.cbSize = sizeof(old_);

		if (GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_))
			restore_ = true;
	}

	~font_restorer()
	{
		if (!restore_)
			return;

		CONSOLE_FONT_INFOEX now = {};
		now.cbSize = sizeof(now);

		if (!GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &now))
			return;

		if (std::wcsncmp(old_.FaceName, now.FaceName, LF_FACESIZE) != 0)
			restore();
	}

	void restore()
	{
		::SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &old_);
	}

private:
	CONSOLE_FONT_INFOEX old_;
	bool restore_;
};

}	// namespace
