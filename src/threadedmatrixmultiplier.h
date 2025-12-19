#ifndef THREADEDMATRIXMULTIPLIER_H
#define THREADEDMATRIXMULTIPLIER_H

#include <queue>
#include <vector>

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


/// Buffer class for job distribution using Hoare monitor
///
template<class T>
class Buffer : public PcoHoareMonitor
{
public:
    int nbJobFinished{0};

    ///
    /// \brief sends a job to the buffer
    /// \param params reference to a ComputeParameters object
    ///
    void sendJob(ComputeParameters<T> params) {
		monitorIn();
		jobs.push(params);
		signal(jobAvailable);
		monitorOut();
	}

    ///
    /// \brief requests a job to the buffer
    /// \param parameters reference to a ComputeParameters object
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
	/// \brief registers a new computation and returns its job ID
	/// \param totalJobs the total number of jobs for this computation
	/// \return the job ID for this computation
	///
	int registerComputation(int totalJobs) {
		monitorIn();
		int jobId = nextJobId++;
		size_t neededSize = jobId + 1;
		if (neededSize > remainingJobs.size()) {
			size_t newSize = neededSize * 2;
			remainingJobs.resize(newSize);
		}
		remainingJobs[jobId] = totalJobs;
		monitorOut();
		return jobId;
	}

	///
	/// \brief notifies that a job has been finished
	/// \param jobId the ID of the computation this job is from
	///
	void notifyJobFinished(int jobId) {
		monitorIn();
		nbJobFinished++;
		
		size_t idx = jobId;
		if (idx < remainingJobs.size() && remainingJobs[jobId] > 0) {
			remainingJobs[jobId]--;
			if (remainingJobs[jobId] == 0) {
				signal(jobCompletionCond);
			}
		}
		monitorOut();
	}

	///
	/// \brief waits for all jobs of a computation to finish
	/// \param jobId the ID of the computation to wait for
	///
	void waitForCompletion(int jobId) {
		monitorIn();
		while (remainingJobs[jobId] > 0) {
			wait(jobCompletionCond);
		}
		monitorOut();
	}
	
	///
	/// \brief resets the job counter for a new computation
	///
	void resetJobCounter() {
		monitorIn();
		nbJobFinished = 0;
		monitorOut();
	}
	
	///
	/// \brief signal all threads to terminate
	///
	void signalTermination() {
		monitorIn();
		shouldTerminate = true;
		for (int i = 0; i < 100; i++) {
			signal(jobAvailable);
		}
		monitorOut();
	}
	
	///
	/// \brief resets termination flag (for reentering)
	///
	void resetTermination() {
		monitorIn();
		shouldTerminate = false;
		monitorOut();
	}

private:
	std::queue<ComputeParameters<T>> jobs;
	Condition jobAvailable;

	Condition jobCompletionCond;
	std::vector<int> remainingJobs;

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
	static void workerThreadFunction(ThreadedMatrixMultiplier<S>* multiplier) {
		ComputeParameters<S> params;
		while(multiplier->buf.getJob(params)) {
			// calculate block boundaries
			int startI = params.blockI * params.blockSize;
			int endI = startI + params.blockSize;
			int startJ = params.blockJ * params.blockSize;
			int endJ = startJ + params.blockSize;
			int startK = params.blockK * params.blockSize;
			int endK = startK + params.blockSize;

			// compute partial sum for this (i,j,k) block
			// multiple k-blocks contribute to same C[i][j], so we batch updates
			std::vector<std::vector<S>> partialSums(endI - startI, std::vector<S>(endJ - startJ, S(0)));
			for (int i = startI; i < endI; i++) {
				for (int j = startJ; j < endJ; j++) {
					for (int k = startK; k < endK; k++) {
						partialSums[i - startI][j - startJ] += params.A->element(k, j) * params.B->element(i, k);
					}
				}
			}
			
			// accumulate partial sums into result matric
			// we need mutex here because multiple threads are going to write to
			// the same result matrix
			multiplier->resultMutex.lock();
			for (int i = startI; i < endI; i++) {
				for (int j = startJ; j < endJ; j++) {
					S current = params.C->element(i, j);
					params.C->setElement(i, j, current + partialSums[i - startI][j - startJ]);
				}
			}
			multiplier->resultMutex.unlock();

			multiplier->buf.notifyJobFinished(params.jobId);
		}
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
        buf.signalTermination();
        
        for (size_t i = 0; i < workerThreads.size(); i++) {
            if (workerThreads[i]) {
                workerThreads[i]->join();
                delete workerThreads[i];
                workerThreads[i] = nullptr;
            }
        }
        
        workerThreads.clear();
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
		buf.resetJobCounter();
		buf.resetTermination();

		int blockSize = A.size() / nbBlocksPerRow;
		
		// initialize result matrix C to 0s to make sure it is empty
		int matrixSize = A.size();
		for (int i = 0; i < matrixSize; i++) {
			for (int j = 0; j < matrixSize; j++) {
				C.setElement(i, j, T(0));
			}
		}

    	int totalJobs = nbBlocksPerRow * nbBlocksPerRow * nbBlocksPerRow;
    	int jobId = buf.registerComputation(totalJobs);

		for (int i = 0; i < nbBlocksPerRow; i++) {
			for (int j = 0; j < nbBlocksPerRow; j++) {
				for (int k = 0; k < nbBlocksPerRow; k++) {
					ComputeParameters<T> params;
					params.blockSize = blockSize;
					params.blockI = i;
					params.blockJ = j;
					params.blockK = k;
					params.A = &A;
					params.B = &B;
					params.C = &C;
					params.jobId = jobId;
					buf.sendJob(params);
				}
			}
		}

		buf.waitForCompletion(jobId);
    }

protected:
    int nbThreads;
    int nbBlocksPerRow;
	std::vector<PcoThread*> workerThreads;
    Buffer<T> buf;
    PcoMutex resultMutex;
};




#endif // THREADEDMATRIXMULTIPLIER_H
