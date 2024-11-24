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

    // Inicializar a FAT
    for (i = 0; i < FAT_BLOCKS; i++) {
        fat[i] = 0x7ffe; // Blocos reservados para a FAT
    }
    fat[ROOT_BLOCK] = 0x7fff; // Bloco do diretório raiz
    for (i = ROOT_BLOCK + 1; i < BLOCKS; i++) {
        fat[i] = 0x0000; // Blocos livres
    }

    // Escrever a FAT no arquivo
    write_fat("filesystem.dat", fat);

    // Inicializar o bloco do diretório raiz
    uint8_t root_data_block[BLOCK_SIZE];
    memset(root_data_block, 0, BLOCK_SIZE);
    write_block("filesystem.dat", ROOT_BLOCK, root_data_block);

    // Inicializar os demais blocos de dados
    uint8_t empty_block[BLOCK_SIZE];
    memset(empty_block, 0, BLOCK_SIZE);
    for (i = ROOT_BLOCK + 1; i < BLOCKS; i++) {
        write_block("filesystem.dat", i, empty_block);
    }

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

int find_directory_block_recursive(const char *path, uint32_t current_block) {
    if (path == NULL || *path == '\0') {
        return current_block; // Diretório atual
    }

    // Ignorar barras iniciais
    while (*path == '/') {
        path++;
    }

    if (*path == '\0') {
        return current_block;
    }

    // Extrair o próximo componente do caminho
    char name[25];
    const char *next_slash = strchr(path, '/');
    size_t name_len = next_slash ? (size_t)(next_slash - path) : strlen(path);

    if (name_len >= sizeof(name)) {
        printf("Erro: Nome do diretório muito longo.\n");
        return -1;
    }

    strncpy(name, path, name_len);
    name[name_len] = '\0';

    // Ler o bloco atual do diretório em um buffer local
    uint8_t dir_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", current_block, dir_data_block);

    struct dir_entry_s entry;
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &dir_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

        if (entry.attributes == 0x02 && strcmp((const char *)entry.filename, name) == 0) {
            // Encontrou o diretório, continuar recursivamente
            return find_directory_block_recursive(next_slash, entry.first_block);
        }
    }

    // Diretório não encontrado
    return -1;
}

int find_directory_block(const char *path) {
    // Remover barra inicial se houver
    const char *clean_path = (path[0] == '/') ? path + 1 : path;
    return find_directory_block_recursive(clean_path, ROOT_BLOCK);
}

int find_file_block_recursive(const char *path, uint32_t current_block) {
    if (path == NULL || *path == '\0') {
        return -1; // Arquivo não especificado
    }

    // Ignorar barras iniciais
    while (*path == '/') {
        path++;
    }

    if (*path == '\0') {
        return -1;
    }

    // Extrair o próximo componente do caminho
    char name[25];
    const char *next_slash = strchr(path, '/');
    size_t name_len = next_slash ? (size_t)(next_slash - path) : strlen(path);

    if (name_len >= sizeof(name)) {
        printf("Erro: Nome do arquivo muito longo.\n");
        return -1;
    }

    strncpy(name, path, name_len);
    name[name_len] = '\0';

    // Ler o bloco atual do diretório em um buffer local
    uint8_t dir_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", current_block, dir_data_block);

    struct dir_entry_s entry;
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &dir_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));

        if (strcmp((const char *)entry.filename, name) == 0) {
            if (entry.attributes == 0x02 && next_slash != NULL) {
                // É um diretório, continuar recursivamente
                return find_file_block_recursive(next_slash, entry.first_block);
            } else if (entry.attributes == 0x01 && next_slash == NULL) {
                // Encontrou o arquivo
                return entry.first_block;
            } else {
                // Caminho inválido
                return -1;
            }
        }
    }

    // Arquivo não encontrado
    return -1;
}

int find_file_block(const char *path) {
    // Remover barra inicial se houver
    const char *clean_path = (path[0] == '/') ? path + 1 : path;
    return find_file_block_recursive(clean_path, ROOT_BLOCK);
}

