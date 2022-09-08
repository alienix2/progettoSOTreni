creaProcessi: RBC registro treno launcher pulizia

RBC: RBC.o sharedFunctions.o
	gcc -o RBC RBC.o sharedLibraries.o

registro: registro.o
	gcc -o registro registro.o

treno: treno.o sharedFunctions.o
	gcc -o treno treno.o sharedFunctions.o

launcher: launcher.o
	gcc -o launcher launcher.o

registro.o: registro.c
	gcc -c registro.c

RBC.o: RBC.c
	gcc -c RBC.c

treno.o: treno.c
	gcc -c treno.c

launcher.o: launcher.c
	gcc -c launcher.c

sharedFunctions.o: ./utilities/sharedFunctions.c ./utilities/sharedFunctions.h
	gcc -c ./utilities/sharedFunctions.c

pulizia:
	rm sharedFunctions.o treno.o registro.o RBC.o launcher.o
