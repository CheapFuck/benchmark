CC      = gcc
CFLAGS  = -O2 -fopenmp -lm
TARGET  = benchmark
SRC     = benchmark.c

ifeq ($(OS),Windows_NT)
    EXT = .exe
    RM  = del /Q
else
    EXT =
    RM  = rm -f
endif

all: $(TARGET)$(EXT)

$(TARGET)$(EXT): $(SRC)
	$(CC) $(SRC) $(CFLAGS) -o $(TARGET)$(EXT)

run: $(TARGET)$(EXT)
	./$(TARGET)$(EXT)

clean:
	$(RM) $(TARGET)$(EXT)

.PHONY: all run clean
