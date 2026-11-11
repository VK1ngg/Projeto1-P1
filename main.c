#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "pdfgen.h"   // Cauã

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
    transacao *transacoesFim;   // Cauã: guarda a última, pra não andar a lista toda
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

    // Cauã: se chegou depois da última, vai direto pro fim (sofri menos agora)
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


// Cauã: strtoll/strtod no lugar do sscanf, que é bem mais pesado
int lerLinha(char *p, linhaCSV *d) {
    char *fim;
    d->trans_datetime = strtoll(p, &fim, 10); if (fim == p || *fim != ',') return 0; p = fim + 1;
    d->cc_num = strtoll(p, &fim, 10);         if (fim == p || *fim != ',') return 0; p = fim + 1;
    d->category = (int)strtol(p, &fim, 10);   if (fim == p || *fim != ',') return 0; p = fim + 1;
    d->amt = strtod(p, &fim);                 if (fim == p || *fim != ',') return 0; p = fim + 1;
    if (*p == ',' || *p == '\0') return 0;
    d->gender[0] = *p; d->gender[1] = '\0';
    p = strchr(p, ',');                       if (p == NULL) return 0; p++;
    d->lat = strtod(p, &fim);                 if (fim == p || *fim != ',') return 0; p = fim + 1;
    d->longi = strtod(p, &fim);               if (fim == p || *fim != ',') return 0; p = fim + 1;
    d->merch_lat = strtod(p, &fim);           if (fim == p || *fim != ',') return 0; p = fim + 1;
    d->merch_long = strtod(p, &fim);          if (fim == p) return 0;
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

        if (!lerLinha(linha, &dados)) {
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
        int lido = scanf("%lld", &numeroCartao);
        if (lido == EOF) break;   // Cauã: entrada acabou, sem isso ficava em loop
        if (lido != 1){
            if (numeroCartao == 0){
                break;
            }
            printf("Numero de cartao invalido.\n");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        } else {
            gerarRelatorio(inicioGeral, numeroCartao);
        }
    }
}

#define ROXO     PDF_RGB(0x82, 0x0a, 0xd1)
#define PRETO    PDF_RGB(0x00, 0x00, 0x00)
#define BRANCO   PDF_RGB(0xff, 0xff, 0xff)
#define CINZA    PDF_RGB(0xf2, 0xf2, 0xf2)
#define VERMELHO PDF_RGB(0xcc, 0x00, 0x00)
#define LOGO     "assets/nu.jpg"
#define LINHAS_PAG1   32
#define LINHAS_OUTRAS 44

void textoDireita(struct pdf_doc *pdf, const char *texto, float tamanho, float x, float y, uint32_t cor) {
    pdf_add_text_wrap(pdf, NULL, texto, tamanho, x - 200, y, 0, cor, 200, PDF_ALIGN_RIGHT, NULL);
}

void formatarValor(double valor, char *texto, size_t tamanho) {
    snprintf(texto, tamanho, "%.2f", valor);
    char *ponto = strchr(texto, '.');
    if (ponto != NULL) *ponto = ',';
}

int ehDoMes(transacao *t, int mes, int ano) {
    int a, m, d;
    if (t == NULL) return 0;
    obterData(t->trans_datetime, &a, &m, &d);
    return a == ano && m == mes;
}

void verificarFraudes(transacao *anterior, transacao *atual, int mes, int ano,
                      int qtdMes, double somaMes, char *letras) {
    letras[0] = '\0';

    if ((ehDoMes(anterior, mes, ano) && atual->trans_datetime - anterior->trans_datetime < 120) ||
        (ehDoMes(atual->prox, mes, ano) && atual->prox->trans_datetime - atual->trans_datetime < 120)) {
        strcat(letras, "A ");
    }

    if (qtdMes > 1 && atual->amt >= 3.0 * (somaMes - atual->amt) / (qtdMes - 1)) {
        strcat(letras, "B ");
    }

    if (atual->fraudeC == 'Y') {
        strcat(letras, "C");
    }
}

