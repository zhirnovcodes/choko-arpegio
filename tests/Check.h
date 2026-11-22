#pragma once
#include <cstdio>

// Each failing check prints one "FAIL <file>:<line> <what>" line. main() exits 1 when any failed.
extern int gFailures;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++gFailures;                                                         \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        auto va_ = (a);                                                          \
        auto vb_ = (b);                                                          \
        if (!(va_ == vb_)) {                                                     \
            ++gFailures;                                                         \
            std::printf("FAIL %s:%d %s == %s (%g vs %g)\n", __FILE__, __LINE__, #a, #b, \
                        static_cast<double>(va_), static_cast<double>(vb_));      \
        }                                                                        \
    } while (0)

void sequencesTests();
void timeTableTests();
void chaosTests();
void engineTests();
