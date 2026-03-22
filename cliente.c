#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include "pacote.h"

void die(char *s) {
    perror(s);
    exit(1);
}

int main(int argc, char const *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <ip_servidor> <porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1]; // IP do servidor
    int port = atoi(argv[2]); // Porta do servidor
    int client_fd;
    struct sockaddr_in server_addr;
    Pacote pct_enviar, pct_resposta;

    srand(time(NULL));
    
    // Criação do socket
    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        die("socket");
    } 

    memset(&server_addr, 0, sizeof(server_addr)); // Limpa a estrutura de endereço do servidor
    server_addr.sin_family = AF_INET;  // Define a família de endereços como IPv4
    server_addr.sin_port = htons(port); // Define a porta do servidor

    // Converte o endereço IP do servidor de texto para binário
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0){
        die("inet_pton");
    }
    
    // Loop principal para enviar mensagens
    while (1) {
        char tipo_msg;
        printf("\nDigite o tipo de mensagem ('D' para Dados, 'P' para Pesquisa): ");
        scanf(" %c", &tipo_msg);

        int tipoCombustivel;
        double latitude, longitude;

        printf("Digite o tipo de combustível a pesquisar (0-Diesel, 1-Álcool, 2-Gasolina): ");
        scanf("%d", &tipoCombustivel);

        if (tipo_msg == 'D' || tipo_msg == 'd') {
            pct_enviar.tipo = DADO;
            int preco;

            printf("Digite o preço multiplicado por 1000 (ex: 4449): "); 
            scanf("%d", &preco);

            printf("Digite a latitude: "); 
            scanf("%lf", &latitude);
            
            printf("Digite a longitude: "); 
            scanf("%lf", &longitude);

            sprintf(pct_enviar.dados, "D %d %d %lf %lf", tipoCombustivel, preco, latitude, longitude);

        } else if (tipo_msg == 'P' || tipo_msg == 'p') {
            pct_enviar.tipo = PESQUISA;
            int raioDeBusca;

            printf("Digite o raio de busca em metros: ");
            scanf("%d", &raioDeBusca);

            printf("Digite a latitude do centro da busca: ");
            scanf("%lf", &latitude);

            printf("Digite a longitude do centro da busca: ");
            scanf("%lf", &longitude);

            sprintf(pct_enviar.dados, "P %d %d %lf %lf", tipoCombustivel, raioDeBusca, latitude, longitude);
        } else {
            printf("Tipo inválido. Tente novamente.\n");
            continue;
        }

        int ack_recebido = 0;
        while (!ack_recebido) {
            pct_enviar.erro = ((double)rand() / RAND_MAX > 0.5) ? 1 : 0;
            printf("\nEnviando requisição: \"%s\" (erro simulado: %s)...\n", pct_enviar.dados, pct_enviar.erro ? "Sim" : "Não");
            sendto(client_fd, &pct_enviar, sizeof(Pacote), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

            printf("Aguardando ACK/NAK da requisição...\n");

            if (recvfrom(client_fd, &pct_resposta, sizeof(Pacote), 0, NULL, NULL) < 0) {
                die("recvfrom");
            } else {
                 printf("  -> Pacote recebido do servidor. Tipo: %d\n", pct_resposta.tipo);
                 // Verifica se o pacote é ACK ou NAK
                if (pct_resposta.tipo == ACK) {
                    printf("  -> ACK da requisição recebido!\n");
                    ack_recebido = 1;
                } else if (pct_resposta.tipo == NAK) {
                    printf("  -> NAK recebido! Retransmitindo...\n");
                }
            }
        }
        
        if (pct_enviar.tipo == PESQUISA) {
            printf("\nAguardando resultado da pesquisa (espera infinita)...\n");

            if (recvfrom(client_fd, &pct_resposta, sizeof(Pacote), 0, NULL, NULL) < 0) {
                 die("recvfrom_resultado");
            } else {
                printf("  -> Resposta da pesquisa recebida: \"%s\"\n", pct_resposta.dados);
            }
        }
    }

    close(client_fd);
    return 0;
}