void novaPagina(struct pdf_doc *pdf, int mes, int ano, float yTabela) {
    char texto[64];
    pdf_append_page(pdf);
    pdf_add_image_file(pdf, NULL, 40, 790, 54, 30, LOGO);
    pdf_set_font(pdf, "Helvetica-Bold");
    snprintf(texto, sizeof(texto), "Fatura do cartão de crédito - %02d/%d", mes, ano);
    pdf_add_text(pdf, NULL, texto, 14, 108, 800, PRETO);
    pdf_add_line(pdf, NULL, 40, 780, 555, 780, 1.5, ROXO);

    pdf_add_filled_rectangle(pdf, NULL, 40, yTabela - 5, 515, 16, 0, ROXO, PDF_TRANSPARENT);
    pdf_add_text(pdf, NULL, "Data", 9, 44, yTabela, BRANCO);
    pdf_add_text(pdf, NULL, "Hora", 9, 104, yTabela, BRANCO);
    pdf_add_text(pdf, NULL, "Lançamento", 9, 140, yTabela, BRANCO);
    pdf_add_text(pdf, NULL, "Suspeita", 9, 380, yTabela, BRANCO);
    textoDireita(pdf, "Valor (R$)", 9, 551, yTabela, BRANCO);
    pdf_set_font(pdf, "Helvetica");
    pdf_add_text(pdf, NULL, "A) outra compra a menos de 2 min    B) 3 vezes ou mais a média    "
                 "C loja a mais de 78 km", 7.5, 40, 30, PRETO);
}

int gerarFaturaPDF(cartaoNode *inicioGeral, long long numeroCartao, int mes, int ano) {
    const char *nomes[] = {"entertainment", "food_dining", "gas_transport", "grocery_net", "grocery_pos",
                           "health_fitness", "home", "kids_pets", "misc_net", "misc_pos",
                           "personal_care", "shopping_net", "shopping_pos", "travel"};
    cartaoNode *cartao = inicioGeral;
    while (cartao != NULL && cartao->cc_num != numeroCartao) {
        cartao = cartao->prox;
    }
    if (cartao == NULL) {
        printf("Cartao nao encontrado.\n");
        return 0;
    }

    transacao *primeira = NULL, *anterior = NULL, *t, *ant;
    int qtd = 0, suspeitas = 0;
    double total = 0;
    char letras[8], texto[96];
    for (t = cartao->transacoesInicio; t != NULL; t = t->prox) {
        if (ehDoMes(t, mes, ano)) {
            if (primeira == NULL) primeira = t;
            qtd++;
            total += t->amt;
        } else if (primeira == NULL) {
            anterior = t;
        } else {
            break;
        }
    }
    if (qtd == 0) {
        printf("Nenhuma transacao desse cartao em %02d/%d.\n", mes, ano);
        return 0;
    }

    ant = anterior;
    t = primeira;
    for (int i = 0; i < qtd; i++, ant = t, t = t->prox) {
        verificarFraudes(ant, t, mes, ano, qtd, total, letras);
        suspeitas += (letras[0] != '\0');
    }

    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, NULL);
    if (pdf == NULL) {
        printf("Sem memoria para o PDF.\n");
        return 0;
    }

    novaPagina(pdf, mes, ano, 600);
    snprintf(texto, sizeof(texto), "Cartão: %lld", numeroCartao);
    pdf_add_text(pdf, NULL, texto, 10, 40, 755, PRETO);
    snprintf(texto, sizeof(texto), "Vencimento: 10/%02d/%d", mes % 12 + 1, ano + (mes == 12));
    pdf_add_text(pdf, NULL, texto, 10, 40, 740, PRETO);
    snprintf(texto, sizeof(texto), "Lançamentos: %d    Com suspeita de fraude: %d", qtd, suspeitas);
    pdf_add_text(pdf, NULL, texto, 10, 40, 712, PRETO);
    formatarValor(total, texto, sizeof(texto));
    pdf_set_font(pdf, "Helvetica-Bold");
    pdf_add_text(pdf, NULL, "Total desta fatura: R$", 13, 40, 680, PRETO);
    pdf_add_text(pdf, NULL, texto, 13, 190, 680, PRETO);
    pdf_add_text(pdf, NULL, "Lançamentos", 11, 40, 625, PRETO);
    pdf_set_font(pdf, "Helvetica");

    float y = 600;
    int linha = 0, limite = LINHAS_PAG1;
    ant = anterior;
    t = primeira;
    for (int i = 0; i < qtd; i++, ant = t, t = t->prox) {
        if (linha == limite) {
            novaPagina(pdf, mes, ano, 760);
            y = 760;
            linha = 0;
            limite = LINHAS_OUTRAS;
        }
        y -= 15;
        verificarFraudes(ant, t, mes, ano, qtd, total, letras);
        time_t tempo = (time_t)t->trans_datetime;
        struct tm *d = gmtime(&tempo);
        uint32_t cor = letras[0] ? VERMELHO : PRETO;
        if (linha % 2) pdf_add_filled_rectangle(pdf, NULL, 40, y - 4, 515, 15, 0, CINZA, PDF_TRANSPARENT);

        snprintf(texto, sizeof(texto), "%02d/%02d/%d", d->tm_mday, d->tm_mon + 1, d->tm_year + 1900);
        pdf_add_text(pdf, NULL, texto, 9, 44, y, PRETO);
        snprintf(texto, sizeof(texto), "%02d:%02d", d->tm_hour, d->tm_min);
        pdf_add_text(pdf, NULL, texto, 9, 104, y, PRETO);
        pdf_add_text(pdf, NULL, (t->category >= 1 && t->category <= 14) ? nomes[t->category - 1] : "outros",
                     9, 140, y, PRETO);
        pdf_add_text(pdf, NULL, letras, 9, 380, y, cor);
        formatarValor(t->amt, texto, sizeof(texto));
        textoDireita(pdf, texto, 9, 551, y, cor);
        linha++;
    }

    formatarValor(total, texto, sizeof(texto));
    pdf_add_line(pdf, NULL, 40, y - 8, 555, y - 8, 1, PRETO);
    pdf_set_font(pdf, "Helvetica-Bold");
    pdf_add_text(pdf, NULL, "Total desta fatura", 9, 44, y - 22, PRETO);
    textoDireita(pdf, texto, 9, 551, y - 22, PRETO);

    char nome[80];
    snprintf(nome, sizeof(nome), "fatura_%lld_%d-%02d.pdf", numeroCartao, ano, mes);
    int ok = pdf_save(pdf, nome) == 0;
    pdf_destroy(pdf);
    if (ok) printf("Fatura gerada: %s (%d lancamentos).\n", nome, qtd);
    else printf("Nao foi possivel gravar %s.\n", nome);
    return ok;
}

