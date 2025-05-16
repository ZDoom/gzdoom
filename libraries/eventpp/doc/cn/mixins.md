# Mixins

## 目录

- [介绍](#description)
- [定义一个 mixin](#define-a-mixin)
- [将 mixin 注入到 EventDispatcher](#inject-mixins-to-eventdispatcher)
- [可选拦截点（Optional interceptor points）](#optional-interceptor-points)
- [MixinFilter](#mixin-filter)
  - [公共类型](#public-types)
  - [函数](#functions)
  - [MixinFilter 示例代码](#sample-code-for-mixinfilter)

<a id="description"></a>

## 介绍

Mixin 用于向 EventDispatcher / EventQueue 继承层次中注入代码，以扩展它们的功能。本文将以 EventDispatcher 为例进行介绍，EventQueue 的 mixin 相关用法与 EventDispatcher 完全一致。

原有的 EventDispatcher 继承层次如下：

```
EventDispatcher <- EventDispatcherBase
```

`EventDispatcher` 是一个空类，其继承来自 `EventDispatcherBase` 的所有函数和数据。

在注入两个 mixin 类 A 、 B 后，层次如下：

```
EventDispatcher <- MixinA <- MixinB <- EventDispatcherBase
```

mixin 能够使用所有 EventDispatcherBase 中的所有 public 和 protected 成员（类型、函数和数据）。mixin 中的所有公共成员对用户来说都是可见、可用的。

<a id="define-a-mixin"></a>

## 定义一个 mixin

一个 mixin 是有着一个模板参数的模板类。mixin 必须继承其模板参数。一个典型的 mixin 如下：

```cpp
template <typename Base>
class MyMixin : public Base
{  
};
```

<a id="inject-mixins-to-eventdispatcher"></a>

## 将 mixin 注入到 EventDispatcher

想要启用 mixin ，需要将 mixin 加入到策略类的 `Mixins` 类型中。例如，要启动 `MixinFilter` ，可以像下面这样来定义调度器

```cpp
struct MyPolicies {
    using Mixins = eventpp::MixinList<eventpp::MixinFilter>;
};
eventpp::EventDispatcher<int, void (), MyPolicies> dispatcher;
```

若有多个 mixin ，像是 `using Mixins = eventpp::MixinList<MixinA, MixinB, MixinC>` ，则继承层次如下：

```
EventDispatcher <- MixinA <- MixinB <- MixinC <- EventDispatcherBase
```

最前面的排在继承层次的最底部

<a id="optional-interceptor-points"></a>

## 可选拦截点（ Optional interceptor points ）

一个 mixin 可以有一些具有特殊名称的函数，这些函数会被在特定的时候调用。这些特殊的函数必须是 public 的。当前只有一个特殊函数，如下：

```cpp
template <typename ...Args>
bool mixinBeforeDispatch(Args && ...args) const;
```

`mixinBeforeDispatch` 会在 EventDispatcher、EventQueue 开始调度事件之前被调用，其接收所有传给 EventDispatcher::dispatch 的参数，除了所有的参数都是以左值引用的方式传递的，无论它们在回调函数原型中是否被引用（无法修改 const 引用）。所以该函数能够修改实参，让后续的监听器都看到修改后的值。  
因此该函数可以修改实参，所有的监听器看到的参数都是修改后的值。  
该函数返回 `true` 时将继续调度，返回 `false` 时会停止继续调度。  
对于多 mixin 的情况，该函数会被按照这些 mixin 出现在策略类的 MixinList 中的顺序来执行。

<a id="mixin-filter"></a>

## MixinFilter

MixinFilter 可以在调度开始前过滤或修改所有的事件。

`MixinFilter::appendFilter(filter)` 向调度器中添加新的事件过滤器。`filter` 接收参数的类型是带有左值引用类型的回调函数原型。

所有的事件都会在执行任何监听器前触发事件过滤器的执行。

因为所有的实参都是以左值引用的形式传递的，因此事件过滤器也能修改这些实参，无论这些实参是否被在回调原型中引用（当然，const 引用是改不了的）。

下面的表格展示了事件过滤器如何接收实参

| 回调原型的实参类型       | 过滤器收到的实参类型       | 过滤器是否能修改实参？ | 备注                      |
| ------------------------ | -------------------------- | ---------------------- | ------------------------- |
| int, const int           | int &, int &               | 是                     | 抛弃 const 值的不变性     |
| int &, std::string &     | int &, std::string &       | 是                     |                           |
| const int &, const int * | const int &, const int * & | 否                     | 必须保持引用/指针的不变性 |

事件过滤器强大且实用，下面是一些用例示例

1. 捕捉并阻塞所有感兴趣的事件。例如，在一个 GUI 窗口系统中，所有的窗口都能接收到鼠标的点击事件。然而，当一个窗口正在被鼠标拖拽时，只有被拖拽的窗口才应该能接收到鼠标事件，即使鼠标正在其他窗口上移动也应如此。所以当开始拖动时，窗口可以添加一个过滤器，该过滤器重定向所有发到窗口的鼠标事件，阻止其他窗口的监听器获得鼠标事件，但扔放行鼠标事件外的所有其他事件。
2. 设置捕捉所有事件的监听器。例如，在一个电话本系统中，系统根据动作（action）来发送事件（删除添加电话号、查找电话号等）。如果想要在该系统中实现这样的一个模块：其只需要取电话号的区号，而不关心具体发生了的动作，该怎么做呢？一种方法是让该模块可以监听所有的事件（添加、删除、查找），但这种方法比较脆弱——若添加了一种新的动作事件，而忘记了为该模块定义相应的监听逻辑，就会导致出现未定义的行为。更好的方法是为该模块添加一个过滤器，并在过滤器中检查区号。

<a id="public-types"></a>

### 公共类型

`FilterHandle`：appendFilter 返回的句柄类型。过滤器句柄可用于移除过滤器。可以通过将 FilterHandle 实例转换为 bool 类型来检查其是否为空，当值为 false 时，该句柄为空。`FilterHandle` 是可拷贝的。

<a id="functions"></a>

### 函数

```cpp
FilterHandle appendFilter(const Filter & filter);
```

将 `filter` 添加进调度器。

该函数将返回一个可用于 `removeFilter` 的过滤器句柄

```cpp
bool removeFilter(const FilterHandle & filterHandle);
```

从调度其中移除一个过滤器。

当过滤器被成功移除时返回 true。

<a id="sample-code-for-mixinfilter"></a>

### MixinFilter 示例代码

#### 代码

```cpp
struct MyPolicies {
    using Mixins = eventpp::MixinList<eventpp::MixinFilter>;
};
eventpp::EventDispatcher<int, void (int e, int i, std::string), MyPolicies> dispatcher;

dispatcher.appendListener(3, [](const int e, const int i, const std::string & s) {
    std::cout
        << "Got event 3, i was 1 but actual is " << i
        << " s was Hello but actual is " << s
        << std::endl
    ;
});
dispatcher.appendListener(5, [](const int e, const int i, const std::string & s) {
    std::cout << "Shout not got event 5" << std::endl;
});

// 添加三个事件过滤器.

// 第一个过滤器将输入的实参改为其他值，然后后续的过滤器及监听器将看到修改后的值
dispatcher.appendFilter([](const int e, int & i, std::string & s) -> bool {
    std::cout << "Filter 1, e is " << e << " passed in i is " << i << " s is " << s << std::endl;
    i = 38;
    s = "Hi";
    std::cout << "Filter 1, changed i is " << i << " s is " << s << std::endl;
    return true;
});

// 第二个过滤器将所有 5 事件都过滤出来。因此监听事件 5 的过滤器都不会被触发。
// 第三个过滤器也不会在事件 5 触发
dispatcher.appendFilter([](const int e, int & i, std::string & s) -> bool {
    std::cout << "Filter 2, e is " << e << " passed in i is " << i << " s is " << s << std::endl;
    if(e == 5) {
        return false;
    }
    return true;
});

// 第三个过滤器只打印输入的实参
dispatcher.appendFilter([](const int e, int & i, std::string & s) -> bool {
    std::cout << "Filter 3, e is " << e << " passed in i is " << i << " s is " << s << std::endl;
    return true;
});

// 调度所有事件，第一个实参总是事件类型
dispatcher.dispatch(3, 1, "Hello");
dispatcher.dispatch(5, 2, "World");
```

输出

> Filter 1, e is 3 passed in i is 1 s is Hello  
> Filter 1, changed i is 38 s is Hi  
> Filter 2, e is 3 passed in i is 38 s is Hi  
> Filter 3, e is 3 passed in i is 38 s is Hi  
> Got event 3, i was 1 but actual is 38 s was Hello but actual is Hi  
> Filter 1, e is 5 passed in i is 2 s is World  
> Filter 1, changed i is 38 s is Hi  
> Filter 2, e is 5 passed in i is 38 s is Hi

