#pragma once
#include <string>

// Imprime los comandos disponibles
void print_help();

// Lee y despacha comandos del usuario desde stdin en un bucle
// sockfd: socket conectado al servidor
// username: nombre de usuario registrado
// ip: IP local del cliente
void input_loop(int sockfd, const std::string &username, const std::string &ip);