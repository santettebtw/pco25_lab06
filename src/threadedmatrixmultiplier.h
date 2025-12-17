#ifndef THREADEDMATRIXMULTIPLIER_H
#define THREADEDMATRIXMULTIPLIER_H

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
	
	int registerComputation(int totalJobs) {
		monitorIn();
		int jobId = nextJobId++;
		totalJobsExpected[jobId] = totalJobs;
		jobCompletion[jobId] = 0;
		monitorOut();
		return jobId;
	}

	void notifyJobFinished(int jobId) {
		monitorIn();
		nbJobFinished++;
		jobCompletion[jobId]++;
		// check if all threads for this computation is done
		if (jobCompletion[jobId] == totalJobsExpected[jobId]) {
			signal(jobCompletionCond[jobId]);
		}
		monitorOut();
	}

	void waitForCompletion(int jobId) {
		while (jobCompletion[jobId] < totalJobsExpected[jobId]) {
		}
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
        // TODO
        for (int x = 0; x < nbThreads;++x) {
            PcoThreads([this]{
            while (!ThreadsStop) {

                buf.getjob();

            }
            });
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
    Buffer<T> buf;
    bool ThreadsStop = false;
};




#endif // THREADEDMATRIXMULTIPLIER_H
