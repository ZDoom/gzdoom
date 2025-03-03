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

#include "test.h"
#include "eventpp/eventqueue.h"
#include "eventpp/utilities/anydata.h"

#include <vector>
#include <map>
#include <string>
#include <any>

namespace {

template <typename T>
bool isWithinAnyData(const T & anyData, const void * address)
{
	return (std::ptrdiff_t)address >= (std::ptrdiff_t)&anyData
		&& (std::ptrdiff_t)address < (std::ptrdiff_t)&anyData + (std::ptrdiff_t)sizeof(T)
	;
}

template <typename T>
bool isWithinAnyData(const T & anyData)
{
	return isWithinAnyData(anyData, anyData.getAddress());
}

template <typename T>
bool isLargeAnyData(const T & anyData)
{
	return ! isWithinAnyData(anyData);
}

TEST_CASE("AnyData, maxSizeOf")
{
	REQUIRE(eventpp::maxSizeOf<
		std::uint8_t,
		std::uint16_t,
		std::uint32_t,
		std::uint64_t
		>() == sizeof(std::uint64_t)
	);
	REQUIRE(eventpp::maxSizeOf<
		std::uint64_t,
		std::uint32_t,
		std::uint8_t,
		std::uint16_t
		>() == sizeof(std::uint64_t)
	);
}

struct LargeDataBase
{
	std::array<void *, 100> largeData;
};

template <typename T>
bool isLargeDataBase()
{
	return sizeof(T) >= sizeof(LargeDataBase);
}

struct DataUniquePtr
{
	std::unique_ptr<std::string> ptr;
};

struct LargeDataUniquePtr : DataUniquePtr, LargeDataBase {};

TEMPLATE_TEST_CASE("AnyData, DataUniquePtr", "", DataUniquePtr, LargeDataUniquePtr)
{
	using Type = TestType;
	using MyAnyData = eventpp::AnyData<sizeof(DataUniquePtr)>;
	Type data {};
	data.ptr = std::unique_ptr<std::string>(new std::string("Hello"));
	REQUIRE(data.ptr);
	MyAnyData anyData { std::move(data) };
	REQUIRE(anyData.isType<Type>());
	REQUIRE(! data.ptr);
	REQUIRE(isLargeAnyData(anyData) == isLargeDataBase<Type>());
	REQUIRE(*anyData.get<DataUniquePtr>().ptr == "Hello");
	REQUIRE(*anyData.get<Type>().ptr == "Hello");
}

struct DataSharedPtr
{
	std::shared_ptr<std::string> ptr;
};

struct LargeDataSharedPtr : DataSharedPtr, LargeDataBase {};

TEMPLATE_TEST_CASE("AnyData, DataSharedPtr", "", DataSharedPtr, LargeDataSharedPtr)
{
	using Type = TestType;
	using MyAnyData = eventpp::AnyData<sizeof(DataSharedPtr)>;
	Type data {};
	data.ptr = std::make_shared<std::string>("Hello");
	REQUIRE(data.ptr.use_count() == 1);
	SECTION("copy") {
		MyAnyData anyData { data };
		REQUIRE(anyData.isType<Type>());
		REQUIRE(data.ptr.use_count() == 2);
		REQUIRE(anyData.get<DataSharedPtr>().ptr.use_count() == 2);
		REQUIRE(isLargeAnyData(anyData) == isLargeDataBase<Type>());
		REQUIRE(*anyData.get<DataSharedPtr>().ptr == "Hello");
		REQUIRE(*anyData.get<Type>().ptr == "Hello");
		*data.ptr = "world";
		REQUIRE(*anyData.get<DataSharedPtr>().ptr == "world");
		REQUIRE(*anyData.get<Type>().ptr == "world");
	}
	SECTION("move") {
		MyAnyData anyData { std::move(data) };
		REQUIRE(anyData.isType<Type>());
		REQUIRE(data.ptr.use_count() == 0);
		REQUIRE(anyData.get<DataSharedPtr>().ptr.use_count() == 1);
		REQUIRE(isLargeAnyData(anyData) == isLargeDataBase<Type>());
		REQUIRE(*anyData.get<DataSharedPtr>().ptr == "Hello");
		REQUIRE(*anyData.get<Type>().ptr == "Hello");
	}
}

struct LifeCounter
{
	int ctors;
};

struct DataLifeCounter
{
	explicit DataLifeCounter(LifeCounter * counter) : counter(counter) {
		++counter->ctors;
	}

