#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "filesystem.h"

uint16_t fat[BLOCKS];
uint8_t data_block[BLOCK_SIZE];
struct dir_entry_s dir_block[DIR_ENTRIES];
char block_names[BLOCKS][26];

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
    fseek(f, 0, SEEK_SET);
    fread(fat, sizeof(uint16_t), BLOCKS, f);
    fclose(f);
}

void write_fat(char *file, uint16_t *fat) {
    FILE *f = fopen(file, "r+");
    fseek(f, 0, SEEK_SET);
    fwrite(fat, 2, BLOCKS, f);
    fclose(f);
}

void init_filesystem() {
    FILE *f;
    int i;

    f = fopen("filesystem.dat", "w+");
    fclose(f);

    for (i = 0; i < FAT_BLOCKS; i++) {
        fat[i] = 0x7ffe;
    }
    fat[ROOT_BLOCK] = 0x7fff;

    for (i = ROOT_BLOCK + 1; i < BLOCKS; i++) {
        fat[i] = 0x0000;
    }

    write_fat("filesystem.dat", fat);

    memset(data_block, 0, BLOCK_SIZE);

    write_block("filesystem.dat", ROOT_BLOCK, data_block);

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
        if (fat[i] == 0x0000) {
            if (first_block == -1) {
                first_block = i;
            } else {
                fat[last_allocated] = i;
            }
            last_allocated = i;
            blocks_allocated++;
        }
    }

    if (blocks_allocated == num_blocks) {
        fat[last_allocated] = 0x7fff;
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

    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    token = strtok(temp_path, "/");
    while (token != NULL) {
        int found = 0;

        read_block("filesystem.dat", current_block, data_block);

        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

            if (strncmp((const char *)entry.filename, token, 25) == 0) {
                if (entry.attributes == 0x01) { 
                    if ((token = strtok(NULL, "/")) == NULL) {
                        return entry.first_block;
                    } else {
                        printf("Erro: '%s' é um arquivo, não um diretório.\n", token);
                        return -1;
                    }
                } else if (entry.attributes == 0x02) {
                    current_block = entry.first_block;
                    found = 1;
                    break;
                }
            }
        }

        if (!found) {
            printf("Erro: Caminho '%s' não encontrado no diretório '%s'.\n", path, token);
            return -1;
        }

        token = strtok(NULL, "/");
    }

    printf("Erro: Arquivo '%s' não encontrado.\n", path);
    return -1;
}

void load_filesystem() {
    read_fat("filesystem.dat", fat);

    read_block("filesystem.dat", ROOT_BLOCK, data_block);
    memcpy(dir_block, data_block, sizeof(dir_block));

    printf("Sistema de arquivos carregado.\n");
}

int find_directory_block(const char *path) {
    struct dir_entry_s entry;
    char temp_path[256];
    char *token;
    uint32_t current_block = ROOT_BLOCK;

    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    token = strtok(temp_path, "/");
    while (token != NULL) {
        int found = 0;

        read_block("filesystem.dat", current_block, data_block);

        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

            if (entry.attributes == 0x02 && strncmp((const char *)entry.filename, token, 25) == 0) {
                current_block = entry.first_block;
                found = 1;
                break;
            }
        }

        if (!found) {
            return -1;
        }

        token = strtok(NULL, "/");
    }

    return current_block;
}

int count_entries(uint8_t *data_block) {
    struct dir_entry_s entry;
    int count = 0;

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00) {
            count++;
        }
    }
    return count;
}

void ls(const char *path) {
    struct dir_entry_s entry;
    int block;

    block = find_directory_block(path);
    if (block != -1) {
        read_block("filesystem.dat", block, data_block);
        printf("Listando o diretório: %s\n", path);
        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
            if (entry.attributes != 0x00) {
                printf("%s - %s\n", entry.filename, (entry.attributes == 0x01) ? "Arquivo" : "Diretório");
                printf("Tamanho: %d bytes\n", entry.size);
                printf("Bloco inicial: %d\n", entry.first_block);
                printf("File attributes: %d\n", entry.attributes);
                printf("Nome do arquivo: %s\n", entry.filename);
            }
        }
        return;
    }

    block = find_file_block(path);
    if (block != -1) {
        printf("Informações do arquivo '%s':\n", path);
        printf("Bloco inicial: %d\n", block);
        return;
    }

    printf("Erro: Caminho '%s' não encontrado.\n", path);
}

