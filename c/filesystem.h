#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

#define BLOCK_SIZE        1024
#define BLOCKS            2048
#define FAT_SIZE          (BLOCKS * 2)
#define FAT_BLOCKS        (FAT_SIZE / BLOCK_SIZE)
#define ROOT_BLOCK        FAT_BLOCKS
#define DIR_ENTRY_SIZE    32
#define DIR_ENTRIES       (BLOCK_SIZE / DIR_ENTRY_SIZE)

/* Estrutura da FAT */
extern uint16_t fat[BLOCKS];

/* Estrutura de entrada de diretório */
struct dir_entry_s {
    int8_t filename[25];
    uint8_t attributes;
    uint16_t first_block;
    uint32_t size;
};

/* Funções para manipulação do sistema de arquivos */
void read_block(const char *file, uint32_t block, uint8_t *record);
void write_block(const char *file, uint32_t block, const uint8_t *record);
void read_fat(const char *file, uint16_t *fat);
void write_fat(const char *file, const uint16_t *fat);
void init_filesystem();
void load_filesystem();
void map_directory(uint32_t block);

/* Protótipos das funções auxiliares */
int allocate_blocks(int num_blocks);
int find_directory_block(const char *path);
int find_file_block(const char *path);
void free_blocks_recursively(uint32_t block, uint8_t attributes);
int is_directory_empty(uint32_t block);

/* Funções de comandos */
void ls(const char *path);
void mkdir(const char *path);
void create(const char *path);
void unlink(const char *path);
void write_data(const char *data, int rep, const char *path);
void append_data(const char *data, int rep, const char *path);
void read_file(const char *path);
void export_fat_to_file(const char *filename);

#endif /* FILESYSTEM_H */