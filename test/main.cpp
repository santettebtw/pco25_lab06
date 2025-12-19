#include <gtest/gtest.h>
#include <pcosynchro/pcotest.h>

#include "multipliertester.h"
#include "multiplierthreadedtester.h"
#include "threadedmatrixmultiplier.h"

#define ThreadedMultiplierType ThreadedMatrixMultiplier<int>

// Decommenting the next line allows to check for interlocking
#define CHECK_DURATION

TEST(Multiplier, SingleThread){

#ifdef CHECK_DURATION
        ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                               constexpr int MATRIXSIZE = 500;
                               constexpr int NBTHREADS = 1;
                               constexpr int NBBLOCKSPERROW = 5;

                               MultiplierTester<ThreadedMultiplierType> tester;

                               tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                           }))
#endif // CHECK_DURATION

}


TEST(Multiplier, Simple){

#ifdef CHECK_DURATION
        ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                               constexpr int MATRIXSIZE = 500;
                               constexpr int NBTHREADS = 4;
                               constexpr int NBBLOCKSPERROW = 5;

                               MultiplierTester<ThreadedMultiplierType> tester;

                               tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                           }))
#endif // CHECK_DURATION

}


TEST(Multiplier, Reentering)
{

#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 500;
                           constexpr int NBTHREADS = 4;
                           constexpr int NBBLOCKSPERROW = 5;

                           MultiplierThreadedTester<ThreadedMultiplierType> tester(2);

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}

TEST(Multiplier, SmallMatrix)
{
#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(10, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 100;
                           constexpr int NBTHREADS = 2;
                           constexpr int NBBLOCKSPERROW = 4;

                           MultiplierTester<ThreadedMultiplierType> tester;

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}

TEST(Multiplier, ManyBlocks)
{
#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 400;
                           constexpr int NBTHREADS = 8;
                           constexpr int NBBLOCKSPERROW = 10;

                           MultiplierTester<ThreadedMultiplierType> tester;

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}

TEST(Multiplier, HighReentrancy)
{
#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 300;
                           constexpr int NBTHREADS = 4;
                           constexpr int NBBLOCKSPERROW = 6;

                           // Test with 4 concurrent multiply() calls (stress test for reentrancy)
                           MultiplierThreadedTester<ThreadedMultiplierType> tester(4);

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}

TEST(Multiplier, SingleBlock)
{
#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(10, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 200;
                           constexpr int NBTHREADS = 4;
                           constexpr int NBBLOCKSPERROW = 1;

                           // Edge case: only 1 block = 1 job total (minimal parallelism)
                           // Tests that implementation handles case where threads > jobs
                           MultiplierTester<ThreadedMultiplierType> tester;

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}

TEST(Multiplier, FewBlocksManyThreads)
{
#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(20, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 200;
                           constexpr int NBTHREADS = 16;
                           constexpr int NBBLOCKSPERROW = 2;

                           // Edge case: 2 blocks = 8 jobs, but 16 threads
                           // Tests thread management when threads > jobs
                           MultiplierTester<ThreadedMultiplierType> tester;

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}

TEST(Multiplier, ExtremeReentrancy)
{
#ifdef CHECK_DURATION
    ASSERT_DURATION_LE(30, ({
#endif // CHECK_DURATION
                           constexpr int MATRIXSIZE = 200;
                           constexpr int NBTHREADS = 4;
                           constexpr int NBBLOCKSPERROW = 4;

                           // Stress test: 8 concurrent multiply() calls
                           // Tests reentrancy with many concurrent computations
                           MultiplierThreadedTester<ThreadedMultiplierType> tester(8);

                           tester.test(MATRIXSIZE, NBTHREADS, NBBLOCKSPERROW);

#ifdef CHECK_DURATION
                       }))
#endif // CHECK_DURATION
}


int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
