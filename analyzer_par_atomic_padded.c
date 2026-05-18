#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define HT_SIZE     131071
#define LINE_BUFFER 512

typedef struct CacheNodePadded
{
    char* url;
    long hit_count;
    struct CacheNodePadded* next;
    long padding[5];
} CacheNodePadded;

typedef struct
{
    size_t size;
    CacheNodePadded** table;
} HashTablePadded;

static size_t hash_djb2(const char* str, size_t size)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % size;
}

static CacheNodePadded* create_node(const char* url)
{
    CacheNodePadded* node = (CacheNodePadded*)malloc(sizeof(CacheNodePadded));
    if (!node)
    {
        printf("erro ao alocar node\n");
        exit(1);
    }
    node->url = (char*)malloc(strlen(url) + 1);
    strcpy(node->url, url);
    node->hit_count = 0;
    node->next = NULL;
    for (int i = 0; i < 5; i++) node->padding[i] = 0;
    return node;
}

static HashTablePadded* ht_create(size_t size)
{
    HashTablePadded* ht = (HashTablePadded*)malloc(sizeof(HashTablePadded));
    ht->table = (CacheNodePadded**)calloc(size, sizeof(CacheNodePadded*));
    ht->size = size;
    return ht;
}

static void ht_put(HashTablePadded* ht, const char* url)
{
    size_t idx = hash_djb2(url, ht->size);
    CacheNodePadded* cur = ht->table[idx];
    while (cur)
    {
        if (strcmp(cur->url, url) == 0) return;
        cur = cur->next;
    }
    CacheNodePadded* node = create_node(url);
    node->next = ht->table[idx];
    ht->table[idx] = node;
}

static CacheNodePadded* ht_get(HashTablePadded* ht, const char* url)
{
    size_t idx = hash_djb2(url, ht->size);
    CacheNodePadded* cur = ht->table[idx];
    while (cur)
    {
        if (strcmp(cur->url, url) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

static void ht_save_results(HashTablePadded* ht, const char* filename)
{
    FILE* fp = fopen(filename, "w");
    if (!fp) { printf("erro ao salvar\n"); return; }
    for (size_t i = 0; i < ht->size; i++)
    {
        CacheNodePadded* cur = ht->table[i];
        while (cur)
        {
            fprintf(fp, "%s,%ld\n", cur->url, cur->hit_count);
            cur = cur->next;
        }
    }
    fclose(fp);
}

static void ht_destroy(HashTablePadded* ht)
{
    for (size_t i = 0; i < ht->size; i++)
    {
        CacheNodePadded* cur = ht->table[i];
        while (cur)
        {
            CacheNodePadded* nx = cur->next;
            free(cur->url);
            free(cur);
            cur = nx;
        }
    }
    free(ht->table);
    free(ht);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Uso: %s <arquivo de log>\n", argv[0]);
        return 1;
    }

    printf("[Padded] sizeof(CacheNodePadded) = %zu bytes\n", sizeof(CacheNodePadded));
    printf("[Padded] Carregando manifest.txt\n");

    HashTablePadded* ht = ht_create(HT_SIZE);

    FILE* manifest = fopen("manifest.txt", "r");
    if (!manifest)
    {
        printf("erro ao abrir arquivo!\n");
        return 1;
    }

    char linha[LINE_BUFFER];
    while (fgets(linha, sizeof(linha), manifest))
    {
        linha[strcspn(linha, "\n")] = '\0';
        if (linha[0])
        {
            ht_put(ht, linha);
        }
    }
    fclose(manifest);
    printf("[Padded] manifest carregado\n");
    printf("[Padded] leitura log\n");

    FILE* fp = fopen(argv[1], "r");
    if (!fp)
    {
        printf("erro ao abrir log\n");
        return 1;
    }

    long line_count = 0;
    char tmp[LINE_BUFFER];
    while (fgets(tmp, sizeof(tmp), fp))
    {
        line_count++;
    }
    rewind(fp);

    char** linhas = (char**)malloc(line_count * sizeof(char*));
    for (long i = 0; i < line_count; i++)
    {
        linhas[i] = (char*)malloc(LINE_BUFFER);
        if (!fgets(linhas[i], LINE_BUFFER, fp)) linhas[i][0] = '\0';
    }
    fclose(fp);

    printf("[Padded] processando com %d threads\n", omp_get_max_threads());

    #pragma omp parallel for
    for (long i = 0; i < line_count; i++)
    {
        char* s = strchr(linhas[i], '"');
        if (!s) continue;
        s++;

        char* url = strchr(s, ' ');
        if (!url) continue;
        url++;

        char* fim = strchr(url, ' ');
        if (!fim) continue;
        *fim = '\0';

        CacheNodePadded* node = ht_get(ht, url);
        if (node)
        {
            #pragma omp atomic update
            node->hit_count++;
        }
    }

    for (long i = 0; i < line_count; i++)
    {
        free(linhas[i]);
    }
    free(linhas);

    printf("[Padded] salvando resultados\n");
    ht_save_results(ht, "results.csv");
    printf("[Padded] Concluido!\n");

    ht_destroy(ht);
    return 0;
}
