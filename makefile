make: 
	gcc -o cliente cliente.c -lm -Wextra -Wall
	gcc -o servidor servidor.c -lm -Wextra -Wall

clean:
	rm  cliente servidor