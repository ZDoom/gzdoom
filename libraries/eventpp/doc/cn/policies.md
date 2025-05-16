# 策略（Policies）

## 目录

- [介绍](#introduction)
- [策略](#policies)
  - [getEvent 函数](#getevent)
  - [canContinueInvoking 函数](#cancontinueinvoking)
  - [Mixins 类型](#mixins)
  - [Callback 类型](#callback)
  - [Threading 类型](#threading)
  - [ArgumentPassingMode 类型](#argumentpassingmode)
  - [Map 模板](#map)
  - [QueueList 模板](#queuelist)
- [如何使用策略](#how-to-use-policies)

<a id="introduction"></a>

## 介绍

eventpp 使用了基于策略的设计来配置和扩展各个组件的行为。EventDispatcher、EventQueue、CallbackList 的最后一个模板参数就是策略类。这三个类都有名为 `DefaultPolicies` 的默认策略类。

一项策略可以是策略类中的一个类型或一个静态成员函数。所有策略都必须是 public 的，所以通常可以用 `struct` 来定义策略类。

所有策略都是可选的。若省略了一个策略，那么这项策略就将使用其默认值。实际上，`DefaultPolicies` 本身就是一个空结构体。

EventDispatcher、EventQueue、CallbackList 这三个类使用了相同的策略机制，尽管不是所有的类都需要相同的策略。

<a id="policies"></a>

## 策略

<a id="getevent"></a>

### 函数： getEvent

**原型**：`static EventKey getEvent(const Args &...)` 。该函数接收与 `EventDispatcher::dispatch` 和 `EventQueue::enqueue` 相同的参数，且必须返回一个事件类型。

**默认值**：默认实现是返回 `getEvent` 的第一个实参。

**适用于**：EventDispatcher, EventQueue

evetpp 将 `EventDispatcher::dispatch` 和 `EventQueue::enqueue` 的所有实参（这两个函数的参数相同）都转发给 `getEvent` 以获取事件类型，然后再触发执行该事件对应的回调函数列表。

`getEvnet` 既可以是模板函数，也可以是非模板函数。只要 `getEvent` 能够使用与 `EventDispatcher::dispatch` 和 `EventQueue::enqueue` 相同的参数调用即可。

示例代码如下：

```cpp
// 定义用于保存所有参数的事件结构
struct MyEvent {
    int type;
    std::string message;
    int param;
};

// 为调度器定义如何分解事件类型的策略
struct MyEventPolicies
{
    static int getEvent(const MyEvent & e, bool /*b*/) {
        return e.type;
    }
};

// 将 MyEventPolicies 作为 EventDispatcher 的第三个模板参数
// 注意：第一个模板参数是整型的事件类型，而非 MyEvent。
eventpp::EventDispatcher<
    int,
    void (const MyEvent &, bool),
    MyEventPolicies
> dispatcher;

// 添加一个监听器。
// 注意：第一个参数是整型的事件类型，而非 MyEvent。
dispatcher.appendListener(3, [](const MyEvent & e, bool b) {
    std::cout
        << std::boolalpha
        << "Got event 3" << std::endl
        << "Event::type is " << e.type << std::endl
        << "Event::message is " << e.message << std::endl
        << "Event::param is " << e.param << std::endl
        << "b is " << b << std::endl
    ;
});

// 调度事件。第一个参数是 Event
dispatcher.dispatch(MyEvent { 3, "Hello world", 38 }, true);
```

<a id="cancontinueinvoking"></a>

### 函数： canContinueInvoking

**原型**：`static bool canContinueInvoking(const Args &...)` 。该函数接收与 `EventDispatcher::dispatch` 和 `EventQueue::enqueue` 相同的参数，且必须在事件调度或回调列表调用可以继续进行的时候返回 true ，在调用需要停止的时候返回 false 。

**默认值**：默认实现总是返回 true 。

**适用于**：CallbackList, EventDispatcher, EventQueue

示例代码如下：

```cpp
struct MyEvent {
    MyEvent() : type(0), canceled(false) {
    }
    explicit MyEvent(const int type)
        : type(type), canceled(false) {
    }

    int type;
    mutable bool canceled;
};

struct MyEventPolicies
{
    static int getEvent(const MyEvent & e) {
        return e.type;
    }

    static bool canContinueInvoking(const MyEvent & e) {
        return ! e.canceled;
    }
};

eventpp::EventDispatcher<int, void (const MyEvent &), MyEventPolicies> dispatcher;

dispatcher.appendListener(3, [](const MyEvent & e) {
    std::cout << "Got event 3" << std::endl;
    e.canceled = true;
});
dispatcher.appendListener(3, [](const MyEvent & e) {
    std::cout << "Should not get this event 3" << std::endl;
});

dispatcher.dispatch(MyEvent(3));
```

<a id="mixins"></a>

### 类型： Mixins

**默认值**：`using Mixins = eventpp::MixinList<>` 。未启用任何 mixin 。

**适用于**：CallbackList, EventDispatcher, EventQueue

Mixin 用于向 EventDispatcher / EventQueue 继承层次中注入代码，以扩展它们的功能。更多细节请参阅 https://github.com/wqking/eventpp/blob/master/doc/mixins.md

<a id="callback"></a>

### 类型： Callback

**默认值**：`using Callback = std::function<Parameters of callback>` 。

**适用于**：CallbackList, EventDispatcher, EventQueue

`Callback` 是用于在底层维护回调函数。默认是 `std::function`

<a id="threading"></a>

### 类型： Threading

**默认值**：`using Threading = eventpp::MultipleThreading` 。

**适用于**：CallbackList, EventDispatcher, EventQueue, HeterCallbackList, HeterEventDispatcher, HeterEventQueue.

`Threading` 控制线程执行模型。默认是“多线程”（ Multiple Threading ）。可取值：

- `MultipleThreading` ： 使用 mutex 保护核心数据。该选项为默认值。
- `SingleThreading` ： 不保护核心数据，且这些数据无法从其他线程访问

一个典型的 `Threading` 类型如下：

```cpp
struct MultipleThreading
{
    using Mutex = std::mutex;
    
    template <typename T>
    using Atomic = std::atomic<T>;
    
    using ConditionVariable = std::condition_variable;
};
```

对于 `SingleThreading` 而言，所有的 `Mutex` 、 `Atomic` 和 `ConditionVariable` 类型都是不会起任何作用的假类型。

对于多线程而言，默认的 `Mutex` 是 `std::mutex` 。 eventpp 也提供了一个使用自旋锁作为互斥量的 `SpinLock` 类。

当只有较少的线程时（和 CPU 核心数差不多的线程数），`eventpp::SpinLock` 的性能比 `std::mutex` 更高一些。当线程数超过 CPU 核心数时， `eventpp::SpinLock` 的性能弱于 `std::mutex` 。

基准测试相关数据请参阅：https://github.com/wqking/eventpp/blob/master/doc/benchmark.md

下面是使用 `SpinLock` 的示例代码：

```cpp
struct MultipleThreadingSpinLock
{
	using Mutex = eventpp::SpinLock;
    
    template <typename T>
    using Atomic = std::atomic<T>;
    
    using ConditionVariable = std::condition_variable;
};
struct MyEventPolicies {
    using Threading = MultipleThreadingSpinLock;
};
eventpp::EventDispatcher<int, void (), MyEventPolicies> dispatcher;
eventpp::CallbackList<void (), MyEventPolicies> callbackList;
```

`eventpp` 还提供了一个简化易用的自定义线程管理的模板类。

```cpp
template <
	typename Mutex_,
	template <typename > class Atomic_ = sd::atomic,
    typename ConditionVariable_ = std::condition_variable
>
struct GeneralThreading
{
    using Mutex = Mutex_;
    
    template <typename T>
    using Atomic = Atomic_<T>;
    
    using ConditionVariable = ConditionVariable_;
};
```

因此前面自旋锁的示例代码可以重写为

```cpp
struct MyEventPolicies {
    using Threading = eventpp::GeneralThreading<eventpp::SpinLock>;
};
eventpp::EventDispatcher<int, void (), MyEventPolicies> dispatcher;
eventpp::CallbackList<void (), MyEventPolicies> callbackList;
```

<a id="argumentpassingmode"></a>

### 类型： ArgumentPassingMode

**默认值**：`using ArgumentPassingMode = ArgumentPassingAutoDetect` 。

**适用于**：EventDispatcher, EventQueue

`ArgumentPassingMode` 是实参传递的模式。默认值是 `ArgumentPassingAutoDetect` 。

示例代码如下。假设我们有一个调度器

```cpp
eventpp::EventDispatcher<int, void(int, const std::string &)> dispatcher;
```

事件类型是 `int` 。

监听器的第一个参数也是 `int` 。根据具体的事件调度方式，监听器的第一个参数可以是事件类型，也可以是一个额外的参数。

```cpp
dispatcher.dispatch(3, "hello");
```

事件 3 会被使用一个参数 "hello" 调度，监听器将会被使用参数 `(3, "hello")` 触发执行，第一个参数是事件类型。

```cpp
dispatcher.dispatch(3, 8, "hello");
```

事件 3 会被使用两个参数 8 和 "hello" 调度，监听器将会被使用参数 `(8, "hello")` 触发执行，第一个参数就是额外参数，此时的事件类型参数将被忽略。

因此，在默认情况下，EventDispatcher 会自动监测 `dispatch` 的参数数量和监听器原型，以决定是否使用事件类型来调用监听器。

默认规则简便、宽松但容易出错。可以通过 `ArgumentPassingMode` 策略控制具体的行为

```cpp
struct ArgumentPassingAutoDetect;
struct ArgumentPassingIncludeEvent;
struct ArgumentPassingExcludeEvent;
```

`ArgumentPassingAutoDetect`：默认策略。自动检查是否要传递事件类型。

`ArgumentPassingIncludeEvent`：总是传递事件类型。参数数量不符会导致编译失败。

`ArgumentPassingExcludeEvent`：总是忽略且不会传递事件类型。参数数量不符会导致编译失败。

假设监听器原型有 P 个参数，`dispatch` 中的参数数量（包括事件类型在内）为 D ，则 P 和 D 的关系为：

对于 `ArgumentPassingAutoDetect`：P == D 或 P + 1 == D

对于 `ArgumentPassingIncludeEvent`：P == D

对于 `ArgumentPassingExcludeEvent`： P + 1 == D

**注意**：同样的规则也适用于 `EventQueue::enqueue` ，因为 `enqueue` 的参数与 `dispatch` 相同。

参数传递模式的示例代码如下：

```cpp
struct MyPolicies
{
    using ArgumentPassingMode = ArgumentPassingAutoDetect;
};
eventpp::EventDispatcher<
    int,
    void(int, const std::string &),
    MyPolicies
> dispatcher;
// 或用下面的简便写法
//eventpp::EventDispatcher<int, void(int, const std::string &)> dispatcher;
dispatcher.dispatch(3, "hello"); // 编译通过
dispatcher.dispatch(3, 8, "hello"); // 编译通过
```

```cpp
struct MyPolicies
{
    using ArgumentPassingMode = ArgumentPassingIncludeEvent;
};
eventpp::EventDispatcher<
    int,
    void(int, const std::string &),
    MyPolicies
> dispatcher;
dispatcher.dispatch(3, "hello"); // 编译通过
//dispatcher.dispatch(3, 8, "hello"); // 编译失败
```

```cpp
struct MyPolicies
{
    using ArgumentPassingMode = ArgumentPassingExcludeEvent;
};
eventpp::EventDispatcher<
    int,
    void(int, const std::string &),
    MyPolicies
> dispatcher;
//dispatcher.dispatch(3, "hello"); // 编译失败
dispatcher.dispatch(3, 8, "hello"); // 编译通过
```

<a id="map"></a>

### 模板： Map

**原型**：

```cpp
template <typename Key, typename T>
using Map = // std::map <Key, T> 或其他 map 类型
```

**默认值**：自动确定

**应用于**：EventDispatcher, EventQueue

`Map` 是 EventDispatcher 和 EventQueue 用于维护底层键值对（事件类型，CallbackList）的关联容器类型。

`Map` 是有两个参数的模板，两个参数分别是键和值。

`Map` 必须能够支持 `[]`、`find()`、`end()` 操作。

若没有指定 `Map` ，eventpp 会自动确定类型。若事件类型支持 `std::hash` 会使用 `std::unordered_map` ，否则会使用 `std::map`

<a id="queuelist"></a>

### 模板： QueueList

**原型**：

```cpp
template <typename Item>
using QueueList = std::list<Item>;
```

**默认值**：`std::list`

**应用于**：EventQueue

`QueueList` 用于管理 EventQueue 内部的事件，作为队列使用。事件会被追加到 `QueueList` 的末尾，当被处理时，事件会从 `QueueList` 的头部弹出。

使用不同的 `QueueList` 能够更好地控制队列。例如，若 `QueueList` 能够管理事件的顺序，那么队列中的事件就能以不同于加入顺序的新顺序被处理。

一个 `QueueList` 不需要实现 `std::list` 的所有成员，其只需要实现下面的类型和函数即可：

```cpp
type iterator;
type const_iterator;
bool empty() const;
iterator begin();
const_iterator begin() const;
iterator end();
const_iterator end() const;
const_reference front() const;
void swap(QueueList & other);
void emplace_back();
void splice(const_iterator pos, QueueList & other );
void splice(const_iterator pos, QueueList & other, const_iterator it);
```

eventpp 中的有序队列列表 OrderedQueueList 就是一个应用实例。详细内容请阅 https://github.com/wqking/eventpp/blob/master/doc/orderedqueuelist.md

<a id="how-to-use-policies"></a>

## 如何使用策略

想要使用策略，只需要声明一个结构体，在其中定义策略然后传递给 CallbackList, EventDispatcher 或 EventQueue 即可。

```cpp
struct MyPolicies // 结构体的名字并不重要
{
    template <typename ...Args>
    static int getEvent(const MyEvent & e, const Args &...) {
        return e.type;
    }
};
EventDispatcher<int, void(const MyEvent &), MyPolicies> dispatcher;
```

上面的示例代码展示了一个重定义了 `getEvent` 策略类，除该策略外的其他策略都保持默认值。