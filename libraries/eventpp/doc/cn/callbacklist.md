# CallbackList 类参考手册

## 目录

* [描述](#a2_1)
* [API 参考](#a2_2)
  * [头文件](#a3_1)
  * [模板参数](#a3_2)
  * [公共类型](#a3_3)
  * [成员函数](#a3_4)
* [嵌套回调函数安全性](#a2_3)
* [时间复杂度](#a2_4)
* [内部数据结构](#a2_5)

<a id="a2_1"></a>

## 描述

CallbackList 是 eventpp 中最为核心、基础的类。EventDispatcher、EventQueue 都是以 CallbackList 类为基础开发的。

CallbackList 内部维护一个回调函数的列表。在 CallbackList 被调用时，其会逐个调用列表中的所有回调函数。可以将 CallbackList 看做 Qt 中的信号/槽系统，或某些 Windows API 中的回调函数指针（例如 `ReadFileEx` 中的 IpCompletionRoutine ）。  
回调函数可以是任何回调目标 —— 函数、函数指针、指向成员函数的指针、lambda 表达式、函数对象等。

<a id="a2_2"></a>

## API 参考

<a id="a3_1"></a>
### 头文件

eventpp/callbacklist.h

<a id="a3_2"></a>
### 模板参数

```c++
template <
	typename Prototype,
	typename Policies = DefaultPolicies
>
class CallbackList;
```

`Prototype`：回调函数原型。该参数应为 C++ 函数类型，如 `void(int, std::string, const MyClass *)`。  
`Policies`：用于配置和扩展回调函数列表的规则。默认值为 `DefaultPolicies` 。详见 [policy 文档](https://github.com/marsCatXdu/eventpp/blob/master/doc/policies.md)

<a id="a3_3"></a>
### 公共类型

`Handle`：由 `append`、`prepend`、`insert` 函数返回的句柄类型。句柄可用于插入或移除一个回调函数。可以通过将句柄转为 bool 类型来检查其是否为空：句柄为空时值为 `false` 。`Handle` 时可拷贝的。

<a id="a3_4"></a>
### 成员函数

#### 构造函数

```C++
CallbackList() noexcept;
CallbackList(const CallbackList & other);
CallbackList(CallbackList && other) noexcept;
CallbackList & operator = (const CallbackList & other);
CallbackList & operator = (CallbackList && other) noexcept;
void swap(CallbackList & other) noexcept;
```

CallbackList 可以被拷贝、move、赋值、move 赋值和交换。

#### empty

```c++
bool empty() const;
```

当回调列表为空时返回 true 。  
提示：在多线程程序中，该函数的返回值无法保证确定就是列表的真实状态。可能会出现刚刚返回 true 之后列表马上变为非空的情况，反之亦然。

#### bool 转换运算符

```c++
operator bool() const;
```

若回调列表非空则返回 true。  
借助该运算符，能够实现在条件语句中使用 CallbackList 实例

#### append

```c++
Handle append(const Callback & callback);
```

向回调函数列表中添加回调函数。  
新的回调函数会被加在回调函数列表的末尾。  
该函数返回一个代表回调函数的句柄。该句柄能够用于移除该回调函数，或在该回调函数前插入其他的回调函数。  
如果`append`在回调函数列表执行的过程中被其他的回调函数调用，则新添加的回调函数一定不会在该回调函数列表执行的过程中被执行。  
该函数的时间复杂度为 O(1) 。

#### prepend

```c++
Handle prepend(const Callback & callback);
```

向回调函数列表中添加回调函数。  
回调函数将被加在回调函数列表的最前端。  
该函数会返回一个代表回调函数的句柄（handler）。该句柄可被用于移除该回调函数，也可用于在该回调函数前插入其他回调函数。
如果 `prepend` 在回调函数列表执行的过程中被其他回调函数调用，则新添加的回调函数一定不会在该回调函数列表执行的过程中被执行。  
该函数的时间复杂度为 O(1) 。

#### insert

```c++
Handle insert(const Callback & callback, const Handle & before);
```

将 *callback* 插入到回调列表中 *before* 前面的一个位置处。若找不到 *before* ，则 *callback* 会被加到回调列表的末尾。  
该函数返回一个代表回调函数的句柄。该句柄可被用于移除该回调函数，也可用于在该回调函数前插入其他回调函数。  
如果 `insert` 是在回调函数列表执行的过程中被其他回调函数调用的，则新添加的回调函数一定不会在该回调函数列表执行的过程中被执行。  
该函数的时间复杂度为 O(1) 。

提示：该函数的调用者必须提前确定 `before` 是由 `this` 所指向的 CallbackList 调用的。如果不能确定，可以用 `ownsHandle` 函数来检查 `before` 是否属于 `this` CallbackList 。`insert` 函数仅能在 `ownsHandle(before)` 返回 true 的时候被调用，否则可能引发未定义的行为并带来诡异的 bug 。  
需要确保只在 `assert(ownsHandle(before))` 时 `insert` ，但出于性能方面的考量，发布的代码中没有包含相关的检查。

#### remove

```c++
bool remove(const Handle & handle);
```

从回调函数列表中移除 *handle* 指向的回调函数。  
移除成功会返回 true ，找不到回调函数则会返回 false 。  
该函数的时间复杂度为 O(1)

提示：`handle` 必须是由 `this` 指向的 CallbackList 所创建的。更多细节请查看 `insert` 中的“提示”部分

#### ownsHandle

```c++
bool ownsHandle(const Handle & handle) const;
```

当 `handle` 是由当前 CallbackList 创建时返回 true，否则返回 false 。  
该函数时间复杂度为 O(N)

#### forEach

```c++
template <typename Func>
void forEach(Func && func) const;
```

对所有的回调函数使用 `func` 。  
`func` 可以是下面两种原型的其中一种：

```C++
AnyReturnType func(const CallbackList::Handle &, const CallbackList::Callback &);
AnyReturnType func(const CallbackList:Callback &);
```

提示：`func` 可以安全地移除和添加新的回调函数

#### forEachIf

```c++
template <typename Func>
bool forEachIf(Func && func) const;
```

对所有的回调函数使用 `func` 。 `func` 必须返回一个 bool 值，若返回值为 false ，则 forEachIf 将会立即停止循环。  
当所有回调函数都被触发后，或未找到 `event` 时，该函数会返回 true 。当 `func` 返回 `false` 时返回 `false`

#### 调用运算符

```c++
void operator() (Args ...args) const;
```

触发回调函数列表中所有回调函数的运行。  
回调函数会被用 `args` 参数作为参数调用。  
回调函数会在 `operator()` 所在的线程中调用。

<a id="a2_3"></a>
## 嵌套回调函数安全性

1. 若一个回调函数在自身运行时，向回调函数列表中添加了新的回调函数，则新的回调函数不会在本次回调函数列表执行的过程中被执行。该行为是由一个 64 位无符号类型的计数器所保证的。如果在一次触发的过程中使该计数器溢出到 0 则会破坏上述的行为。
2. 所有在回调函数列表运行过程中被移除的回调函数都不会被触发运行。
3. 上面这几点在多线程情况下未必成立。也就是说，如果一个线程正在执行回调函数列表，如果另一个线程的函数向这个列表中添加或移除了函数，则被添加或移除的函数仍有可能在此次回调函数列表执行的过程中被执行。

<a id="a2_4"></a>
## 时间复杂度

- `append`：O(1)
- `prepend`：O(1)
- `insert`：O(1)
- `remove`：O(1)

<a id="a2_5"></a>

## 内部数据结构

CallbackList 使用双向链表管理回调函数。  
每个节点都使用共享指针（shared pointer）连接。使用共享指针可以实现在迭代的过程中移除节点。

