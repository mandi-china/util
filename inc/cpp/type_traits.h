//
// Created by hujianzhe on 20-9-3
//

#ifndef	UTIL_CPP_TYPE_IS_CLASS_H
#define	UTIL_CPP_TYPE_IS_CLASS_H

#ifdef	__cplusplus
#include "cpp_compiler_define.h"
namespace util {
template <typename T>
struct is_class {
private:
	template <typename C> static char test(int C::*);
	template <typename C> static int test(...);

public:
	enum { value = (sizeof(test<T>(0)) == sizeof(char)) };
};

template <typename T, bool isClass = is_class<T>::value> struct is_inherit_impl;
template <typename T>
struct is_inherit_impl <T, true> {
private:
	template <typename C> static char test(const T*);
	template <typename C> static int test(...);

public:
	template <typename C>
	struct Check {
	private:
		static C* getType();
	public:
		enum { value = (sizeof(test<T>(getType())) == sizeof(char)) };
	};
};
template <typename T>
struct is_inherit_impl <T, false> {
public:
	template <typename C>
	struct Check {
	public:
		enum { value = false };
	};
};
template <class Sub, class Parent>
struct is_inhert {
	enum { value =  is_inherit_impl<Parent>::template Check<Sub>::value };
};
}
#endif

#endif
