# eventpp 库介绍

eventpp 有三个核心的类：CallbackList, EventDispatcher 和 EventQueue ，三者各有其特定的用途。

## CallbackList 类

CallbackList 是 eventpp 中最基础的类，EventDispatcher 和 EventQueue 类的实现都建立在 CallbackList 之上。

CallbackList 会维护一个回调函数列表。当一个 CallbackList 被调用时，该 CallbackList 会逐个调用其中的回调函数。CallbackList 可以类比为 Qt 中的信号槽系统，以及某些 Windows API 中的回调函数指针（比如 `ReadFileEx` 中的 IpCompletionRoutine ）

这里的“回调函数”可以是任何能够回调的目标——函数、函数指针、成员函数指针、lambda表达式、函数对象等。

eventpp 中的 CallbackList 相当于是 Qt 这类事件系统中的 “信号”，但 eventpp 中并没有特定的相当于 “槽”（回调函数）的东西 —— 在 eventpp 中，任何可调用的目标都可以是槽（回调函数）

当应用场景中的事件种类很少时，直接用 CallbackList 即可满足需求：为每个事件创建一个 CallbackList，每个 CallbackList 都可以有不同的原型（ prototype ），例如：

```c++
eventpp::CallbackList<void()> onStart;
eventpp::CallbackList<void(MyStopReason)> onStop;
```

当程序中有成百上千个事件时（ GUI 和游戏程序中经常会有这么多的事件），上面这种写法就难以应付了——为每个事件都单独创建 CallbackList 肯定不是什么好选择。针对存在大量事件的场景，eventpp 设计了 EventDispatcher 

## EventDispatcher 类

EventDispatcher 类似于 `std::map<EventType, CallbackList>` ，能根据 EventType 寻找对应 CallbackList。

EventDispatcher 维护一个 `<EventType, CallbackList>` 映射表。在进行事件分发时， EventDispatcher 会根据事件类型（EventType）查找并调用对应的回调列表（CallbackList） 。该调用过程是同步的，监听器会在 `EventDispatcher::dispatch` 被调用时触发。

EventDispatcher 适用于事件种类繁多或无法预先确定事件数量的场景。每个事件都可以根据事件类型进行分类，例如：

```c++
enum class MyEventType
{
    redraw,
    mouseDown,
    mouseUp,
    // 后面也许还有 2000 个事件
};

struct MyEvent
{
    MyEventType type;
    // 此处定义所有事件可能会需要的数据
};

struct MyEventPolicies
{
    static MyEventType getEvent(const MyEvent &e) {
        return e.type;
    }
};

eventpp::EventDispatcher<MyEventType, void(const MyEvent &), MyEventPolicies> dispatcher;
dispatcher.dispatch(MyEvent { MyEventType::redraw });
```

（提示：若想了解上面代码中的 `MyEventPolicies` ，请阅读https://github.com/wqking/eventpp/blob/master/doc/policies.md 文档。本篇文档中可以暂且只关注分发器： `eventpp::EventDispatcher<MyEventType, void(const MyEvent &)> dispatcher`）

EventDispatcher 的缺点是，分发器中所有事件都必须有相同的回调原型（例如示例代码中的 `void(const MyEvnet &)`）。通常可以通过下面的方案来规避该缺点：将 Event 基类作为回调函数的参数，然后所有的事件都通过继承 Event 来传递他们自己的数据。在示例代码中，`MyEvent` 就是事件的基类，回调函数接收 `const MyEvent &` 。

## EventQueue 类

EventQueue 包含了 EventDispatcher 的所有特性，并在此基础上添加了事件队列。注意：EventQueue 并不是 EventDispatcher 的子类，因此不要进行类型转换。

EventQueue 是异步的。事件会在调用 `EventQueue::enqueue` 时被保存在队列中，并在 `EventQueue::process` 被调用时被处理。

EventQueue 相当于 Qt 中的事件系统（QEvent）或 Windows API 中的消息处理（ message processing ）。

```c++
eventpp::EventQueue<int, void (const std::string &, const bool)> queue;

// 将事件加入队列。第一个参数永远是事件类型
// 监听器不会在进入队列期间被触发
queue.enqueue(3, "Hello", true);
queue.enqueue(5, "World", false);

// 处理事件队列，运行队列中的所有事件
queue.process();
```

## 线程安全

所有类都是线程安全的，可以在多个线程中同时调用所有的公共函数。如果出现问题，请到 eventpp 的项目仓库报告 bug。
本库能够确保每个单独函数调用的整体性，如 `EventDispatcher::appendListener`, `CallbackList::remove` ，但无法确保多个线程中的操作执行顺序。例如，若在某个线程分发一个事件的同一时刻，另一个线程移除了一个监听器（listener），则被移除的监听器仍有可能在其被移除之后触发。

## 异常安全

所有的类都不会抛出异常。但库代码所依赖的代码可能会在下列情况发生时抛出异常：

1. 内存耗尽，无法分配新的内存空间；
2. 监听器（回调函数）在复制、移动、对比或调用时抛出异常

几乎所有操作都有很强的异常安全性，这意味着下层数据抛出异常时能够保持其原始值。唯一的例外是 `EventQueue::process`：在抛出异常时，剩余的事件就不再会被分发了，而且队列会被清空。


