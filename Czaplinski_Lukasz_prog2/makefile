CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -pedantic -Werror
LFLAGS=-lpthread -lrt
EXC=1 2 3 4 5 6 7 8
TRASH=*.o $(EXC) .depend

all: .depend $(EXC)

clean : 
	-rm -f $(TRASH)

dist : clean
	cd ../ && rm -f Czapl*prog2.tgz && tar czf Czaplinski_Lukasz_prog2.tgz Czap*prog2


.PHONY : all clean $(EXC)

depend : .depend
	@echo "done"

.depend: cmd = gcc -MM -MF depend $(var).c && echo $(var)": aux"$(var) >> depend	&& cat depend >> .depend ;
.depend:
	 @echo "Generating dependencies..."
	 @-rm -f .depend
	 @$(foreach var, $(EXC), $(cmd))
	 @rm -f depend
		
-include .depend

aux%: %.o
	@echo "Generating "$*"..."
	$(CC) -o $* $< $(LFLAGS)
