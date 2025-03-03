# EventQueue （事件队列）类参考手册

## 目录

- [描述](#description)
- [API 参考](#api-ref)
  - [头文件](#header)
  - [模板参数](#template-param)
  - [公共类型](#public-types)
  - [成员函数](#member-functions)
  - [内部类 EventQueue::DisableQueueNotify](#inner-class-eventqueue-disablequeuenotify)
- [内部数据结构](#internal-data-structure)

<a id="description"></a>

## 描述

EventQueue 在包含了 EventDispatcher 所有特性的基础上新增了事件队列特性。注意：EventQueue 并非派生自 EventDispatcher ，请勿尝试将 EventQueue 转换为 EventDispatcher 类型。

EventQueue 是异步的。事件会在调用 `EventQueue::enqueue` 时被缓存进 EventQueue 的事件队列，并在后续调用 `EventQueue::process` 时被调度。

EventQueue 相当于是 Qt 中的事件系统（QEvent），或 Windows API 中的信息处理（message processing）。

<a id="api-ref"></a>

## API 参考

<a id="header"></a>

### 头文件

eventpp/eventqueue.h

<a id="template-param"></a>

### 模板参数

```cpp
template <
	typename Event,
	typename Prototype,
	typename Policies = DefaultPolicies
>
class EventQueue;
```

EventQueue 的模板参数与 EventDispatcher 的模板参数完全一致。详细信息请参阅 EventDispatcher 文档。

<a id="public-types"></a>

### 公共类型

`QueuedEvent`：存储在队列中的事件的数据类型。其声明的伪代码表示如下：

```cpp
struct EventQueue::QueuedEvent
{
    EventType event;
    std::tuple<ArgTypes...> argument;
    
    // 获取事件
    EventType getEvent() const;
    
    // 获取索引为 N 的实参
    // 与 std::get<N>(queuedEvent.arguments) 相同
    template <std::size_t N>
    NthArgType getArgument() const;
};
```

`event` 是 EventQueue::Event ， `arguments` 是 `enqueue` 中传递的实参。

<a id="member-functions"></a>

### 成员函数

#### 构造函数

```cpp
EventQueue();
EventQueue(const EventQueue & other);
EventQueue(EventQueue && other) noexcept;
EventQueue & operator = (const EventQueue & other);
EventQueue & operator = (EventQueue && other) noexcept;
```

EventQueue 可以拷贝、移动、赋值和移动赋值

注意：已排入队列的事件无法被拷贝、移动、赋值和移动赋值，这些操作只会对监听器生效。这就意味着，已排入队列的事件不会在 EventQueue 被复制和赋值时复制。

#### enqueue

```cpp
template <typename ...A>
void enqueue(A && ...args);

template <typename T, typename ...A>
void enqueue(T && first, A && ...args);
```

将一个事件加入事件队列。事件的类型包含在传给 `enqueue` 函数的实参中。

所有可拷贝实参都会被拷贝到内部的数据结构中。所有不可拷贝但可移动的实参都会被移动。

EventQueue 的参数必须满足可拷贝和可移动两项中的一项。

如果定义的参数是基类的引用，却传入了一个派生类的对象，那么就只会保存基类，派生部分则会丢失。这种情况下一般可以使用共享指针来满足相关需求。

如果参数是指针，那么 EventQueue 就只会存储指针。该指针所指向的对象必须在事件处理结束之前都是可用的。

`enqueue` 会唤醒所有由 `wait` 或 `waitFor` 阻塞的线程。

该函数的时间复杂度为 O(1) 。

这两个重载函数略有不同，具体的用法取决于 `ArgumentPassingMode` 策略。详情请阅读https://github.com/wqking/eventpp/blob/master/doc/policies.md 文档。

注意：参数的生命周期可能比预期的要长。`EventQueue`将参数拷贝到内部数据结构，当事件被派发以后，该数据被缓存以备后续使用，因此参数在数据重新使用前不会被释放。这么做是为了性能优化。这通常不是问题，但如果你用shared pointer来传递大量数据，那么这些数据在内存中存在更长的事件。

#### process

```cpp
bool process();
```

处理事件队列。所有事件队列中的事件都会被一次性调度，并从队列中移除。

若有事件被处理，该函数将返回 true 。返回 false 则代表未处理任何事件。

在哪个线程中调用了 `process` ，所有的监听器就会在哪个线程中执行。

在 `process()` 执行过程中新添加进队列的事件不会在当前的 `process()` 中被调度。

`process()` 能高效完成单线程事件处理，其会在当前线程中处理队列中的所有事件。若想在多个线程中高效处理事件，请使用 `processOne()` 。

注意：若 `process()` 被同时在多个线程中调用，事件队列中的事件也将只被处理一次。

#### processOne

```c++
bool processOne();
```

处理事件队列中的一个事件。该函数将会调度事件队列中的第一个事件，并将该事件移除队列。

若事件成功被处理，该函数返回 true ，否则返回 false 。

在哪个线程中调用了 `processOne()` ，监听器就会在哪个线程中执行。

在执行 `processOne()` 时被添加进队列的新事件将不会在当前的 `processOne()` 过程中被调度。

若有多个线程处理事件，`processOne()` 要比 `process()` 更高效，因为其能将事件处理分散到不同的线程中执行。但若只有一个事件处理线程，则 `process()` 更高效。

注意：若 `processOne()` 被同时在多个线程中调用，那么事件队列中的事件也只会被处理一次。

#### processIf

```c++
template <typename Predictor>
bool processIf(Predictor && predictor);
```

处理事件队列。在处理一个事件前，该事件将先被传给 `predictor` ，仅当 `predictor` 返回 true 时，该事件才会被处理。若返回 false ，则会跳过该事件继续处理后面的事件。被跳过的事件则会被继续保留在队列中。

`predictor` 是一个可调用对象（函数， lambda 表达式等），其接收的参数与 `EventQueue::enqueue` 接收的参数一致或不接收参数，返回值应为 bool 类型值。 eventpp 会正确地传递所有参数。若有事件被处理，该函数将返回 true 。返回 false 则代表未处理任何事件。

`processIf` 在下面这些场景中很有用：

- 在特定的线程中处理特定的事件。例如在 GUI 应用中，UI 相关事件只应该在主线程中处理。则在该场景中， `predictor` 可以只对 UI 事件返回 true ，而对所有的非 UI 事件返回 false 。

#### processUntil

```cpp
template <typename Predictor>
bool processUnitl(Predictor && predictor);
```

处理事件队列。在处理一个事件前，该事件将先被传给 `predictor` ，若其返回 true ， `processUnitl` 将会立即停止事件处理并返回。若 `predictor` 返回 false ，则 `processUntil` 将继续处理事件。

`predictor` 是一个可调用对象（函数， lambda 表达式等），其接收的参数与 `EventQueue::enqueue` 接收的参数一致或不接收参数，返回值应为 bool 类型值。 eventpp 会正确地传递所有参数。若有事件被处理，该函数将返回 true 。返回 false 则代表未处理任何事件。

`processUnitl` 可通过限制事件处理时间来模拟“超时”。例如在游戏引擎中，一次事件处理时间要被限制在几毫秒之内，没处理完的事件需要留到下一个循环中进行处理。该需求就可以通过让 `predictor` 在超时的时候返回 true 来实现。

#### emptyQueue

```cpp
bool emptyQueue() const;
```

在事件队列中没有事件时返回 true ，否则返回 false 。

注意：在多线程环境下，空状态可能在该函数返回后马上发生改变。

注意：不要用 `while(!eventQueue.emptyQueue()) {}` 的写法来写事件循环。因为编译器会内联代码，导致该循环永远检查不到空状态变化，进而造成死循环。安全的写法应该是 `while(eventQueue.waitFor(std::chrono::nanoseconds(0)));`

#### clearEvents

```cpp
void clearEvents();
```

在不调度事件的情况下清空队列中的所有事件。

该函数可用于清空已排队事件中的引用（比如共享指针），以避免循环引用。

#### wait

```cpp
void wait() const;
```

`wait` 将让当前线程持续阻塞，直至队列非空（加入了新的事件）。

注意：尽管 `wait` 在内部解决了假唤醒的问题，但并不能保证 ` wait` 返回后队列非空。

`wait` 在使用一个线程处理事件队列时很有用，用法如下：

```cpp
for(;;) {
    eventQueue.wait();
    eventQueue.process();
}
```

尽管上面的代码中不带 `wait` 也能正常运行，但那样做将浪费 CPU 性能。

#### waitFor

```cpp
template <class Rep, class Period>
bool waitFor(const std::chrono::duration<Rep, Period> & duration) const;
```

等待不超过 `duration` 所指定的超时时间。

当队列非空时返回 true ，当超时时返回 false 。

`waitFor` 在当事件队列处理线程需要做其他条件检查时很有用，例如：

```cpp
std::atomic<bool> shouldStop(false);
for(;;) {
    while(!eventQueue.waitFor(std::chrono::milliseconds(10) && !shouldStop.load());
    if(shouldStop.load()) {
        break;
    }
    
    eventQueue.process();
}
```

#### peekEvent

```cpp
bool peekEvent(EventQueue::QueuedEvent * queuedEvent);
```

从事件队列中取出一个事件。事件将在 `queuedEvent` 中返回。

```cpp
struct EventQueue::QueuedEvent
{
    TheEventType event;
    std::tuple<ArgumentTypes...> arguemnts;
};
```

`queuedEvent` 是一个 EventQueue::QueuedEvent 结构体。`event` 是 `EventQueue::Event` ，`arguments` 是 `enqueue` 中传入的参数。

该函数在事件队列为空时返回 false ，事件成功取回时返回 true 。

在函数返回后，原事件不会被移除，而会仍然留在队列中。

注意：`peekEvent` 无法和不可拷贝的事件参数一起使用。若 `peekEvent` 在有不可拷贝参数时被调用，会导致编译失败。

#### takeEvent

```cpp
bool takeEvent(EventQueue::QueuedEvent * queuedEvent);
```

从事件队列中取走一个事件，并将该事件从事件队列中移除。事件将在 `queuedEvent` 中返回。

该函数在事件队列为空时返回 false ，事件成功取回时返回 true 。

在函数返回后，原来的事件将被从事件队列中移除。

注意：`takeEvent` 可以和不可拷贝事件参数一起会用。

#### dispatch

```cpp
void dispatch(const QueuedEvent & queuedEvent);
```

调度由 `peekEvent` 或 `takeEvent` 返回的事件。

<a id="inner-class-eventqueue-disablequeuenotify"></a>

### 内部类 EventQueue::DisableQueueNotify 

`EventQueue::DisableQueueNotify` 是一个 RAII 类，其用于临时防止事件队列唤醒等待的线程。当存在 `DisableQueueNotify` 对象时，调用 `enqueue` 不会唤醒任何由 `wait` 阻塞的线程。当离开 `DisableQueueNotify` 的作用域时，事件队列就重新可被唤醒了。若存在超过一个 `DisableQueueNotify` 对象，线程就只能够在所有的对象都被销毁后才能重新可被唤醒。`DisableQueueNotify` 在需要批量向事件队列中加入事件时，能够有效提升性能。例如，在游戏引擎的主循环中，可以在一帧的开始时创建 `DisableQueueNotify` ，紧接着向队列中添加一系列事件，然后在这一帧的末尾销毁 `DisableQueueNotify` ，开始处理这一帧中添加的所有事件。

`DisableQueueNotify` 的实例化，需要传入指向事件队列的指针。示例代码如下：

```cpp
using EQ = eventpp::EventQueue<int, void()>;
EQ queue;
{
    EQ::DisableQueueNotify disableNotify(&queue);
    // 任何阻塞的线程都不会被下面的两行代码唤醒
    queue.enqueue(1);
    queue.enqueue(2);
}
// 任何阻塞的线程都会在此处被立即唤醒

// 因为这里没有 DisableQueueNotify 实例，因此任何阻塞线程都会被下面这行代码唤醒
queue.enqueue(3);
```



<a id="internal-data-structure"></a>

## 内部数据结构

EventQueue 使用三个 `std::list` 来管理事件队列。

第一个忙列表（ busy list ）维护已入列事件的所有节点。

第二个等待列表（ idle list ）维护所有等待中的节点。在一个事件完成调度并被从队列中移除后，EventQueue 将把没有用过的节点移入等待列表，而不是直接释放相应的内存。这能够改善性能并避免产生内存碎片。

第三个列表是在 `process()` 函数中使用的本地临时列表（ local temporary list ）。在一次处理的过程中，忙列表会被交换（ swap ）到临时列表，所有事件都是在临时列表中被调度的。在这之后，临时列表会被返回，并追加到等待列表中。

