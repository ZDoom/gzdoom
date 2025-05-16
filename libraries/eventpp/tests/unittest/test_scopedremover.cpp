#include "test.h"

#include "eventpp/utilities/scopedremover.h"
#include "eventpp/eventdispatcher.h"
#include "eventpp/hetereventdispatcher.h"

TEST_CASE("ScopedRemover, EventDispatcher")
{
	using ED = eventpp::EventDispatcher<int, void ()>;
	ED dispatcher;
	using Remover = eventpp::ScopedRemover<ED>;
	constexpr int event = 3;
	Remover r4;
	
	std::vector<int> dataList(5);
	
	dispatcher.appendListener(event, [&dataList]() {
		++dataList[0];
	});
	
	{
		Remover r1(dispatcher);
		r1.prependListener(event, [&dataList]() {
			++dataList[1];
		});
		{
			Remover r2(dispatcher);
			auto handle = r2.appendListener(event, [&dataList]() {
				++dataList[2];
			});
			{
				Remover r3(dispatcher);
				r3.insertListener(event, [&dataList]() {
					++dataList[3];
				}, handle);
				{
					r4.setDispatcher(dispatcher);
					r4.appendListener(event, [&dataList]() {
						++dataList[4];
					});

					REQUIRE(dataList == std::vector<int> { 0, 0, 0, 0, 0 });

					dispatcher.dispatch(event);
					REQUIRE(dataList == std::vector<int> { 1, 1, 1, 1, 1 });
					r4.reset();
				}

				dispatcher.dispatch(event);
				REQUIRE(dataList == std::vector<int> { 2, 2, 2, 2, 1 });
			}

			dispatcher.dispatch(event);
			REQUIRE(dataList == std::vector<int> { 3, 3, 3, 2, 1 });
		}

		dispatcher.dispatch(event);
		REQUIRE(dataList == std::vector<int> { 4, 4, 3, 2, 1 });
	}
	
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 5, 4, 3, 2, 1 });
}

TEST_CASE("ScopedRemover, CallbackList")
{
	using CL = eventpp::CallbackList<void ()>;
	CL callbackList;
	using Remover = eventpp::ScopedRemover<CL>;
	Remover r4;
	
	std::vector<int> dataList(5);
	
	callbackList.append([&dataList]() {
		++dataList[0];
	});
	
	{
		Remover r1(callbackList);
		r1.prepend([&dataList]() {
			++dataList[1];
		});
		{
			Remover r2(callbackList);
			auto handle = r2.append([&dataList]() {
				++dataList[2];
			});
			{
				Remover r3(callbackList);
				r3.insert([&dataList]() {
					++dataList[3];
				}, handle);
				{
					r4.setCallbackList(callbackList);
					r4.append([&dataList]() {
						++dataList[4];
					});

					REQUIRE(dataList == std::vector<int> { 0, 0, 0, 0, 0 });
					
					callbackList();
					REQUIRE(dataList == std::vector<int> { 1, 1, 1, 1, 1 });
					r4.reset();
				}
				
				callbackList();
				REQUIRE(dataList == std::vector<int> { 2, 2, 2, 2, 1 });
			}

			callbackList();
			REQUIRE(dataList == std::vector<int> { 3, 3, 3, 2, 1 });
		}

		callbackList();
		REQUIRE(dataList == std::vector<int> { 4, 4, 3, 2, 1 });
	}
	
	callbackList();
	REQUIRE(dataList == std::vector<int> { 5, 4, 3, 2, 1 });
}

TEST_CASE("ScopedRemover, HeterEventDispatcher")
{
	using ED = eventpp::HeterEventDispatcher<int, eventpp::HeterTuple<void ()> >;
	ED dispatcher;
	using Remover = eventpp::ScopedRemover<ED>;
	constexpr int event = 3;

	std::vector<int> dataList(4);

	dispatcher.appendListener(event, [&dataList]() {
		++dataList[0];
	});

	{
		Remover r1(dispatcher);
		r1.prependListener(event, [&dataList]() {
			++dataList[1];
		});
		{
			Remover r2(dispatcher);
			auto handle = r2.appendListener(event, [&dataList]() {
				++dataList[2];
			});
			{
				Remover r3(dispatcher);
				r3.insertListener(event, [&dataList]() {
					++dataList[3];
				}, handle);

				REQUIRE(dataList == std::vector<int> { 0, 0, 0, 0 });

				dispatcher.dispatch(event);
				REQUIRE(dataList == std::vector<int> { 1, 1, 1, 1 });
			}

			dispatcher.dispatch(event);
			REQUIRE(dataList == std::vector<int> { 2, 2, 2, 1 });
		}

		dispatcher.dispatch(event);
		REQUIRE(dataList == std::vector<int> { 3, 3, 2, 1 });
	}

	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 4, 3, 2, 1 });
}

