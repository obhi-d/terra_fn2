
#include "Worker.h"
/*
using namespace Magnum;
Worker::Worker()
{
    quit = false;

    thread = std::thread( [this]() {
        std::function<void()> task;
        do
        {
            {

                std::unique_lock<std::mutex> lk( lock );
                cv.wait( lk, [this] { return !actors.empty(); } );
                if( !actors.empty() )
                {
                    task = std::move( actors.front() );
                    actors.pop_front();
                }
            }
            if( task )
                task();
            task = {};
        } while( !quit );
    } );
}
*/