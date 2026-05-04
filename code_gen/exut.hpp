#ifndef CODE_GEN_EXUT_HPP
#define CODE_GEN_EXUT_HPP

#include "pe64.hpp"

namespace Virtual {

#ifdef PLATFORM_WINDOWS

inline void create_minimal_pe64(x86_64_CodeGen& cg, std::string filename) {
  PE64Generator::CreateExecutable(cg.code, cg.data, filename, &cg);
}

#else
void create_minimal_elf64(x86_64_CodeGen& cg, const std::string& filename) {
  // ELF64 для Linux
  FILE* f = fopen(filename.c_str(), "wb");
  if (!f) {
    printf("Failed to create file: %s\n", filename.c_str());
    return;
  }

  // ELF Header
  uint8_t elf_header[64] = {
      0x7F, 'E', 'L', 'F',              // Magic
      2,                                // 64-bit
      1,                                // Little-endian
      1,                                // ELF version
      0,                                // System V ABI
      0,                                // ABI version
      0, 0, 0, 0, 0, 0, 0,              // Padding
      2, 0,                             // ET_EXEC
      0x3E, 0,                          // x86-64
      1, 0, 0, 0,                       // ELF version
      0x78, 0x10, 0x40, 0, 0, 0, 0, 0,  // Entry point (0x401078)
      0x40, 0, 0, 0, 0, 0, 0, 0,        // Program header offset (64)
      0, 0, 0, 0, 0, 0, 0, 0,           // Section header offset
      0, 0, 0, 0,                       // Flags
      0x40, 0,                          // Size of this header (64)
      0x38, 0,                          // Size of program header
      1, 0,                             // Number of program headers
      0x40, 0,                          // Size of section headers
      0, 0,                             // Number of section headers
      0, 0                              // Section name string table index
  };

  fwrite(elf_header, 1, 64, f);

  // Program Header (LOAD segment)
  size_t code_size = cg.code.size();
  size_t file_size = 64 + 56 + code_size;  // ELF header + PHDR + code

  uint8_t phdr[56] = {
      1, 0, 0, 0,                 // PT_LOAD
      0x07, 0, 0, 0,              // Flags (R|W|X)
      0, 0, 0, 0, 0, 0, 0, 0,     // Offset in file (заполним)
      0, 0x40, 0, 0, 0, 0, 0, 0,  // Virtual address (0x400000)
      0, 0x40, 0, 0, 0, 0, 0, 0,  // Physical address
      0, 0, 0, 0, 0, 0, 0, 0,     // File size (заполним)
      0, 0, 0, 0, 0, 0, 0, 0,     // Memory size (заполним)
      0, 0x10, 0, 0, 0, 0, 0, 0   // Alignment (0x1000)
  };

  // Заполняем размеры
  uint64_t offset = 64 + 56;  // После заголовков
  uint64_t filesz = code_size;
  uint64_t memsz = code_size;

  memcpy(phdr + 8, &offset, 8);
  memcpy(phdr + 32, &filesz, 8);
  memcpy(phdr + 40, &memsz, 8);

  fwrite(phdr, 1, 56, f);

  // Код
  fwrite(cg.code.data(), 1, code_size, f);

  fclose(f);

  printf("Created ELF64 executable: %s (code: %zu bytes)\n",
         filename.c_str(), code_size);

  // Делаем файл исполняемым
  chmod(filename.c_str(), 0755);
}
#endif

}  // namespace Virtual

#endif