all: fatsoa recuperar_pdf

fatsoa: fatsoa.o parser.o
	gcc -g -Wall -o fatsoa fatsoa.o parser.o

recuperar_pdf: recuperar_pdf.o
	gcc -g -Wall -o recuperar_pdf recuperar_pdf.o

parser.o: parser.c parser.h
	gcc -g -Wall -c -o parser.o parser.c

fatsoa.o: fatsoa.c fatsoa.h
	gcc -g -Wall -c -o fatsoa.o fatsoa.c

recuperar_pdf.o: recuperar_pdf.c fatsoa.h
	gcc -g -Wall -c -o recuperar_pdf.o recuperar_pdf.c

clean:
	rm -f fatsoa recuperar_pdf
	rm -f *.o


