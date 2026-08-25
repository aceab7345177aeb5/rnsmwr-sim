#include "randombytes.h"

#include <windows.h>
#include <wincrypt.h>

int randombytes(uint8_t *output, size_t n) {
    HCRYPTPROV prov = 0;
    size_t done = 0;

    if (!CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_FULL,
                              CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        return -1;
    }
    while (done < n) {
        DWORD chunk = (DWORD)(n - done);
        if (chunk > 0x7fffffff) {
            chunk = 0x7fffffff;
        }
        if (!CryptGenRandom(prov, chunk, output + done)) {
            CryptReleaseContext(prov, 0);
            return -1;
        }
        done += chunk;
    }
    CryptReleaseContext(prov, 0);
    return 0;
}