    
// Implementação simples de quicksort genérico (iterativo)
#include "quicksort.h"
#include <string.h>

static void swap_bytes(char* a, char* b, int n) {
    while (n--) { char t = *a; *a++ = *b; *b++ = t; }
}

static int partition(char* base, int elemSize, int lo, int hi, cmp_fn cmp) {
    int mid = lo + (hi - lo) / 2;
    char* pivot = base + mid * elemSize;
    int i = lo - 1;
    int j = hi + 1;
    for (;;) {
        do { i++; } while (cmp(base + i*elemSize, pivot) < 0);
        do { j--; } while (cmp(base + j*elemSize, pivot) > 0);
        if (i >= j) return j;
        swap_bytes(base + i*elemSize, base + j*elemSize, elemSize);
    }
}

static void quicksort_rec(char* base, int elemSize, int lo, int hi, cmp_fn cmp) {
    while (lo < hi) {
        int p = partition(base, elemSize, lo, hi, cmp);
        // Ordena o lado menor primeiro para reduzir profundidade
        if (p - lo < hi - (p + 1)) {
            quicksort_rec(base, elemSize, lo, p, cmp);
            lo = p + 1;
        } else {
            quicksort_rec(base, elemSize, p + 1, hi, cmp);
            hi = p;
        }
    }
}

void quicksort(void* base, int count, int elemSize, cmp_fn cmp) {
    if (!base || count <= 1 || elemSize <= 0 || !cmp) return;
    quicksort_rec((char*)base, elemSize, 0, count - 1, cmp);
}
