#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <ctype.h>


#define MAX_LINE_LENGTH 1024
#define MAX_DEVICES 100
#define MAX_MONTHS 120 // 10 anos * 12 meses
#define MAX_SENSORS 6

// Estrutura para armazenar dados de um registro
typedef struct {
    char device[50];
    time_t date;
    float temperatura;
    float umidade;
    float luminosidade;
    float ruido;
    float eco2;
    float etvoc;
} Registro;

// Estrutura para armazenar estatísticas de um sensor em um mês
typedef struct {
    float min;
    float max;
    double sum;
    int count;
} EstatisticasSensor;

// Estrutura para armazenar estatísticas de um dispositivo em um mês
typedef struct {
    char device[50];
    int year;
    int month;
    EstatisticasSensor sensors[MAX_SENSORS]; // 0:temp, 1:umid, 2:lum, 3:ruido, 4:eco2, 5:etvoc
} EstatisticasMensaisDispositivo;

// Estrutura para passar dados para as threads
typedef struct {
    Registro *records;
    int start;
    int end;
    EstatisticasMensaisDispositivo *stats;
    int *stats_count;
    pthread_mutex_t *mutex;
} DadosThread;

// Converte uma string de data para um formato de tempo (time_t)
time_t converter_data(const char *date_str) {
    struct tm tm = {0};
    if (strptime(date_str, "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
        return -1;
    }
    return mktime(&tm);
}

// Extrai o ano e o mês de uma data no formato time_t
void obter_ano_mes(time_t date, int *year, int *month) {
    struct tm *tm = localtime(&date);
    *year = tm->tm_year + 1900;
    *month = tm->tm_mon + 1;
}

// Processa uma linha do CSV e preenche a estrutura de Registro
int processar_linha(char *line, Registro *record) {
    char *token;
    int field = 0;
    
    // Ignorar linhas vazias ou sem dados suficientes
    if (strlen(line) < 10) return 0;
    
    token = strtok(line, "|");
    while (token != NULL && field < 12) {
        switch (field) {
            case 1: // device
                if (strlen(token) > 0) {
                    strncpy(record->device, token, sizeof(record->device) - 1);
                } else {
                    return 0;
                }
                break;
            case 3: // data
                if (strlen(token) > 0) {
                    record->date = converter_data(token);
                    if (record->date == -1) return 0;
                } else {
                    return 0;
                }
                break;
            case 4: // temperatura
                if (strlen(token) > 0) record->temperatura = atof(token);
                break;
            case 5: // umidade
                if (strlen(token) > 0) record->umidade = atof(token);
                break;
            case 6: // luminosidade
                if (strlen(token) > 0) record->luminosidade = atof(token);
                break;
            case 7: // ruido
                if (strlen(token) > 0) record->ruido = atof(token);
                break;
            case 8: // eco2
                if (strlen(token) > 0) record->eco2 = atof(token);
                break;
            case 9: // etvoc
                if (strlen(token) > 0) record->etvoc = atof(token);
                break;
        }
        token = strtok(NULL, "|");
        field++;
    }
    
    // Considerar apenas registros a partir de março de 2024
    struct tm *tm = localtime(&record->date);
    if (tm->tm_year + 1900 < 2024 || (tm->tm_year + 1900 == 2024 && tm->tm_mon + 1 < 3)) {
        return 0;
    }
    
    return 1;
}

// Função executada por cada thread para calcular as estatísticas
void *processar_registros(void *arg) {
    DadosThread *data = (DadosThread *)arg;
    
    for (int i = data->start; i < data->end; i++) {
        Registro *record = &data->records[i];
        
        int year, month;
        obter_ano_mes(record->date, &year, &month);
        
        // Verificar se já temos estatísticas para este dispositivo e mês
        int found = 0;
        EstatisticasMensaisDispositivo *stat = NULL;
        
        pthread_mutex_lock(data->mutex);
        
        for (int j = 0; j < *data->stats_count; j++) {
            if (strcmp(data->stats[j].device, record->device) == 0 &&
                data->stats[j].year == year &&
                data->stats[j].month == month) {
                stat = &data->stats[j];
                found = 1;
                break;
            }
        }
        
        if (!found) {
            if (*data->stats_count >= MAX_DEVICES * MAX_MONTHS) {
                pthread_mutex_unlock(data->mutex);
                continue;
            }
            
            stat = &data->stats[*data->stats_count];
            strncpy(stat->device, record->device, sizeof(stat->device) - 1);
            stat->year = year;
            stat->month = month;
            
            // Inicializar estatísticas dos sensores
            for (int s = 0; s < MAX_SENSORS; s++) {
                stat->sensors[s].min = 0;
                stat->sensors[s].max = 0;
                stat->sensors[s].sum = 0;
                stat->sensors[s].count = 0;
            }
            
            (*data->stats_count)++;
        }
        
        pthread_mutex_unlock(data->mutex);
        
        // Atualizar estatísticas para cada sensor
        float sensor_values[MAX_SENSORS] = {
            record->temperatura,
            record->umidade,
            record->luminosidade,
            record->ruido,
            record->eco2,
            record->etvoc
        };
        
        for (int s = 0; s < MAX_SENSORS; s++) {
            if (sensor_values[s] == 0) continue; // Ignorar valores zero
            
            pthread_mutex_lock(data->mutex);
            
            if (stat->sensors[s].count == 0) {
                stat->sensors[s].min = sensor_values[s];
                stat->sensors[s].max = sensor_values[s];
                stat->sensors[s].sum = sensor_values[s];
                stat->sensors[s].count = 1;
            } else {
                if (sensor_values[s] < stat->sensors[s].min) {
                    stat->sensors[s].min = sensor_values[s];
                }
                if (sensor_values[s] > stat->sensors[s].max) {
                    stat->sensors[s].max = sensor_values[s];
                }
                stat->sensors[s].sum += sensor_values[s];
                stat->sensors[s].count++;
            }
            
            pthread_mutex_unlock(data->mutex);
        }
    }
    
    return NULL;
}

// Escreve as estatísticas finais no arquivo resultados2.csv
void escrever_resultados(EstatisticasMensaisDispositivo *stats, int stats_count) {
    FILE *file = fopen("resultados.csv", "w");
    if (!file) {
        perror("Erro ao abrir arquivo de resultados");
        return;
    }
    
    // Cabeçalho
    fprintf(file, "device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo\n");
    
    const char *sensor_names[MAX_SENSORS] = {
        "temperatura", "umidade", "luminosidade", "ruido", "eco2", "etvoc"
    };
    
    for (int i = 0; i < stats_count; i++) {
        EstatisticasMensaisDispositivo *stat = &stats[i];
        
        for (int s = 0; s < MAX_SENSORS; s++) {
            if (stat->sensors[s].count > 0) {
                float avg = (float)(stat->sensors[s].sum / stat->sensors[s].count);
                fprintf(file, "%s;%04d-%02d;%s;%.2f;%.2f;%.2f\n",
                       stat->device,
                       stat->year, stat->month,
                       sensor_names[s],
                       stat->sensors[s].max,
                       avg,
                       stat->sensors[s].min);
            }
        }
    }
    
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <arquivo.csv>\n", argv[0]);
        return 1;
    }
    
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Erro ao abrir arquivo");
        return 1;
    }
    
    // Conta quantas linhas o arquivo tem para alocar memória para os registros
    int line_count = 0;
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) line_count++;
    rewind(file);
    
    Registro *records = malloc(line_count * sizeof(Registro));
    if (!records) {
        perror("Erro ao alocar memória");
        fclose(file);
        return 1;
    }
    
    // Lê e processa cada linha do CSV
    int valid_records = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        // Remover nova linha
        buffer[strcspn(buffer, "\n")] = 0;
        
        // Fazer uma cópia para o strtok não modificar o buffer original
        char line_copy[MAX_LINE_LENGTH];
        strncpy(line_copy, buffer, sizeof(line_copy));
        
        if (processar_linha(line_copy, &records[valid_records])) {
            valid_records++;
        }
    }
    fclose(file);
    
    printf("Total de registros válidos: %d\n", valid_records);
    
    // Define automaticamente o número de threads conforme os núcleos do processador
    int num_threads = get_nprocs();
    printf("Usando %d threads\n", num_threads);
    
    // Alocar memória para estatísticas
    EstatisticasMensaisDispositivo *stats = malloc(MAX_DEVICES * MAX_MONTHS * sizeof(EstatisticasMensaisDispositivo));
    if (!stats) {
        perror("Erro ao alocar memória para estatísticas");
        free(records);
        return 1;
    }
    int stats_count = 0;
    
    // Mutex para evitar condições de corrida ao acessar a estrutura compartilhada
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    
    // Cria as threads e distribui os registros para elas
    pthread_t threads[num_threads];
    DadosThread thread_data[num_threads];
    
    int records_per_thread = valid_records / num_threads;
    int remaining_records = valid_records % num_threads;
    int start = 0;
    
    for (int i = 0; i < num_threads; i++) {
        int end = start + records_per_thread + (i < remaining_records ? 1 : 0);
        
        thread_data[i].records = records;
        thread_data[i].start = start;
        thread_data[i].end = end;
        thread_data[i].stats = stats;
        thread_data[i].stats_count = &stats_count;
        thread_data[i].mutex = &mutex;
        
        pthread_create(&threads[i], NULL, processar_registros, &thread_data[i]);
        
        start = end;
    }
    
    // Aguarda todas as threads finalizarem o processamento
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Gera o arquivo de saída com os resultados
    escrever_resultados(stats, stats_count);
    
    // Libera os recursos alocados após o processamento
    pthread_mutex_destroy(&mutex);
    free(records);
    free(stats);
    
    printf("Processamento concluído. Resultados salvos em resultados.csv\n");
    
    return 0;
}