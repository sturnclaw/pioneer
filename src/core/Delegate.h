// https://github.com/marcmo/delegates, modified for use in Pioneer.
//
// The MIT License (MIT)
// Copyright (c) 2020 Pioneer Developers. See AUTHORS.txt for details
// Copyright (c) 2015 oliver.mueller@gmail.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include <type_traits>

/**
 * non specialized template declaration for delegate
 */
template <typename T>
class delegate;

/**
 * specialization for member functions
 *
 * \tparam T            class-type of the object who's member function to call
 * \tparam R            return type of the function that gets captured
 * \tparam params       variadic template list for possible arguments
 *                      of the captured function
 */
template <typename T, typename R, typename... Params>
class delegate<R (T::*)(Params...)> {
public:
	typedef R (T::*func_type)(Params...);

	delegate(func_type func, T &callee) :
		callee_(callee), func_(func)
	{}

	R operator()(Params... args) const
	{
		return (callee_.*func_)(args...);
	}

	bool operator==(const delegate &other) const
	{
		return (&callee_ == &other.callee_) && (func_ == other.func_);
	}
	bool operator!=(const delegate &other) const
	{
		return !((*this) == other);
	}

private:
	T &callee_;
	func_type func_;
};

/**
 * specialization for const member functions
 */
template <typename T, typename R, typename... Params>
class delegate<R (T::*)(Params...) const> {
public:
	typedef R (T::*func_type)(Params...) const;

	delegate(func_type func, const T &callee) :
		callee_(callee), func_(func)
	{}

	R operator()(Params... args) const
	{
		return (callee_.*func_)(args...);
	}

	bool operator==(const delegate &other) const
	{
		return (&callee_ == &other.callee_) && (func_ == other.func_);
	}
	bool operator!=(const delegate &other) const
	{
		return !(*this == other);
	}

private:
	const T &callee_;
	func_type func_;
};

/**
 * specialization for free functions
 *
 * \tparam R            return type of the function that gets captured
 * \tparam params       variadic template list for possible arguments
 *                      of the captured function
 */
template <typename R, typename... Params>
class delegate<R (*)(Params...)> {
public:
	typedef R (*func_type)(Params...);

	delegate(func_type func) :
		func_(func)
	{}

	R operator()(Params... args) const
	{
		return (*func_)(args...);
	}

	bool operator==(const delegate &other) const
	{
		return func_ == other.func_;
	}
	bool operator!=(const delegate &other) const
	{
		return !((*this) == other);
	}

private:
	func_type func_;
};

template <typename R, typename... Params>
struct traits_type {
	using return_type = R;
};

/**
 * function to deduce template parameters from call-context
 */
template <typename F, typename T>
delegate<F> make_delegate(F func, T &obj)
{
	return delegate<F>(func, obj);
}

template <typename T>
delegate<decltype(&T::operator())> make_delegate(T func)
{
	return delegate<decltype(&T::operator())>(&T::operator(), func);
}

template <typename T, typename std::enable_if<!std::is_member_function_pointer<decltype(&T::operator())>::value>::type = 0>
delegate<T> make_delegate(T func)
{
	return delegate<T>(func);
}
