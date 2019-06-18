
all:
	gcc pace.c -lpthread -O4 -lm

clean:
	rm -f a.out

run: all
	./a.out 175

