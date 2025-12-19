#ifndef THREADEDMATRIXMULTIPLIER_H
#define THREADEDMATRIXMULTIPLIER_H

#include <queue>
#include <map>

#include <pcosynchro/pcoconditionvariable.h>
#include <pcosynchro/pcohoaremonitor.h>
#include <pcosynchro/pcomutex.h>
#include <pcosynchro/pcosemaphore.h>
#include <pcosynchro/pcothread.h>

#include "abstractmatrixmultiplier.h"
#include "matrix.h"


///
/// A class that holds the necessary parameters for a thread to do a job.
///
template<class T>
class ComputeParameters
{
public:
    const SquareMatrix<T>* A;
    const SquareMatrix<T>* B;
    SquareMatrix<T>* C;

	int blockI; // block row index in C matrix
	int blockJ; // block column index in C matrix
	int blockK; // block index for the sum
	int blockSize; // (one dimension)
	int jobId;
};


/// As a suggestion, a buffer class that could be used to communicate between
/// the workers and the main thread...
///
/// Here we only wrote two potential methods, but there could be more at the end...
///
template<class T>
class Buffer : public PcoHoareMonitor
{
public:
    int nbJobFinished{0}; // Keep this updated
    /* Maybe some parameters */

    ///
    /// \brief Sends a job to the buffer
    /// \param Reference to a ComputeParameters object which holds the necessary parameters to execute a job
    ///
    void sendJob(ComputeParameters<T> params) {
		monitorIn();
		jobs.push(params);
		signal(jobAvailable);
		monitorOut();
	}

    ///
    /// \brief Requests a job to the buffer
    /// \param Reference to a ComputeParameters object which holds the necessary parameters to execute a job
    /// \return true if a job is available, false otherwise
    ///
    bool getJob(ComputeParameters<T>& parameters) { 
		monitorIn();
		while (jobs.empty() && !shouldTerminate) {
			wait(jobAvailable);
		}

		if(shouldTerminate && jobs.empty()) {
			monitorOut();
			return false;
		}

		parameters = jobs.front();
		jobs.pop();
		monitorOut();
		return true;
	}
	
	///
	/// \brief Registers a new computation and returns its job ID
	/// \param totalJobs Total number of jobs for this computation
	/// \return The job ID for this computation
	///
	int registerComputation(int totalJobs) {
		monitorIn();
		int jobId = nextJobId++;
		totalJobsExpected[jobId] = totalJobs;
		jobCompletion[jobId] = 0;
		// Create condition variable for this jobId (will be created automatically by map access)
		jobCompletionCond[jobId] = Condition();
		monitorOut();
		return jobId;
	}

	///
	/// \brief Notifies that a job has been finished
	/// \param jobId The ID of the computation this job belongs to
	///
	void notifyJobFinished(int jobId) {
		monitorIn();
		nbJobFinished++;
		
		// Update per-jobId tracking
		if (jobCompletion.find(jobId) != jobCompletion.end()) {
			jobCompletion[jobId]++;
			// Check if all jobs for this computation are done
			if (jobCompletion[jobId] >= totalJobsExpected[jobId]) {
				signal(jobCompletionCond[jobId]);
			}
		}
		monitorOut();
	}

	///
	/// \brief Waits for all jobs of a computation to finish
	/// \param jobId The ID of the computation to wait for
	///
	void waitForCompletion(int jobId) {
		monitorIn();
		while (jobCompletion[jobId] < totalJobsExpected[jobId]) {
			wait(jobCompletionCond[jobId]);
		}
		// Clean up tracking data
		totalJobsExpected.erase(jobId);
		jobCompletion.erase(jobId);
		jobCompletionCond.erase(jobId);
		monitorOut();
	}
	
	///
	/// \brief Resets the job counter for a new computation
	///
	void resetJobCounter() {
		monitorIn();
		nbJobFinished = 0;
		monitorOut();
	}
	
	///
	/// \brief Signals all threads to terminate
	///
	void signalTermination() {
		monitorIn();
		shouldTerminate = true;
		// Signal multiple times to wake all waiting threads
		// (Hoare monitors don't have broadcast, so we signal many times)
		for (int i = 0; i < 100; i++) {
			signal(jobAvailable);
		}
		monitorOut();
	}
	
	///
	/// \brief Resets termination flag (for reentrancy)
	///
	void resetTermination() {
		monitorIn();
		shouldTerminate = false;
		monitorOut();
	}

private:
	std::queue<ComputeParameters<T>> jobs;
	Condition jobAvailable;

	std::map<int, Condition> jobCompletionCond; // cond to wait on per job
	std::map<int, int> totalJobsExpected; // expected jobs per job id
	std::map<int, int> jobCompletion; // completed jobs per job id

	int nextJobId = 0;
	bool shouldTerminate = false;
};


///
/// A multi-threaded multiplicator. multiply() should at least be reentrant.
/// It is up to you to offer a very good parallelism.
///
template<class T>
class ThreadedMatrixMultiplier : public AbstractMatrixMultiplier<T>
{
private:
	template<class S>
	void workerThreadFunction(ThreadedMatrixMultiplier<S>* multiplier) {
	}

public:
    ///
    /// \brief ThreadedMatrixMultiplier
    /// \param nbThreads Number of threads to start
    /// \param nbBlocksPerRow Default number of blocks per row, for compatibility with SimpleMatrixMultiplier
    ///
    /// The threads shall be started from the constructor
    ///
    ThreadedMatrixMultiplier(int nbThreads, int nbBlocksPerRow = 0)
        : nbThreads(nbThreads), nbBlocksPerRow(nbBlocksPerRow)
    {
		for (int i = 0; i < nbThreads; i++) {
			PcoThread* thread = new PcoThread(workerThreadFunction<T>, this);
			workerThreads.push_back(thread);
		}
    }

    ///
    /// In this destructor we should ask for the termination of the computations. They could be aborted without
    /// ending into completion.
    /// All threads have to be
    ///
    ~ThreadedMatrixMultiplier()
    {
        // TODO
        ThreadsStop = true;
    }

    ///
    /// \brief multiply
    /// \param A First matrix
    /// \param B Second matrix
    /// \param C Result of AxB
    ///
    /// For compatibility reason with SimpleMatrixMultiplier
    void multiply(const SquareMatrix<T>& A, const SquareMatrix<T>& B, SquareMatrix<T>& C) override
    {
        multiply(A, B, C, nbBlocksPerRow);
    }

    ///
    /// \brief multiply
    /// \param A First matrix
    /// \param B Second matrix
    /// \param C Result of AxB
    /// \param nbBlocksPerRow Number of blocks per row (or columns)
    ///
    /// Executes the multithreaded computation, by decomposing the matrices into blocks.
    /// nbBlocksPerRow must divide the size of the matrix.
    ///
    void multiply(const SquareMatrix<T>& A, const SquareMatrix<T>& B, SquareMatrix<T>& C, int nbBlocksPerRow)
    {
        // OK, computation is done correctly, but... Is it really multithreaded?!?
        // TODO : Get rid of the next lines and do something meaningful
        for (int i = 0; i < A.size(); i++) {
            for (int j = 0; j < A.size(); j++) {
                T result = 0.0;
                for (int k = 0; k < A.size(); k++) {
                    result += A.element(k, j) * B.element(i, k);
                }
                C.setElement(i, j, result);
            }
        }
    }

protected:
    int nbThreads;
    int nbBlocksPerRow;
	std::vector<PcoThread*> workerThreads;
    Buffer<T> buf;
    bool ThreadsStop = false;
};




#endif // THREADEDMATRIXMULTIPLIER_H