void menuFatura(cartaoNode *inicioGeral) {
    char linha[64], sobra;
    long long numero;
    int mes, ano;

    while (1) {
        printf("\nFatura em PDF - numero do cartao (0 para voltar): ");
        if (fgets(linha, sizeof(linha), stdin) == NULL) return;
        if (linha[0] == '\n') continue;
        if (sscanf(linha, "%lld %c", &numero, &sobra) != 1 || numero < 0) {
            printf("Digite so os numeros do cartao.\n");
            continue;
        }
        if (numero == 0) return;
        printf("Mes/ano da fatura (ex.: 12/2019): ");
        if (fgets(linha, sizeof(linha), stdin) == NULL) return;
        if (sscanf(linha, "%d/%d %c", &mes, &ano, &sobra) != 2 || mes < 1 || mes > 12 ||
            ano < 1970 || ano > 2100) {
            printf("Mes/ano invalido. Use o formato MM/AAAA.\n");
            continue;
        }
        gerarFaturaPDF(inicioGeral, numero, mes, ano);
    }
}

int carregarBase(tabelaCartoes *tabela, const char *nome) {   // Cauã
    const char *pastas[] = {"", "bases/", "bases_teste/"};
    char caminho[256];

    for (int i = 0; i < 3; i++) {
        snprintf(caminho, sizeof(caminho), "%s%s", pastas[i], nome);
        FILE *teste = fopen(caminho, "r");
        if (teste != NULL) {
            fclose(teste);
            return carregarArquivos(tabela, caminho) == 0;
        }
    }
    printf("%s nao encontrado, seguindo sem ele.\n", nome);
    return 0;
}

int main(int argc, char *argv[]) {

    tabelaCartoes *tabela = calloc(1, sizeof(tabelaCartoes));

    if (tabela == NULL) {
        return 1;
    }

    const char *bases[] = {"1_jan2019.csv", "2_fev_mar2019.csv", "3_abr_jul2019.csv", "4_ago2019_mai2020.csv"};
    int carregadas = 0;

    if (argc > 1) {   // Cauã: ./golpe base1.csv base2.csv ...
        for (int i = 1; i < argc; i++) {
            carregadas += carregarArquivos(tabela, argv[i]) == 0;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            carregadas += carregarBase(tabela, bases[i]);
        }
    }

    if (carregadas == 0) {
        printf("Nenhuma base carregada. Coloque os CSVs na pasta do programa ou em bases/.\n");
        liberarMemoria(tabela->lista);
        free(tabela);
        return 1;
    }

    printf("\n%d base(s) carregada(s)!\n", carregadas);

    /* Gerar os arquivos individuais dos cartoes */
    gerarCSVs(tabela->lista);

    /* Consultar o perfil de compras de um cartao */
    Consulta(tabela->lista);
    menuFatura(tabela->lista);   // Cauã

    /* Liberar memoria */
    liberarMemoria(tabela->lista);
    free(tabela);

    return 0;
}
