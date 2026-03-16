#pragma once

// Inicia el hilo receptor en segundo plano
// sockfd: socket conectado al servidor
// El hilo corre hasta que se cierra la conexion e imprime mensajes recibidos
void start_receiver(int sockfd);