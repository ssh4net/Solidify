/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2023 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#pragma once

#include <queue>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <type_traits>
#include <utility>

class ThreadPool {
public:
    // Constructor that creates a thread pool with the specified number of threads
    ThreadPool(size_t threads) : stop(false) {
        // Create the specified number of worker threads
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    // Lock the queue to access tasks
                    std::unique_lock lock(this->queue_mutex);
                    // Wait for a task to be available or for stop signal
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    // If stop is true and there are no tasks, exit the thread
                    if (this->stop && this->tasks.empty())
                        return;
                    // Get the next task from the queue
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                // Execute the task
                task();
            }
                });
    }

    // Method to add a new task to the thread pool
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        // Create a packaged task that wraps the function and its arguments
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [func = std::forward<F>(f), ...captured_args = std::forward<Args>(args)]() mutable {
                return std::invoke(std::move(func), std::move(captured_args)...);
            }
        );

        // Get a future to allow the caller to retrieve the result of the task
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock lock(queue_mutex);

            // Don't allow enqueueing after the pool has been stopped
            if (stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");

            // Add the task to the queue
            tasks.emplace([task]() { (*task)(); });
        }
        // Notify one waiting thread that a new task is available
        condition.notify_one();
        return res;
    }

    // Destructor that joins all threads and stops the thread pool
    ~ThreadPool() {
        {
            std::unique_lock lock(queue_mutex);
            // Set stop to true to indicate that no more tasks should be processed
            stop = true;
        }
        // Notify all threads to wake up and check the stop condition
        condition.notify_all();
        // Join all worker threads to ensure they complete before destruction
        for (std::thread& worker : workers)
            worker.join();
    }

private:
    // Vector to hold all worker threads
    std::vector<std::thread> workers;
    // Queue to hold pending tasks
    std::queue<std::function<void()>> tasks;
    // Mutex to synchronize access to the task queue
    std::mutex queue_mutex;
    // Condition variable to notify worker threads of new tasks or stop signal
    std::condition_variable condition;
    // Boolean flag to indicate if the pool should stop accepting new tasks
    bool stop;
};