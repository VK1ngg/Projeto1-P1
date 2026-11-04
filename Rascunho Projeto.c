#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define PI 3.14159265358979323846
#define RAIO_TERRA 6371.0 // em km

typedef struct transacao{
 int trans_datetime;
 long long cc_num;
 int category;
 float amt;
 char gender[2];
 float lat;
 float longi;
 float merch_lat;
 float merch_long;
 struct transacao *prox;
} transacao;

typedef struct CartaoNode{
long long cc_num;
transacao *transacoesInicio;
int qtdTransacoes;
float somaValores;
struct CartaoNode *prox;
}cartaoNode;

double paraRadianos(double grau) {
    return grau * (PI / 180.0);
}

// F¨®rmula de Haversine (Regra C)
double calcularHaversine(float lat1, float long1, float lat2, float long2) {
    double dLat = paraRadianos(lat2 - lat1);
    double dLong = paraRadianos(long2 - long1);
    double radLat1 = paraRadianos(lat1);
    double radLat2 = paraRadianos(lat2);

    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(radLat1) * cos(radLat2) *
               sin(dLong / 2) * sin(dLong / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return RAIO_TERRA * c;
}

// Converter Epoch Time para Ano, M¨ºs e Dia
void obterData(int epoch, int *ano, int *mes, int *dia) {
    time_t rawtime = (time_t)epoch;
    struct tm *info = gmtime(&rawtime);
    *ano = info->tm_year + 1900;
    *mes = info->tm_mon + 1;
    *dia = info->tm_mday;
}

void inserirTransacaoOrganizada(cartaoNode **inicioGeral, int trans_datetimeLida, int cc_numLida, int category_lida, float amt_lida, char genderLido[2], float latLida, float longLida, float merch_latLida, float merch_longLida){
 cartaoNode *cartaoAtual=*inicioGeral;

 while (cartaoAtual!=NULL && cartaoAtual->cc_num!= cc_numLida){
        cartaoAtual=cartaoAtual->prox;
 }

  if (cartaoAtual==NULL){
     cartaoNode *novoCartao=malloc(sizeof(cartaoNode));
     if (novoCartao==NULL){
        printf("Erro ao alocar memoria para o cartao./n");
        return;
     }

     novoCartao->cc_num=cc_numLida;
     novoCartao->transacoesInicio=NULL;
     novoCartao->qtdTransacoes=0;
     novoCartao->somaValores=0.0;
     novoCartao->prox=*inicioGeral;
     *inicioGeral=novoCartao;
     cartaoAtual=novoCartao;

  }

  transacao *novaTransacao=malloc(sizeof(transacao));
  if (novaTransacao==NULL){
    printf("Erro ao alocar memoria para a transacao./n");
    return;
  }
 novaTransacao->trans_datetime=trans_datetimeLida;
 novaTransacao->cc_num=cc_numLida;
 novaTransacao->category=category_lida;
 novaTransacao->amt=amt_lida;
 strcpy(novaTransacao->gender, genderLido);
 novaTransacao->lat=latLida;
 novaTransacao->longi=longLida;
 novaTransacao->merch_lat=merch_latLida;
 novaTransacao->merch_long=merch_longLida;
 novaTransacao->prox=NULL;

 if (cartaoAtual->transacoesInicio==NULL){
    cartaoAtual->transacoesInicio=novaTransacao;
 }

 else{
    transacao *aux=cartaoAtual->transacoesInicio;
    while (aux->prox!=NULL){
        aux=aux->prox;
    }
    aux->prox=novaTransacao;
 }
  cartaoAtual->qtdTransacoes++;
  cartaoAtual->somaValores+=amt_lida;
 }

 void gerarCSV(cartaoNode *inicioGeral){

 if (inicioGeral==NULL){
    printf("Arquivos nn foram lidos anteriormente.\n");
    return;
 }

 cartaoNode *cartaoAtual=inicioGeral;


 FILE *arquivoSaida=fopen("Saida.csv", "w");

 if (arquivoSaida==NULL){
    printf("Erro ao abrir o arquivo para escrita.\n");
    return;
 }



 while (cartaoAtual!=NULL){



 }


int carregarArquivos (cartaoNode **inicioGeral, const char *nomeArquivo){

 FILE *PontArquivo=fopen(nomeArquivo, "r");

 if (PontArquivo==NULL){
  printf("Erro ao abrir o arquivo para leitura.\n");
  return -1;
  }

  int trans_datetimeLida, cc_numLida, categoryLida;
  float amt_lido, latLida, longiLida, merch_latLida, merch_longLida ;
  char genderLido[2];

  int total;

  int linhas_lidas;

  while (fscanf(PontArquivo, "%d;%lld;%d;%f;%1s;%f;%f;%f;%f", &trans_datetimeLida, &cc_numLida, &categoryLida, &amt_lido, genderLido, &latLida, &longiLida, &merch_latLida, &merch_longLida)==9){

  linhas_lidas++;
  inserirTransacaoOrganizada(inicioGeral, trans_datetimeLida, cc_numLida, categoryLida,
                       amt_lido, genderLido, latLida, longiLida,
                       merch_latLida, merch_longLida);
  }
  fclose(PontArquivo);
  printf("%d\n", linhas_lidas);
 return 0;
 }





















int main() {

    cartaoNode *inicioGeral = NULL;

    const char *nomeArquivo = "transacoes.csv";

    int resultado=carregarArquivos(&inicioGeral, nomeArquivo);

    if (resultado==0){
        printf("Arquivo carregado e organizado com sucesso.\n");
    }
    else{
        printf("Falha ao carregar o arquivo.\n");
    }



 return 0;
}
