#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define PI 3.14159265358979323846
#define RAIO_TERRA 6371.0

// Cauã: o nó guarda só o que eu uso depois da leitura
typedef struct transacao {
    long long trans_datetime;
    double amt;
    struct transacao *prox;
    int category;
    char fraudeC;
} transacao;

// Cauã: a linha lida do CSV mora aqui só durante a leitura
typedef struct linhaCSV {
    long long trans_datetime;
    long long cc_num;
    int category;
    double amt;
    char gender[2];
    double lat;
    double longi;
    double merch_lat;
    double merch_long;
} linhaCSV;

#define TAM_TABELA 1031   // Cauã: primo perto da qtd de cartões (951)

typedef struct cartaoNode {
    long long cc_num;
    transacao *transacoesInicio;
    transacao *transacoesFim;   // Cauã
    int qtdTransacoes;
    double somaValores;
    struct cartaoNode *prox;
    struct cartaoNode *proxNoBalde;   // Cauã: colisão, igual a aula 9
} cartaoNode;

// Cauã: tabela hash (aula 9), acho o cartão direto em vez de andar a lista toda
typedef struct tabelaCartoes {
    cartaoNode *baldes[TAM_TABELA];
    cartaoNode *lista;
} tabelaCartoes;

typedef struct resumoCategoria {
    int categoria;
    int quantidade;
    double total;
} resumoCategoria;

typedef struct resumoMes {
    int ano;
    int mes;
    int quantidade;
    double soma;
    struct resumoMes *prox;
} resumoMes;

/* Converte graus para radianos */
double paraRadianos(double graus) {
    return graus * PI / 180.0;
}

/* Calcula a distância entre dois pontos */
double calcularHaversine(double lat1, double lon1,
                         double lat2, double lon2) {

    double dLat = paraRadianos(lat2 - lat1);
    double dLon = paraRadianos(lon2 - lon1);

    lat1 = paraRadianos(lat1);
    lat2 = paraRadianos(lat2);

    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1) * cos(lat2) *
               sin(dLon / 2) * sin(dLon / 2);

    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return RAIO_TERRA * c;
}

/* Separa timestamp em ano, mes e dia */
void obterData(long long timestamp, int *ano, int *mes, int *dia) {

    time_t tempo = (time_t)timestamp;
    struct tm *data = gmtime(&tempo);

    if (data != NULL) {
        *ano = data->tm_year + 1900;
        *mes = data->tm_mon + 1;
        *dia = data->tm_mday;
    } else {
        *ano = 0;
        *mes = 0;
        *dia = 0;
    }
}


/* Procura ou cria um cartao */
cartaoNode *buscarOuCriarCartao(tabelaCartoes *tabela, long long numero) {

    unsigned balde = (unsigned)((unsigned long long)numero % TAM_TABELA);
    cartaoNode *atual = tabela->baldes[balde];

    while (atual != NULL) {
        if (atual->cc_num == numero) {
            return atual;
        }

        atual = atual->proxNoBalde;
    }

    cartaoNode *novo = malloc(sizeof(cartaoNode));

    if (novo == NULL) {
        return NULL;
    }

    novo->cc_num = numero;
    novo->transacoesInicio = NULL;
    novo->transacoesFim = NULL;
    novo->qtdTransacoes = 0;
    novo->somaValores = 0;
    novo->prox = tabela->lista;
    tabela->lista = novo;
    novo->proxNoBalde = tabela->baldes[balde];
    tabela->baldes[balde] = novo;

    return novo;
}


