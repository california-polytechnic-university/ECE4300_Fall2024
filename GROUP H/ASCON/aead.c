#include <stdio.h>
#include <string.h>
#include <stdlib.h> // For rand() and srand()
#include <time.h>   // For time()
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
#define AD_SIZE 0     // Size of additional data (can be set to 0 if not used)

// Function to generate a random key
void generate_random_key(unsigned char* key) {
    for (size_t i = 0; i < KEY_SIZE; i++) {
        key[i] = rand() % 256; // Generate a random byte (0-255)
    }
}

// Function to generate a random nonce
void generate_random_nonce(unsigned char* nonce) {
    for (size_t i = 0; i < NONCE_SIZE; i++) {
        nonce[i] = rand() % 256; // Generate a random byte (0-255)
    }
}

// Function to read data from a file
unsigned char* read_large_bin(const char* filename, size_t* file_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("File opening failed");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* buffer = (unsigned char*)malloc(*file_size);
    if (!buffer) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, *file_size, file);
    fclose(file);
    return buffer;
}

// Function to benchmark encryption
unsigned char* benchmark_encryption(const unsigned char* input_data, size_t input_length, unsigned char* key, unsigned char* nonce, unsigned __int64* ciphertext_len) {
    unsigned char* ciphertext = malloc(input_length + 16); // Adjust for overhead (16 bytes for tag)
    if (!ciphertext) {
        perror("Memory allocation failed");
        return NULL;
    }

    *ciphertext_len = 0; // Output length of ciphertext
    const unsigned char* ad = NULL;       // Additional data, if any
    unsigned __int64 adlen = 0;            // Length of additional data

    clock_t start_encrypt = clock();
    int result = crypto_aead_encrypt(ciphertext, ciphertext_len, input_data, input_length, ad, adlen, NULL, nonce, key);
    clock_t end_encrypt = clock();

    if (result != 0) {
        printf("Encryption failed with error code: %d\n", result);
        free(ciphertext);
        return NULL;
    }
    else {
        double encrypt_time = ((double)(end_encrypt - start_encrypt)) / CLOCKS_PER_SEC;
        printf("Encryption time for %zu bytes: %f seconds\n", input_length, encrypt_time);
        printf("Ciphertext length: %llu bytes\n", (unsigned long long) * ciphertext_len); // Display length

        // Calculate and display throughput
        double throughput = input_length / encrypt_time;
        printf("Throughput for encryption: %f bytes/second\n", throughput);
    }

    return ciphertext;
}

// Function to benchmark decryption
void benchmark_decryption(const unsigned char* ciphertext, size_t ciphertext_length, unsigned char* key, unsigned char* nonce) {
    unsigned char* decrypted_text = malloc(ciphertext_length); // Allocate memory for the decrypted text
    if (!decrypted_text) {
        perror("Memory allocation failed");
        return;
    }

    unsigned __int64 decrypted_length = 0; // Output length of decrypted text
    const unsigned char* ad = NULL;         // Additional data, if any
    unsigned __int64 adlen = 0;              // Length of additional data

    clock_t start_decrypt = clock();
    int result = crypto_aead_decrypt(decrypted_text, &decrypted_length, NULL, ciphertext, ciphertext_length, ad, adlen, nonce, key);
    clock_t end_decrypt = clock();

    if (result != 0) {
        printf("Decryption failed with error code: %d\n", result);
    }
    else {
        double decrypt_time = ((double)(end_decrypt - start_decrypt)) / CLOCKS_PER_SEC;
        printf("Decryption time for %zu bytes: %f seconds\n", ciphertext_length, decrypt_time);
        printf("Decrypted length: %llu bytes\n", (unsigned long long)decrypted_length); // Display length

        // Calculate and display throughput
        double throughput = ciphertext_length / decrypt_time;
        printf("Throughput for decryption: %f bytes/second\n", throughput);
    }

    free(decrypted_text);
}

int main() {
    srand(time(NULL)); // Seed the random number generator

    unsigned char key[KEY_SIZE];   // Buffer for the key
    unsigned char nonce[NONCE_SIZE]; // Buffer for the nonce

    // Read the input data from the large binary file
    size_t input_length;
    unsigned char* input_data = read_large_bin("C:\\Users\\zbast\\largefile.bin", &input_length); //Include directory to the file used in ASCON
    if (!input_data) {
        return 1; // Error reading the file
    }

    // Generate random key and nonce
    generate_random_key(key);
    generate_random_nonce(nonce);

    // Print key and nonce in hex for debugging (optional)
    printf("Random Key: ");
    for (int i = 0; i < sizeof(key); i++) {
        printf("%02x", key[i]);
    }
    printf("\n");

    printf("Random Nonce: ");
    for (int i = 0; i < sizeof(nonce); i++) {
        printf("%02x", nonce[i]);
    }
    printf("\n");

    // Variables to hold ciphertext and its length
    unsigned __int64 ciphertext_length;
    unsigned char* ciphertext = benchmark_encryption(input_data, input_length, key, nonce, &ciphertext_length);

    if (ciphertext) {
        // Benchmark decryption using the ciphertext obtained from encryption
        benchmark_decryption(ciphertext, ciphertext_length, key, nonce);
        free(ciphertext); // Clean up the ciphertext after use
    }

    free(input_data); // Clean up the input data
    return 0;
}
