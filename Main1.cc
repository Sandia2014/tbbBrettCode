// -*- C++ -*-
// Main1.cc
// some beginning tbb syntax

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <array>
#include <chrono>
#include <thread>
#include <iostream>
// header files for tbb
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_for.h>
#include <tbb/task_scheduler_init.h>

// reduce the amount of typing we have to do for timing things
using std::chrono::high_resolution_clock;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::size_t;
using std::vector;
using std::array;
using tbb::atomic;

class TbbOutputter {
public:

  unsigned long * _numbers;
  atomic<unsigned long> * _parallelHist;
  unsigned long _numBuckets;
  unsigned long _arrayLen;
  unsigned long _bucketSize;

  TbbOutputter(unsigned long * numbers, atomic<unsigned long> * parallelHist, unsigned long numBuckets, unsigned long arrayLen) : _numbers(numbers), _parallelHist(parallelHist), _numBuckets(numBuckets), _arrayLen(arrayLen) {
   _bucketSize = arrayLen/numBuckets;
 }

  TbbOutputter(const TbbOutputter & other, tbb::split) :
    _numbers(other._numbers), _parallelHist(other._parallelHist), _numBuckets(other._numBuckets), _arrayLen(other._arrayLen), _bucketSize(other._bucketSize) {
   
 }

  void operator()(const tbb::blocked_range<size_t> & range) {
    unsigned long begin = range.begin();
    unsigned long end = range.end();
    for (unsigned long i = begin; i < end; i++) {
      _parallelHist[_numbers[i]/_bucketSize].fetch_and_increment();
    }
  }

  void join(const TbbOutputter & other) {

  }

  private:
    TbbOutputter();

};

int Random(int n) {
  return rand() % n;
}

int main() {

  // a couple of inputs.  change the numberOfIntervals to control the amount
  //  of work done
  const unsigned long arrayLen = 1e7;
  const unsigned long numBuckets = 1e3;
  const unsigned long bucketSize = arrayLen/numBuckets;
  // these are c++ timers...for timing
  high_resolution_clock::time_point tic;
  high_resolution_clock::time_point toc;
	
  unsigned long* numbers = new unsigned long[arrayLen];  
  for (unsigned long i = 0; i < arrayLen; i++) {
    numbers[i] = i;
  }
  unsigned long * serialBuckets = new unsigned long[numBuckets];

  tic = high_resolution_clock::now();
  for (unsigned long i = 0; i < arrayLen; i++) {
    serialBuckets[numbers[i]/bucketSize]++;
  }

  toc = high_resolution_clock::now();
  const double serialElapsedTime = 
  	duration_cast<duration<double> >(toc - tic).count();

  // check serial answer
  for (unsigned long i = 0; i < numBuckets; i++) {
    if (serialBuckets[i] != arrayLen/numBuckets) {
      printf("Serial answer incorrect");
      exit(1);
    }
  }


 // we will repeat the computation for each of the numbers of threads
  vector<unsigned int> numberOfThreadsArray;
  numberOfThreadsArray.push_back(1);
  numberOfThreadsArray.push_back(2);
  numberOfThreadsArray.push_back(4);

  // for each number of threads
  for (unsigned int numberOfThreadsIndex = 0;
       numberOfThreadsIndex < numberOfThreadsArray.size();
       ++numberOfThreadsIndex) {
    const unsigned int numberOfThreads =
      numberOfThreadsArray[numberOfThreadsIndex];

    // initialize tbb's threading system for this number of threads
    tbb::task_scheduler_init init(numberOfThreads);

    printf("processing with %2u threads:\n", numberOfThreads);

    // prepare the tbb functor.
    // note that this inputs variable is stupid and shouldn't be made in a loop,
    //  but i'm just showing you how you'd pass information to your functor
    atomic<unsigned long> * parallelHist = new atomic<unsigned long>[numBuckets];
    TbbOutputter tbbOutputter(numbers, parallelHist, numBuckets, arrayLen);
   
    for (unsigned long i = 0; i < numBuckets; i++) {
      parallelHist[i] = 0;
    }
    // start timing
    tic = high_resolution_clock::now();
    // dispatch threads
    parallel_reduce(tbb::blocked_range<size_t>(0, arrayLen),
                    tbbOutputter);
    // stop timing
    toc = high_resolution_clock::now();
    const double threadedElapsedTime =
      duration_cast<duration<double> >(toc - tic).count();

    // somehow get the threaded integral answer
    for (unsigned long i = 0; i < numBuckets; i++) {
    	if (parallelHist[i] != arrayLen/numBuckets) {
	  fprintf(stderr, "answer too far off %2u and %2u at %2u \n", parallelHist[i], arrayLen/numBuckets, i);
	  exit(1);
	}
    }
    // check the answer

    // output speedup
    printf("%3u : time %8.2e speedup %8.2e (%%%5.1f of ideal)\n",
           numberOfThreads,
           threadedElapsedTime,
           serialElapsedTime / threadedElapsedTime,
           100. * serialElapsedTime / threadedElapsedTime / numberOfThreads);

  }
  return 0;
}
