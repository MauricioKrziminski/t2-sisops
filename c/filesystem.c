#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "filesystem.h"

uint16_t fat[BLOCKS];
char block_names[BLOCKS][26];

void read_block(const char *file, uint32_t block, uint8_t *record) {
    FILE *f = fopen(file, "rb");
    if (f == NULL) {
        printf("Erro: Não foi possível abrir o arquivo '%s' para leitura.\n", file);
        return;
    }
    fseek(f, block * BLOCK_SIZE, SEEK_SET);
    fread(record, 1, BLOCK_SIZE, f);
    fclose(f);
}

void write_block(const char *file, uint32_t block, const uint8_t *record) {
    FILE *f = fopen(file, "rb+");
    if (f == NULL) {
        printf("Erro: Não foi possível abrir o arquivo '%s' para escrita.\n", file);
        return;
    }
    fseek(f, block * BLOCK_SIZE, SEEK_SET);
    fwrite(record, 1, BLOCK_SIZE, f);
    fclose(f);
}

void read_fat(const char *file, uint16_t *fat) {
    FILE *f = fopen(file, "rb");
    if (f == NULL) {
        printf("Erro: Não foi possível abrir o arquivo '%s' para leitura.\n", file);
        return;
    }
    fseek(f, 0, SEEK_SET);
    fread(fat, sizeof(uint16_t), BLOCKS, f);
    fclose(f);
}

void write_fat(const char *file, const uint16_t *fat) {
    FILE *f = fopen(file, "rb+");
    if (f == NULL) {
        printf("Erro: Não foi possível abrir o arquivo '%s' para escrita.\n", file);
        return;
    }
    fseek(f, 0, SEEK_SET);
    fwrite(fat, sizeof(uint16_t), BLOCKS, f);
    fclose(f);
}

void init_filesystem() {
    FILE *f;
    int i;

    f = fopen("filesystem.dat", "wb+");
    if (f == NULL) {
        printf("Erro: Não foi possível criar o arquivo 'filesystem.dat'.\n");
        return;
    }

    for (i = 0; i < FAT_BLOCKS; i++) {
        fat[i] = 0x7ffe; 
    }
    fat[ROOT_BLOCK] = 0x7fff; 
    for (i = ROOT_BLOCK + 1; i < BLOCKS; i++) {
        fat[i] = 0x0000; 
    }

    write_fat("filesystem.dat", fat);

    uint8_t root_data_block[BLOCK_SIZE];
    memset(root_data_block, 0, BLOCK_SIZE);
    write_block("filesystem.dat", ROOT_BLOCK, root_data_block);

    fclose(f);
    printf("Sistema de arquivos inicializado.\n");
}

void load_filesystem() {
    read_fat("filesystem.dat", fat);
    printf("Sistema de arquivos carregado.\n");
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

int find_block_recursive(const char *path, uint32_t current_block, uint8_t target_attribute) {
    if (path == NULL || *path == '\0') {
        return -1;
    }

    while (*path == '/') {
        path++;
    }

    if (*path == '\0') {
        return (target_attribute == 0x02) ? current_block : -1;
    }

    char name[25];
    const char *next_slash = strchr(path, '/');
    size_t name_len = next_slash ? (size_t)(next_slash - path) : strlen(path);

    if (name_len >= sizeof(name)) {
        printf("Erro: Nome muito longo.\n");
        return -1;
    }

    strncpy(name, path, name_len);
    name[name_len] = '\0';

    uint8_t dir_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", current_block, dir_data_block);

    struct dir_entry_s entry;
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &dir_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

        if (strcmp((const char *)entry.filename, name) == 0) {
            if (entry.attributes == target_attribute && next_slash == NULL) {
                return entry.first_block;
            } else if (entry.attributes == 0x02 && next_slash != NULL) {
                return find_block_recursive(next_slash, entry.first_block, target_attribute);
            } else {
                return -1;
            }
        }
    }

    return -1;
}

