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
/* Bloco de dados */
extern uint8_t data_block[BLOCK_SIZE];

/* Estrutura de entrada de diretório */
struct dir_entry_s {
    int8_t filename[25];
    uint8_t attributes;
    uint16_t first_block;
    uint32_t size;
};
extern struct dir_entry_s dir_block[DIR_ENTRIES];

/* Funções para manipulação do sistema de arquivos */
void read_block(char *file, uint32_t block, uint8_t *record);
void write_block(char *file, uint32_t block, uint8_t *record);
void read_fat(char *file, uint16_t *fat);
void write_fat(char *file, uint16_t *fat);
void init_filesystem();
void load_filesystem();
void map_directory(uint32_t block);

#endif
