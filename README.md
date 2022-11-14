
# Homework for Video Team 

------

### Build & Run 
 
- cmake
1. `mkdir build`
2. `cd build`
3. `cmake ..`
4. `make`
5. `./VideoTeamHomework`

- gcc
1. `g++ -std=c++2a -pthread -o vth main.cpp`
2. `./vth`

-----
### About the task
The program creates 3 threads that produce / process / accumulate result. The task follows a "Producer - Consumer"
pattern. 2 flags are added (genFinished and procFinished) as atomics (block will not impact performance in any, as they
are modified at the beginning and ending only) to release processor and aggregator when work is done. Flags also allow 
processes to wait if queues are empty but work before has not been done. 

#### SharedQueue
template class that builds on STL's queue functions with blocking mutex.
Instead of using queue<>.empty(), queue<>.first(), queue<>.pop() for processing objects,
SharedQueue<>.pop() returns a std::optional<> object. 

#### Generator
Generates a vector of random ints and pushes them to the shared queue

##### Things that could be done better 

- Waiting for data:
```
 while(!data)
    {
        data = procQueue->pop();
    }
```
actually blocks writing to the queue, and can block writing to queue for a long time. 
A SIGNAL to start processing (as a data presence) can actually be more reliable to use, but has not been specified in the 
task. 