/* Insere a transacao em ordem cronologica */
int inserirTransacao(tabelaCartoes *inicio, linhaCSV dados) {

    cartaoNode *cartao =
        buscarOuCriarCartao(inicio, dados.cc_num);

    if (cartao == NULL) {
        return 0;
    }

    transacao *novo = malloc(sizeof(transacao));

    if (novo == NULL) {
        return 0;
    }

    novo->trans_datetime = dados.trans_datetime;
    novo->amt = dados.amt;
    novo->category = dados.category;
    // Cauã: já calculo a fraude C aqui, assim não guardo as coordenadas
    novo->fraudeC = calcularHaversine(dados.lat, dados.longi,
                                      dados.merch_lat, dados.merch_long) > 78.0 ? 'Y' : 'N';
    novo->prox = NULL;

    transacao **posicao = &cartao->transacoesInicio;

    // Cauã
    if (cartao->transacoesFim != NULL &&
        cartao->transacoesFim->trans_datetime <= novo->trans_datetime) {
        posicao = &cartao->transacoesFim->prox;
    }

    while (*posicao != NULL &&
           (*posicao)->trans_datetime <= novo->trans_datetime) {

        posicao = &(*posicao)->prox;
    }

    novo->prox = *posicao;
    *posicao = novo;
    if (novo->prox == NULL) {
        cartao->transacoesFim = novo;
    }

    cartao->qtdTransacoes++;
    cartao->somaValores += novo->amt;

    return 1;
}


/* Carrega um arquivo CSV */
int carregarArquivos(tabelaCartoes *inicio, const char *nomeArquivo) {

    FILE *arquivo = fopen(nomeArquivo, "r");

    if (arquivo == NULL) {
        printf("Nao foi possivel abrir %s.\n", nomeArquivo);
        return 1;
    }

    char linha[1024];

    /* Ignora o cabecalho */
    if (fgets(linha, sizeof(linha), arquivo) == NULL) {
        fclose(arquivo);
        return 0;
    }

    int linhasLidas = 0;

    while (fgets(linha, sizeof(linha), arquivo) != NULL) {

        linhaCSV dados;
        char *quebra;

        quebra = strchr(linha, '\n');
        if (quebra != NULL) {
            *quebra = '\0';
        }

        int resultado = sscanf(
            linha,
            "%lld,%lld,%d,%lf,%1[^,],%lf,%lf,%lf,%lf",
            &dados.trans_datetime,
            &dados.cc_num,
            &dados.category,
            &dados.amt,
            dados.gender,
            &dados.lat,
            &dados.longi,
            &dados.merch_lat,
            &dados.merch_long
        );
        if (resultado != 9) {
            printf("Linha invalida em %s: %s\n",
                   nomeArquivo, linha);
            fclose(arquivo);
            return 1;
        }

        if (!inserirTransacao(inicio, dados)) {
            printf("Erro de memoria ao inserir transacao.\n");
            fclose(arquivo);
            return 1;
        }

        linhasLidas++;
    }
    fclose(arquivo);

    printf("%s carregado: %d transacoes.\n",
           nomeArquivo, linhasLidas);

    return 0;
}

/* Procura ou cria um resumo mensal */
resumoMes *buscarOuCriarMes(resumoMes **inicio,
                            int ano, int mes) {

    resumoMes *atual = *inicio;

    while (atual != NULL) {
        if (atual->ano == ano && atual->mes == mes) {
            return atual;
        }

        atual = atual->prox;
    }

    resumoMes *novo = malloc(sizeof(resumoMes));

    if (novo == NULL) {
        return NULL;
    }
    novo->ano = ano;
    novo->mes = mes;
    novo->quantidade = 0;
    novo->soma = 0;
    novo->prox = *inicio;

    *inicio = novo;
    return novo;
}

/* Libera o resumo mensal */
void liberarResumoMes(resumoMes *inicio) {

    while (inicio != NULL) {
        resumoMes *temp = inicio;
        inicio = inicio->prox;
        free(temp);
    }
}

