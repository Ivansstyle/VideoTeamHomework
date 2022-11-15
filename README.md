
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
## About the task
The program creates 3 threads that produce / process / accumulate result. The task follows a "Producer - Consumer"
pattern. 2 flags are added (genFinished and procFinished) as atomics (block will not impact performance in any, as they
are modified at the beginning and ending only) to release processor and aggregator when work is done. Flags also allow 
processes to wait if queues are empty but work before has not been done. 

### SharedQueue
SharedQueue is a template class that modifies STL's queue functions with blocking mutex.
Instead of using queue<>.empty(), queue<>.first(), queue<>.pop() for processing objects,
SharedQueue<>.pop() returns a std::optional<> object. 

Using queue<>.empty between threads as an access guard is 
*unsafe*, because after a guard has been checked queue can be modified. 
This is also a case with queue<>.first() and queue<>.pop(): if an object is added to a 
queue after queue<>.first() is accessed, queue<>.pop() can actually delete the object that just been added,
not the one the queue<>.first() has returned.

Using queue *as-is* would actually require to lock the queue object for whole duration, where .empty, .first, .pop is used, 
blocking any other thread from writing to the queue. 
### Generator
Generates a vector of random sizes of random ints for random size and pushes them to the shared queue. 

#### Generator constraints: 
* generator constraints are defined within random distributions ranges. 
* *For time:* random time of seconds (between 1 and 10)
* *For vector size:* random size of vectors (between 1 and 1000), vector size must always be more than 0. 
* *For vector values:* random value (between -1000 and 1000)

It is important to define vector size and vector values carefully, because if 
maxVectorSize * maxVectorValue < MAX_INT, it can cause value overflow in processor and cause undefined behaviour in some cases.

Because we are using uniform distribution of ints from -1000 to 1000, statistically program should output a value closer to 0.

### Processor 

* Reads vector from SharedQueue. 
* Uses std::reduce / std::vector<>.size() to calculate the average value for the vector. 
* Pushes the value to the SharedQueue. 

### Aggregator

* Reads average (floats) values from SharedQueue.
* Counts and aggregates values
* Writes an average result to result variable.

------

##### Things that could be done better 

- Waiting for data:
```c++
 while(!data)
    {
        data = procQueue->pop();
    }
```
```c++
 } else { // Data has not been added quickly enough, wait for data again
            data = aggrQueue->pop();
        }
```
will actually block writing to the queue, and can block permanently in some cases. 
A SIGNAL to start processing (as a data presence) can actually be more reliable to use, but has not been specified in the 
task. With a signal, the processing thread can be started only after the generator has completed the first vector (waterfall model).

- Aggregation with custom look to avoid integer overload:
```c++
float avg = static_cast<float>(std::reduce(data->begin(), data->end())) / static_cast<float>(count);
```
