#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// A small pool of worker threads for blocking work (HTTP, disk), and a
// queue of callbacks the UI thread drains once per frame. Only the UI thread
// may touch the app state.
class Tasks {
public:
	void start(int workers);
	void stop();

	// Runs `work` on a worker thread.
	void run(std::function<void()> work);
	void foreground(std::function<void()> work);

	// Queues `fn` for the UI thread (callable from any thread).
	void post(std::function<void()> fn);

	// Convenience: compute a value on a worker, deliver it on the UI thread.
	template <typename T>
	void run(std::function<T()> work, std::function<void(T&)> done, bool urgent = false) {
		auto job = [this, work, done]() {
			auto value = std::make_shared<T>(work());
			post([value, done]() { done(*value); });
		};
		if (urgent) foreground(std::move(job)); else run(std::move(job));
	}

	// Called by the UI thread every frame.
	void drain();

	size_t pending() const;

private:
	void worker_loop(bool foreground_only = false);

	mutable std::mutex mutex_;
	std::condition_variable cv_;
	std::deque<std::function<void()>> work_, foreground_;
	std::vector<std::thread> threads_;
	bool stopping_ = false;

	std::mutex ui_mutex_;
	std::vector<std::function<void()>> ui_;
};

extern Tasks g_tasks;
