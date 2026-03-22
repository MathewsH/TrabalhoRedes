#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <pthread.h>
#include "pacote.h"

#define DADOS_FILENAME "dados_postos.csv"
#define EARTH_RADIUS_METERS 6371000.0
#define M_PI 3.14159265358979323846

pthread_mutex_t file_mutex;

// Estrutura para passar argumentos para a thread
typedef struct {
    Pacote pacote;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int server_fd;
} ThreadArgs;


void die(char *s) {
    perror(s);
    exit(1);
}

// Função para converter o código do combustível em string
const char* combustiveis(int codCombustivel) {
    switch (codCombustivel) {
        case 0: return "Diesel";
        case 1: return "Álcool";
        case 2: return "Gasolina";
        default: return "Desconhecido";
    }
}

// Função para converter graus em radianos
double radianos(double degrees) {
    return degrees * M_PI / 180.0;
}

// Função para calcular a distância usando a fórmula de Haversine
double haversine(double lat1, double lon1, double lat2, double lon2) {

    double _lat1 = lat1 * M_PI / 180.0;
    double _lon1 = lon1 * M_PI / 180.0;
    double _lat2 = lat2 * M_PI / 180.0;
    double _lon2 = lon2 * M_PI / 180.0;

    double d_lon = _lon2 - _lon1;
    double d_lat = _lat2 - _lat1;
    // Fórmula de Haversine
    double a = sin(d_lat / 2) * sin(d_lat / 2) + cos(_lat1) * cos(_lat2) * sin(d_lon / 2) * sin(d_lon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return EARTH_RADIUS_METERS * c;
}

// Função que será executada por cada thread para lidar com uma requisição
void *request(void *args) {
    // Recebe os argumentos passados para a thread
    ThreadArgs *thread_args = (ThreadArgs *)args;
    Pacote pct_recebido = thread_args->pacote;
    struct sockaddr_in client_addr = thread_args->client_addr;
    socklen_t client_len = thread_args->client_len;
    int server_fd = thread_args->server_fd;

    printf("\nPacote recebido de %s:%d. Conteúdo: \"%s\"\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), pct_recebido.dados);

    // Verifica se o pacote recebido tem erro simulado
    if (pct_recebido.erro == 1) {
        printf("  -> Pacote com erro simulado. Enviando NAK...\n");
        Pacote resposta_nak = {.tipo = NAK};
        sendto(server_fd, &resposta_nak, sizeof(Pacote), 0, (struct sockaddr *)&client_addr, client_len);
    } else {
        printf("  -> Pacote sem erro. Enviando ACK da requisição...\n");
        Pacote resposta_ack = {.tipo = ACK};
        sendto(server_fd, &resposta_ack, sizeof(Pacote), 0, (struct sockaddr *)&client_addr, client_len);

        // Processa o pacote recebido
        if (pct_recebido.dados[0] == 'D') {
            int tipoCombustivel, preco;
            double latitude, longitude;
            if (sscanf(pct_recebido.dados, "D %d %d %lf %lf", &tipoCombustivel, &preco, &latitude, &longitude) == 4) {
                pthread_mutex_lock(&file_mutex);

                FILE *dataFile = fopen(DADOS_FILENAME, "a");

                if (dataFile) {
                    fprintf(dataFile, "%d,%d,%lf,%lf\n", tipoCombustivel, preco, latitude, longitude);
                    fclose(dataFile);
                    printf("     -> Dados do posto salvos em '%s'\n", DADOS_FILENAME);
                }
                // Libera o mutex após o acesso ao arquivo
                pthread_mutex_unlock(&file_mutex);
            }
        } else if (pct_recebido.dados[0] == 'P') {
            printf("  -> Processando mensagem de PESQUISA...\n");
            int tipoCombustivel, raioDeBusca;
            double pLatitude, pLongitude;

            if (sscanf(pct_recebido.dados, "P %d %d %lf %lf", &tipoCombustivel, &raioDeBusca, &pLatitude, &pLongitude) == 4) {
                int melhor_preco = -1;
                char line[256];

                pthread_mutex_lock(&file_mutex);
                FILE *dataFile = fopen(DADOS_FILENAME, "r");
                if (dataFile == NULL) {
                    printf("     -> Arquivo de dados não encontrado.\n");
                } else {
                    // Lê o arquivo linha por linha, comparando os dados
                    while (fgets(line, sizeof(line), dataFile)) {
                        int  _tipoCombustivel, _preco;
                        double _lat, _lon; 
                        // Lê os dados do arquivo e verifica se o tipo do combustível
                        if (sscanf(line, "%d,%d,%lf,%lf", & _tipoCombustivel, &_preco, &_lat, &_lon) == 4) {

                            if (_tipoCombustivel == tipoCombustivel) {
                                double distance = haversine(pLatitude, pLongitude, _lat, _lon);
                                // Verifica se a distância está dentro do raio de busca
                                if (distance <= raioDeBusca) {
                                    if (melhor_preco == -1 || _preco < melhor_preco) {
                                        melhor_preco = _preco;
                                    }
                                }
                            }
                        }
                    }
                    fclose(dataFile);
                }
                pthread_mutex_unlock(&file_mutex);
                
                Pacote pct_resultado;
                pct_resultado.tipo = DADO;
                if (melhor_preco != -1) {
                    sprintf(pct_resultado.dados, "Menor preço para %s: R$ %.3f", combustiveis(tipoCombustivel), (double)melhor_preco / 1000.0);
                } else {
                    sprintf(pct_resultado.dados, "Nenhum posto encontrado para %s no raio de %d metros.", combustiveis(tipoCombustivel), raioDeBusca);
                }
                
                printf("     -> Enviando resultado da pesquisa para o cliente: \"%s\"\n", pct_resultado.dados);
                sendto(server_fd, &pct_resultado, sizeof(Pacote), 0, (struct sockaddr *)&client_addr, client_len);
            }
        }
    }
    free(thread_args); // Libera a memória alocada para os argumentos
    return NULL;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd;
    struct sockaddr_in server_addr;

    // Inicializa o mutex
    pthread_mutex_init(&file_mutex, NULL);

    // Criação do socket
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        die("socket");
    }
    
    // Configuração do endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Associa o socket ao endereço e porta
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        die("bind");
    }

    printf("Servidor multithread escutando na porta %d...\n", port);
    printf("Os dados dos postos serão salvos/lidos de '%s'\n", DADOS_FILENAME);

    while (1) {
        // Aloca memória para os argumentos da thread
        ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
        if (!args) {
            perror("malloc");
            continue;
        }

        args->client_len = sizeof(args->client_addr);

        // Aguarda recebimento de um pacote
        if (recvfrom(server_fd, &(args->pacote), sizeof(Pacote), 0, (struct sockaddr *)&(args->client_addr), &(args->client_len)) < 0) {
            perror("recvfrom");
            free(args);
            continue;
        }

        args->server_fd = server_fd;

        // Cria uma nova thread para lidar com a requisição
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, request, (void *)args) != 0) {
            perror("pthread_create");
            free(args);
        }

        // Desassocia a thread para que seus recursos sejam liberados automaticamente na sua finalização
        pthread_detach(thread_id);
    }
    
    // Destrói o mutex e fecha o socket ao finalizar o servidor (neste caso, nunca alcançado)
    pthread_mutex_destroy(&file_mutex);
    close(server_fd);
    return 0;
}