int find_directory_block(const char *path) {
    const char *clean_path = (path[0] == '/') ? path + 1 : path;
    return find_block_recursive(clean_path, ROOT_BLOCK, 0x02);
}

int find_file_block(const char *path) {
    const char *clean_path = (path[0] == '/') ? path + 1 : path;
    return find_block_recursive(clean_path, ROOT_BLOCK, 0x01);
}

void free_blocks_recursively(uint32_t block, uint8_t attributes) {
    if (attributes == 0x01) {
        int current_block = block;
        while (current_block != 0x7fff) {
            int next_block = fat[current_block];
            fat[current_block] = 0x0000;
            current_block = next_block;
        }
    } else if (attributes == 0x02) {
        struct dir_entry_s entry;
        uint8_t dir_data[BLOCK_SIZE];

        read_block("filesystem.dat", block, dir_data);

        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &dir_data[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
            if (entry.attributes != 0x00) {
                free_blocks_recursively(entry.first_block, entry.attributes);
                memset(&dir_data[i * DIR_ENTRY_SIZE], 0, DIR_ENTRY_SIZE);
            }
        }

        write_block("filesystem.dat", block, dir_data);

        fat[block] = 0x0000;
    }
}

int is_directory_empty(uint32_t block) {
    struct dir_entry_s entry;
    uint8_t dir_data[BLOCK_SIZE];

    read_block("filesystem.dat", block, dir_data);

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &dir_data[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00) {
            return 0; 
        }
    }
    return 1; 
}

int update_directory_entry(int parent_block, struct dir_entry_s *new_entry) {
    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);

    struct dir_entry_s entry;
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x00 || strcmp((const char *)entry.filename, (const char *)new_entry->filename) == 0) {
            memcpy(&parent_data_block[i * DIR_ENTRY_SIZE], new_entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, parent_data_block);
            write_fat("filesystem.dat", fat);
            return 0;
        }
    }

    printf("Erro: Diretório está cheio ou entrada não encontrada.\n");
    return -1;
}

void ls(const char *path) {
    struct dir_entry_s entry;
    int block;

    block = (strlen(path) == 0 || strcmp(path, "/") == 0) ? ROOT_BLOCK : find_directory_block(path);

    if (block != -1) {
        uint8_t dir_data_block[BLOCK_SIZE];
        read_block("filesystem.dat", block, dir_data_block);
        printf("Listando o diretório '%s':\n", path);
        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &dir_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
            if (entry.attributes != 0x00) {
                printf("%s\t%s\t%d bytes\n",
                       entry.filename,
                       (entry.attributes == 0x01) ? "Arquivo" : "Diretório",
                       entry.size);
            }
        }
        return;
    }

    int file_block = find_file_block(path);
    if (file_block != -1) {
        printf("Arquivo '%s' encontrado. Bloco inicial: %d\n", path, file_block);
        return;
    }

    printf("Erro: Caminho '%s' não encontrado.\n", path);
}

void mkdir(const char *path) {
    struct dir_entry_s entry;
    char dir_name[25];
    int parent_block;

    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *last_slash = strrchr(temp_path, '/');
    if (last_slash == NULL) {
        printf("Erro: Caminho inválido.\n");
        return;
    }

    strncpy(dir_name, last_slash + 1, sizeof(dir_name));
    dir_name[sizeof(dir_name) - 1] = '\0';

    if (last_slash == temp_path) {
        parent_block = ROOT_BLOCK;
    } else {
        *last_slash = '\0';
        parent_block = find_directory_block(temp_path);
        *last_slash = '/';
    }

    if (parent_block == -1) {
        printf("Erro: Caminho '%s' não encontrado.\n", temp_path);
        return;
    }

    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strcmp((const char *)entry.filename, dir_name) == 0) {
            printf("Erro: Já existe um arquivo ou diretório com o nome '%s'.\n", dir_name);
            return;
        }
    }

    int dir_block = allocate_blocks(1);
    if (dir_block == -1) return;

    uint8_t new_dir_block[BLOCK_SIZE];
    memset(new_dir_block, 0, BLOCK_SIZE);
    write_block("filesystem.dat", dir_block, new_dir_block);

    struct dir_entry_s new_entry;
    strncpy((char *)new_entry.filename, dir_name, sizeof(new_entry.filename));
    new_entry.filename[sizeof(new_entry.filename) - 1] = '\0';
    new_entry.attributes = 0x02; 
    new_entry.first_block = dir_block;
    new_entry.size = 0;

    if (update_directory_entry(parent_block, &new_entry) == 0) {
        printf("Diretório '%s' criado no caminho '%s'.\n", dir_name, path);
    }
}

