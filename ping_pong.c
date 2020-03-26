#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>

#include <emmintrin.h>

struct pair_of_line {
    char cache_blocker_1[256];
    _Atomic int marker;
    char cache_blocker_2[256];
    _Atomic int value_read;
    char cache_blocker_3[256];
};

struct pair_of_line _Alignas(4096) ping_pong_lines_a = {
    .marker = -1,
    .value_read = -1,
};

struct pair_of_line _Alignas(4096) ping_pong_lines_b = {
    .marker = -1,
    .value_read = -1,
};

void mfence() {
    __asm volatile("mfence" :::"memory");
}

uint64_t rdtscp() {
    unsigned hi, lo;
    __asm volatile ("rdtscp" : "=a"(lo), "=d"(hi) :: "memory", "rcx");
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

void prefetch(void *a) {
#ifdef PREFETCH
    __builtin_prefetch(a);
#endif
}

void lfence() {
#ifdef LFENCE
    _mm_lfence();
#endif
}

const int MAX_SEND = 10000000;

void *run_pong(void *_ignore) {
    double running_total = 0;
    // Use this to create a data dependency between each loop
    int read_val = 0;
    for (int i = 0; i < MAX_SEND; i++) {
        mfence();
        uint64_t start = rdtscp();
        atomic_store_explicit(&ping_pong_lines_a.value_read, read_val, memory_order_relaxed);
        atomic_store_explicit(&ping_pong_lines_a.marker, read_val, memory_order_release);

        while (atomic_load_explicit(&ping_pong_lines_b.marker, memory_order_acquire) != read_val + 1) {
            prefetch(&ping_pong_lines_b.value_read);
            lfence();
        }
        read_val = atomic_load_explicit(&ping_pong_lines_b.value_read, memory_order_relaxed);
        uint64_t end = rdtscp();
        assert(read_val == i+1);

        // stall for some unpredictable time so the predictor can't learn
        int rounds = (rand() * end) % 10;
        for (int i = 0; i < rounds; i++) {
            mfence();
        }

        // ignore some loops to allow cpus to get to max frequency
        if (end >= start && (i >= MAX_SEND/2)) {
            running_total += (end - start);
        }
    }

    printf("Average cycles per turn is %f\n", running_total / (MAX_SEND/2));
    return NULL;
}

void *run_ping(void *_ignore) {
    // Use this to create a data dependency between each loop
    int read_val = 0;
    uint64_t total_missed_reads = 0;
    for (int i = 0; i < MAX_SEND; i++) {
        while (atomic_load_explicit(&ping_pong_lines_a.marker, memory_order_acquire) != i) {
            total_missed_reads += (i >= MAX_SEND/2);
            prefetch(&ping_pong_lines_a.value_read);
            lfence();
        }
        read_val = atomic_load_explicit(&ping_pong_lines_a.value_read, memory_order_relaxed);

        atomic_store_explicit(&ping_pong_lines_b.value_read, read_val + 1, memory_order_relaxed);
        atomic_store_explicit(&ping_pong_lines_b.marker, read_val + 1, memory_order_release);
    }

    printf("Missed %f reads on average\n", (2*(double)total_missed_reads)/MAX_SEND);
    return NULL;
}

int main() { 
    pthread_t ping_thread, pong_thread;
    pthread_create(&pong_thread, NULL, run_pong, NULL);
    pthread_create(&ping_thread, NULL, run_ping, NULL);
    pthread_join(pong_thread, NULL);
    pthread_join(ping_thread, NULL);
}
