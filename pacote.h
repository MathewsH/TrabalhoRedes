#ifndef PACOTE_H
#define PACOTE_H

// Enum para os tipos de pacotes
typedef enum {
    DADO,
    PESQUISA,
    ACK,
    NAK
} TipoPacote;

// Estrutura do Pacote
typedef struct {
    TipoPacote tipo; // Tipo do pacote (DADO, PESQUISA, ACK, NAK)
    char dados[1024];
    int erro; // 0 para sem erro (false), 1 para com erro (true)
} Pacote;

#endif // PACOTE_H