void mkdir(const char *path) {
    struct dir_entry_s entry;
    char dir_name[25];
    int parent_block, dir_block;

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

    read_block("filesystem.dat", parent_block, data_block);
    if (count_entries(data_block) >= 32) {
        printf("Erro: O diretório está cheio. Não é possível criar mais entradas.\n");
        return;
    }

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strncmp((const char *)entry.filename, dir_name, 25) == 0) {
            printf("Erro: Já existe um arquivo ou diretório com o nome '%s'.\n", dir_name);
            return;
        }
    }

    dir_block = allocate_blocks(1);
    if (dir_block == -1) return;

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

    read_block("filesystem.dat", parent_block, data_block);
    if (count_entries(data_block) >= 32) {
        printf("Erro: O diretório está cheio. Não é possível criar mais entradas.\n");
        return;
    }

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strncmp((const char *)entry.filename, file_name, 25) == 0) {
            printf("Erro: Já existe um arquivo ou diretório com o nome '%s'.\n", file_name);
            return;
        }
    }

    file_block = allocate_blocks(1);
    if (file_block == -1) return;

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

    int current_block = entry.first_block;
    while (current_block != 0x7fff) {
        int next_block = fat[current_block];
        fat[current_block] = 0x0000;
        current_block = next_block;
    }

    memset(&data_block[entry_index * DIR_ENTRY_SIZE], 0, DIR_ENTRY_SIZE);
    write_block("filesystem.dat", parent_block, data_block);
    write_fat("filesystem.dat", fat);

    printf("Arquivo ou diretório '%s' excluído.\n", name);
}

void write(const char *data, int rep, const char *path) {
    struct dir_entry_s entry;
    int file_block = find_file_block(path);

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    int current_block = file_block;
    while (current_block != 0x7fff) {
        int next_block = fat[current_block];
        fat[current_block] = 0x0000;
        current_block = next_block;
    }

    int data_length = strlen(data) * rep;
    int bytes_written = 0;

    current_block = allocate_blocks(1);
    if (current_block == -1) {
        printf("Erro: Não foi possível alocar blocos para o arquivo '%s'.\n", path);
        return;
    }

    int first_block = current_block;

    while (bytes_written < data_length) {
        memset(data_block, 0, BLOCK_SIZE);

        int bytes_to_copy = (data_length - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : (data_length - bytes_written);
        for (int i = 0; i < bytes_to_copy; i++) {
            data_block[i] = data[(bytes_written + i) % strlen(data)];
        }

        write_block("filesystem.dat", current_block, data_block);
        bytes_written += bytes_to_copy;

        if (bytes_written < data_length) {

            int next_block = allocate_blocks(1);
            if (next_block == -1) {
                printf("Erro: Não foi possível alocar mais blocos para o arquivo '%s'.\n", path);
                return;
            }
            fat[current_block] = next_block;
            current_block = next_block;
        } else {
            fat[current_block] = 0x7fff;
        }
    }

    int parent_block = find_directory_block(path);
    read_block("filesystem.dat", parent_block, data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (strcmp((const char *)entry.filename, path + 1) == 0) {
            entry.size = data_length;
            entry.first_block = first_block;
            memcpy(&data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, data_block);
            break;
        }
    }

    write_fat("filesystem.dat", fat);

    printf("Dados sobrescritos no arquivo '%s'.\n", path);
}

void append(const char *data, int rep, const char *path) {
    struct dir_entry_s entry;
    int file_block = find_file_block(path);

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    int data_length = strlen(data) * rep;

    int current_block = file_block;
    while (fat[current_block] != 0x7fff) {
        current_block = fat[current_block];
    }

    read_block("filesystem.dat", current_block, data_block);
    int offset = strlen((char *)data_block);

    int bytes_written = 0;
    while (bytes_written < data_length) {
        int bytes_to_copy = (data_length - bytes_written > BLOCK_SIZE - offset) 
                            ? (BLOCK_SIZE - offset) 
                            : (data_length - bytes_written);
        for (int i = 0; i < bytes_to_copy; i++) {
            data_block[offset + i] = data[(bytes_written + i) % strlen(data)];
        }
        bytes_written += bytes_to_copy;
        offset += bytes_to_copy;

        if (offset == BLOCK_SIZE) {
            write_block("filesystem.dat", current_block, data_block);

            int next_block = allocate_blocks(1);
            if (next_block == -1) {
                printf("Erro: Não foi possível alocar mais blocos para o arquivo '%s'.\n", path);
                return;
            }

            fat[current_block] = next_block;
            current_block = next_block;
            offset = 0;
            memset(data_block, 0, BLOCK_SIZE);
        }
    }

    write_block("filesystem.dat", current_block, data_block);
    fat[current_block] = 0x7fff;

    int parent_block = find_directory_block(path);
    read_block("filesystem.dat", parent_block, data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (strcmp((const char *)entry.filename, path + 1) == 0) {
            entry.size += data_length;
            memcpy(&data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, data_block);
            break;
        }
    }

    write_fat("filesystem.dat", fat);

    printf("Dados anexados no arquivo '%s'.\n", path);
}

void read(const char *path) {
    int file_block = find_file_block(path);

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    printf("Conteúdo de '%s':\n", path);

    int current_block = file_block;
    while (current_block != 0x7fff) {
        read_block("filesystem.dat", current_block, data_block);
        printf("%s", data_block);

        current_block = fat[current_block];
    }
    printf("\n");
}

void map_directory(uint32_t block) {
    struct dir_entry_s entry;
    uint8_t dir_data[BLOCK_SIZE];

    read_block("filesystem.dat", block, dir_data);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &dir_data[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00) {
            strcpy(block_names[entry.first_block], (char *)entry.filename);

            if (entry.attributes == 0x02) {
                map_directory(entry.first_block);
            }
        }
    }
}

