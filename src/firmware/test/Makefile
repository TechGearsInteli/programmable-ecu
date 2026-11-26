# Testes de firmware que rodam no computador, sem placa.
#   make -C src/firmware/test        compila e roda todos
#   make -C src/firmware/test clean  apaga os executaveis
#
# Todo arquivo test_*.c vira um teste, ligado a todos os .c de ../core. Para
# criar um teste novo basta criar o arquivo: nao precisa mexer neste Makefile.
#
# Flags estritas de proposito: aviso de compilador no core vira erro no CI.

# O make traz CC=cc por padrao; so troca para gcc se ninguem definiu o CC.
ifeq ($(origin CC),default)
CC := gcc
endif

CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Wconversion -Werror
CORE    := ../core

ifeq ($(OS),Windows_NT)
EXE := .exe
endif

CORE_SRC := $(wildcard $(CORE)/*.c)
CORE_HDR := $(wildcard $(CORE)/*.h)
TEST_SRC := $(wildcard test_*.c)
TESTS    := $(TEST_SRC:.c=$(EXE))

.PHONY: test clean
test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

$(TESTS): %$(EXE): %.c $(CORE_SRC) $(CORE_HDR) $(wildcard *.h)
	$(CC) $(CFLAGS) -I$(CORE) -I. -o $@ $< $(CORE_SRC)

clean:
	rm -f $(TESTS)
