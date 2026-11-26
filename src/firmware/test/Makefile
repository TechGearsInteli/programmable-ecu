# Testes de firmware que rodam no computador, sem placa.
#   make -C src/firmware/test        compila e roda todos
#   make -C src/firmware/test clean  apaga os executaveis
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

TESTS := test_table$(EXE) test_curve$(EXE) test_fuel$(EXE)

.PHONY: test clean
test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

test_table$(EXE): test_table.c $(CORE)/ecu_table.c $(CORE)/ecu_table.h
	$(CC) $(CFLAGS) -I$(CORE) -o $@ test_table.c $(CORE)/ecu_table.c

test_curve$(EXE): test_curve.c check.h $(CORE)/ecu_curve.c $(CORE)/ecu_curve.h
	$(CC) $(CFLAGS) -I$(CORE) -o $@ test_curve.c $(CORE)/ecu_curve.c

test_fuel$(EXE): test_fuel.c check.h $(CORE)/ecu_fuel.c $(CORE)/ecu_fuel.h \
                 $(CORE)/ecu_curve.c $(CORE)/ecu_curve.h \
                 $(CORE)/ecu_table.c $(CORE)/ecu_table.h
	$(CC) $(CFLAGS) -I$(CORE) -o $@ test_fuel.c $(CORE)/ecu_fuel.c \
	    $(CORE)/ecu_curve.c $(CORE)/ecu_table.c

clean:
	rm -f $(TESTS)
