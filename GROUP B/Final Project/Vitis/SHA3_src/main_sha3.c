#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"
#include "sha3.h"  // Include the provided SHA-3 implementation headers

#define TIMER_DEVICE_ID XPAR_TMRCTR_0_DEVICE_ID
#define MAX_INPUT_SIZE 256
#define CLOCK_FREQUENCY 100000000 // 100 MHz in Hz
#define DYNAMIC_POWER_MW 196      // Power in milliwatts

XTmrCtr TimerInstance;

void init_timer() {
    int status = XTmrCtr_Initialize(&TimerInstance, TIMER_DEVICE_ID);
    if (status != XST_SUCCESS) {
        xil_printf("Timer Initialization Failed!\n\r");
        while (1);
    }
    XTmrCtr_SetOptions(&TimerInstance, 0, XTC_AUTO_RELOAD_OPTION);
}

void compute_sha3(const char *input_data, int mode) {
    uint8_t digest[64]; // SHA3-512 uses 64 bytes
    memset(digest, 0, sizeof(digest));
    char hash_hex[2 * sizeof(digest) + 1];
    memset(hash_hex, 0, sizeof(hash_hex));
    sha3_context ctx;

    switch (mode) {
        case 1: { // SHA3-224
            sha3_Init(&ctx, 224);
            sha3_Update(&ctx, input_data, strlen(input_data));
            const uint8_t *final = sha3_Finalize(&ctx);
            for (int i = 0; i < 28; i++) {
                sprintf(hash_hex + i * 2, "%02x", final[i]);
            }
            xil_printf("SHA3-224 Hash: %s\n\r", hash_hex);
            break;
        }
        case 2: { // SHA3-256
            sha3_Init(&ctx, 256);
            sha3_Update(&ctx, input_data, strlen(input_data));
            const uint8_t *final = sha3_Finalize(&ctx);
            for (int i = 0; i < 32; i++) {
                sprintf(hash_hex + i * 2, "%02x", final[i]);
            }
            xil_printf("SHA3-256 Hash: %s\n\r", hash_hex);
            break;
        }
        case 3: { // SHA3-384
            sha3_Init(&ctx, 384);
            sha3_Update(&ctx, input_data, strlen(input_data));
            const uint8_t *final = sha3_Finalize(&ctx);
            for (int i = 0; i < 48; i++) {
                sprintf(hash_hex + i * 2, "%02x", final[i]);
            }
            xil_printf("SHA3-384 Hash: %s\n\r", hash_hex);
            break;
        }
        case 4: { // SHA3-512
            sha3_Init(&ctx, 512);
            sha3_Update(&ctx, input_data, strlen(input_data));
            const uint8_t *final = sha3_Finalize(&ctx);
            for (int i = 0; i < 64; i++) {
                sprintf(hash_hex + i * 2, "%02x", final[i]);
            }
            xil_printf("SHA3-512 Hash: %s\n\r", hash_hex);
            break;
        }
        default:
            xil_printf("Invalid mode selected.\n\r");
            return;
    }
}

void run_test_case(int sha_variant, int test_type) {
    char *data;
    size_t data_size = 0;

    switch (test_type) {
        case 1: { // Functional Test
            xil_printf("Enter the input data (max %d characters):\n\r", MAX_INPUT_SIZE - 1);
            data = (char *)malloc(MAX_INPUT_SIZE);
            char c;
            int i = 0;
            while (i < MAX_INPUT_SIZE - 1) {
                c = inbyte();
                if (c == '\r' || c == '\n') {
                    data[i] = '\0';
                    break;
                }
                data[i++] = c;
                outbyte(c);
            }
            data_size = strlen(data);
            xil_printf("\n\rRunning Functional Test:\n\r");
            break;
        }
        case 2: { // Edge Case
            data = (char *)malloc(1);
            data[0] = '\0';
            data_size = 0;
            xil_printf("Running Edge Case Test (Empty Input)\n\r");
            break;
        }
        case 3: { // Performance Test
            xil_printf("Choose Performance Test:\n\r");
            xil_printf("1: 1 KB Input\n\r");
            xil_printf("2: 1 MB Input\n\r");

            char choice = inbyte();
            outbyte(choice);
            xil_printf("\n\r");

            if (choice == '1') {
                data_size = 1024; // 1 KB
            } else if (choice == '2') {
                data_size = 1024 * 1024; // 1 MB
            } else {
                xil_printf("Invalid choice. Returning to main menu.\n\r");
                return;
            }

            data = (char *)malloc(data_size);
            memset(data, 'A', data_size);
            xil_printf("Running Performance Test(%lu bytes)\n\r", data_size);
            break;
        }
        default:
            xil_printf("Invalid test case.\n\r");
            return;
    }

    // Measure execution time
    XTmrCtr_Reset(&TimerInstance, 0);
    XTmrCtr_Start(&TimerInstance, 0);

    compute_sha3(data, sha_variant);

    XTmrCtr_Stop(&TimerInstance, 0);
    u64 elapsed_cycles = XTmrCtr_GetValue(&TimerInstance, 0);

    u64 execution_time_us = (elapsed_cycles * 1000000) / CLOCK_FREQUENCY;
    u64 throughput_bps = (data_size * 1000000) / execution_time_us;
    u64 energy_consumed_uj = (DYNAMIC_POWER_MW * execution_time_us) / 1000;

    xil_printf("Estimated Memory Usage: %lu bytes\n\r", data_size);
    xil_printf("Elapsed Cycles: %lu\n\r", elapsed_cycles);
    xil_printf("Execution Time: %lu microseconds\n\r", execution_time_us);
    xil_printf("Throughput: %lu bytes/second\n\r", throughput_bps);
    xil_printf("Estimated Energy Consumed: %lu microjoules\n\r", energy_consumed_uj);

    free(data);
}

int main() {
    init_platform();
    init_timer();

    while (1) {
        xil_printf("\n\r=== SHA-3 Tests ===\n\r");
        xil_printf("Select SHA Variant:\n\r");
        xil_printf("1: SHA3-224\n\r");
        xil_printf("2: SHA3-256\n\r");
        xil_printf("3: SHA3-384\n\r");
        xil_printf("4: SHA3-512\n\r");
        xil_printf("0: Exit\n\r");

        char variant_choice = inbyte();
        outbyte(variant_choice);
        xil_printf("\n\r");

        int sha_variant = variant_choice - '0';
        if (sha_variant == 0) {
            xil_printf("Exiting program. Goodbye!\n\r");
            break;
        }
        if (sha_variant < 1 || sha_variant > 4) {
            xil_printf("Invalid choice. Try again.\n\r");
            continue;
        }

        xil_printf("Select Test Type:\n\r");
        xil_printf("1: Functional Test\n\r");
        xil_printf("2: Edge Case Test (Empty Input)\n\r");
        xil_printf("3: Performance Test\n\r");
        xil_printf("0: Return to Main Menu\n\r");

        char test_choice = inbyte();
        outbyte(test_choice);
        xil_printf("\n\r");

        int test_type = test_choice - '0';
        if (test_type == 0) {
            xil_printf("Returning to main menu.\n\r");
            continue;
        }
        if (test_type < 1 || test_type > 3) {
            xil_printf("Invalid choice. Try again.\n\r");
            continue;
        }

        run_test_case(sha_variant, test_type);
    }

    cleanup_platform();
    return 0;
}
