#ifndef PE64_GEN_HPP
#define PE64_GEN_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "x86_64.hpp"

class PE64Generator {
 public:
  struct ImportFunction {
    std::string name;
    uint32_t hint = 0;
  };

  struct ImportDll {
    std::string dllName;
    std::vector<ImportFunction> functions;
  };

 private:
  std::vector<uint8_t> m_text;   // .text section
  std::vector<uint8_t> m_rdata;  // .rdata section
  std::vector<uint8_t> m_data;   // .data section
  std::vector<ImportDll> m_imports;

  uint32_t m_entryRva = 0;

  static constexpr uint32_t FILE_ALIGN = 0x200;
  static constexpr uint32_t SECT_ALIGN = 0x1000;
  static constexpr uint64_t IMAGE_BASE = 0x140000000ULL;

  // Internal helpers
  void appendU8(std::vector<uint8_t>& buf, uint8_t v) { buf.push_back(v); }
  void appendU16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
  }
  void appendU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 24) & 0xFF);
  }
  void appendU64(std::vector<uint8_t>& buf, uint64_t v) {
    appendU32(buf, (uint32_t)(v & 0xFFFFFFFF));
    appendU32(buf, (uint32_t)(v >> 32));
  }

  void patchU32(std::vector<uint8_t>& buf, size_t off, uint32_t v) {
    buf[off] = v & 0xFF;
    buf[off + 1] = (v >> 8) & 0xFF;
    buf[off + 2] = (v >> 16) & 0xFF;
    buf[off + 3] = (v >> 24) & 0xFF;
  }

  void patchU64(std::vector<uint8_t>& buf, size_t off, uint64_t v) {
    patchU32(buf, off, (uint32_t)(v & 0xFFFFFFFF));
    patchU32(buf, off + 4, (uint32_t)(v >> 32));
  }

  static uint32_t alignUp(uint32_t v, uint32_t a) {
    return (v + a - 1) & ~(a - 1);
  }

  // Build complete .rdata with imports
  // НОВАЯ СТРУКТУРА .rdata:
  // 0x00-0x1F: IAT (32 нуля)
  // 0x20-0x33: IDT
  // 0x34-0x43: INT (сразу после IDT!)
  // 0x60: DLL name
  // 0x80,0x90,0xA0: Hint/Name

  void buildRdata() {
    m_rdata.assign(512, 0);
    if (m_imports.empty()) return;

    // ===== IDT[0]: KERNEL32.DLL (0x00) =====
    patchU32(m_rdata, 0x00 + 0, 0);  // OriginalFirstThunk = 0!
    patchU32(m_rdata, 0x00 + 4, 0);
    patchU32(m_rdata, 0x00 + 8, 0);
    patchU32(m_rdata, 0x00 + 12, 0x2091);  // Name → "KERNEL32.DLL"
    patchU32(m_rdata, 0x00 + 16, 0x205C);  // FirstThunk → IAT

    // ===== IDT[1]: ucrtbase.dll (0x14) =====
    patchU32(m_rdata, 0x14 + 0, 0);  // OriginalFirstThunk = 0!
    patchU32(m_rdata, 0x14 + 4, 0);
    patchU32(m_rdata, 0x14 + 8, 0);
    patchU32(m_rdata, 0x14 + 12, 0x209E);  // Name → "ucrtbase.dll"
    patchU32(m_rdata, 0x14 + 16, 0x206C);  // FirstThunk → IAT

    // ===== INT + IAT =====
    patchU64(m_rdata, 0x5C, 0);
    patchU64(m_rdata, 0x6C, 0);

    patchU64(m_rdata, 0x5C, 0x207C);
    patchU64(m_rdata, 0x6C, 0x208A);

    // Hint/Name ExitProcess (0x7C)
    m_rdata[0x7C] = 0;
    m_rdata[0x7D] = 0;
    memcpy(&m_rdata[0x7E], "ExitProcess", 12);

    // Hint/Name puts (0x8A)
    m_rdata[0x8A] = 0;
    m_rdata[0x8B] = 0;
    memcpy(&m_rdata[0x8C], "puts", 5);

    // DLL names
    memcpy(&m_rdata[0x91], "KERNEL32.DLL", 13);
    memcpy(&m_rdata[0x9E], "ucrtbase.dll", 12);
  }

  void writeHeaders(std::vector<uint8_t>& pe,
                    uint32_t textRva, uint32_t rdataRva, uint32_t dataRva,
                    uint32_t textSize, uint32_t rdataSize, uint32_t dataSize,
                    uint32_t headersSize, uint32_t imageSize) {
    // DOS Header
    pe.resize(64, 0);
    pe[0] = 'M';
    pe[1] = 'Z';
    pe[60] = 0x40;

    // PE Signature
    appendU8(pe, 'P');
    appendU8(pe, 'E');
    appendU8(pe, 0);
    appendU8(pe, 0);

    // COFF Header
    appendU16(pe, 0x8664);      // Machine AMD64
    appendU16(pe, 3);           // NumberOfSections (always 3 for simplicity)
    appendU32(pe, 0x5EADBEEF);  // TimeDateStamp
    appendU32(pe, 0);           // SymbolTable
    appendU32(pe, 0);           // NumberOfSymbols
    appendU16(pe, 0xF0);        // SizeOfOptionalHeader
    appendU16(pe, 0x0023);      // Characteristics

    // Optional Header
    appendU16(pe, 0x020B);  // PE32+
    appendU8(pe, 14);       // MajorLinker
    appendU8(pe, 0);        // MinorLinker

    appendU32(pe, textSize);              // SizeOfCode
    appendU32(pe, rdataSize + dataSize);  // SizeOfInitializedData
    appendU32(pe, 0);                     // SizeOfUninitializedData
    appendU32(pe, textRva + m_entryRva);  // EntryPoint
    appendU32(pe, textRva);               // BaseOfCode

    appendU64(pe, IMAGE_BASE);
    appendU32(pe, SECT_ALIGN);
    appendU32(pe, FILE_ALIGN);

    appendU16(pe, 6);
    appendU16(pe, 0);  // OS 6.0
    appendU16(pe, 0);
    appendU16(pe, 0);  // Image 0.0
    appendU16(pe, 6);
    appendU16(pe, 0);  // Subsys 6.0

    appendU32(pe, 0);            // Win32Version
    appendU32(pe, imageSize);    // SizeOfImage
    appendU32(pe, headersSize);  // SizeOfHeaders
    appendU32(pe, 0);            // CheckSum
    appendU16(pe, 3);            // Subsystem: Console
    appendU16(pe, 0x0120);       // DllCharacteristics

    appendU64(pe, 0x100000);
    appendU64(pe, 0x1000);  // Stack
    appendU64(pe, 0x100000);
    appendU64(pe, 0x1000);  // Heap

    appendU32(pe, 0);   // LoaderFlags
    appendU32(pe, 16);  // NumberOfRvaAndSizes

    // Data Directories
    for (int i = 0; i < 16; i++) {
      appendU32(pe, 0);
      appendU32(pe, 0);
    }

    size_t optBase = 64 + 4 + 20; 

    // Patch Import Directory (index 1)
    patchU32(pe, optBase + 120, rdataRva + 0x00);  // Import RVA
    patchU32(pe, optBase + 124, 60);               // Import Size

    // Patch IAT Directory (index 12)
    patchU32(pe, optBase + 208, rdataRva + 0x5C);    // IAT RVA
    patchU32(pe, optBase + 212, 48);        // Size = 48

    // Section Headers
    auto appendSection = [&](const char* name, uint32_t vs, uint32_t va,
                             uint32_t rs, uint32_t ro, uint32_t chars) {
      uint8_t n[8] = {0};
      memcpy(n, name, strlen(name));
      for (int i = 0; i < 8; i++) appendU8(pe, n[i]);
      appendU32(pe, vs);
      appendU32(pe, va);
      appendU32(pe, rs);
      appendU32(pe, ro);
      appendU32(pe, 0);
      appendU32(pe, 0);
      appendU16(pe, 0);
      appendU16(pe, 0);
      appendU32(pe, chars);
    };

    appendSection(".text", (uint32_t)m_text.size(), textRva, textSize, headersSize, 0x60000020);
    appendSection(".idata", (uint32_t)m_rdata.size(), rdataRva, rdataSize, headersSize + textSize, 0x40000040);
    appendSection(".data", (uint32_t)m_data.size(), dataRva, dataSize, headersSize + textSize + rdataSize, 0xC0000040);
  }

 public:
  PE64Generator() = default;

  // Set code and data
  void setCode(const std::vector<uint8_t>& code) { m_text = code; }
  void setCode(const x86_64_CodeGen& cg) { m_text = cg.code; }
  void setData(const std::vector<uint8_t>& data) { m_data = data; }
  void setEntryOffset(uint32_t offset) { m_entryRva = offset; }

  // Add import
  void addImport(const std::string& dll, const std::string& func, uint32_t hint = 0) {
    for (auto& d : m_imports) {
      if (d.dllName == dll) {
        d.functions.push_back({func, hint});
        return;
      }
    }
    ImportDll d;
    d.dllName = dll;
    d.functions.push_back({func, hint});
    m_imports.push_back(d);
  }

  // Generate wrapper that calls user code then exits
  void createWrapper() {
    std::vector<uint8_t> wrapper(23);

    // sub rsp, 0x28
    wrapper[0] = 0x48;
    wrapper[1] = 0x83;
    wrapper[2] = 0xEC;
    wrapper[3] = 0x28;
    // call user_code
    wrapper[4] = 0xE8;
    int32_t rel = 23 - 9;  // next instruction - call offset
    *(int32_t*)&wrapper[5] = rel;
    // xor ecx, ecx
    wrapper[9] = 0x31;
    wrapper[10] = 0xC9;
    // mov rax, IAT[2] (ExitProcess)
    wrapper[11] = 0x48;
    wrapper[12] = 0xB8;
    *(uint64_t*)&wrapper[13] = IMAGE_BASE + 0x2000 + 0x10;  // IAT[2]
    // call [rax]
    wrapper[21] = 0xFF;
    wrapper[22] = 0x10;

    // Prepend wrapper to code
    m_text.insert(m_text.begin(), wrapper.begin(), wrapper.end());
    m_entryRva = 0;
  }

  // Save to file
  void save(const std::string& filename) {
    buildRdata();

    uint32_t textSize = alignUp((uint32_t)m_text.size(), FILE_ALIGN);
    uint32_t rdataSize = alignUp((uint32_t)m_rdata.size(), FILE_ALIGN);
    uint32_t dataSize = alignUp((uint32_t)m_data.size(), FILE_ALIGN);
    uint32_t headersSize = alignUp(64 + 4 + 20 + 240 + 3 * 40, FILE_ALIGN);
    uint32_t imageSize = alignUp(0x3000 + dataSize, SECT_ALIGN);

    uint32_t textRva = 0x1000;
    uint32_t rdataRva = 0x2000;
    uint32_t dataRva = 0x3000;

    std::vector<uint8_t> pe;
    writeHeaders(pe, textRva, rdataRva, dataRva,
                 textSize, rdataSize, dataSize,
                 headersSize, imageSize);

    // Pad to headersSize
    while (pe.size() < headersSize) pe.push_back(0);

    // .text section
    size_t textStart = pe.size();
    pe.insert(pe.end(), m_text.begin(), m_text.end());
    while (pe.size() < textStart + textSize) pe.push_back(0);

    // .rdata section
    size_t rdataStart = pe.size();
    pe.insert(pe.end(), m_rdata.begin(), m_rdata.end());
    while (pe.size() < rdataStart + rdataSize) pe.push_back(0);

    // .data section — ВАЖНО: данные должны быть на offset 0x600!
    // Сейчас pe.size() должен быть равен headersSize + textSize + rdataSize
    size_t dataStart = pe.size();
    pe.insert(pe.end(), m_data.begin(), m_data.end());
    while (pe.size() < dataStart + dataSize) pe.push_back(0);

    // Проверяем, что dataStart == headersSize + textSize + rdataSize
    printf("Data section starts at: 0x%zX (expected 0x%X)\n",
           dataStart, headersSize + textSize + rdataSize);

    // Write file
    FILE* f = fopen(filename.c_str(), "wb");
    if (f) {
      fwrite(pe.data(), 1, pe.size(), f);
      fclose(f);
    }
  }

  // Convenience: create minimal working EXE
  static void CreateExecutable(const std::vector<uint8_t>& vmCode,
                               const std::vector<uint8_t>& vmData,
                               const std::string& filename) {
    PE64Generator gen;
    gen.setCode(vmCode);
    gen.setData(vmData);
    gen.addImport("KERNEL32.dll", "GetStdHandle");
    gen.addImport("KERNEL32.dll", "WriteConsoleA");
    gen.addImport("KERNEL32.dll", "ExitProcess");
    gen.addImport("ucrt64.dll", "puts");
    gen.createWrapper();
    gen.save(filename);
  }
};

#endif  // PE64_GEN_HPP