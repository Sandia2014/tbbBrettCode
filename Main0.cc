// -*- C++ -*-
// Main0.cc
// some beginning tbb syntax

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <array>
#include <chrono>

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

class TbbOutputter {
public:

  
  const double _startBound;
  const double _endBound;
  const double _dx;
  double _integral;

  TbbOutputter(const double startBound, const double endBound,
  		const double dx) :
    _startBound(startBound), _endBound(endBound),
    _dx(dx), _integral(0) {
    printf("constructor called\n");
  }

  TbbOutputter(const TbbOutputter & other,
               tbb::split) :
    _startBound(other._startBound), _endBound(other._endBound),
    _dx(other._dx), _integral(0) {
    printf("split copy constructor called\n");
  }

  void operator()(const tbb::blocked_range<size_t> & range) {
    printf("TbbOutputter asked to process range from %7zu to %7zu\n",
           range.begin(), range.end());
    double sum = _integral;
    double begin = range.begin()*_dx;
    double end = range.end()*_dx;
    for (double i = begin+.5*_dx; i < end; i+= _dx) {
        sum += std::sin(i);
    }
    _integral = sum;
  }

  void join(const TbbOutputter & other) {
    printf("join called\n");
    _integral += other._integral;
  }

private:
  TbbOutputter();

};

int main() {

  // a couple of inputs.  change the numberOfIntervals to control the amount
  //  of work done
  const unsigned long numberOfIntervals = 1e8;
  // the integration bounds
  const array<double, 2> bounds = {{0, 1.314}};

  // these are c++ timers...for timing
  high_resolution_clock::time_point tic;
  high_resolution_clock::time_point toc;

  // first, do the serial calculation
  const double dx = (bounds[1] - bounds[0]) / numberOfIntervals;
  double serialIntegral = 0;
  tic = high_resolution_clock::now();
  for (unsigned int intervalIndex = 0;
       intervalIndex < numberOfIntervals; ++intervalIndex) {
    if (intervalIndex % (numberOfIntervals / 10) == 0) {
      printf("serial calculation on interval %8.2e / %8.2e (%%%5.1f)\n",
             double(intervalIndex),
             double(numberOfIntervals),
             100. * intervalIndex / double(numberOfIntervals));
    }
    const double evaluationPoint =
      bounds[0] + (double(intervalIndex) + 0.5) * dx;
    serialIntegral += std::sin(evaluationPoint);
  }
  serialIntegral *= dx;
  toc = high_resolution_clock::now();
  const double serialElapsedTime =
    duration_cast<duration<double> >(toc - tic).count();

  // calculate analytic solution
  const double libraryAnswer =
    std::cos(bounds[0]) - std::cos(bounds[1]);

  // check our serial answer
  const double serialRelativeError =
    std::abs(libraryAnswer - serialIntegral) / std::abs(libraryAnswer);
  if (serialRelativeError > 1e-6) {
    fprintf(stderr, "our answer is too far off: %15.8e instead of %15.8e\n",
            serialIntegral, libraryAnswer);
    exit(1);
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
    vector<double> inputs(1e3);
    TbbOutputter tbbOutputter(bounds[0], bounds[1], dx);

    // start timing
    tic = high_resolution_clock::now();
    // dispatch threads
    parallel_reduce(tbb::blocked_range<size_t>(0, numberOfIntervals),
                    tbbOutputter);
    // stop timing
    toc = high_resolution_clock::now();
    const double threadedElapsedTime =
      duration_cast<duration<double> >(toc - tic).count();

    // somehow get the threaded integral answer
    const double threadedIntegral = tbbOutputter._integral*dx;
    // check the answer
    const double threadedRelativeError =
      std::abs(libraryAnswer - threadedIntegral) / std::abs(libraryAnswer);
    if (threadedRelativeError > 1e-6) {
      fprintf(stderr, "our answer is too far off: %15.8e instead of %15.8e\n",
              threadedIntegral, libraryAnswer);
      exit(1);
    }

    // output speedup
    printf("%3u : time %8.2e speedup %8.2e (%%%5.1f of ideal)\n",
           numberOfThreads,
           threadedElapsedTime,
           serialElapsedTime / threadedElapsedTime,
           100. * serialElapsedTime / threadedElapsedTime / numberOfThreads);

  }
  return 0;
}
