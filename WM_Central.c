#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

int s; /* socket */

void finalizar(int senyal)
{
	printf("\n\rRecibida la senyal de fin (cntr-C)\n\r");
	close(s); /* cerrar para que accept termine con un error y salir del bucle principal */
}

int main(int argc, char *argv[])
{
	char *servidor_puerto;
	char mensaje[1024], respuesta[1024];
	char copia_mensaje[1024];
	struct sockaddr_in dir_servidor, dir_cliente;
	unsigned int long_dir_cliente;
	int s2;
	int n, enviados, recibidos;
	int proceso;
	int contador = 0;

	/* Comprobar los argumentos */
	if (argc != 2)
	{
		fprintf(stderr, "Error. Debe indicar el puerto del servidor\r\n");
		fprintf(stderr, "Sintaxis: %s <puerto>\n\r", argv[0]);
		fprintf(stderr, "Ejemplo : %s 8574\n\r", argv[0]);
		return 1;
	}

	/* Tomar los argumentos */		
	servidor_puerto = argv[1];

	/**** Paso 1: Abrir el socket ****/
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1)
	{
		fprintf(stderr, "Error. No se puede abrir el socket\n\r");
		return 1;
	}
	printf("Socket abierto en WM_Central\n\r");

	/**** Paso 2: Establecer la direccion de escucha ****/
	dir_servidor.sin_family = AF_INET;
	dir_servidor.sin_port = htons(atoi(servidor_puerto));
	dir_servidor.sin_addr.s_addr = INADDR_ANY;

	int opcion = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion));

	if (bind(s, (struct sockaddr *)&dir_servidor, sizeof(dir_servidor)) == -1)
	{
		fprintf(stderr, "Error. No se puede asociar el puerto al servidor\n\r");
		close(s);
		return 1;
	}
	printf("Puerto de escucha establecido (%s)\n\r", servidor_puerto);

	/**** Paso 3: Preparar el servidor para escuchar ****/
	if (listen(s, 10) == -1)
	{
		fprintf(stderr, "Error preparando servidor\n\r");
		close(s);
		return 1;
	}
	printf("WM_Central a la escucha de estaciones...\n\r");

	/**** Paso 4: Esperar conexiones ****/
	signal(SIGINT, finalizar);

	while (1)
	{
		long_dir_cliente = sizeof(dir_cliente);
		s2 = accept(s, (struct sockaddr *)&dir_cliente, &long_dir_cliente);
		contador++;

		if (s2 == -1)
		{
			break; /* Salir del bucle al recibir senyal */
		}

		/* Crear proceso hijo para atender a la estacion de forma concurrente */
		proceso = fork();
		if (proceso == -1) exit(1);

		if (proceso == 0) /* Proceso Hijo */
		{
			close(s); /* El hijo no necesita el descriptor de escucha general */

			/**** Paso 5: Leer la trama de la estacion ****/
			n = sizeof(mensaje);
			recibidos = read(s2, mensaje, n - 1);
			if (recibidos == -1)
			{
				fprintf(stderr, "Error leyendo el mensaje\n\r");
				close(s2);
				exit(1);
			}
			mensaje[recibidos] = '\0';
			printf("Trama recibida [%d bytes]: %s\n\r", recibidos, mensaje);

			/* Guardamos copia para parsear con strtok_r */
			strncpy(copia_mensaje, mensaje, sizeof(copia_mensaje));

			char *campos[10];
			int numCampos = 0;
			char *saveptr;

			char *token = strtok_r(copia_mensaje, "#", &saveptr);
			while (token != NULL && numCampos < 10)
			{
				campos[numCampos++] = token;
				token = strtok_r(NULL, "#", &saveptr);
			}

			bool valida = true;

			/* Validacion del protocolo del Anexo */
			if (numCampos == 3)
			{
				if (strcmp(campos[0], "REGISTRO") != 0)
					valida = false;
				if (strncmp(campos[1], "WS-", 3) != 0)
					valida = false;
				if (strlen(campos[2]) == 0)
					valida = false;
			}
			else
			{
				valida = false;
			}

			/**** Paso 6: Generar y enviar respuesta ****/
			if (valida)
			{
				printf("-> Estacion registrada correctamente:\n\r");
				printf("   ID        : %s\n\r", campos[1]);
				printf("   Ubicacion : %s\n\r", campos[2]);
				snprintf(respuesta, sizeof(respuesta), "STATUS#OK#Estacion registrada correctamente");
			}
			else
			{
				printf("-> Error en los datos de la estacion recibida\n\r");
				snprintf(respuesta, sizeof(respuesta), "STATUS#ERROR#Datos de estacion invalidos");
			}

			n = strlen(respuesta);
			enviados = write(s2, respuesta, n);
			if (enviados == -1 || enviados < n)
			{
				fprintf(stderr, "Error enviando la respuesta\n\r");
				close(s2);
				exit(1);
			}
			printf("Respuesta enviada a la estacion\n\r");

			close(s2);
			exit(0); /* Finaliza el hijo tras responder a la estacion */
		}
		else /* Proceso Padre */
		{
			close(s2); /* El padre no usa el socket de la estacion individual */
		}
	}

	close(s);
	printf("Socket cerrado. Servidor finalizado\n\r");
	return 0;
}