void free_blocks_recursively(uint32_t block, uint8_t attributes) {
    if (attributes == 0x01) {
        // É um arquivo, liberar blocos de dados
        int current_block = block;
        while (current_block != 0x7fff) {
            int next_block = fat[current_block];
            fat[current_block] = 0x0000;
            current_block = next_block;
        }
    } else if (attributes == 0x02) {
        // É um diretório, percorrer recursivamente
        struct dir_entry_s entry;
        uint8_t dir_data[BLOCK_SIZE];

        read_block("filesystem.dat", block, dir_data);

        for (int i = 0; i < DIR_ENTRIES; i++) {
            memcpy(&entry, &dir_data[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
            if (entry.attributes != 0x00) {
                // Chamada recursiva para subdiretórios ou arquivos
                free_blocks_recursively(entry.first_block, entry.attributes);
                // Limpar a entrada do diretório
                memset(&dir_data[i * DIR_ENTRY_SIZE], 0, DIR_ENTRY_SIZE);
            }
        }

        // Escrever as alterações no diretório
        write_block("filesystem.dat", block, dir_data);

        // Liberar o bloco do diretório
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
            // Se for um diretório ou arquivo, não está vazio
            return 0;
        }
    }
    return 1; // Diretório está vazio
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

    // Separar o caminho do nome do diretório
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *last_slash = strrchr(temp_path, '/');
    if (last_slash == NULL) {
        printf("Erro: Caminho inválido.\n");
        return;
    }

    // Extrair o nome do diretório
    strncpy(dir_name, last_slash + 1, sizeof(dir_name));
    dir_name[sizeof(dir_name) - 1] = '\0';

    // Determinar o bloco do diretório pai
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

    // Ler o diretório pai em um buffer local
    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);

    // Verificar se já existe um arquivo ou diretório com o mesmo nome
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strcmp((const char *)entry.filename, dir_name) == 0) {
            printf("Erro: Já existe um arquivo ou diretório com o nome '%s'.\n", dir_name);
            return;
        }
    }

    // Criar o novo diretório
    int dir_block = allocate_blocks(1);
    if (dir_block == -1) return;

    // Inicializar o novo diretório em um buffer local
    uint8_t new_dir_block[BLOCK_SIZE];
    memset(new_dir_block, 0, BLOCK_SIZE);
    write_block("filesystem.dat", dir_block, new_dir_block);

    // Adicionar a entrada no diretório pai
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x00) {
            strncpy((char *)entry.filename, dir_name, sizeof(entry.filename));
            entry.filename[sizeof(entry.filename) - 1] = '\0';
            entry.attributes = 0x02; // Diretório
            entry.first_block = dir_block;
            entry.size = 0;

            memcpy(&parent_data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, parent_data_block);
            write_fat("filesystem.dat", fat);

            printf("Diretório '%s' criado no caminho '%s'.\n", dir_name, path);
            return;
        }
    }

    printf("Erro: Diretório está cheio.\n");
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

    // Ler o diretório pai em um buffer local
    uint8_t parent_data_block[BLOCK_SIZE];
    read_block("filesystem.dat", parent_block, parent_data_block);

    // Verificar se já existe um arquivo ou diretório com o mesmo nome
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes != 0x00 && strcmp((const char *)entry.filename, file_name) == 0) {
            printf("Erro: Já existe um arquivo ou diretório com o nome '%s'.\n", file_name);
            return;
        }
    }

    int file_block = allocate_blocks(1);
    if (file_block == -1) return;

    // Inicializar o bloco de dados do arquivo em um buffer local
    uint8_t file_data_block[BLOCK_SIZE];
    memset(file_data_block, 0, BLOCK_SIZE);
    write_block("filesystem.dat", file_block, file_data_block);

    // Adicionar a entrada do arquivo no diretório pai
    for (int i = 0; i < DIR_ENTRIES; i++) {
        memcpy(&entry, &parent_data_block[i * DIR_ENTRY_SIZE], sizeof(struct dir_entry_s));
        if (entry.attributes == 0x00) {
            strncpy((char *)entry.filename, file_name, sizeof(entry.filename));
            entry.filename[sizeof(entry.filename) - 1] = '\0';
            entry.attributes = 0x01; // Arquivo
            entry.first_block = file_block;
            entry.size = 0;

            memcpy(&parent_data_block[i * DIR_ENTRY_SIZE], &entry, sizeof(struct dir_entry_s));
            write_block("filesystem.dat", parent_block, parent_data_block);
            write_fat("filesystem.dat", fat);

            printf("Arquivo '%s' criado no caminho '%s'.\n", file_name, path);
            return;
        }
    }

    printf("Erro: Diretório está cheio.\n");
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

    // Ler o diretório pai em um buffer local
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

    if (entry.attributes == 0x02) {
        // Verificar recursivamente se o diretório está vazio
        if (!is_directory_empty(entry.first_block)) {
            printf("Erro: Diretório '%s' não está vazio.\n", name);
            return;
        }
    }

    // Liberar os blocos alocados (recursivamente se for um diretório)
    free_blocks_recursively(entry.first_block, entry.attributes);

    // Remover a entrada do diretório
    memset(&parent_data_block[entry_index * DIR_ENTRY_SIZE], 0, DIR_ENTRY_SIZE);
    write_block("filesystem.dat", parent_block, parent_data_block);
    write_fat("filesystem.dat", fat);

    printf("Arquivo ou diretório '%s' excluído.\n", name);
}

