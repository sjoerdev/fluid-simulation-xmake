#include "threadpool.h"

void thread_pool::start_pool() // initialize thread pool
{
    int available_threads = std::thread::hardware_concurrency();
    for (int i = 0; i < available_threads; i++) threads.emplace_back(std::thread(&thread_pool::search, this));
}

void thread_pool::search() // infinite loop that searches for jobs
{
    while (true)
    {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock{queue_mutex};

            mutex_condition.wait(lock, [this]()
            {
                return !jobs.empty() || should_terminate;
            });

            if (should_terminate) return;

            job = jobs.front();
            jobs.pop();
            active_jobs++;
        }

        job(); // execute job

        active_jobs -= 1;
    }
}

void thread_pool::add_job(const std::function<void()>& job) // add job to the thread pool
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        jobs.push(job);
    }
    mutex_condition.notify_one();
}

bool thread_pool::is_busy() // check if jobs are running
{
    bool poolbusy;
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        poolbusy = !jobs.empty();
    }
    return poolbusy;
}

void thread_pool::stop_pool() // destroy thread pool
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        should_terminate = true;
    }

    mutex_condition.notify_all();
    for (std::thread& active_thread : threads) active_thread.join();

    threads.clear();
}

void thread_pool::parallel_for(int start, int end, std::function<void(int)>&& func)
{
    int length = end - start;
    if (length <= 0) return;

    const int num_threads = std::thread::hardware_concurrency();
    const int batch_size = std::max(1, length / num_threads);

    for (int i = start; i < end; i += batch_size)
    {
        int batch_start = i;
        int batch_end = std::min(i + batch_size, end);

        auto batch_job = [=]()
        {
            for (int j = batch_start; j < batch_end; ++j) func(j);
        };

        add_job(batch_job);
    }

    // wait for the other jobs
    while (is_busy() || active_jobs > 0) std::this_thread::yield();
}