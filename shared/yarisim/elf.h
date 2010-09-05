#ifndef _ELF_H_
#define _ELF_H_ 1

#include <stdint.h>

#define EI_NIDENT 16

typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;

typedef struct {
        unsigned char  e_ident[EI_NIDENT];
        uint16_t       e_type;
        uint16_t       e_machine;
        uint32_t       e_version;
        Elf32_Addr     e_entry;
        Elf32_Off      e_phoff;
        Elf32_Off      e_shoff;
        uint32_t       e_flags;
        uint16_t       e_ehsize;
        uint16_t       e_phentsize;
        uint16_t       e_phnum;
        uint16_t       e_shentsize;
        uint16_t       e_shnum;
        uint16_t       e_shstrndx;
} Elf32_Ehdr;

typedef struct {
        uint32_t        p_type;
        Elf32_Off       p_offset;
        Elf32_Addr      p_vaddr;
        Elf32_Addr      p_paddr;
        uint32_t        p_filesz;
        uint32_t        p_memsz;
        uint32_t        p_flags;
        uint32_t        p_align;
} Elf32_Phdr;

typedef struct {
        uint32_t        sh_name;
        uint32_t        sh_type;
        uint32_t        sh_flags;
        Elf32_Addr      sh_addr;
        Elf32_Off       sh_offset;
        uint32_t        sh_size;
        uint32_t        sh_link;
        uint32_t        sh_info;
        uint32_t        sh_addralign;
        uint32_t        sh_entsize;
} Elf32_Shdr;

#define	ELFMAG "\177ELF"
#define	SELFMAG 4

#define EI_DATA 5

#define ET_EXEC 2

#define EM_MIPS 8

#define PT_LOAD 1

#endif
