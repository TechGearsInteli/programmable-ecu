# Testes de firmware que rodam no computador, sem placa.
#   make -C src/firmware/test        compila e roda
#   make -C src/firmware/test clean  apaga o executavel
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

TESTS := test_table$(EXE)

.PHONY: test clean
test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

test_table$(EXE): test_table.c $(CORE)/ecu_table.c $(CORE)/ecu_table.h
	$(CC) $(CFLAGS) -I$(CORE) -o $@ test_table.c $(CORE)/ecu_table.c

clean:
	rm -f $(TESTS)