TEST_CASE("ScopedRemover, HeterCallbackList")
{
	using CL = eventpp::HeterCallbackList<eventpp::HeterTuple<void ()> >;
	CL callbackList;
	using Remover = eventpp::ScopedRemover<CL>;

	std::vector<int> dataList(4);

	callbackList.append([&dataList]() {
		++dataList[0];
	});

	{
		Remover r1(callbackList);
		r1.prepend([&dataList]() {
			++dataList[1];
		});
		{
			Remover r2(callbackList);
			auto handle = r2.append([&dataList]() {
				++dataList[2];
			});
			{
				Remover r3(callbackList);
				r3.insert([&dataList]() {
					++dataList[3];
				}, handle);

				REQUIRE(dataList == std::vector<int> { 0, 0, 0, 0 });

				callbackList();
				REQUIRE(dataList == std::vector<int> { 1, 1, 1, 1 });
			}

			callbackList();
			REQUIRE(dataList == std::vector<int> { 2, 2, 2, 1 });
		}

		callbackList();
		REQUIRE(dataList == std::vector<int> { 3, 3, 2, 1 });
	}

	callbackList();
	REQUIRE(dataList == std::vector<int> { 4, 3, 2, 1 });
}

TEST_CASE("ScopedRemover, EventDispatcher, move assignment")
{
	using ED = eventpp::EventDispatcher<int, void()>;
	ED dispatcher;
	using Remover = eventpp::ScopedRemover<ED>;
	constexpr int event = 3;

	std::vector<int> dataList(2);

	dispatcher.appendListener(event, [&dataList]() {
		++dataList[0];
	});

	{
		Remover moved;

		{
			Remover r1(dispatcher);
			r1.appendListener(event, [&dataList]() {
				++dataList[1];
			});
			dispatcher.dispatch(event);
			REQUIRE(dataList == std::vector<int> { 1, 1 });

			// moved = r1; // not compilable
			moved = std::move(r1);
			dispatcher.dispatch(event);
			REQUIRE(dataList == std::vector<int> { 2, 2 });
		}
		dispatcher.dispatch(event);
		REQUIRE(dataList == std::vector<int> { 3, 3 });
	}
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 4, 3 });
}

TEST_CASE("ScopedRemover, EventDispatcher, move")
{
	using ED = eventpp::EventDispatcher<int, void()>;
	ED dispatcher;
	using Remover = eventpp::ScopedRemover<ED>;
	constexpr int event = 3;

	std::vector<int> dataList(2);

	dispatcher.appendListener(event, [&dataList]() {
		++dataList[0];
	});

	{
		Remover r1(dispatcher);
		r1.appendListener(event, [&dataList]() {
			++dataList[1];
		});
		dispatcher.dispatch(event);
		REQUIRE(dataList == std::vector<int> { 1, 1 });

		{
			// Remover moved1(r1); // not compilable
			Remover moved(std::move(r1));
			dispatcher.dispatch(event);
			REQUIRE(dataList == std::vector<int> { 2, 2 });
		}
		dispatcher.dispatch(event);
		REQUIRE(dataList == std::vector<int> { 3, 2 });
	}
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 4, 2 });
}

TEST_CASE("ScopedRemover, EventDispatcher, swap")
{
	using std::swap;
	using ED = eventpp::EventDispatcher<int, void()>;
	ED dispatcher;
	using Remover = eventpp::ScopedRemover<ED>;
	constexpr int event = 3;

	std::vector<int> dataList(3);

	dispatcher.appendListener(event, [&dataList]() {
		++dataList[0];
	});

	Remover r1(dispatcher);
	r1.appendListener(event, [&dataList]() {
		++dataList[1];
	});
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 1, 1, 0 });
		
	r1.reset();
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 2, 1, 0 });

	r1.appendListener(event, [&dataList]() {
		++dataList[1];
	});
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 3, 2, 0 });

	Remover r2(dispatcher);
	swap(r1, r2);
	r1.reset();
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 4, 3, 0 });
		
	r2.appendListener(event, [&dataList]() {
		++dataList[2];
	});
	swap(r1, r2);
	r2.reset();
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 5, 4, 1 });
}

TEST_CASE("ScopedRemover, CallbackList, move assignment")
{
	using CL = eventpp::CallbackList<void()>;
	CL callbackList;
	using Remover = eventpp::ScopedRemover<CL>;

	std::vector<int> dataList(2);

	callbackList.append([&dataList]() {
		++dataList[0];
	});

	{
		Remover moved;

		{
			Remover r1(callbackList);
			r1.append([&dataList]() {
				++dataList[1];
			});
			callbackList();
			REQUIRE(dataList == std::vector<int> { 1, 1 });

			// moved = r1; // not compilable
			moved = std::move(r1);
			callbackList();
			REQUIRE(dataList == std::vector<int> { 2, 2 });
		}
		callbackList();
		REQUIRE(dataList == std::vector<int> { 3, 3 });
	}
	callbackList();
	REQUIRE(dataList == std::vector<int> { 4, 3 });
}

