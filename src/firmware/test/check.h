/*
 * check.h - verificacoes minimas para os testes que rodam no computador.
 * Cada teste e um executavel proprio, entao os contadores sao estaticos.
 */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int g_checks;
static int g_failures;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            g_failures++;                                                      \
            printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, (msg));           \
        }                                                                      \
    } while (0)

/* Confere um numero inteiro sem sinal e imprime esperado x obtido se falhar. */
#define CHECK_EQ_U(got, want, msg)                                             \
    do {                                                                       \
        const unsigned long long g_ = (unsigned long long)(got);               \
        const unsigned long long w_ = (unsigned long long)(want);              \
        g_checks++;                                                            \
        if (g_ != w_) {                                                        \
            g_failures++;                                                      \
            printf("FALHOU %s:%d: %s: deu %llu, esperado %llu\n", __FILE__,    \
                   __LINE__, (msg), g_, w_);                                   \
        }                                                                      \
    } while (0)

/* Fim do teste: imprime o resumo e devolve o codigo de saida do programa. */
static int check_finish(void)
{
    if (g_failures != 0) {
        printf("%d de %d verificacoes falharam\n", g_failures, g_checks);
        return 1;
    }
    printf("ok: %d verificacoes passaram\n", g_checks);
    return 0;
}

#endif /* CHECK_H */
