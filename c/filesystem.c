#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "filesystem.h"

/* Estrutura da FAT */
uint16_t fat[BLOCKS];
/* Bloco de dados */
uint8_t data_block[BLOCK_SIZE];
/* Bloco de diretório */
struct dir_entry_s dir_block[DIR_ENTRIES];

/* Funções de leitura e escrita de blocos e FAT */

void read_block(char *file, uint32_t block, uint8_t *record) {
    FILE *f = fopen(file, "r+");
    fseek(f, block * BLOCK_SIZE, SEEK_SET);
    fread(record, 1, BLOCK_SIZE, f);
    fclose(f);
}

void write_block(char *file, uint32_t block, uint8_t *record) {
    FILE *f = fopen(file, "r+");
    fseek(f, block * BLOCK_SIZE, SEEK_SET);
    fwrite(record, 1, BLOCK_SIZE, f);
    fclose(f);
}

void read_fat(char *file, uint16_t *fat) {
    FILE *f = fopen(file, "r+");
    fseek(f, 0, SEEK_SET);  // Garante que a leitura começa do início do arquivo
    fread(fat, sizeof(uint16_t), BLOCKS, f);  // Lê toda a FAT (2048 blocos de 16 bits cada)
    fclose(f);
}

void write_fat(char *file, uint16_t *fat) {
    FILE *f = fopen(file, "r+");
    fseek(f, 0, SEEK_SET);
    fwrite(fat, 2, BLOCKS, f);
    fclose(f);
}