void create(const char *path) {
    struct dir_entry_s entry;
    char file_name[25];
    int parent_block;

    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *last_slash = strrchr(temp_path, '/');
    if (last_slash == NULL) {
        printf("Erro: Caminho inválido.\n");
        return;
    }

    strncpy(file_name, last_slash + 1, sizeof(file_name));
    file_name[sizeof(file_name) - 1] = '\0';

    if (last_slash == temp_path) {
        parent_block = ROOT_BLOCK;
    } else {
        *last_slash = '\0';
        parent_block = find_directory_block(temp_path);
        *last_slash = '/';
    }

    if (parent_block == -1) {
        printf("Erro: Caminho '%s' não encontrado.\n", temp_path);
        return;
    }

    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strcmp((const char *)entry.filename, file_name) == 0) {
            printf("Erro: Já existe um arquivo ou diretório com o nome '%s'.\n", file_name);
            return;
        }
    }

    int file_block = allocate_blocks(1);
    if (file_block == -1) return;

    uint8_t file_data_block[BLOCK_SIZE];
    memset(file_data_block, 0, BLOCK_SIZE);
    write_block("filesystem.dat", file_block, file_data_block);

    struct dir_entry_s new_entry;
    strncpy((char *)new_entry.filename, file_name, sizeof(new_entry.filename));
    new_entry.filename[sizeof(new_entry.filename) - 1] = '\0';
    new_entry.attributes = 0x01; // Arquivo
    new_entry.first_block = file_block;
    new_entry.size = 0;

    if (update_directory_entry(parent_block, &new_entry) == 0) {
        printf("Arquivo '%s' criado no caminho '%s'.\n", file_name, path);
    }
}

void unlink(const char *path) {
    struct dir_entry_s entry;
    int parent_block, entry_index = -1;

    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *last_slash = strrchr(temp_path, '/');
    if (last_slash == NULL) {
        printf("Erro: Caminho inválido.\n");
        return;
    }

    char name[25];
    strncpy(name, last_slash + 1, sizeof(name));
    name[sizeof(name) - 1] = '\0';
    *last_slash = '\0';

    parent_block = (strlen(temp_path) == 0) ? ROOT_BLOCK : find_directory_block(temp_path);
    *last_slash = '/';

    if (parent_block == -1) {
        printf("Erro: Caminho '%s' não encontrado.\n", temp_path);
        return;
    }

    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strcmp((const char *)entry.filename, name) == 0) {
            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        printf("Erro: Arquivo ou diretório '%s' não encontrado.\n", name);
        return;
    }

    if (entry.attributes == 0x02 && !is_directory_empty(entry.first_block)) {
        printf("Erro: Diretório '%s' não está vazio.\n", name);
        return;
    }

    free_blocks_recursively(entry.first_block, entry.attributes);

    memset(&parent_data_block[entry_index * DIR_ENTRY_SIZE], 0, DIR_ENTRY_SIZE);
    write_block("filesystem.dat", parent_block, parent_data_block);
    write_fat("filesystem.dat", fat);

    printf("Arquivo ou diretório '%s' excluído.\n", name);
}