TEST_CASE("ScopedRemover, CallbackList, move")
{
	using CL = eventpp::CallbackList<void()>;
	CL callbackList;
	using Remover = eventpp::ScopedRemover<CL>;

	std::vector<int> dataList(2);

	callbackList.append([&dataList]() {
		++dataList[0];
	});

	{
		Remover r1(callbackList);
		r1.append([&dataList]() {
			++dataList[1];
		});
		callbackList();
		REQUIRE(dataList == std::vector<int> { 1, 1 });

		{
			// Remover moved1(r1); // not compilable
			Remover moved(std::move(r1));
			callbackList();
			REQUIRE(dataList == std::vector<int> { 2, 2 });
		}
		callbackList();
		REQUIRE(dataList == std::vector<int> { 3, 2 });
	}
	callbackList();
	REQUIRE(dataList == std::vector<int> { 4, 2 });
}

TEST_CASE("ScopedRemover, CallbackList, swap")
{
	using std::swap;
	using CL = eventpp::CallbackList<void()>;
	CL callbackList;
	using Remover = eventpp::ScopedRemover<CL>;

	std::vector<int> dataList(3);

	callbackList.append([&dataList]() {
		++dataList[0];
	});

	Remover r1(callbackList);
	r1.append([&dataList]() {
		++dataList[1];
	});
	callbackList();
	REQUIRE(dataList == std::vector<int> { 1, 1, 0 });

	r1.reset();
	callbackList();
	REQUIRE(dataList == std::vector<int> { 2, 1, 0 });

	r1.append([&dataList]() {
		++dataList[1];
	});
	callbackList();
	REQUIRE(dataList == std::vector<int> { 3, 2, 0 });

	Remover r2(callbackList);
	swap(r1, r2);
	r1.reset();
	callbackList();
	REQUIRE(dataList == std::vector<int> { 4, 3, 0 });

	r2.append([&dataList]() {
		++dataList[2];
	});
	swap(r1, r2);
	r2.reset();
	callbackList();
	REQUIRE(dataList == std::vector<int> { 5, 4, 1 });
}

TEST_CASE("ScopedRemover, EventDispatcher, removeListener")
{
	using ED = eventpp::EventDispatcher<int, void()>;
	ED dispatcher;
	using Remover = eventpp::ScopedRemover<ED>;
	constexpr int event = 3;

	std::vector<int> dataList(3);

	decltype(dispatcher)::Handle ha;
	decltype(dispatcher)::Handle hb;
	decltype(dispatcher)::Handle hc;

	Remover remover(dispatcher);
	ha = remover.appendListener(event, [&dataList, event , &remover , &ha]() {
		++dataList[0];
		remover.removeListener(event, ha);
	});
	hb = dispatcher.appendListener(event, [&dataList]() {
		++dataList[1];
	});
	hc = remover.appendListener(event, [&dataList]() {
		++dataList[2];
	});

	REQUIRE(! remover.removeListener(event, hb));
	
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 1, 1, 1 });
	
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 1, 2, 2 });

	REQUIRE(remover.removeListener(event, hc));
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 1, 3, 2 });

	REQUIRE(! remover.removeListener(event, hc));
	dispatcher.dispatch(event);
	REQUIRE(dataList == std::vector<int> { 1, 4, 2 });
}

TEST_CASE("ScopedRemover, CallbackList, remove")
{
	using std::swap;
	using CL = eventpp::CallbackList<void()>;
	CL callbackList;
	using Remover = eventpp::ScopedRemover<CL>;

	std::vector<int> dataList(3);

	decltype(callbackList)::Handle ha;
	decltype(callbackList)::Handle hb;
	decltype(callbackList)::Handle hc;

	Remover remover(callbackList);
	ha = remover.append([&dataList, &remover , &ha]() {
		++dataList[0];
		remover.remove(ha);
	});
	hb = callbackList.append([&dataList]() {
		++dataList[1];
	});
	hc = remover.append([&dataList]() {
		++dataList[2];
	});

	REQUIRE(! remover.remove(hb));
	
	callbackList();
	REQUIRE(dataList == std::vector<int> { 1, 1, 1 });
	
	callbackList();
	REQUIRE(dataList == std::vector<int> { 1, 2, 2 });

	REQUIRE(remover.remove(hc));
	callbackList();
	REQUIRE(dataList == std::vector<int> { 1, 3, 2 });

	REQUIRE(! remover.remove(hc));
	callbackList();
	REQUIRE(dataList == std::vector<int> { 1, 4, 2 });
}
