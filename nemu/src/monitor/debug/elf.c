#include "nemu.h"
#include "monitor/elf.h"
#include <stdlib.h>
#include <elf.h>

char *exec_file = NULL;

static char *strtab = NULL;
static Elf32_Sym *symtab = NULL;
static int nr_symtab_entry;
static size_t strtab_size;

void load_elf_tables(int argc, char *argv[]) {
	int ret;
	Assert(argc == 2, "run NEMU with format 'nemu [program]'");
	exec_file = argv[1];

	FILE *fp = fopen(exec_file, "rb");
	Assert(fp, "Can not open '%s'", exec_file);

	uint8_t buf[sizeof(Elf32_Ehdr)];
	ret = fread(buf, sizeof(Elf32_Ehdr), 1, fp);
	assert(ret == 1);

	/* The first several bytes contain the ELF header. */
	Elf32_Ehdr *elf = (void *)buf;
	char magic[] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3};

	/* Check ELF header */
	assert(memcmp(elf->e_ident, magic, 4) == 0);		// magic number
	assert(elf->e_ident[EI_CLASS] == ELFCLASS32);		// 32-bit architecture
	assert(elf->e_ident[EI_DATA] == ELFDATA2LSB);		// littel-endian
	assert(elf->e_ident[EI_VERSION] == EV_CURRENT);		// current version
	assert(elf->e_ident[EI_OSABI] == ELFOSABI_SYSV || 	// UNIX System V ABI
			elf->e_ident[EI_OSABI] == ELFOSABI_LINUX); 	// UNIX - GNU
	assert(elf->e_ident[EI_ABIVERSION] == 0);			// should be 0
	assert(elf->e_type == ET_EXEC);						// executable file
	assert(elf->e_machine == EM_386);					// Intel 80386 architecture
	assert(elf->e_version == EV_CURRENT);				// current version


	/* Load symbol table and string table for future use */

	/* Load section header table */
	uint32_t sh_size = elf->e_shentsize * elf->e_shnum;
	Elf32_Shdr *sh = malloc(sh_size);
	fseek(fp, elf->e_shoff, SEEK_SET);
	ret = fread(sh, sh_size, 1, fp);
	assert(ret == 1);

	/* Load section header string table */
	char *shstrtab = malloc(sh[elf->e_shstrndx].sh_size);
	fseek(fp, sh[elf->e_shstrndx].sh_offset, SEEK_SET);
	ret = fread(shstrtab, sh[elf->e_shstrndx].sh_size, 1, fp);
	assert(ret == 1);

	int i;
	for(i = 0; i < elf->e_shnum; i ++) {
		if(sh[i].sh_type == SHT_SYMTAB && 
				strcmp(shstrtab + sh[i].sh_name, ".symtab") == 0) {
			/* Load symbol table from exec_file */
			symtab = malloc(sh[i].sh_size);
			fseek(fp, sh[i].sh_offset, SEEK_SET);
			ret = fread(symtab, sh[i].sh_size, 1, fp);
			assert(ret == 1);
			nr_symtab_entry = sh[i].sh_size / sizeof(symtab[0]);
		}
		else if(sh[i].sh_type == SHT_STRTAB && 
				strcmp(shstrtab + sh[i].sh_name, ".strtab") == 0) {
			/* Load string table from exec_file */
			strtab = malloc(sh[i].sh_size);
			strtab_size = sh[i].sh_size;
			fseek(fp, sh[i].sh_offset, SEEK_SET);
			ret = fread(strtab, sh[i].sh_size, 1, fp);
			assert(ret == 1);
		}
	}

	free(sh);
	free(shstrtab);

	assert(strtab != NULL && symtab != NULL);

	fclose(fp);
}

bool lookup_symbol(const char *name, uint32_t *address) {
	int i;

	if(name == NULL || address == NULL || strtab == NULL || symtab == NULL) {
		return false;
	}

	for(i = 0; i < nr_symtab_entry; i ++) {
		unsigned type = ELF32_ST_TYPE(symtab[i].st_info);
		uint32_t name_offset = symtab[i].st_name;

		if(name_offset >= strtab_size ||
				(type != STT_OBJECT && type != STT_FUNC)) {
			continue;
		}

		if(strcmp(name, strtab + name_offset) == 0) {
			*address = symtab[i].st_value;
			return true;
		}
	}

	return false;
}

const char *lookup_function(swaddr_t address, uint32_t *start) {
	int i;
	const Elf32_Sym *best = NULL;

	if(strtab == NULL || symtab == NULL) {
		return NULL;
	}

	for(i = 0; i < nr_symtab_entry; i ++) {
		unsigned type = ELF32_ST_TYPE(symtab[i].st_info);
		uint32_t name_offset = symtab[i].st_name;
		uint32_t symbol_start = symtab[i].st_value;
		uint32_t symbol_end;

		if(type != STT_FUNC || name_offset >= strtab_size ||
				strtab[name_offset] == '\0' || address < symbol_start) {
			continue;
		}

		if(symtab[i].st_size == 0) {
			if(address != symbol_start) {
				continue;
			}
		}
		else {
			symbol_end = symbol_start + symtab[i].st_size;
			if(address >= symbol_end) {
				continue;
			}
		}

		if(best == NULL || symbol_start > best->st_value) {
			best = &symtab[i];
		}
	}

	if(best == NULL) {
		return NULL;
	}
	if(start != NULL) {
		*start = best->st_value;
	}
	return strtab + best->st_name;
}

void print_backtrace(void) {
	swaddr_t ebp = cpu.ebp;
	swaddr_t eip = cpu.eip;
	int depth;

	for(depth = 0; depth < 64 && eip != 0; depth ++) {
		uint32_t function_start = 0;
		const char *function = lookup_function(eip, &function_start);
		int i;

		if(function != NULL) {
			printf("0x%08x <%s+%u>", eip, function,
					eip - function_start);
		}
		else {
			printf("0x%08x <unknown>", eip);
		}

		if(ebp > HW_MEM_SIZE - 24) {
			/* reg_test intentionally randomizes the registers before the first
			 * command, so do not dereference an unavailable frame. */
			printf("()\n");
			break;
		}

		printf("(");
		for(i = 0; i < 4; i ++) {
			if(i != 0) {
				printf(", ");
			}
			printf("0x%08x", swaddr_read(ebp + 8 + i * 4, 4));
		}
		printf(")\n");

		if(ebp == 0) {
			break;
		}

		/* With frame pointers, caller EBP is above the current frame. */
		{
			swaddr_t next_ebp = swaddr_read(ebp, 4);
			swaddr_t next_eip = swaddr_read(ebp + 4, 4);
			if(next_ebp == 0 || next_ebp <= ebp) {
				break;
			}
			ebp = next_ebp;
			eip = next_eip;
		}
	}
}