void export_fat_to_file(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Erro: Não foi possível abrir o arquivo '%s' para escrita.\n", filename);
        return;
    }

    fprintf(f, "=== Tabela de Alocação de Arquivos (FAT) ===\n");

    for (int i = 0; i < BLOCKS; i++) {
        strcpy(block_names[i], "");
    }

    map_directory(ROOT_BLOCK);

    for (int i = 0; i < BLOCKS; i++) {
        if (i < FAT_BLOCKS) {
            fprintf(f, "Bloco %d: Reservado para FAT [Código: 0x7ffe]\n", i);
        } else if (i == ROOT_BLOCK) {
            fprintf(f, "Bloco %d: Diretório raiz [Código: 0x7fff]\n", i);
        } else if (fat[i] == 0x0000) {
            fprintf(f, "Bloco %d: Livre [Código: 0x0000]\n", i);
        } else if (fat[i] == 0x7fff) {
            if (strlen(block_names[i]) > 0) {
                fprintf(f, "Bloco %d: Fim de arquivo (%s) [Código: 0x7fff]\n", i, block_names[i]);
            } else {
                fprintf(f, "Bloco %d: Fim de arquivo ou diretório [Código: 0x7fff]\n", i);
            }
        } else if (fat[i] >= 0x0001 && fat[i] <= 0x7ffd) {
            if (strlen(block_names[i]) > 0) {
                fprintf(f, "Bloco %d: Alocado para (%s) - Próximo bloco %d [Código: 0x%04x]\n", i, block_names[i], fat[i], fat[i]);
            } else {
                fprintf(f, "Bloco %d: Alocado - Próximo bloco %d [Código: 0x%04x]\n", i, fat[i], fat[i]);
            }
        } else {
            fprintf(f, "Bloco %d: Estado desconhecido [Código: 0x%04x]\n", i, fat[i]);
        }
    }

    fclose(f);
    printf("Tabela FAT e informações exportadas para o arquivo '%s'.\n", filename);
}

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
        } else if (strncmp(command, "unlink", 6) == 0) {
            char path[256];
            sscanf(command + 7, "%s", path);
            unlink(path);
        } else if (strncmp(command, "write", 5) == 0) {
            char data[1024], path[256];
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
        } else if (strncmp(command, "export", 6) == 0) {
            char filename[256];
            sscanf(command + 7, "%s", filename);
            export_fat_to_file(filename);
        } else {
            printf("Comando desconhecido: %s", command);
        }
    }

    return 0;
}
