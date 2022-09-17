
#pragma once
/*
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

namespace Magnum
{

    class Worker
    {
    public:
        Worker();

        template<typename L>
        inline auto add( L&& l )
        {
            std::packaged_task<void()> work( std::forward<L>( l ) );
            auto                       f = work.get_future();
            {
                std::lock_guard guard( lock );
                actors.emplace_back( std::move( work ) );
            }
            cv.notify_all();
            return f;
        }

        inline void join()
        {
            auto f = add( [this]() { quit = true; } );
            cv.notify_all();
            f.wait();
            if( thread.joinable() )
                thread.join();
        }

    private:
        std::condition_variable_any            cv;
        bool                                   quit;
        std::mutex                             lock;
        std::deque<std::packaged_task<void()>> actors;
        std::thread                            thread;
    };
} // namespace Magnum
*/