void write_data(const char *data, int rep, const char *path) {
    struct dir_entry_s entry;
    int file_block = find_file_block(path);

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    // Encontrar a entrada do arquivo no diretório pai
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *last_slash = strrchr(temp_path, '/');
    char file_name[25];
    strncpy(file_name, last_slash + 1, sizeof(file_name));
    file_name[sizeof(file_name) - 1] = '\0';
    *last_slash = '\0';

    int parent_block = (strlen(temp_path) == 0) ? ROOT_BLOCK : find_directory_block(temp_path);
    *last_slash = '/';

    if (parent_block == -1) {
        printf("Erro: Diretório pai não encontrado.\n");
        return;
    }

    // Remover blocos antigos
    int current_block = file_block;
    while (current_block != 0x7fff) {
        int next_block = fat[current_block];
        fat[current_block] = 0x0000;
        current_block = next_block;
    }

    // Escrever os novos dados
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

    // Atualizar a entrada do arquivo no diretório pai
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
    struct dir_entry_s entry;
    int file_block = find_file_block(path);

    if (file_block == -1) {
        printf("Erro: Arquivo '%s' não encontrado.\n", path);
        return;
    }

    // Encontrar a entrada do arquivo no diretório pai
    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path));
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *last_slash = strrchr(temp_path, '/');
    char file_name[25];
    strncpy(file_name, last_slash + 1, sizeof(file_name));
    file_name[sizeof(file_name) - 1] = '\0';
    *last_slash = '\0';

    int parent_block = (strlen(temp_path) == 0) ? ROOT_BLOCK : find_directory_block(temp_path);
    *last_slash = '/';

    if (parent_block == -1) {
        printf("Erro: Diretório pai não encontrado.\n");
        return;
    }

    // Encontrar o último bloco do arquivo
    int current_block = file_block;
    while (fat[current_block] != 0x7fff) {
        current_block = fat[current_block];
    }

    // Ler o último bloco para verificar espaço livre
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

    // Atualizar o tamanho do arquivo na entrada do diretório
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
                // Chamada recursiva para subdiretórios
                map_directory(entry.first_block);
            } else if (entry.attributes == 0x01) {
                // Mapear os blocos de dados do arquivo
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

int main() {
    char command[1024];

    while (1) {
        printf("filesystem> ");
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        // Remover o caractere de nova linha
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
            int offset = 5; // Posição após 'write'

            // Ignorar espaços em branco após o comando
            while (command[offset] == ' ') offset++;

            // Verificar se a string começa com aspas duplas
            if (command[offset] != '\"') {
                printf("Erro: Formato do comando 'write' inválido. Uso: write \"string\" rep path\n");
                continue;
            }
            offset++; // Avançar após a aspa dupla inicial

            // Extrair a string até a próxima aspa dupla
            int i = 0;
            while (command[offset] != '\"' && command[offset] != '\0') {
                data[i++] = command[offset++];
            }
            data[i] = '\0';

            // Verificar se encontramos a aspa dupla de fechamento
            if (command[offset] != '\"') {
                printf("Erro: String não fechada com aspas duplas.\n");
                continue;
            }
            offset++; // Avançar após a aspa dupla de fechamento

            // Ignorar espaços em branco antes do número de repetições
            while (command[offset] == ' ') offset++;

            // Ler o número de repetições
            if (sscanf(command + offset, "%d", &rep) != 1) {
                printf("Erro: Número de repetições inválido.\n");
                continue;
            }
            // Avançar o offset após o número de repetições
            while (command[offset] != ' ' && command[offset] != '\0') offset++;

            // Ignorar espaços em branco antes do caminho
            while (command[offset] == ' ') offset++;

            // Copiar o caminho
            strncpy(path, command + offset, sizeof(path));
            path[sizeof(path) - 1] = '\0'; // Garantir terminação da string

            // Chamar a função write_data
            write_data(data, rep, path);
        } else if (strncmp(command, "append", 6) == 0) {
            char data[1024], path[256];
            int rep;
            int offset = 6; // Posição após 'append'

            // Ignorar espaços em branco após o comando
            while (command[offset] == ' ') offset++;

            // Verificar se a string começa com aspas duplas
            if (command[offset] != '\"') {
                printf("Erro: Formato do comando 'append' inválido. Uso: append \"string\" rep path\n");
                continue;
            }
            offset++; // Avançar após a aspa dupla inicial

            // Extrair a string até a próxima aspa dupla
            int i = 0;
            while (command[offset] != '\"' && command[offset] != '\0') {
                data[i++] = command[offset++];
            }
            data[i] = '\0';

            // Verificar se encontramos a aspa dupla de fechamento
            if (command[offset] != '\"') {
                printf("Erro: String não fechada com aspas duplas.\n");
                continue;
            }
            offset++; // Avançar após a aspa dupla de fechamento

            // Ignorar espaços em branco antes do número de repetições
            while (command[offset] == ' ') offset++;

            // Ler o número de repetições
            if (sscanf(command + offset, "%d", &rep) != 1) {
                printf("Erro: Número de repetições inválido.\n");
                continue;
            }
            // Avançar o offset após o número de repetições
            while (command[offset] != ' ' && command[offset] != '\0') offset++;

            // Ignorar espaços em branco antes do caminho
            while (command[offset] == ' ') offset++;

            // Copiar o caminho
            strncpy(path, command + offset, sizeof(path));
            path[sizeof(path) - 1] = '\0'; // Garantir terminação da string

            // Chamar a função append_data
            append_data(data, rep, path);
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