void print_fat() {
    printf("Tabela de Alocação de Arquivos (FAT):\n");
    for (int i = 0; i < BLOCKS; i++) {
        if (i < FAT_BLOCKS) {
            printf("Bloco %d: Reservado para FAT\n", i);
        } else if (i == ROOT_BLOCK) {
            printf("Bloco %d: Diretório raiz\n", i);
        } else if (fat[i] == 0x0000) {
            printf("Bloco %d: Livre\n", i);
        } else if (fat[i] == 0x7fff) {
            printf("Bloco %d: Fim de arquivo ou diretório\n", i);
        } else if (fat[i] >= 0x0001 && fat[i] <= 0x7ffd) {
            printf("Bloco %d: Alocado - Próximo bloco %d\n", i, fat[i]);
        } else {
            // Adicionar lógica para verificar se o bloco é parte de um arquivo ou diretório específico
            struct dir_entry_s entry;
            read_block("filesystem.dat", i, data_block);

            for (int j = 0; j < DIR_ENTRIES; j++) {
                memcpy(&entry, &data_block[j * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
                if (entry.first_block == i) {
                    if (entry.attributes == 0x01) {
                        printf("Bloco %d: Arquivo '%s'\n", i, entry.filename);
                    } else if (entry.attributes == 0x02) {
                        printf("Bloco %d: Diretório '%s'\n", i, entry.filename);
                    }
                    break;
                }
            }
        }
    }
}

/* Função de inicialização do sistema de arquivos */
void init_filesystem() {
    FILE *f;
    int i;

    // Cria o arquivo `filesystem.dat` se não existir
    f = fopen("filesystem.dat", "w+");
    fclose(f);

    // Inicializa a FAT com as entradas apropriadas
    for (i = 0; i < FAT_BLOCKS; i++) {
        fat[i] = 0x7ffe;  // Blocos reservados para FAT (primeiros quatro blocos)
    }
    fat[ROOT_BLOCK] = 0x7fff;  // Diretório raiz marcado como fim de arquivo

    // Marca os blocos restantes como livres
    for (i = ROOT_BLOCK + 1; i < BLOCKS; i++) {
        fat[i] = 0x0000;  // Blocos livres
    }

    // Grava a FAT no disco
    write_fat("filesystem.dat", fat);

    // Inicializa o bloco de dados com zeros
    memset(data_block, 0, BLOCK_SIZE);

    // Grava o diretório raiz vazio
    write_block("filesystem.dat", ROOT_BLOCK, data_block);

    // Grava os blocos de dados restantes
    for (i = ROOT_BLOCK + 1; i < BLOCKS; i++) {
        write_block("filesystem.dat", i, data_block);
    }

    printf("Sistema de arquivos inicializado.\n");
}

int allocate_blocks(int num_blocks) {
    int first_block = -1;
    int last_allocated = -1;
    int blocks_allocated = 0;

    for (int i = ROOT_BLOCK + 1; i < BLOCKS && blocks_allocated < num_blocks; i++) {
        if (fat[i] == 0x0000) { // Bloco livre encontrado
            if (first_block == -1) {
                first_block = i; // Primeiro bloco alocado
            } else {
                fat[last_allocated] = i; // Encadeia o último bloco alocado ao próximo
            }
            last_allocated = i;
            blocks_allocated++;
        }
    }

    if (blocks_allocated == num_blocks) {
        fat[last_allocated] = 0x7fff; // Marca o último bloco como fim
        return first_block;
    } else {
        printf("Erro: Não há blocos suficientes disponíveis.\n");
        return -1;
    }
}

int find_file_block(const char *path) {
    struct dir_entry_s entry;
    char temp_path[256];
    char *token;
    uint32_t current_block = ROOT_BLOCK;

    // Copia o caminho para que possamos usar strtok sem modificar o original
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    // Usa strtok para dividir o caminho por "/"
    token = strtok(temp_path, "/");
    while (token != NULL) {
        int found = 0;

        // Lê o bloco atual do diretório
        read_block("filesystem.dat", current_block, data_block);

        // Procura pelo próximo diretório ou arquivo no bloco atual
        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

            // Verifica se a entrada corresponde ao token
            if (strncmp((const char *)entry.filename, token, 25) == 0) {
                // Se for o último token e um arquivo, retorna o bloco
                if (strtok(NULL, "/") == NULL && entry.attributes == 0x01) {
                    return entry.first_block; // Bloco do arquivo encontrado
                }
                // Se for um diretório, avança para o próximo bloco
                if (entry.attributes == 0x02) {
                    current_block = entry.first_block;
                    found = 1;
                    break;
                }
            }
        }

        // Se o arquivo ou diretório não for encontrado, retorna erro
        if (!found) {
            printf("Erro: Arquivo ou diretório '%s' não encontrado no caminho '%s'.\n", token, path);
            return -1;
        }

        // Move para o próximo token
        token = strtok(NULL, "/");
    }

    // Se não encontrou o arquivo, retorna -1
    printf("Erro: Arquivo '%s' não encontrado.\n", path);
    return -1;
}

/* Função para carregar o sistema de arquivos */
void load_filesystem() {
    // Carrega a FAT do disco
    read_fat("filesystem.dat", fat);

    // Carrega o diretório raiz do disco
    read_block("filesystem.dat", ROOT_BLOCK, data_block);
    memcpy(dir_block, data_block, sizeof(dir_block));

    printf("Sistema de arquivos carregado.\n");
}

int find_directory_block(const char *path) {
    struct dir_entry_s entry;
    char temp_path[256];
    char *token;
    uint32_t current_block = ROOT_BLOCK;

    // Cria uma cópia do caminho para que possamos usar strtok (sem modificar o original)
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    // Usa strtok para dividir o caminho por "/"
    token = strtok(temp_path, "/");
    while (token != NULL) {
        int found = 0;

        // Lê o bloco atual do diretório
        read_block("filesystem.dat", current_block, data_block);

        // Procura pelo próximo diretório no bloco atual
        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

            // Verifica se a entrada atual é um diretório e se o nome corresponde ao token
            if (entry.attributes == 0x02 && strncmp((const char *)entry.filename, token, 25) == 0) {
                current_block = entry.first_block; // Avança para o bloco do próximo diretório
                found = 1;
                break;
            }
        }

        // Se o diretório não foi encontrado, retorna erro
        if (!found) {
            printf("Erro: Diretório '%s' não encontrado no caminho '%s'.\n", token, path);
            return -1;
        }

        // Move para o próximo token (subdiretório no caminho)
        token = strtok(NULL, "/");
    }

    // Retorna o bloco final correspondente ao último diretório no caminho
    return current_block;
}

