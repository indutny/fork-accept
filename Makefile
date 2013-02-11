CCFLAGS :=
LDFLAGS :=

OBJS += fork-accept.o

%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

fork-accept: $(OBJS)
	$(CC) $(LDFLAGS) $< -o $@

clean:
	rm -rf $(OBJS) fork-accept

all: fork-accept

.PHONY: all
