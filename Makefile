TARGETS = ringmaster player

.PHONY: all clean

all: $(TARGETS)

ringmaster: ringmaster.cpp potato.h
	g++ -g -o $@ $<

player: player.cpp potato.h
	g++ -g -o $@ $<

clean:
	rm -f $(TARGETS) *.o *.out *~