	DataLifeCounter(const DataLifeCounter & other) : counter(other.counter) {
		++counter->ctors;
	}
	
	DataLifeCounter(DataLifeCounter && other) : counter(other.counter) {
		++counter->ctors;
	}
	
	~DataLifeCounter() {
		--counter->ctors;
	}

	LifeCounter * counter;
};

struct LargeDataLifeCounter : DataLifeCounter, LargeDataBase
{
	explicit LargeDataLifeCounter(LifeCounter * counter) : DataLifeCounter(counter), LargeDataBase() {
	}
};

TEMPLATE_TEST_CASE("AnyData, DataLifeCounter", "", DataLifeCounter, LargeDataLifeCounter)
{
	using Type = TestType;
	using MyAnyData = eventpp::AnyData<sizeof(DataLifeCounter)>;
	LifeCounter lifeCounter {};
	REQUIRE(lifeCounter.ctors == 0);
	{
		Type data { &lifeCounter };
		REQUIRE(lifeCounter.ctors == 1);
		{
			MyAnyData anyData { data };
			REQUIRE(anyData.isType<Type>());
			REQUIRE(lifeCounter.ctors == 2);
			REQUIRE(isLargeAnyData(anyData) == isLargeDataBase<Type>());
		}
		REQUIRE(lifeCounter.ctors == 1);
	}
	REQUIRE(lifeCounter.ctors == 0);
}

enum class EventType {
	mouse = 1,
	key = 2
};

struct Event {
	EventType type;

	explicit Event(const EventType type) : type(type) {
	}
};

struct EventKey : Event {
	int key;

	explicit EventKey(const int key) : Event(EventType::key), key(key) {
	}
};

struct EventMouse : Event {
	int x;
	int y;

	EventMouse(const int x, const int y) : Event(EventType::mouse), x(x), y(y) {
	}
};

struct LargeEventKey : EventKey, LargeDataBase {
	explicit LargeEventKey(const int key) : EventKey(key), LargeDataBase() {
	}
};

struct LargeEventMouse : EventMouse, LargeDataBase {
	explicit LargeEventMouse(const int x, const int y) : EventMouse(x, y), LargeDataBase() {
	}
};

constexpr std::size_t eventMaxSize = eventpp::maxSizeOf<
	Event, EventKey, EventMouse, std::string
>();

TEST_CASE("AnyData, use AnyData as callback argument")
{
	using Data = eventpp::AnyData<eventMaxSize>;
	eventpp::EventQueue<EventType, void (const Data &)> queue;
	queue.appendListener(EventType::key, [](const Data & value) {
		REQUIRE(value.isType<EventKey>());
		REQUIRE(value.get<EventKey>().type == EventType::key);
		REQUIRE(value.get<EventKey>().key == 5);
	});
	queue.enqueue(EventType::key, EventKey(5));
	queue.process();
}

TEST_CASE("AnyData, use Event as callback argument")
{
	using Data = eventpp::AnyData<eventMaxSize>;
	eventpp::EventQueue<EventType, void (const Data &)> queue;
	int expectedKey;
	int expectedX;
	int expectedY;
	queue.appendListener(EventType::key, [&expectedKey](const Event & event) {
		REQUIRE(event.type == EventType::key);
		REQUIRE(static_cast<const EventKey &>(event).key == expectedKey);
	});
	queue.appendListener(EventType::mouse, [&expectedX, &expectedY](const Event & event) {
		REQUIRE(event.type == EventType::mouse);
		REQUIRE(static_cast<const EventMouse &>(event).x == expectedX);
		REQUIRE(static_cast<const EventMouse &>(event).y == expectedY);
	});
	expectedKey = 5;
	queue.enqueue(EventType::key, EventKey(5));
	queue.process();
	expectedKey = 8;
	expectedX = 12345678;
	expectedY = 9876532;
	queue.enqueue(EventType::mouse, LargeEventMouse(12345678, 9876532));
	queue.enqueue(EventType::key, LargeEventKey(8));
	queue.process();
}

} // unnamed namespace