void ls(const char *path) {
    struct dir_entry_s entry;
    int dir_block = find_directory_block(path);

    if (dir_block == -1) {
        printf("Erro: Caminho '%s' não encontrado.\n", path);
        return;
    }

    // Lê o diretório localizado
    read_block("filesystem.dat", dir_block, data_block);

    printf("Listando o diretório: %s\n", path);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

        if (entry.attributes != 0x00) { // Entrada não vazia
            printf("%s - %s\n", entry.filename, (entry.attributes == 0x01) ? "Arquivo" : "Diretório");
        }
    }
}

void mkdir(const char *path) {
    struct dir_entry_s entry;
    char dir_name[25];
    int parent_block, dir_block;

    // Divide o caminho para obter o bloco do diretório pai e o nome do novo diretório
    char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        printf("Erro: Caminho inválido.\n");
        return;
    }

    strncpy(dir_name, last_slash + 1, 25);
    dir_name[24] = '\0';
    *last_slash = '\0';
    parent_block = find_directory_block(path);
    *last_slash = '/';

    if (parent_block == -1) {
        printf("Erro: Caminho '%s' não encontrado.\n", path);
        return;
    }

    dir_block = allocate_blocks(1); // Aloca um bloco para o novo diretório
    if (dir_block == -1) return;

    read_block("filesystem.dat", parent_block, data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x00) {
            strncpy((char *)entry.filename, dir_name, 25);
            entry.attributes = 0x02;
            entry.first_block = dir_block;
            entry.size = 0;

            memcpy(&data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, data_block);
            printf("Diretório '%s' criado no caminho '%s'.\n", dir_name, path);
            return;
        }
    }
    printf("Erro: Diretório está cheio.\n");
}

void create(const char *path) {
    struct dir_entry_s entry;
    char file_name[25];
    int parent_block, file_block;

    char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        printf("Erro: Caminho inválido.\n");
        return;
    }

    strncpy(file_name, last_slash + 1, 25);
    file_name[24] = '\0';
    *last_slash = '\0';
    parent_block = find_directory_block(path);
    *last_slash = '/';

    if (parent_block == -1) {
        printf("Erro: Caminho '%s' não encontrado.\n", path);
        return;
    }

    file_block = allocate_blocks(1); // Aloca um bloco para o novo arquivo
    if (file_block == -1) return;

    read_block("filesystem.dat", parent_block, data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x00) {
            strncpy((char *)entry.filename, file_name, 25);
            entry.attributes = 0x01;
            entry.first_block = file_block;
            entry.size = 0;

            memcpy(&data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, data_block);
            printf("Arquivo '%s' criado no caminho '%s'.\n", file_name, path);
            return;
        }
    }
    printf("Erro: Diretório está cheio.\n");
}

/* Função para remover um arquivo ou diretório vazio */
void unlink(const char *path) {
    struct dir_entry_s entry;
    int parent_block, entry_index = -1;

    char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        printf("Erro: Caminho inválido.\n");
        return;
    }

    char name[25];
    strncpy(name, last_slash + 1, 25);
    name[24] = '\0';
    *last_slash = '\0';
    parent_block = find_directory_block(path);
    *last_slash = '/';

    if (parent_block == -1) {
        printf("Erro: Caminho '%s' não encontrado.\n", path);
        return;
    }

    read_block("filesystem.dat", parent_block, data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strncmp((const char *)entry.filename, name, 25) == 0) {
            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        printf("Erro: Arquivo ou diretório '%s' não encontrado.\n", name);
        return;
    }

    if (entry.attributes == 0x02) {
        struct dir_entry_s check;
        read_block("filesystem.dat", entry.first_block, data_block);
        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&check, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
            if (check.attributes != 0x00) {
                printf("Erro: Diretório '%s' não está vazio.\n", name);
                return;
            }
        }
    }

    fat[entry.first_block] = 0x0000;
    memset(&data_block[entry_index * DIR_ENTRY_SIZE], 0, DIR_ENTRY_SIZE);
    write_block("filesystem.dat", parent_block, data_block);
    write_fat("filesystem.dat", fat);

    printf("Arquivo ou diretório '%s' excluído.\n", name);
}

