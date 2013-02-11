CCFLAGS :=
LDFLAGS :=

OBJS += fork-accept.o

%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

all: $(OBJS)
	$(CC) $(LDFLAGS) $< -o fork-accept
