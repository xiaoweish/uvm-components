// Copyright (c) 2013, The Regents of the University of California (Regents).
// All Rights Reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the Regents nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.

// IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
// OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
// HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
// MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

#include "vpi_user.h"
// #include "elfdpi.h"
#include "vc_hdrs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>
#include "elf.h"
#include <map>
#include <string>
#include <string.h>
#include <vpi_user.h>

std::map<std::string, uint64_t> symbols;
std::map<std::string, uint64_t> sections;
std::map<std::string, uint64_t> section_length;

uint64_t entry_address; // Global variable to store the entry address

// Function to get the entry address without input arguments
unsigned long long get_entry_address() {
  return entry_address;
}

unsigned long long get_section_address (const char* symb) {
  return sections[symb];
}

unsigned long long get_symbol_address (const char* symb) {
  return symbols[symb];
}

unsigned long long get_section_size (const char* symb) {
  return section_length[symb];
}

void* read_elf(const char* fn) {

  int fd = open(fn, O_RDONLY);
  struct stat s;
  // check thtat the file exists
  assert(fd != -1);

  if (fstat(fd, &s) < 0)
    abort();
  size_t size = s.st_size;
  // map the file to memory
  char* buf = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  assert(buf != MAP_FAILED);
  close(fd);
  // check that this is an elf file
  assert(size >= sizeof(Elf64_Ehdr));
  const Elf64_Ehdr* eh64 = (const Elf64_Ehdr*)buf;
  assert(IS_ELF32(*eh64) || IS_ELF64(*eh64) && "This file is not a valid ELF file");

  #define LOAD_ELF(ehdr_t, phdr_t, shdr_t, sym_t) do { \
    ehdr_t* eh = (ehdr_t*)buf; \
    phdr_t* ph = (phdr_t*)(buf + eh->e_phoff); \
    assert(size >= eh->e_phoff + eh->e_phnum*sizeof(*ph)); \
    for (unsigned i = 0; i < eh->e_phnum; i++) { \
      if(ph[i].p_type == PT_LOAD && ph[i].p_memsz) { \
        if (ph[i].p_filesz) { \
          assert(size >= ph[i].p_offset + ph[i].p_filesz); \
        } \
      } \
    } \
    shdr_t* sh = (shdr_t*)(buf + eh->e_shoff); \
    assert(size >= eh->e_shoff + eh->e_shnum*sizeof(*sh)); \
    assert(eh->e_shstrndx < eh->e_shnum); \
    assert(size >= sh[eh->e_shstrndx].sh_offset + sh[eh->e_shstrndx].sh_size); \
    char *shstrtab = buf + sh[eh->e_shstrndx].sh_offset; \
    unsigned strtabidx = 0, symtabidx = 0; \
    for (unsigned i = 0; i < eh->e_shnum; i++) { \
      unsigned max_len = sh[eh->e_shstrndx].sh_size - sh[i].sh_name; \
      assert(sh[i].sh_name < sh[eh->e_shstrndx].sh_size); \
      assert(strnlen(shstrtab + sh[i].sh_name, max_len) < max_len); \
      sections[shstrtab + sh[i].sh_name] = sh[i].sh_addr; \
      section_length[shstrtab + sh[i].sh_name] =  sh[i].sh_size; \
      if (sh[i].sh_type & SHT_NOBITS) continue; \
      assert(size >= sh[i].sh_offset + sh[i].sh_size); \
      if (strcmp(shstrtab + sh[i].sh_name, ".strtab") == 0) \
        strtabidx = i; \
      if (strcmp(shstrtab + sh[i].sh_name, ".symtab") == 0) \
        symtabidx = i; \
    } \
    if (strtabidx && symtabidx) { \
      char* strtab = buf + sh[strtabidx].sh_offset; \
      sym_t* sym = (sym_t*)(buf + sh[symtabidx].sh_offset); \
      for (unsigned i = 0; i < sh[symtabidx].sh_size/sizeof(sym_t); i++) { \
        unsigned max_len = sh[strtabidx].sh_size - sym[i].st_name; \
        symbols[strtab + sym[i].st_name] = sym[i].st_value; \
        assert(sym[i].st_name < sh[strtabidx].sh_size); \
        assert(strnlen(strtab + sym[i].st_name, max_len) < max_len); \
      } \
    } \
  } while(0)

  if (IS_ELF32(*eh64)) {
    LOAD_ELF(Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Sym);
    entry_address = ((Elf32_Ehdr*)eh64)->e_entry; // Store the entry address for 32-bit
  }
  else {
    LOAD_ELF(Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Sym);
    entry_address = ((Elf64_Ehdr*)eh64)->e_entry; // Store the entry address for 64-bit
  }
  munmap(buf, size);

  return &symbols;
}