/* Gera um CSV individual para cada cartao */
void gerarCSVs(cartaoNode *inicioGeral) {
    printf("Gerando CSVs...\n");
    cartaoNode *cartao = inicioGeral;

    while (cartao != NULL) {

        resumoMes *meses = NULL;
        transacao *atual = cartao->transacoesInicio;

        /* Calcula a soma e a quantidade de compras por mes */
        while (atual != NULL) {

            int ano, mes, dia;
            obterData(atual->trans_datetime, &ano, &mes, &dia);

            resumoMes *resumo =
                buscarOuCriarMes(&meses, ano, mes);

            if (resumo == NULL) {
                printf("Erro de memoria ao calcular medias.\n");
                liberarResumoMes(meses);
                return;
            }

            resumo->quantidade++;
            resumo->soma += atual->amt;

            atual = atual->prox;
        }
        char nomeArquivo[100];

        snprintf(nomeArquivo, sizeof(nomeArquivo),
                 "%lld.csv", cartao->cc_num);

        FILE *arquivo = fopen(nomeArquivo, "w");

        if (arquivo == NULL) {
            printf("Erro ao criar %s.\n", nomeArquivo);
            liberarResumoMes(meses);
            cartao = cartao->prox;
            continue;
        }
        fprintf(arquivo,
            "year,month,day,trans_datetime,category,amt,"
            "fraud_typeA,fraud_typeB,fraud_typeC\n");

        atual = cartao->transacoesInicio;
        // Cauã: guardo a anterior enquanto ando, em vez de voltar pro começo
        transacao *anterior = NULL;
        resumoMes *resumo = NULL;

        while (atual != NULL) {

            int ano, mes, dia;
            int anoAnt, mesAnt, diaAnt;
            int anoProx, mesProx, diaProx;
            obterData(atual->trans_datetime, &ano, &mes, &dia);

            char fraudeA = 'N';
            char fraudeB = 'N';
            char fraudeC = 'N';

            /* Fraude A: outra compra em menos de 2 minutos */
            transacao *proxima = atual->prox;

            if (anterior != NULL) {
                obterData(anterior->trans_datetime,
                          &anoAnt, &mesAnt, &diaAnt);

                if (anoAnt == ano && mesAnt == mes &&
                    atual->trans_datetime -
                    anterior->trans_datetime < 120) {

                    fraudeA = 'Y';
                }
            }
            if (proxima != NULL) {
                obterData(proxima->trans_datetime,
                          &anoProx, &mesProx, &diaProx);

                if (anoProx == ano && mesProx == mes &&
                    proxima->trans_datetime -
                    atual->trans_datetime < 120) {

                    fraudeA = 'Y';
                }
            }
            /* Fraude B: valor pelo menos 3 vezes a media das outras */
            if (resumo == NULL || resumo->ano != ano || resumo->mes != mes) {
                resumo = meses;
                while (resumo != NULL) {
                    if (resumo->ano == ano && resumo->mes == mes) {
                        break;
                    }
                    resumo = resumo->prox;
                }
            }
            if (resumo != NULL && resumo->quantidade > 1) {

                double mediaOutras =
                    (resumo->soma - atual->amt) /
                    (resumo->quantidade - 1);

                if (atual->amt >= 3.0 * mediaOutras) {
                    fraudeB = 'Y';
                }
            }
            /* Fraude C: distancia maior que 78 km */
            fraudeC = atual->fraudeC;
            fprintf(arquivo,
                "%d,%d,%d,%lld,%d,%.2f,%c,%c,%c\n",
                ano, mes, dia,
                atual->trans_datetime,
                atual->category,
                atual->amt,
                fraudeA,
                fraudeB,
                fraudeC
            );
            anterior = atual;
            atual = atual->prox;
        }
        fclose(arquivo);
        liberarResumoMes(meses);
        cartao = cartao->prox;
    }

    printf("\nTodos os arquivos individuais foram gerados!\n");
}

