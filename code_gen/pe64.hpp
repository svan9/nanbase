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
  std::unordered_map<std::string, uint64_t> m_iatMap;
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

  void resolveRelocations(x86_64_CodeGen* cg, uint32_t textRva) {
    if (!cg) return;

    constexpr uint32_t wrapperSize = 4;  // 48 83 EC 28

    printf("[resolveRelocations] count = %zu\n", cg->relocations.size());

    for (auto& reloc : cg->relocations) {
      printf("[reloc] name=%s code_offset=0x%X\n",
             reloc.name.c_str(),
             reloc.code_offset);

      auto it = m_iatMap.find(reloc.name);
      if (it == m_iatMap.end()) {
        printf("  ERROR: IAT RVA not found for '%s'\n", reloc.name.c_str());

        printf("  Available IAT names:\n");
        for (auto& kv : m_iatMap) {
          printf("    %s -> RVA 0x%llX\n",
                 kv.first.c_str(),
                 (unsigned long long)kv.second);
        }

        continue;
      }

      uint64_t iatRva = it->second;
      uint64_t iatVa = IMAGE_BASE + iatRva;

      /*
          ВАЖНО:

          reloc.code_offset должен указывать на disp32,
          то есть на 4 нулевых байта после FF 15.

          Если инструкция:

              FF 15 00 00 00 00

          и FF находится на offset X,
          то disp32 находится на offset X + 2.
      */

      uint32_t dispOff = wrapperSize + reloc.code_offset;

      if (dispOff + 4 > m_text.size()) {
        printf("  ERROR: dispOff out of range: 0x%X, m_text.size=0x%zX\n",
               dispOff,
               m_text.size());
        continue;
      }

      uint64_t rip = IMAGE_BASE + textRva + dispOff + 4;

      int64_t diff = (int64_t)iatVa - (int64_t)rip;

      if (diff < INT32_MIN || diff > INT32_MAX) {
        printf("  ERROR: RIP displacement out of int32 range\n");
        continue;
      }

      int32_t disp = (int32_t)diff;

      printf("  IAT RVA = 0x%llX\n", (unsigned long long)iatRva);
      printf("  IAT VA  = 0x%llX\n", (unsigned long long)iatVa);
      printf("  RIP     = 0x%llX\n", (unsigned long long)rip);
      printf("  DISP    = 0x%X (%d)\n", (uint32_t)disp, disp);
      printf("  patch m_text[0x%X]\n", dispOff);

      patchU32(m_text, dispOff, (uint32_t)disp);
    }
  }

  uint32_t numDlls = 0;
  uint32_t iatSize = 0;
  uint32_t idtOff = 0;
  uint32_t iatOff = 0;

  void buildRdata(uint32_t rdataRva) {
    m_iatMap.clear();
    iatSize = 0;
    numDlls = 0;

    if (m_imports.empty()) {
      m_rdata.assign(512, 0);
      return;
    }

    numDlls = (uint32_t)m_imports.size();

    // СЧИТАЕМ РАЗМЕРЫ
    for (auto& dll : m_imports) {
      iatSize += ((uint32_t)dll.functions.size() + 1) * 8;
    }

    uint32_t idtSize = (numDlls + 1) * 20;

    uint32_t intSize = 0;
    for (auto& dll : m_imports) {
      intSize += ((uint32_t)dll.functions.size() + 1) * 8;
    }

    uint32_t hintNameSize = 0;
    for (auto& dll : m_imports) {
      for (auto& fn : dll.functions) {
        hintNameSize += 2 + (uint32_t)fn.name.size() + 1;
      }
    }

    uint32_t dllNamesSize = 0;
    for (auto& dll : m_imports) {
      dllNamesSize += (uint32_t)dll.dllName.size() + 1;
    }

    uint32_t totalSize = iatSize + idtSize + intSize + hintNameSize + dllNamesSize;
    m_rdata.assign(totalSize, 0);

    // ===== НОВЫЙ ПОРЯДОК: IDT → INT → IAT → Hint → DLL (как в рабочем!) =====
    idtOff = 0;                                // IDT в начале
    uint32_t intOff = idtOff + idtSize;        // INT после IDT
    iatOff = intOff + intSize;                 // IAT после INT
    uint32_t hintOff = iatOff + iatSize;       // Hint после IAT
    uint32_t dllOff = hintOff + hintNameSize;  // DLL после Hint

    printf("[RDATA] Offsets: IDT=0x%X INT=0x%X IAT=0x%X Hint=0x%X DLL=0x%X\n",
           idtOff, intOff, iatOff, hintOff, dllOff);

    uint32_t currentIdt = idtOff;
    uint32_t currentInt = intOff;
    uint32_t currentIat = iatOff;
    uint32_t currentHint = hintOff;
    uint32_t currentDll = dllOff;

    for (uint32_t di = 0; di < numDlls; di++) {
      auto& dll = m_imports[di];
      uint32_t funcCount = (uint32_t)dll.functions.size();

      // IDT запись
      patchU32(m_rdata, currentIdt + 0, rdataRva + currentInt);  // OriginalFirstThunk → INT
      patchU32(m_rdata, currentIdt + 4, 0);
      patchU32(m_rdata, currentIdt + 8, 0);
      patchU32(m_rdata, currentIdt + 12, rdataRva + currentDll);  // Name → DLL
      patchU32(m_rdata, currentIdt + 16, rdataRva + currentIat);  // FirstThunk → IAT
      currentIdt += 20;

      for (uint32_t fi = 0; fi < funcCount; fi++) {
        auto& fn = dll.functions[fi];

        // Hint/Name
        m_rdata[currentHint + 0] = fn.hint & 0xFF;
        m_rdata[currentHint + 1] = (fn.hint >> 8) & 0xFF;
        memcpy(&m_rdata[currentHint + 2], fn.name.c_str(), fn.name.size() + 1);

        uint64_t hintNameRva = rdataRva + currentHint;

        patchU64(m_rdata, currentInt, hintNameRva);  // INT / ILT
        patchU64(m_rdata, currentIat, hintNameRva);  // IAT initial value

        m_iatMap[fn.name] = rdataRva + currentIat;

        currentIat += 8;
        currentInt += 8;
        currentHint += 2 + (uint32_t)fn.name.size() + 1;
      }

      // Терминаторы
      patchU64(m_rdata, currentIat, 0);
      patchU64(m_rdata, currentInt, 0);
      currentIat += 8;
      currentInt += 8;

      // DLL name
      memcpy(&m_rdata[currentDll], dll.dllName.c_str(), dll.dllName.size() + 1);
      currentDll += (uint32_t)dll.dllName.size() + 1;
    }
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

    // Patch Import Directory (index 1)
    size_t optBase = 64 + 4 + 20;
    if (numDlls > 0) {
      uint32_t idtSize = (numDlls + 1) * 20;

      patchU32(pe, optBase + 120, rdataRva + idtOff);
      patchU32(pe, optBase + 124, idtSize);

      patchU32(pe, optBase + 208, rdataRva + iatOff);
      patchU32(pe, optBase + 212, iatSize);
    }

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

    appendSection(".text",
                  (uint32_t)m_text.size(),
                  textRva,
                  textSize,
                  headersSize,
                  IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
    appendSection(".idata",
                  (uint32_t)m_rdata.size(),
                  rdataRva,
                  rdataSize,
                  headersSize + textSize,
                  IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE);
    appendSection(".data",
                  (uint32_t)m_data.size(),
                  dataRva,
                  dataSize,
                  headersSize + textSize + rdataSize,
                  IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE);
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
    std::vector<uint8_t> wrapper(4);

    wrapper[0] = 0x48;
    wrapper[1] = 0x83;
    wrapper[2] = 0xEC;
    wrapper[3] = 0x28;

    m_text.insert(m_text.begin(), wrapper.begin(), wrapper.end());
    m_entryRva = 0;
  }

  // Save to file
  void save(const std::string& filename, x86_64_CodeGen* cg = nullptr) {
    constexpr uint32_t NUM_SECTIONS = 3;

    uint32_t headersSize = alignUp(
        64 + 4 + 20 + 240 + NUM_SECTIONS * 40,
        FILE_ALIGN);

    // Если передан codegen — сначала берём его код
    if (cg) {
      setCode(*cg);
    }

    // Добавляем wrapper ДО расчёта размеров и ДО relocation patch
    createWrapper();

    uint32_t textRva = alignUp(headersSize, SECT_ALIGN);

    uint32_t textVirtualSize = (uint32_t)m_text.size();

    uint32_t rdataRva = alignUp(textRva + textVirtualSize, SECT_ALIGN);

    // buildRdata должен быть ДО resolveRelocations,
    // потому что он заполняет m_iatMap
    buildRdata(rdataRva);

    uint32_t rdataVirtualSize = (uint32_t)m_rdata.size();

    uint32_t dataRva = alignUp(rdataRva + rdataVirtualSize, SECT_ALIGN);

    uint32_t dataVirtualSize = (uint32_t)m_data.size();

    uint32_t textSize = alignUp(textVirtualSize, FILE_ALIGN);
    uint32_t rdataSize = alignUp(rdataVirtualSize, FILE_ALIGN);
    uint32_t dataSize = alignUp(dataVirtualSize, FILE_ALIGN);

    uint32_t imageSize = alignUp(dataRva + dataVirtualSize, SECT_ALIGN);

    // Теперь патчим уже финальный m_text
    if (cg) {
      resolveRelocations(cg, textRva);
    }

    std::vector<uint8_t> pe;

    writeHeaders(
        pe,
        textRva,
        rdataRva,
        dataRva,
        textSize,
        rdataSize,
        dataSize,
        headersSize,
        imageSize);

    while (pe.size() < headersSize) pe.push_back(0);

    size_t textStart = pe.size();
    pe.insert(pe.end(), m_text.begin(), m_text.end());
    while (pe.size() < textStart + textSize) pe.push_back(0);

    size_t rdataStart = pe.size();
    pe.insert(pe.end(), m_rdata.begin(), m_rdata.end());
    while (pe.size() < rdataStart + rdataSize) pe.push_back(0);

    size_t dataStart = pe.size();
    pe.insert(pe.end(), m_data.begin(), m_data.end());
    while (pe.size() < dataStart + dataSize) pe.push_back(0);

    printf(".text  RVA=0x%X RAW=0x%zX VSIZE=0x%X RSIZE=0x%X\n",
           textRva, textStart, textVirtualSize, textSize);
    printf(".idata RVA=0x%X RAW=0x%zX VSIZE=0x%X RSIZE=0x%X\n",
           rdataRva, rdataStart, rdataVirtualSize, rdataSize);
    printf(".data  RVA=0x%X RAW=0x%zX VSIZE=0x%X RSIZE=0x%X\n",
           dataRva, dataStart, dataVirtualSize, dataSize);

    FILE* f = fopen(filename.c_str(), "wb");
    if (f) {
      fwrite(pe.data(), 1, pe.size(), f);
      fclose(f);
    }
  }

  // Convenience: create minimal working EXE
  static void CreateExecutable(const std::vector<uint8_t>& vmCode,
                               const std::vector<uint8_t>& vmData,
                               const std::string& filename,
                               x86_64_CodeGen* cg = nullptr) {
    PE64Generator gen;
    if (cg) {
      gen.setCode(cg->code);
    }
    else {
      gen.setCode(vmCode);
    }
    gen.setData(vmData);
    gen.addImport("KERNEL32.DLL", "ExitProcess");
    gen.addImport("msvcrt.dll", "puts");
    gen.save(filename, cg);
  }
};

#endif  // PE64_GEN_HPP