# Tip - samples for typical use cases

- [Tip - samples for typical use cases](#tip---samples-for-typical-use-cases)
    - [Overview](#overview)
    - [Implement signals and slots](#implement-signals-and-slots)
    - [Implement event loop in GUI application, similar to Win32 message processing or Qt event loop](#implement-event-loop-in-gui-application-similar-to-win32-message-processing-or-qt-event-loop)
    - [Process multiple EventQueue instances in a single main loop](#process-multiple-eventqueue-instances-in-a-single-main-loop)

## Overview

Here lists several samples of typical use cases when using `eventpp`. The purpose is to inspire you, not to restrict you to do so.  
All code are pseudo code, they can't be compiled.

## Implement signals and slots

Signal/slot is widely used in Qt, and there are some C++ libraries implement signal/slot.  
`eventpp` does not expose an explicit signal/slot concept, although it has equivalent functionality covered by `CallbackList`.  
A `CallbackList` object is a signal. Appending a callback to it is connecting a slot to the signal, and the callback is the slot.

Below is the simplified Qt example code got from [Qt document](https://doc.qt.io/qt-6/signalsandslots.html),  

```c++
class Counter
{
public slots:
    void setValue(int value) {
        m_value = value;
        emit valueChanged(value);
    }

signals:
    void valueChanged(int newValue);

private:
    int m_value;
};

Counter a, b;
QObject::connect(&a, &Counter::valueChanged, &b, &Counter::setValue);
a.setValue(12);     // a.m_value == 12, b.m_value == 12
b.setValue(48);     // a.m_value == 12, b.m_value == 48
```

Below is the equivalence in `eventpp`.

```c++
class Counter
{
public:
    void setValue(int value) {
        m_value = value;
        // invoke the CallbackList
        valueChanged(value);
    }

    // The "signal"
    eventpp::CallbackList<void (int)> valueChanged;

private:
    int m_value;
};

Counter a, b;
// connect the "signal and slot"
a.valueChanged.append([&b](int value) {
    b.setValue(value);
});
a.setValue(12);     // a.m_value == 12, b.m_value == 12
b.setValue(48);     // a.m_value == 12, b.m_value == 48
```

## Implement event loop in GUI application, similar to Win32 message processing or Qt event loop

Both Win32 message processing (using GetMessage and DispatchMessage) and Qt event loop are not typical observer pattern. They do not use listeners, but instead send events to the target widget. This is done in the event loop. If you are making GUI system, you may want to mimic the event loop found in Windows API and Qt. Below is the pseduo code for implementing such an event loop using `eventpp`.  

```c++

struct Event
{
    Widget * widget; // This is similar to the `HWND hWnd` parameter in Win32 PostMessageA function.
    int eventType; // Similar to `UINT Msg` in Win32 PostMessageA.
    AnyOtherData; // Similar to `WPARAM wParam` and `LPARAM lParam` in Win32 PostMessageA.
}

using MainEventQueue = eventpp::EventQueue<int, void (const std::shared_ptr<Event>)>
MainEventQueue mainEventQueue;

Widget widgetA;

// Put a message into the queue. Equivalent to PostMessageA(&widgetA, MOUSE_DOWN, x, y)
mainEventQueue.enqueue(shared_pointer_of MouseEvent { widget = &widgetA, eventType = MOUSE_DOWN, AnyOtherData = x/y location, etc } );

// Dispatch immediately without using the queue. Equivalent to SendMessageA(&widgetA, MOUSE_DOWN, x, y)
mainEventQueue.dispatch(shared_pointer_of MouseEvent { widget = &widgetA, eventType = MOUSE_DOWN, AnyOtherData = x/y location, etc } );

// The event loop
for(;;) {
    MainEventQueue::QueuedEvent queuedEvent;
    if(mainEventQueue.takeEvent(&queuedEvent)) {
        Event & event = std::get<0>(queuedEvent.arguments);
        if(event.eventType == MOUSE_DOWN) {
            event.widget.onMouseDown(event);
        }
        else if(event.eventType == KEY_DOWN) {
            event.widget.onKeyDown(event);
        }
        else if ...
    }
}
```

Below is the Windows equivalent message loop, it's simplified version from [Windows document](https://learn.microsoft.com/en-us/windows/win32/winmsg/using-messages-and-message-queues),

```c++
while( (bRet = GetMessage( &msg, NULL, 0, 0 )) != 0)
{ 
    TranslateMessage(&msg); 
    DispatchMessage(&msg); 
} 
```

## Process multiple EventQueue instances in a single main loop  

It's common to have a single main loop in a GUI or game application, and there are various EventQueue instances in the system. The function `EventQueue::process` needs to be called on each queue to drive the queue moving. Below is a tip to process all the EventQueue instances in a single place.  

```c++

// Here mainLoopTasks is global for simplify, in real application it can be in some object and passed around
using MainLoopCallbackList = eventpp::CallbackList<void ()>;
MainLoopCallbackList mainLoopTasks;

void mainLoop()
{
    for(;;) {
        // Do any stuff in the loop
        
        mainLoopTasks();
    }
}

class MyEventQueue : public eventpp::EventQueue<blah blah>
{
public:
    MyEventQueue() {
        handle = mainLoopTasks.append([this]() {
            process();
        });
    }
    ~MyEventQueue() {
        mainLoopTasks.remove(handle);
    }

private:
    MainLoopCallbackList::Handle handle;
};
```

The idea is, the main loop invoke a callback list in each loop, and each event queue registers its `process` to the callback list.

