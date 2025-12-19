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
		// Resize vector if needed
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
	/// \brief Notifies that a job has been finished
	/// \param jobId The ID of the computation this job belongs to
	///
	void notifyJobFinished(int jobId) {
		monitorIn();
		nbJobFinished++;
		
		// Decrement remaining jobs for this jobId
		size_t idx = jobId;
		if (idx < remainingJobs.size() && remainingJobs[jobId] > 0) {
			remainingJobs[jobId]--;
			// If all jobs are done, signal waiting threads
			if (remainingJobs[jobId] == 0) {
				signal(jobCompletionCond);
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
		// Wait until all jobs for this computation are done
		while (remainingJobs[jobId] > 0) {
			wait(jobCompletionCond);
		}
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

	Condition jobCompletionCond; // condition variable for job completion
	std::vector<int> remainingJobs; // remaining jobs per job id (decremented to 0)

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
			// calcul limites des blocks
			int startI = params.blockI * params.blockSize;
			int endI = startI + params.blockSize;
			int startJ = params.blockJ * params.blockSize;
			int endJ = startJ + params.blockSize;
			int startK = params.blockK * params.blockSize;
			int endK = startK + params.blockSize;

			for (int i = startI; i < endI; i++) {
				for (int j = startJ; j < endJ; j++) {
					S partialSum = S(0);
					for (int k = startK; k < endK; k++) {
						partialSum += params.A->element(k, j) * params.B->element(i, k);
					}
					params.C->setElement(i, j, params.C->element(i, j) + partialSum);
				}
			}

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
        // Signaler la terminaison à tous les threads workers
        buf.signalTermination();
        
        // Attendre que tous les threads se terminent et les supprimer
        for (size_t i = 0; i < workerThreads.size(); i++) {
            if (workerThreads[i]) {
                workerThreads[i]->join();
                delete workerThreads[i];
                workerThreads[i] = nullptr;
            }
        }
        
        // Nettoyer le vecteur
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
};




#endif // THREADEDMATRIXMULTIPLIER_H
