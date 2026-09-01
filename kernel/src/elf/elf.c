#include "common.h"
#include "memory.h"
#include <string.h>
#include <elf.h>

#define ELF_OFFSET_IN_DISK 0

#ifdef HAS_DEVICE
void ide_read(uint8_t *, uint32_t, uint32_t);
#else
void ramdisk_read(uint8_t *, uint32_t, uint32_t);
#endif

#define STACK_SIZE (1 << 20)

void create_video_mapping();
uint32_t get_ucr3();

uint32_t loader() {
	Elf32_Ehdr *elf;
	Elf32_Phdr ph;

	uint8_t buf[4096];

#ifdef HAS_DEVICE
	ide_read(buf, ELF_OFFSET_IN_DISK, 4096);
#else
	ramdisk_read(buf, ELF_OFFSET_IN_DISK, 4096);
#endif

	elf = (void*)buf;

	/* Validate the ELF header before using offsets from the image. */
	nemu_assert(elf->e_ident[EI_MAG0] == ELFMAG0 &&
			elf->e_ident[EI_MAG1] == ELFMAG1 &&
			elf->e_ident[EI_MAG2] == ELFMAG2 &&
			elf->e_ident[EI_MAG3] == ELFMAG3);
	nemu_assert(elf->e_ident[EI_CLASS] == ELFCLASS32);
	nemu_assert(elf->e_ident[EI_DATA] == ELFDATA2LSB);
	nemu_assert(elf->e_type == ET_EXEC && elf->e_machine == EM_386);
	nemu_assert(elf->e_phentsize == sizeof(Elf32_Phdr));
	nemu_assert(elf->e_phnum != 0);

	/* Load each program segment */
	for(uint16_t i = 0; i < elf->e_phnum; i ++) {
		uint32_t ph_offset = elf->e_phoff + i * elf->e_phentsize;

#ifdef HAS_DEVICE
		ide_read((uint8_t *)&ph, ph_offset, sizeof(ph));
#else
		ramdisk_read((uint8_t *)&ph, ph_offset, sizeof(ph));
#endif

		/* Scan the program header table, load each segment into memory */
		if(ph.p_type == PT_LOAD) {
			uint8_t segment_buf[4096];
			uint32_t loaded = 0;
			uint32_t destination = ph.p_vaddr;

			nemu_assert(ph.p_filesz <= ph.p_memsz);
			nemu_assert(ph.p_vaddr + ph.p_memsz >= ph.p_vaddr);

			while(loaded < ph.p_filesz) {
				uint32_t chunk = ph.p_filesz - loaded;
				if(chunk > sizeof(segment_buf)) {
					chunk = sizeof(segment_buf);
				}

#ifdef HAS_DEVICE
				ide_read(segment_buf, ph.p_offset + loaded, chunk);
#else
				ramdisk_read(segment_buf, ph.p_offset + loaded, chunk);
#endif
				memcpy(pa_to_va(destination + loaded), segment_buf, chunk);
				loaded += chunk;
			}

			/* The loader must zero the BSS tail of every loadable segment. */
			memset(pa_to_va(destination + ph.p_filesz), 0,
					ph.p_memsz - ph.p_filesz);


#ifdef IA32_PAGE
			/* Record the program break for future use. */
			extern uint32_t cur_brk, max_brk;
			uint32_t new_brk = ph.p_vaddr + ph.p_memsz;
			if(cur_brk < new_brk) { max_brk = cur_brk = new_brk; }
#endif
		}
	}

	volatile uint32_t entry = elf->e_entry;

#ifdef IA32_PAGE
	mm_malloc(KOFFSET - STACK_SIZE, STACK_SIZE);

#ifdef HAS_DEVICE
	create_video_mapping();
#endif

	write_cr3(get_ucr3());
#endif

	return entry;
}
