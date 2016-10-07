
#pragma once

#include "r_draw.h"
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

// Use multiple threads when drawing
EXTERN_CVAR(Bool, r_multithreaded)

// Redirect drawer commands to worker threads
void R_BeginDrawerCommands();

// Wait until all drawers finished executing
void R_EndDrawerCommands();

// Worker data for each thread executing drawer commands
class DrawerThread
{
public:
	std::thread thread;

	// Thread line index of this thread
	int core = 0;

	// Number of active threads
	int num_cores = 1;

	// Range of rows processed this pass
	int pass_start_y = 0;
	int pass_end_y = MAXHEIGHT;

	uint32_t dc_temp_rgbabuff_rgba[MAXHEIGHT * 4];
	uint32_t *dc_temp_rgba;

	// Checks if a line is rendered by this thread
	bool line_skipped_by_thread(int line)
	{
		return line < pass_start_y || line >= pass_end_y || line % num_cores != core;
	}

	// The number of lines to skip to reach the first line to be rendered by this thread
	int skipped_by_thread(int first_line)
	{
		int pass_skip = MAX(pass_start_y - first_line, 0);
		int core_skip = (num_cores - (first_line + pass_skip - core) % num_cores) % num_cores;
		return pass_skip + core_skip;
	}

	// The number of lines to be rendered by this thread
	int count_for_thread(int first_line, int count)
	{
		int lines_until_pass_end = MAX(pass_end_y - first_line, 0);
		count = MIN(count, lines_until_pass_end);
		int c = (count - skipped_by_thread(first_line) + num_cores - 1) / num_cores;
		return MAX(c, 0);
	}

	// Calculate the dest address for the first line to be rendered by this thread
	uint32_t *dest_for_thread(int first_line, int pitch, uint32_t *dest)
	{
		return dest + skipped_by_thread(first_line) * pitch;
	}
};

// Task to be executed by each worker thread
class DrawerCommand
{
protected:
	int _dest_y;

public:
	DrawerCommand()
	{
		_dest_y = static_cast<int>((dc_dest - dc_destorg) / (dc_pitch * 4));
	}

	virtual void Execute(DrawerThread *thread) = 0;
};

// Manages queueing up commands and executing them on worker threads
class DrawerCommandQueue
{
	enum { memorypool_size = 16 * 1024 * 1024 };
	char memorypool[memorypool_size];
	size_t memorypool_pos = 0;

	std::vector<DrawerCommand *> commands;

	std::vector<DrawerThread> threads;

	std::mutex start_mutex;
	std::condition_variable start_condition;
	std::vector<DrawerCommand *> active_commands;
	bool shutdown_flag = false;
	int run_id = 0;

	std::mutex end_mutex;
	std::condition_variable end_condition;
	size_t finished_threads = 0;

	int threaded_render = 0;
	DrawerThread single_core_thread;
	int num_passes = 1;
	int rows_in_pass = MAXHEIGHT;

	void StartThreads();
	void StopThreads();
	void Finish();

	static DrawerCommandQueue *Instance();

	DrawerCommandQueue();
	~DrawerCommandQueue();

public:
	// Allocate memory valid for the duration of a command execution
	static void* AllocMemory(size_t size);

	// Queue command to be executed by drawer worker threads
	template<typename T, typename... Types>
	static void QueueCommand(Types &&... args)
	{
		auto queue = Instance();
		if (queue->threaded_render == 0 || !r_multithreaded)
		{
			T command(std::forward<Types>(args)...);
			command.Execute(&queue->single_core_thread);
		}
		else
		{
			void *ptr = AllocMemory(sizeof(T));
			if (!ptr) // Out of memory - render what we got
			{
				queue->Finish();
				ptr = AllocMemory(sizeof(T));
				if (!ptr)
					return;
			}
			T *command = new (ptr)T(std::forward<Types>(args)...);
			queue->commands.push_back(command);
		}
	}

	// Redirects all drawing commands to worker threads until End is called
	// Begin/End blocks can be nested.
	static void Begin();

	// End redirection and wait until all worker threads finished executing
	static void End();

	// Waits until all worker threads finished executing
	static void WaitForWorkers();
};