void write_data(const char *data, int rep, const char *path) {
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

    int data_length = strlen(data);
    int total_length = data_length * rep;
    int bytes_written = 0;

    int first_block = allocate_blocks(1);
    if (first_block == -1) {
        printf("Erro: Não foi possível alocar blocos para o arquivo '%s'.\n", path);
        return;
    }

    current_block = first_block;

    while (bytes_written < total_length) {
        uint8_t file_data_block[BLOCK_SIZE];
        memset(file_data_block, 0, BLOCK_SIZE);
        int to_write = (total_length - bytes_written > BLOCK_SIZE) ? BLOCK_SIZE : total_length - bytes_written;
        for (int i = 0; i < to_write; i++) {
            file_data_block[i] = data[(bytes_written + i) % data_length];
        }
        write_block("filesystem.dat", current_block, file_data_block);
        bytes_written += to_write;

        if (bytes_written < total_length) {
            int next_block = allocate_blocks(1);
            if (next_block == -1) {
                printf("Erro: Não foi possível alocar mais blocos.\n");
                return;
            }
            fat[current_block] = next_block;
            current_block = next_block;
        } else {
            fat[current_block] = 0x7fff;
        }
    }

    struct dir_entry_s entry;
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    char *last_slash = strrchr(temp_path, '/');
    char file_name[25];
    strncpy(file_name, last_slash + 1, sizeof(file_name));
    file_name[sizeof(file_name) - 1] = '\0';
    *last_slash = '\0';
    int parent_block = (strlen(temp_path) == 0) ? ROOT_BLOCK : find_directory_block(temp_path);
    *last_slash = '/';

    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x01 && strcmp((const char *)entry.filename, file_name) == 0) {
            entry.first_block = first_block;
            entry.size = total_length;
            memcpy(&parent_data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, parent_data_block);
            break;
        }
    }

    write_fat("filesystem.dat", fat);

    printf("Dados escritos no arquivo '%s'.\n", path);
}

void append_data(const char *data, int rep, const char *path) {
    int file_block = find_file_block(path);

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    int current_block = file_block;
    while (fat[current_block] != 0x7fff) {
        current_block = fat[current_block];
    }

    uint8_t file_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", current_block, file_data_block);
    int offset = strlen((char *)file_data_block);

    int data_length = strlen(data);
    int total_length = data_length * rep;
    int bytes_written = 0;

    while (bytes_written < total_length) {
        int to_write = (total_length - bytes_written > BLOCK_SIZE - offset) ? BLOCK_SIZE - offset : total_length - bytes_written;
        for (int i = 0; i < to_write; i++) {
            file_data_block[offset + i] = data[(bytes_written + i) % data_length];
        }
        offset += to_write;
        bytes_written += to_write;

        if (offset == BLOCK_SIZE) {
            write_block("filesystem.dat", current_block, file_data_block);
            int next_block = allocate_blocks(1);
            if (next_block == -1) {
                printf("Erro: Não foi possível alocar mais blocos.\n");
                return;
            }
            fat[current_block] = next_block;
            current_block = next_block;
            offset = 0;
            memset(file_data_block, 0, BLOCK_SIZE);
        }
    }

    write_block("filesystem.dat", current_block, file_data_block);
    fat[current_block] = 0x7fff;

    struct dir_entry_s entry;
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    char *last_slash = strrchr(temp_path, '/');
    char file_name[25];
    strncpy(file_name, last_slash + 1, sizeof(file_name));
    file_name[sizeof(file_name) - 1] = '\0';
    *last_slash = '\0';
    int parent_block = (strlen(temp_path) == 0) ? ROOT_BLOCK : find_directory_block(temp_path);
    *last_slash = '/';

    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x01 && strcmp((const char *)entry.filename, file_name) == 0) {
            entry.size += total_length;
            memcpy(&parent_data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, parent_data_block);
            break;
        }
    }

    write_fat("filesystem.dat", fat);

    printf("Dados anexados ao arquivo '%s'.\n", path);
}