/* Mostra o perfil de compras de um cartao */
void gerarRelatorio(cartaoNode *inicioGeral,
                    long long numeroCartao) {

    cartaoNode *cartao = inicioGeral;

    while (cartao != NULL &&
           cartao->cc_num != numeroCartao) {

        cartao = cartao->prox;
    }

    if (cartao == NULL) {
        printf("\nCartao nao encontrado.\n");
        return;
    }

    resumoCategoria *resumo = NULL;
    int quantidadeCategorias = 0;

    transacao *atual = cartao->transacoesInicio;

    while (atual != NULL) {

        int posicao = -1;

        for (int i = 0; i < quantidadeCategorias; i++) {
            if (resumo[i].categoria == atual->category) {
                posicao = i;
                break;
            }
        }
        if (posicao == -1) {
            resumoCategoria *novo = realloc(
                resumo,
                (quantidadeCategorias + 1) *
                sizeof(resumoCategoria)
            );
            if (novo == NULL) {
                printf("Erro de memoria ao gerar relatorio.\n");
                free(resumo);
                return;
            }
            resumo = novo;
            posicao = quantidadeCategorias;

            resumo[posicao].categoria = atual->category;
            resumo[posicao].quantidade = 0;
            resumo[posicao].total = 0;

            quantidadeCategorias++;
        }

        resumo[posicao].quantidade++;
        resumo[posicao].total += atual->amt;
        atual = atual->prox;
    }

    const char *nomesCategorias[] = {
        " Entertainment",
        " Food Dining",
        " Gas Transport",
        " Grocery Net",
        " Grocery POS",
        " Health Fitness",
        " Home",
        " Kids Pets",
        " Misc Net",
        " Misc POS",
        " Personal Care",
        " Shopping Net",
        " Shopping POS",
        " Travel"
    };

    printf("\n===== PERFIL DE COMPRAS =====\n");
    printf("Cartao: %lld\n", numeroCartao);
    printf("--------------------------------------------------\n");
    printf("%-20s %-15s %-15s\n",
           " Categoria", "Num. de compras", "Valor total");

    for (int i = 0; i < quantidadeCategorias; i++) {
        int codigo = resumo[i].categoria;
        const char *nome = "Categoria desconhecida";

        if (codigo >= 1 && codigo <= 14) {
            nome = nomesCategorias[codigo - 1];
        }
        printf("%-20s %-15d R$ %.2f\n",
               nome,
               resumo[i].quantidade,
               resumo[i].total);
    }
    free(resumo);
}

/* Libera toda a memoria das listas */
void liberarMemoria(cartaoNode *inicioGeral) {
    while (inicioGeral != NULL) {
        cartaoNode *cartaoTemp = inicioGeral;
        transacao *atual = inicioGeral->transacoesInicio;
        while (atual != NULL) {
            transacao *transacaoTemp = atual;
            atual = atual->prox;
            free(transacaoTemp);
        }
        inicioGeral = inicioGeral->prox;
        free(cartaoTemp);
    }
}

void Consulta(cartaoNode *inicioGeral){
    long long numeroCartao = 1;

    /* Digitar 0 encerra a consulta */
    while (numeroCartao != 0){
        printf("\nDigite o numero do cartao para consultar: ");
        printf("Ou digite '0' para sair.\n");
        if (scanf("%lld", &numeroCartao) != 1){
            if (numeroCartao == 0){
                break;
            }
            printf("Numero de cartao invalido.\n");
            while (getchar() != '\n');
        } else {
            gerarRelatorio(inicioGeral, numeroCartao);
        }
    }
}

int main() {

    tabelaCartoes *tabela = calloc(1, sizeof(tabelaCartoes));

    if (tabela == NULL) {
        return 1;
    }

    /* Carregar os quatro arquivos */
    if (carregarArquivos(tabela, "1_jan2019.csv") != 0) {
        liberarMemoria(tabela->lista);
        free(tabela);
        return 1;
    }

    if (carregarArquivos(tabela, "2_fev_mar2019.csv") != 0) {
        liberarMemoria(tabela->lista);
        free(tabela);
        return 1;
    }

    if (carregarArquivos(tabela, "3_abr_jul2019.csv") != 0) {
        liberarMemoria(tabela->lista);
        free(tabela);
        return 1;
    }

    if (carregarArquivos(tabela, "4_ago2019_mai2020.csv") != 0) {
        liberarMemoria(tabela->lista);
        free(tabela);
        return 1;
    }

    printf("\nTodos os arquivos foram carregados!\n");

    /* Gerar os arquivos individuais dos cartoes */
    gerarCSVs(tabela->lista);

    /* Consultar o perfil de compras de um cartao */
    Consulta(tabela->lista);

    /* Liberar memoria */
    liberarMemoria(tabela->lista);
    free(tabela);

    return 0;
}
