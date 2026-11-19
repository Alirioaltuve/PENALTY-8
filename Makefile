# =====================================================================
#  PENALTY-8  -  STRIKER-8
#  Compilacion del computador y de las pruebas con SystemC.
#
#  Linux / WSL (entorno principal, probado con g++ y SystemC 3.0.2):
#      make            compila las pruebas
#      make run        compila y ejecuta las pruebas
#      make clean      borra el ejecutable
#
#  Windows (MSYS2 / MinGW / SystemC 2.3.4):
#      make SYSTEMC_HOME=<ruta a systemc> run
#
#  En Linux se detectan automaticamente las opciones de SystemC con
#  pkg-config; si no existe el archivo .pc se usa -lsystemc -lpthread.
# =====================================================================

CXX        ?= g++
PKG_CONFIG ?= pkg-config

PRUEBAS   := pruebas_striker
FUENTES   := striker.cpp pruebas_striker.cpp
CABECERAS := striker.h programa_striker.h
ASM       ?= programa.asm

ifeq ($(OS),Windows_NT)
  # ------------------------- Windows -------------------------
  EXE             := .exe
  SYSTEMC_HOME    ?= C:/SystemC/systemc-2.3.4
  SYSTEMC_CFLAGS  ?= -I$(SYSTEMC_HOME)/src
  SYSTEMC_LDFLAGS ?= -L$(SYSTEMC_HOME)/build_mingw/src/.libs
  SYSTEMC_LIBS    ?= -lsystemc -lws2_32
  CXXSTD          ?= -std=c++20
else
  # ---------------------- Linux / WSL -----------------------
  EXE :=
  ifeq ($(shell $(PKG_CONFIG) --exists systemc 2>/dev/null && echo yes),yes)
    SYSTEMC_CFLAGS  := $(shell $(PKG_CONFIG) --cflags systemc)
    SYSTEMC_LIBDIR  :=
    SYSTEMC_LIBS    := $(shell $(PKG_CONFIG) --libs systemc)
  else
    SYSTEMC_CFLAGS  :=
    SYSTEMC_LIBDIR  :=
    SYSTEMC_LIBS    := -lsystemc -lpthread
  endif
  SYSTEMC_LDFLAGS :=
  CXXSTD          ?= -std=c++17
endif

CXXFLAGS := $(CXXSTD) -O2 -Wall -Wextra $(SYSTEMC_CFLAGS)

# ----------------------------- Objetivos -----------------------------
all: $(PRUEBAS)$(EXE)

$(PRUEBAS)$(EXE): $(FUENTES) $(CABECERAS)
	$(CXX) $(CXXFLAGS) $(FUENTES) -o $@ $(SYSTEMC_LDFLAGS) $(SYSTEMC_LIBS)

run: $(PRUEBAS)$(EXE)
	./$(PRUEBAS)$(EXE)

asm: $(PRUEBAS)$(EXE)
	./$(PRUEBAS)$(EXE) $(ASM)

clean:
ifeq ($(OS),Windows_NT)
	-@if exist $(PRUEBAS).exe del /Q $(PRUEBAS).exe
	-@if exist $(PRUEBAS) del /Q $(PRUEBAS)
else
	-$(RM) $(PRUEBAS)
endif

.PHONY: all run asm clean