void read_file(const char *path) {
    int file_block = find_file_block(path);

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    printf("Conteúdo de '%s':\n", path);

    int current_block = file_block;
    while (current_block != 0x7fff) {
        uint8_t file_data_block[BLOCK_SIZE];
        read_block("filesystem.dat", current_block, file_data_block);
        fwrite(file_data_block, 1, BLOCK_SIZE, stdout);
        current_block = fat[current_block];
    }
    printf("\n");
}

void map_directory(uint32_t block) {
    struct dir_entry_s entry;
    uint8_t dir_data[BLOCK_SIZE];

    read_block("filesystem.dat", block, dir_data);

    for (int i = 0; i < BLOCKS; i++) {
        strcpy(block_names[i], "");
    }

    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &dir_data[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00) {
            strcpy(block_names[entry.first_block], (char *)entry.filename);

            if (entry.attributes == 0x02) {
                map_directory(entry.first_block);
            } else if (entry.attributes == 0x01) {
                int current_block = entry.first_block;
                while (current_block != 0x7fff) {
                    strcpy(block_names[current_block], (char *)entry.filename);
                    current_block = fat[current_block];
                }
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

int parse_write_append_command(const char *command, char *data, int *rep, char *path, const char *cmd_name) {
    int offset = strlen(cmd_name);
    while (command[offset] == ' ') offset++;

    if (command[offset] != '\"') {
        printf("Erro: Formato do comando '%s' inválido. Uso: %s \"string\" rep path\n", cmd_name, cmd_name);
        return -1;
    }
    offset++;

    int i = 0;
    while (command[offset] != '\"' && command[offset] != '\0') {
        data[i++] = command[offset++];
    }
    data[i] = '\0';

    if (command[offset] != '\"') {
        printf("Erro: String não fechada com aspas duplas.\n");
        return -1;
    }
    offset++;

    while (command[offset] == ' ') offset++;

    if (sscanf(command + offset, "%d", rep) != 1) {
        printf("Erro: Número de repetições inválido.\n");
        return -1;
    }
    while (command[offset] != ' ' && command[offset] != '\0') offset++;

    while (command[offset] == ' ') offset++;

    strncpy(path, command + offset, 256);
    path[255] = '\0';

    return 0;
}

int main() {
    char command[1024];

    while (1) {
        printf("filesystem> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        command[strcspn(command, "\n")] = '\0';

        if (strncmp(command, "init", 4) == 0) {
            init_filesystem();
        } else if (strncmp(command, "load", 4) == 0) {
            load_filesystem();
        } else if (strncmp(command, "ls", 2) == 0) {
            char path[256] = "";
            sscanf(command + 2, "%s", path);
            ls(path);
        } else if (strncmp(command, "mkdir", 5) == 0) {
            char path[256];
            sscanf(command + 5, "%s", path);
            mkdir(path);
        } else if (strncmp(command, "create", 6) == 0) {
            char path[256];
            sscanf(command + 6, "%s", path);
            create(path);
        } else if (strncmp(command, "unlink", 6) == 0) {
            char path[256];
            sscanf(command + 6, "%s", path);
            unlink(path);
        } else if (strncmp(command, "write", 5) == 0) {
            char data[1024], path[256];
            int rep;
            if (parse_write_append_command(command, data, &rep, path, "write") == 0) {
                write_data(data, rep, path);
            }
        } else if (strncmp(command, "append", 6) == 0) {
            char data[1024], path[256];
            int rep;
            if (parse_write_append_command(command, data, &rep, path, "append") == 0) {
                append_data(data, rep, path);
            }
        } else if (strncmp(command, "read", 4) == 0) {
            char path[256];
            sscanf(command + 4, "%s", path);
            read_file(path);
        } else if (strncmp(command, "export", 6) == 0) {
            char filename[256];
            sscanf(command + 6, "%s", filename);
            export_fat_to_file(filename);
        } else if (strncmp(command, "exit", 4) == 0) {
            break;
        } else {
            printf("Comando desconhecido: %s\n", command);
        }
    }
    return 0;
}