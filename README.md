# Análise de Dados de Sensores IoT com Pthreads

Este projeto tem como objetivo processar uma base de dados contendo registros de dispositivos IoT com diversos sensores ambientais, utilizando programação paralela com `pthreads` para distribuir a carga de trabalho entre os processadores disponíveis.

## 🔧 Compilação

Para compilar o programa, execute:

```bash
gcc -o analise_iot Analise_IoT.c -lpthread
```

## ▶️ Execução

```bash
./analise_iot caminho/para/arquivo.csv
```

> O programa espera um arquivo `.csv` com registros de sensores no formato descrito abaixo.

## 📄 Formato do CSV de entrada

O arquivo CSV deve conter os seguintes campos:

```
id;device;contagem;data;temperatura;umidade;luminosidade;ruido;eco2;etvoc;latitude;longitude
```

A análise considera apenas os campos:

- `device`
- `data`
- `temperatura`
- `umidade`
- `luminosidade`
- `ruido`
- `eco2`
- `etvoc`

E ignora registros com data anterior a **março de 2024**.

> ⚠️ Atenção: o delimitador esperado atualmente no código é `|`, mas pode ser ajustado para `;` conforme a base real.

## 🧠 Lógica de Processamento

### 🔄 Leitura do CSV

- O programa lê todas as linhas do arquivo e armazena os registros válidos (com data >= março/2024) em um vetor de estruturas `Record`.

### 🔀 Distribuição de trabalho entre threads

- O número de threads é definido automaticamente com base no número de processadores (`get_nprocs()`).
- Cada thread recebe uma fatia do vetor de registros para processar.

### 📊 Processamento por thread

- Cada thread analisa os registros do seu intervalo e agrega estatísticas mensais por dispositivo e sensor.
- Para cada par `dispositivo + mês`, são calculados:
  - Valor **mínimo**
  - Valor **máximo**
  - **Média** dos valores válidos

- O acesso à estrutura de estatísticas compartilhada entre as threads é protegido por `pthread_mutex_t` para evitar condições de corrida.

### 📁 Geração do CSV de saída

Ao final do processamento, é gerado um arquivo chamado `resultados.csv` com a seguinte estrutura:

```
device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo
```

Exemplo:

```
sensorA;2024-04;temperatura;29.5;25.3;20.1
```

## 🧵 Modo de Execução das Threads

As threads são executadas em **modo usuário**, utilizando a API `pthreads` da biblioteca padrão (`libpthread`).

## ⚠️ Concorrência

- O uso de mutex evita problemas de acesso simultâneo à estrutura de agregação de estatísticas;
- Como potencial melhoria, seria possível particionar previamente os dados por dispositivo para reduzir o uso de mutex e aumentar a escalabilidade.

## 🧪 Exemplo de execução

```bash
./analise_iot devices.csv
Total de registros válidos: 4175008
Usando 2 threads
Processamento concluído. Resultados salvos em resultados.csv
```

---
