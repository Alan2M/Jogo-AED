// Quicksort genérico para ordenar arrays em C
#ifndef QUICKSORT_H
#define QUICKSORT_H

#ifdef __cplusplus
extern "C" {
#endif

// Assinatura compatível com qsort: retorna <0, 0, >0
typedef int (*cmp_fn)(const void*, const void*);

void quicksort(void* base, int count, int elemSize, cmp_fn cmp);

#ifdef __cplusplus
}
#endif

#endif
