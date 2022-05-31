
o=o

NAME:=libcompat-to-msvc


%.$o: %.c
	gcc -Wall -Wextra -O3 -c $< -o $@

%.$o: %.S
	gcc -c $< -o $@

OBJS=dll_math.$o io_math.$o dll_dependency.$o vsnprintf.$o

.INTERMEDIATE: $(OBJS)

$(NAME).a: $(OBJS)
	ar rc $@ $^
	ranlib $@

clean:
	-rm -f *.a *.o
