// eventpp library
// Copyright (C) 2018 Wang Qi (wqking)
// Github: https://github.com/wqking/eventpp
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//   http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ANYDATA_H_320827729782
#define ANYDATA_H_320827729782

#include <array>
#include <type_traits>
#include <cstdint>
#include <cassert>
#include <memory>

namespace eventpp {

namespace anydata_internal_ {

template <typename T>
void funcDeleteObject(void * object)
{
	delete static_cast<T *>(object);
}

template <typename T>
void funcFreeObject(void * object)
{
	static_cast<T *>(object)->~T();
}

template <typename T>
auto doFuncMoveConstruct(void * object, void * buffer)
	-> typename std::enable_if<std::is_move_constructible<T>::value>::type
{
	new (buffer) T(std::move(*(T *)object));
}

template <typename T>
auto doFuncMoveConstruct(void * object, void * buffer)
	-> typename std::enable_if<! std::is_move_constructible<T>::value>::type
{
	static_assert(std::is_move_constructible<T>::value, "AnyData: object must be either copy or move constructible");
}

template <typename T>
void funcMoveConstruct(void * object, void * buffer)
{
	doFuncMoveConstruct<T>(object, buffer);
}

struct AnyDataFunctions
{
	void (*free)(void *);
	void (*moveConstruct)(void *, void *);
};

template <typename T>
const AnyDataFunctions * doGetAnyDataFunctions()
{
	static const AnyDataFunctions functions {
		&funcFreeObject<T>,
		&funcMoveConstruct<T>
	};
	return &functions;
}

template <typename T>
struct RemoveCvRef
{
	using Type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

template <typename T>
const AnyDataFunctions * getAnyDataFunctions()
{
	using U = typename RemoveCvRef<T>::Type;
	return doGetAnyDataFunctions<U>();
}

template <typename ...Ts>
struct MaxSizeOf;

template <typename T, typename ...Ts>
struct MaxSizeOf <T, Ts...>
{
	static constexpr std::size_t otherSize = MaxSizeOf<Ts...>::value;
	static constexpr std::size_t tSize = sizeof(T);

	static constexpr std::size_t value = tSize > otherSize ? tSize : otherSize;
};

template <typename T>
struct MaxSizeOf <T>
{
	static constexpr std::size_t value = sizeof(T);
};

class LargeData
{
public:
	template <typename T>
	explicit LargeData(T && object) : data(), deleter() {
		using U = typename RemoveCvRef<T>::Type;
		deleter = &funcDeleteObject<U>;
		data = new U(std::forward<T>(object));
	}

	~LargeData() {
		if(data != nullptr) {
			assert(deleter != nullptr);
			deleter(data);
		}
	}

	LargeData(LargeData && other) : data(), deleter() {
		std::swap(data, other.data);
		std::swap(deleter, other.deleter);
	}

	LargeData(const LargeData & other) = delete;
	LargeData & operator = (const LargeData & other) = delete;
	LargeData & operator = (LargeData && other) = delete;

	const void * getAddress() const {
		return data;
	}

	template <typename T>
	bool isType() const {
		using U = typename RemoveCvRef<T>::Type;
		return deleter == &funcDeleteObject<U>;
	}

private:
	void * data;
	void (*deleter)(void *);
};

} //namespace anydata_internal_

template <std::size_t maxSize_>
class AnyData
{
private:
	using LargeData = anydata_internal_::LargeData;
	static constexpr std::size_t maxSize = maxSize_ < sizeof(LargeData) ? sizeof(LargeData) : maxSize_;

	static_assert(maxSize > 0, "AnyData: maxSize must be greater than 0");

public:
	~AnyData() {
		if(functions != nullptr) {
			functions->free(buffer.data());
		}
	}

	template <typename T>
	AnyData(T && object,
		typename std::enable_if<(sizeof(typename anydata_internal_::RemoveCvRef<T>::Type) <= maxSize)>::type * = 0)
		: functions(anydata_internal_::getAnyDataFunctions<T>()), buffer() {
		using U = typename std::remove_reference<T>::type;
		new (buffer.data()) U(std::forward<T>(object));
	}

	template <typename T>
	AnyData(T && object,
		typename std::enable_if<(sizeof(typename anydata_internal_::RemoveCvRef<T>::Type) > maxSize)>::type * = 0)
		: functions(anydata_internal_::getAnyDataFunctions<LargeData>()), buffer() {
		new (buffer.data()) LargeData(std::forward<T>(object));
	}

	AnyData(AnyData && other) : functions(other.functions), buffer() {
		if(functions != nullptr) {
			functions->moveConstruct(other.buffer.data(), buffer.data());
		}
	}

	AnyData(const AnyData & other) = delete;
	AnyData(AnyData & other) = delete;
	AnyData & operator = (const AnyData & other) = delete;
	AnyData & operator = (AnyData && other) = delete;

	template <typename T>
	const T & get() const {
		return *(T *)getAddress();
	}

	const void * getAddress() const {
		assert(functions != nullptr);

		if(! isLargerData()) {
			return buffer.data();
		}
		else {
			return ((const LargeData *)buffer.data())->getAddress();
		}
	}

	template <typename T>
	bool isType() const {
		if(! isLargerData()) {
			return anydata_internal_::getAnyDataFunctions<T>() == functions;
		}
		else {
			return ((const LargeData *)buffer.data())->isType<T>();
		}
	}

	template <typename T>
	operator T & () const {
		return *(T *)getAddress();
	}

	template <typename T>
	operator T * () const {
		return (T *)getAddress();
	}

private:
	bool isLargerData() const {
		return functions == anydata_internal_::getAnyDataFunctions<LargeData>();
	}

private:
	const anydata_internal_::AnyDataFunctions * functions;
	std::array<std::uint8_t, maxSize> buffer;
};

template <typename ...Ts>
constexpr std::size_t maxSizeOf()
{
	return anydata_internal_::MaxSizeOf<Ts...>::value;
}

} //namespace eventpp


#endif

