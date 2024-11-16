#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "api.h"
#include "ascon.h"
#include "permutations.h"
#include "printstate.h"
#include "word.h"

int crypto_aead_encrypt(unsigned char* c, unsigned long long* clen,
    const unsigned char* m, unsigned long long mlen,
    const unsigned char* ad, unsigned long long adlen,
    const unsigned char* nsec, const unsigned char* npub,
    const unsigned char* k) {
    (void)nsec;

    /* set ciphertext size */
    *clen = mlen + CRYPTO_ABYTES;

    /* load key and nonce */
    const uint64_t K0 = LOADBYTES(k, 8);
    const uint64_t K1 = LOADBYTES(k + 8, 8);
    const uint64_t N0 = LOADBYTES(npub, 8);
    const uint64_t N1 = LOADBYTES(npub + 8, 8);

    /* initialize */
    ascon_state_t s;
    s.x[0] = ASCON_128_IV;
    s.x[1] = K0;
    s.x[2] = K1;
    s.x[3] = N0;
    s.x[4] = N1;
    printstate("init 1st key xor", &s);
    P12(&s);
    s.x[3] ^= K0;
    s.x[4] ^= K1;
    printstate("init 2nd key xor", &s);

    if (adlen) {
        /* full associated data blocks */
        while (adlen >= ASCON_128_RATE) {
            s.x[0] ^= LOADBYTES(ad, 8);
            printstate("absorb adata", &s);
            P6(&s);
            ad += ASCON_128_RATE;
            adlen -= ASCON_128_RATE;
        }
        /* final associated data block */
        s.x[0] ^= LOADBYTES(ad, adlen);
        s.x[0] ^= PAD(adlen);
        printstate("pad adata", &s);
        P6(&s);
    }
    /* domain separation */
    s.x[4] ^= DSEP();
    printstate("domain separation", &s);

    /* full plaintext blocks */
    while (mlen >= ASCON_128_RATE) {
        s.x[0] ^= LOADBYTES(m, 8);
        STOREBYTES(c, s.x[0], 8);
        printstate("absorb plaintext", &s);
        P6(&s);
        m += ASCON_128_RATE;
        c += ASCON_128_RATE;
        mlen -= ASCON_128_RATE;
    }
    /* final plaintext block */
    s.x[0] ^= LOADBYTES(m, mlen);
    STOREBYTES(c, s.x[0], mlen);
    s.x[0] ^= PAD(mlen);
    c += mlen;
    printstate("pad plaintext", &s);

    /* finalize */
    s.x[1] ^= K0;
    s.x[2] ^= K1;
    printstate("final 1st key xor", &s);
    P12(&s);
    s.x[3] ^= K0;
    s.x[4] ^= K1;
    printstate("final 2nd key xor", &s);

    /* get tag */
    STOREBYTES(c, s.x[3], 8);
    STOREBYTES(c + 8, s.x[4], 8);

    return 0;
}

int crypto_aead_decrypt(unsigned char* m, unsigned long long* mlen,
    unsigned char* nsec, const unsigned char* c,
    unsigned long long clen, const unsigned char* ad,
    unsigned long long adlen, const unsigned char* npub,
    const unsigned char* k) {
    (void)nsec;

    if (clen < CRYPTO_ABYTES) return -1;

    /* set plaintext size */
    *mlen = clen - CRYPTO_ABYTES;

    /* load key and nonce */
    const uint64_t K0 = LOADBYTES(k, 8);
    const uint64_t K1 = LOADBYTES(k + 8, 8);
    const uint64_t N0 = LOADBYTES(npub, 8);
    const uint64_t N1 = LOADBYTES(npub + 8, 8);

    /* initialize */
    ascon_state_t s;
    s.x[0] = ASCON_128_IV;
    s.x[1] = K0;
    s.x[2] = K1;
    s.x[3] = N0;
    s.x[4] = N1;
    printstate("init 1st key xor", &s);
    P12(&s);
    s.x[3] ^= K0;
    s.x[4] ^= K1;
    printstate("init 2nd key xor", &s);

    if (adlen) {
        /* full associated data blocks */
        while (adlen >= ASCON_128_RATE) {
            s.x[0] ^= LOADBYTES(ad, 8);
            printstate("absorb adata", &s);
            P6(&s);
            ad += ASCON_128_RATE;
            adlen -= ASCON_128_RATE;
        }
        /* final associated data block */
        s.x[0] ^= LOADBYTES(ad, adlen);
        s.x[0] ^= PAD(adlen);
        printstate("pad adata", &s);
        P6(&s);
    }
    /* domain separation */
    s.x[4] ^= DSEP();
    printstate("domain separation", &s);

    /* full ciphertext blocks */
    clen -= CRYPTO_ABYTES;
    while (clen >= ASCON_128_RATE) {
        uint64_t c0 = LOADBYTES(c, 8);
        STOREBYTES(m, s.x[0] ^ c0, 8);
        s.x[0] = c0;
        printstate("insert ciphertext", &s);
        P6(&s);
        m += ASCON_128_RATE;
        c += ASCON_128_RATE;
        clen -= ASCON_128_RATE;
    }
    /* final ciphertext block */
    uint64_t c0 = LOADBYTES(c, clen);
    STOREBYTES(m, s.x[0] ^ c0, clen);
    s.x[0] = CLEARBYTES(s.x[0], clen);
    s.x[0] |= c0;
    s.x[0] ^= PAD(clen);
    c += clen;
    printstate("pad ciphertext", &s);

    /* finalize */
    s.x[1] ^= K0;
    s.x[2] ^= K1;
    printstate("final 1st key xor", &s);
    P12(&s);
    s.x[3] ^= K0;
    s.x[4] ^= K1;
    printstate("final 2nd key xor", &s);

    /* get tag */
    uint8_t t[16];
    STOREBYTES(t, s.x[3], 8);
    STOREBYTES(t + 8, s.x[4], 8);

    /* verify should be constant time, check compiler output */
    int i;
    int result = 0;
    for (i = 0; i < CRYPTO_ABYTES; ++i) result |= c[i] ^ t[i];
    result = (((result - 1) >> 8) & 1) - 1;

    return result;
}