/* Função para sobrescrever dados em um arquivo */
void write(const char *data, int rep, const char *path) {
    struct dir_entry_s entry;
    int file_block = find_file_block(path);  // Usando find_file_block para localizar o arquivo

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    // Carrega o diretório contendo o arquivo para atualizar a entrada
    read_block("filesystem.dat", file_block, data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x01 && strncmp((const char *)entry.filename, path, 25) == 0) {
            // Limpa o conteúdo do bloco de dados antes de sobrescrever
            memset(data_block, 0, BLOCK_SIZE);
            for (int j = 0; j < rep; j++) {
                strcat((char *)data_block, data);
            }
            entry.size = strlen((const char *)data_block);

            // Atualiza o bloco de dados no arquivo
            write_block("filesystem.dat", entry.first_block, data_block);
            printf("Dados sobrescritos em '%s'.\n", path);
            return;
        }
    }
}

/* Função para anexar dados a um arquivo */
void append(const char *data, int rep, const char *path) {
    struct dir_entry_s entry;
    int file_block = find_directory_block(path);
    
    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x01 && strncmp((const char *)entry.filename, path, 25) == 0) {
            read_block("filesystem.dat", entry.first_block, data_block);
            for (int j = 0; j < rep; j++) {
                strcat((char *)data_block, data);
            }
            entry.size = strlen((const char *)data_block);
            write_block("filesystem.dat", entry.first_block, data_block);
            printf("Dados anexados em '%s'.\n", path);
            return;
        }
    }
}

/* Função para ler o conteúdo de um arquivo */
void read(const char *path) {
    struct dir_entry_s entry;
    int file_block = find_directory_block(path);
    
    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x01 && strncmp((const char *)entry.filename, path, 25) == 0) {
            read_block("filesystem.dat", entry.first_block, data_block);
            printf("Conteúdo de '%s':\n%s\n", path, data_block);
            return;
        }
    }
}

/* Função main - ponto de entrada para o shell */
int main() {
    char command[256];

    while (1) {
        printf("filesystem> ");
        fgets(command, 256, stdin);

        if (strncmp(command, "init", 4) == 0) {
            init_filesystem();
        } else if (strncmp(command, "load", 4) == 0) {
            load_filesystem();
        } else if (strncmp(command, "ls", 2) == 0) {
            char path[256];
            sscanf(command + 3, "%s", path);
            ls(path);
        } else if (strncmp(command, "mkdir", 5) == 0) {
            char path[256];
            sscanf(command + 6, "%s", path);
            mkdir(path);
        } else if (strncmp(command, "create", 6) == 0) {
            char path[256];
            sscanf(command + 7, "%s", path);
            create(path);
        } else if (strncmp(command, "showfat", 7) == 0) {
            print_fat();
        } else if (strncmp(command, "unlink", 6) == 0) {
            char path[256];
            sscanf(command + 7, "%s", path);
            unlink(path);
        } else if (strncmp(command, "write", 5) == 0) {
            char data[256], path[256];
            int rep;
            sscanf(command + 6, "\"%[^\"]\" %d %s", data, &rep, path);
            write(data, rep, path);
        } else if (strncmp(command, "append", 6) == 0) {
            char data[256], path[256];
            int rep;
            sscanf(command + 7, "\"%[^\"]\" %d %s", data, &rep, path);
            append(data, rep, path);
        } else if (strncmp(command, "read", 4) == 0) {
            char path[256];
            sscanf(command + 5, "%s", path);
            read(path);
        } else if (strncmp(command, "exit", 4) == 0) {
            break;
        } else {
            printf("Comando desconhecido: %s", command);
        }
    }

    return 0;
}
