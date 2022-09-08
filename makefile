creaProcessi: RBC registro treno launcher pulizia

RBC: RBC.o sharedLibraries.o
	gcc -o RBC RBC.o sharedLibraries.o

registro: registro.o
	gcc -o registro registro.o

treno: treno.o sharedLibraries.o
	gcc -o treno treno.o sharedLibraries.o

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

sharedLibraries.o: ./utilities/sharedLibraries.c ./utilities/sharedLibraries.h
	gcc -c ./utilities/sharedLibraries.c

pulizia:
	rm sharedLibraries.o treno.o registro.o RBC.o launcher.o