#define KEY_SIZE 16   // ASCON key size
#define NONCE_SIZE 12 // ASCON nonce size
#define CHUNK_SIZE (1 * 1024 * 1024) // 1 MB

typedef struct {
    unsigned char* input_data;
    unsigned char* output_data;
    unsigned char* key;
    unsigned char* nonce;
    int thread_num;
    unsigned long long iterations;
} ThreadData;

void generate_random_key(unsigned char* key) {
    for (size_t i = 0; i < KEY_SIZE; i++) {
        key[i] = rand() % 256;
    }
}

void generate_random_nonce(unsigned char* nonce) {
    for (size_t i = 0; i < NONCE_SIZE; i++) {
        nonce[i] = rand() % 256;
    }
}

void* encrypt_chunk(void* args) {
    ThreadData* data = (ThreadData*)args;

    unsigned long long ciphertext_len = 0;
    for (unsigned long long i = 0; i < data->iterations; i++) {
        crypto_aead_encrypt(data->output_data, &ciphertext_len, data->input_data, CHUNK_SIZE, NULL, 0, NULL, data->nonce, data->key);
    }
    return NULL;
}

void* decrypt_chunk(void* args) {
    ThreadData* data = (ThreadData*)args;

    unsigned long long plaintext_len = 0;
    for (unsigned long long i = 0; i < data->iterations; i++) {
        crypto_aead_decrypt(data->output_data, &plaintext_len, NULL, data->input_data, CHUNK_SIZE, NULL, 0, data->nonce, data->key);
    }
    return NULL;
}

void benchmark_multithreaded(int num_threads, unsigned long long total_size, int is_encryption) {
    unsigned char* input_data = malloc(CHUNK_SIZE);
    unsigned char* output_data = malloc(CHUNK_SIZE + 16);
    if (!input_data || !output_data) {
        perror("Memory allocation failed");
        return;
    }

    // Generate random input data, key, and nonce
    for (unsigned long long i = 0; i < CHUNK_SIZE; i++) {
        input_data[i] = rand() % 256;
    }

    unsigned char key[KEY_SIZE];
    unsigned char nonce[NONCE_SIZE];
    generate_random_key(key);
    generate_random_nonce(nonce);

#ifdef _WIN32
    HANDLE* threads = malloc(num_threads * sizeof(HANDLE));
#else
    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
#endif

    ThreadData* thread_data = malloc(num_threads * sizeof(ThreadData));
    unsigned long long iterations_per_thread = (total_size / CHUNK_SIZE) / num_threads;

    clock_t start = clock();
    for (int i = 0; i < num_threads; i++) {
        thread_data[i] = (ThreadData){
            .input_data = input_data,
            .output_data = output_data,
            .key = key,
            .nonce = nonce,
            .thread_num = i,
            .iterations = iterations_per_thread
        };

#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(is_encryption ? encrypt_chunk : decrypt_chunk), &thread_data[i], 0, NULL);
#else
        pthread_create(&threads[i], NULL, is_encryption ? encrypt_chunk : decrypt_chunk, &thread_data[i]);
#endif
    }

#ifdef _WIN32
    WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);
    for (int i = 0; i < num_threads; i++) {
        CloseHandle(threads[i]);
    }
#else
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
#endif

    clock_t end = clock();
    double elapsed_time = (double)(end - start) / CLOCKS_PER_SEC;
    double throughput = (double)total_size / (1024 * 1024) / elapsed_time;

    printf("Multithreaded %s time for %llu MB: %f seconds\n",
        is_encryption ? "encryption" : "decryption",
        total_size / (1024 * 1024),
        elapsed_time);
    printf("Throughput: %f MB/s\n", throughput);

    free(input_data);
    free(output_data);
    free(thread_data);
    free(threads);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <total_size_in_MB> <num_threads> <mode: 0=encryption, 1=decryption>\n", argv[0]);
        return 1;
    }

    unsigned long long total_size_in_mb = atoi(argv[1]);
    unsigned long long total_size = total_size_in_mb * 1024 * 1024;
    int num_threads = atoi(argv[2]);
    int mode = atoi(argv[3]);

    printf("Running multithreaded %s with %d threads...\n",
        mode == 0 ? "encryption" : "decryption", num_threads);
    benchmark_multithreaded(num_threads, total_size, mode == 0);

    return 0;
}
