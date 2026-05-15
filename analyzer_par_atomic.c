#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "hash_table.h"

#define HT_SIZE 131071
#define LINE_BUFFER 512

int main(int argc,  char*argv[])
{
    if (argc<2)
    {
        printf("Uso: %s <arquivo de log>\n", argv[0]);
        return 1;
    }

    //carrega o manifest na tabela hash
    printf("[Atomic] Carregando manifest.txt\n");

    HashTable* ht = ht_create(HT_SIZE);

    FILE* manifest = fopen("manifest.txt","r");
    if(!manifest)
    {
        printf("erro ao abrir arquivo!\n");
        return 1;
    }

    char linha[LINE_BUFFER];
    while(fgets(linha,sizeof(linha),manifest))
    {
        linha[strcspn(linha,"\n")]='\0';
        if (linha[0])
        {
            ht_put(ht,linha);
        }
    }
    fclose(manifest);
    printf("[Atomic] manifest carregado\n");
    printf("[Atomic] leitura log\n");

    FILE *fp = fopen(argv[1],"r");
    if (!fp)
    {
        printf("erro ao abrir log\n");
        return 1;
    }
    long line_count=0;
    char tmp[LINE_BUFFER];
    while(fgets(tmp,sizeof(tmp),fp))
    {
        line_count++;
    }
    rewind(fp);
    char** linhas=(char**)malloc(line_count*sizeof(char*));
    for (long i=0;i<line_count;i++)
    {
        linhas[i]=(char*)malloc(LINE_BUFFER);
        if (!fgets(linhas[i],LINE_BUFFER,fp)) linhas[i][0]='\0';

    }
    fclose(fp);

    printf("[Atomic] processando com %d threads",omp_get_max_threads());

    #pragma omp paralell for
    for (long i=0;i<line_count;i++)
    {
        char* s=strchr(linhas[i],'"');
        if (!s)
        {
            continue;
        }
        s++;
        char* url=strchr(s,' ');
        if (!url)
        {
            continue;
        }
        url++;

        char* fim=strchr(url,' ');
        if(!fim)
        {
            continue;
        }
        *fim='\0';

        CacheNode* node = ht_get(ht, url);
        {
            if (node)
            {
                #pragma omp atomic update
                node->hit_count++;
            }
        }
    }
    for (long i=0;i<line_count;i++)
    {
        free(linhas[i]);
    }
    free(linhas);
    
    printf("[Atomic] salvando resultados\n");
    ht_save_results(ht,"results.csv");
    printf("[Atomic] Concluído!\n");

    ht_destroy(ht);
    return 0;



}
