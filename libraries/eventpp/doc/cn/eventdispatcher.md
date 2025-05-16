# EventDispatcher （事件调度器）类参考手册

## 目录

- [描述](#description)
- [API 参考](#api-ref)
  - [头文件](#header)
  - [模板参数](#template-param)
  - [公共类型](#public-types)
  - [成员函数](#member-functions)
- [嵌套监听器安全](#nested-listener-safety)
- [时间复杂度](#time-complexities)
- [内部数据结构](#internal-data-structure)

<a id="description"></a>

## 描述

EventDispatcher 类似于 `std::map<EventType, CallbackList>` 。

EventDispatcher 维护一个 `<EventType, CallbackList>` 映射表。在进行事件调度时， EventDispatcher 会根据事件类型（EventType）查找并调用对应的回调列表（CallbackList） 。该调用过程是同步的，监听器会在 `EventDispatcher::dispatch` 被调用时触发。

<a id="api-ref"></a>

## API 参考

<a id="header"></a>

### 头文件

eventpp/eventdispatcher.h

<a id="template-param"></a>

### 模板参数

```c++
template <
    typename Event,
    typename Prototype,
    typename Policies = DefaultPolicies
>
class EventDispatcher;
```

`Event`：事件的类型。该类型用于识别事件，具有相同类型的事件就是同一个事件。事件类型必须是能在 `std::map` 或 `std::unordered_map` 中用作 key 的类型，所以该类型应该要么可以使用 `operator <` 进行比较，要么派生自 `std::hash`。

`Prototype`：监听器的原型。其应为 C++ 函数类型，例如 `void(int, std::string, const MyClass *)`。

`Policies`：配置和扩展调度器的规则。默认值是 `DefaultPolicies`。 详情请阅读https://github.com/wqking/eventpp/blob/master/doc/policies.md 文档。

<a id="public-types"></a>

### 公共类型

`Handle`：由 `appendListener`、`prependListener`、`insertListener` 函数返回的 `Handle` （句柄）类型。可用于插入或移除一个监听器。可以通过将句柄转为 bool 类型来检查其是否为空：句柄为空时值为 `false` 。`Handle` 时可拷贝的。

`Callback`：回调函数存储类型。

`Event`：事件类型。

<a id="member-functions"></a>

### 成员函数

#### 构造函数

```c++
EventDispatcher();
EventDispatcher(const EventDispatcher & other);
EventDispatcher(EventDispatcher && other) noexcept;
EventDispatcher & operator = (const EventDispatcher & other);
EventDispatcher & operator = (EventDispatcher && other) noexcept;
void swap(EventDispatcher & other) noexcept;
```

EventDispatcher 可以拷贝、move、赋值、move 赋值和交换

#### appendListener

```c++
Handler appendListener(const Event & event, const Callback & callback);
```

向调度器中添加用于监听 *event* 事件的回调函数 *callback*。

监听器会被添加到监听器列表的末尾。

该函数会返回一个代表监听器的句柄。该句柄可用于移除该监听器，或在该监听器之前插入其他监听器。

若 `appendListener` 在一次事件调度的过程中被另一个监听器调用，则新的监听器不会在本次事件调度的过程中被触发。

如果同一个回调函数被添加了两次，则会调度器中出现两个相同的监听器。

该函数的时间复杂度为 O(1) + 在内部映射表中寻找事件的时间

#### prependListener

```c++
Handle prependListener(const Event & event, const Callback & callback);
```

向调度器中添加用于监听 *event* 事件的回调函数 *callback* 。

监听器会被添加到监听器列表的最前端。

该函数返回一个代表该监听器的句柄。该句柄可用于移除该监听器，也可以用于在该监听器前面再插入其他监听器。

若 `prependListener` 在一次事件调度的过程中被另一个监听器调用，则新的监听器不会在本次事件调度的过程中被触发。

该函数的时间复杂度为 O(1) + 在内部映射表中寻找事件的时间。

#### insertListener

```cpp
Handle insertListener(const Event & event, const Callback & callback, const Handle before);
```

将回调函数 *callback* 插入到调度器中 *before* 监听器前面的一个位置，以监听 *event* 事件。如果没找到 *before* ，*callback* 会被添加到监听器列表的末尾。

该函数会返回一个代表该监听器的句柄。该句柄可用于移除监听器，也可以用于在该监听器前面再插入其他监听器。

若 `insertListener` 在一次事件调度的过程中被另一个监听器调用，则用该函数新插入的监听器不会在本次事件调度的过程中被触发。

该函数的时间复杂度为 O(1) + 在内部映射表中寻找事件的时间。

注意：调用者必须确保 `before` 句柄是由 `this` EventDispatcher 创建的。若不能确定，则可用 `ownsHandle` 来检查 `before` 句柄是否属于 `this` EventDispatcher。 `insert` 函数只能在 `ownsHandle(before)` 返回 true 时才能被调用，否则会出现未定义的行为，导致出现诡异的 bug。

`insertListener` 会在自身的回调列表中进行一次 `assert(ownsHandle(before))` ，但出于性能方面的考量，在发布的代码中并不会进行检查。

#### removeListener

```cpp
bool removeListener(const Event & event, const Handle handle);
```

移除调度器中 *handle* 所指向的监听 *event* 的监听器。

若监听器被成功移除，该函数返回 tue。若未找到对应监听器则返回 false。

该函数的时间复杂度为 O(1) + 在内部映射表中寻找事件的时间。

注意：`handle` 必须是由 `this` EventDispatcher 创建的。详细说明请查看 `insertListener` 中的注意部分。

#### hasAnyListener

```cpp
bool hasAnyListener(const Event & event) const;
```

当存在与 `event` 对应的监听器时返回 ture ，否则返回 false 。

注意：在多线程中，该函数返回 true 时并不能保证 event 中一定存在监听器。回调列表可能在该函数返回 true 后就被清空了，反之亦然。该函数的时间复杂度为 O(1) + 在内部映射表中寻找事件的时间。

#### ownsHandle

```cpp
bool ownsHandle(const Event & event, const Handle & handle) const;
```

若 `handle` 是由 EventDispatcher 为 `event` 创建的，返回 true，否则返回 false。

时间复杂度为 O(N)

#### forEach

```cpp
template <typename Func>
void forEach(const Event & event, Func && func);
```

对 `event` 的所有监听器使用 `func`。

`func` 可以是下面两个属性中的一个：

```cpp
AnyReturnType func(const EventDispatcher::Handle &, const EventDispatcher::Callback &);
AnyReturnType func(const EventDispatcher::Callback &);
```

注意：`func` 可以安全地移除任何监听器或添加其他的监听器。

#### forEachIf

```cpp
template <typename Func>
bool forEachIf(const Event & event, Func && func);
```

对 `event` 的所有监听器使用 `func`。`func` 必须返回一个 bool 值，若返回值为 false， forEachIf 将立即停止循环。

当所有监听器都被触发执行，或未找到 `event` 时返回 `true` 。当 `func` 返回 `false` 时返回 `false` 。

#### dispatch

```cpp
void dispatch(Args ...args);

template <typename T>
void dispatch(T && first, Args ...args);
```

调度一个事件。`dispatch` 函数的参数是要被调度的事件类型。

所有的监听器都会被使用 `args` 参数调用。

该函数是同步的。所有监听器都会在调用 `dispatch` 的线程中被调用。

这两个重载函数略有区别，具体如何使用要根据 `ArgumentPassingMode` 策略而定。详情请阅读https://github.com/wqking/eventpp/blob/master/doc/policies.md 文档。

<a id="nested-listener-safety"></a>

## 嵌套监听器安全

1. 若一个监听器在一次调度的过程中，向调度器中加入另一个有着相同事件类型的监听器，则新的监听器可以保证不会在本次时间调度的过程中被调用。这是由一个 64 位无符号整数类型的计数器保证的，若在一次调度的过程中该计数器值溢出到 0 则会破坏该规则，但该规则将继续处理子序列调度。
2. 一次调度过程中移除的所有监听器都不会被触发。
3. 上面的两点在多线程中不成立。若在一个线程正在触发回调列表的时候，其他线程添加或移除了一个回调函数，则被添加或移除的这个回调函数可能会在本次触发执行的过程中执行。

<a id="time-complexities"></a>

## 时间复杂度

此处讨论的时间复杂度是操作回调列表中的监听器的复杂度，`n` 是监听器的数量。并不包含搜索 `std::map` 中事件所消耗的时间，该部分的时间复杂度为 O(log n) 。

- `appendListener`：O(1)
- `prependListener`：O(1)
- `insertListener`：O(1)
- `removeListener`：O(1)

<a id="internal-data-structure"></a>

## 内部数据结构

EventDispatcher 使用 CallbackList 来管理监听器回调函数。





















