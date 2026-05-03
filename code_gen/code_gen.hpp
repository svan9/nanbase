#ifndef CODE_GEN_HPP
#define CODE_GEN_HPP

#include <set>

#include "asm.hpp"
#include "exut.hpp"
#include "virtual.hpp"
#include "x86_64.hpp"

namespace Virtual {

// Mapping from Virtual registers to x86-64 registers
struct RegMapping {
  // Virtual register indices (0-4) mapped to x86-64 registers
  static uint8_t R(int idx) {
    static const uint8_t map[] = {
        Reg::RBX,  // _r[0]
        Reg::R12,  // _r[1]
        Reg::R13,  // _r[2]
        Reg::R14,  // _r[3]
        Reg::R15   // _r[4]
    };
    return map[idx];
  }

  static uint8_t RX(int idx) {
    static const uint8_t map[] = {
        Reg::R8,   // _rx[0]
        Reg::R9,   // _rx[1]
        Reg::R10,  // _rx[2]
        Reg::R11,  // _rx[3]
        Reg::RAX   // _rx[4] - used as scratch
    };
    return map[idx];
  }

  static uint8_t FX(int idx) {
    // x87 FPU or XMM registers
    // For simplicity, use general purpose registers for now
    static const uint8_t map[] = {
        Reg::RBX, Reg::R12, Reg::R13, Reg::R14, Reg::R15};
    return map[idx];
  }

  static uint8_t DX(int idx) {
    static const uint8_t map[] = {
        Reg::R8, Reg::R9, Reg::R10, Reg::R11, Reg::RAX};
    return map[idx];
  }
};

class NativeCompiler {
 private:
  x86_64_CodeGen cg;
  Code* src;
  std::unordered_map<u64, std::string> jump_labels;
  std::set<u64> all_positions;  // Все позиции инструкций
  u64 current_offset = 0;

  std::string make_label(u64 offset) {
    return "L_" + std::to_string(offset);
  }

  u64 puts = 0;  // Адрес функции puts (для печати строк)

  u64 find_func(const char* lib_name, const char* func_name) {
#ifdef PLATFORM_WINDOWS
    HMODULE hLib = LoadLibraryA(lib_name);
    if (!hLib) {
      printf("Failed to load library: %s\n", lib_name);
      return 0;
    }
    FARPROC proc = GetProcAddress(hLib, func_name);
    if (!proc) {
      printf("Failed to find function: %s in %s\n", func_name, lib_name);
      return 0;
    }
    return (u64)proc;
#else
#error "find_func not implemented for this platform"
#endif
  }

  void scan_dll_functions() {
    puts = find_func("msvcrt.dll", "puts");
    if (puts) {
      printf("[INFO] Found puts at 0x%llX\n", puts);
    } else {
      printf("[WARN] Failed to find puts function\n");
    }
  }

  // Сканируем ВСЕ позиции инструкций
  void scan_all_positions() {
    byte* vm_code = (byte*)src->playground;
    byte* end = vm_code + src->capacity;
    byte* ptr = vm_code;

    printf("[DEBUG] Scanning all positions in code of size %llu\n", src->capacity);

    while (ptr < end) {
      u64 pos = ptr - vm_code;
      all_positions.insert(pos);

      Instruction inst = (Instruction)*ptr++;

      size_t inst_size = get_instruction_size(inst, ptr - 1);
      ptr += inst_size;
    }

    printf("[DEBUG] Found %zu instruction positions\n", all_positions.size());
  }

  // Сканируем только целевые адреса прыжков
  void scan_jump_targets() {
    byte* vm_code = (byte*)src->playground;
    byte* end = vm_code + src->capacity;
    byte* ptr = vm_code;

    printf("[DEBUG] Scanning jump targets\n");

    while (ptr < end) {
      u64 pos = ptr - vm_code;
      Instruction inst = (Instruction)*ptr++;

      switch (inst) {
        case Instruction_JMP:
        case Instruction_CALL: {
          u64 target;
          memcpy(&target, ptr, sizeof(u64));
          printf("[DEBUG] Found JMP/CALL at 0x%llX to 0x%llX\n", pos, target);
          jump_labels[target] = make_label(target);
          all_positions.insert(target);  // Убеждаемся, что цель тоже в списке
          ptr += sizeof(u64);
          break;
        }
        case Instruction_JE:
        case Instruction_JNE:
        case Instruction_JL:
        case Instruction_JM:
        case Instruction_JEL:
        case Instruction_JEM: {
          u64 target;
          memcpy(&target, ptr, sizeof(u64));
          printf("[DEBUG] Found Jcc at 0x%llX to 0x%llX\n", pos, target);
          jump_labels[target] = make_label(target);
          all_positions.insert(target);
          ptr += sizeof(u64);
          break;
        }
        default: {
          size_t inst_size = get_instruction_size(inst, ptr - 1);
          ptr += inst_size;
          break;
        }
      }
    }
  }

  size_t get_instruction_size(Instruction inst, byte* ptr) {
    switch (inst) {
      case Instruction_NONE:
        return 0;
      case Instruction_PUSH:
        return get_push_size(ptr);
      case Instruction_POP:
        return 1;
      case Instruction_RPOP:
        return 1 + get_arg_size(ptr[1]);
      case Instruction_ADD:
      case Instruction_SUB:
      case Instruction_MUL:
      case Instruction_DIV:
      case Instruction_XOR:
      case Instruction_OR:
      case Instruction_AND:
      case Instruction_LS:
      case Instruction_RS:
      case Instruction_MOV:
      case Instruction_SWAP:
      case Instruction_TEST: {
        size_t sz = 1;  // сам opcode
        byte type1 = ptr[1];
        sz += get_arg_size(type1);
        if (ptr + sz < ptr + 100) {  // защита от выхода за границы
          byte type2 = ptr[1 + get_arg_size(type1)];
          sz += get_arg_size(type2);
        }
        return sz;
      }
      case Instruction_PUTS:
        return 1 + 7;
      case Instruction_INC:
      case Instruction_DEC:
      case Instruction_NOT:
      case Instruction_PUTC:
      case Instruction_PUTI:
      case Instruction_GETCH:
      case Instruction_GBCH:
      case Instruction_SRDI:
      case Instruction_ARDI:
        return 1 + get_arg_size(ptr[1]);
      case Instruction_JMP:
      case Instruction_CALL:
      case Instruction_JE:
      case Instruction_JNE:
      case Instruction_JL:
      case Instruction_JM:
      case Instruction_JEL:
      case Instruction_JEM:
        return 8;  // opcode + u64 address
      case Instruction_RET:
      case Instruction_EXIT:
        return 1;
      case Instruction_MSET:
        return 1 + 24;  // opcode + 3*u64
      case Instruction_OPEN:
      case Instruction_CLOSE:
      case Instruction_WINE:
      case Instruction_WRITE:
      case Instruction_READ:
      case Instruction_WTM:
      case Instruction_DCALL:
        return 1 + 8;
      case Instruction_LM:
        return 1 + get_arg_size(ptr[1]) + get_arg_size(ptr[2 + get_arg_size(ptr[1])]);
      default:
        printf("[WARN] Unknown instruction 0x%X at %p\n", inst, ptr);
        return 0;
    }
  }

  size_t get_arg_size(byte type) {
    switch (type) {
      case Instruction_ST:
        return 4;  // u32 offset
      case Instruction_NUM:
      case Instruction_FLT:
        return 4;  // u32/s32/float
      case Instruction_REG:
        return 2;  // type + idx
      case Instruction_MEM:
        return 12;  // offset(8) + size(4)
      case Instruction_BYTE:
        return 1;
      case Instruction_STRUCT:
        return 4;  // size prefix
      default:
        return 0;
    }
  }

  size_t get_push_size(byte* ptr) {
    byte type = ptr[1];
    switch (type) {
      case Instruction_NUM:
      case Instruction_FLT:
        return 1 + 4;  // opcode + type + 4 bytes
      case Instruction_BYTE:
        return 1 + 1;
      case Instruction_REG:
        return 1 + 2;
      case Instruction_MEM:
        return 5;
      case Instruction_ST:
        return 5;  // PUSH ST <offset>
      case Instruction_STRUCT:
        return 1 + 4 + 1;  // PUSH STRUCT <size>
      default:
        return 2;
    }
  }

 public:
  NativeCompiler() {}

  void do_compile(x86_64_CodeGen& gen) {
    gen.label("_native_entry");

    // Пролог
    gen.push_r64(Reg::RBP);
    gen.mov_r64_r64(Reg::RBP, Reg::RSP);
    gen.push_r64(Reg::RBX);
    gen.push_r64(Reg::R12);
    gen.push_r64(Reg::R13);
    gen.push_r64(Reg::R14);
    gen.push_r64(Reg::R15);
    gen.sub_r64_imm32(Reg::RSP, 8);  // alignment

    // Сохраняем VM*
#ifdef PLATFORM_WINDOWS
    gen.mov_r64_r64(Reg::RBX, Reg::RCX);
#else
    gen.mov_r64_r64(Reg::RBX, Reg::RDI);
#endif

    byte* vm_code = (byte*)src->playground;
    byte* ptr = vm_code;
    byte* end = vm_code + src->capacity;

    printf("[COMPILE] Processing %lld bytes of VM code\n", (long long)(end - ptr));

    // Пропускаем начальный JMP если есть
    if (ptr < end) {
      Instruction first = (Instruction)*ptr;
      printf("[COMPILE] First byte: 0x%02X\n", (int)first);

      if (first == Instruction_JMP) {
        u64 target;
        memcpy(&target, ptr + 1, 8);
        printf("[COMPILE] JMP to 0x%llX\n", target);
        if (target < src->capacity) {
          ptr = vm_code + target;
        }
      }
    }

    int count = 0;
    while (ptr < end) {
      u64 pos = ptr - vm_code;
      gen.label(make_label(pos));

      Instruction inst = (Instruction)*ptr++;
      size_t op_size = get_instruction_size(inst, ptr - 1);

      printf("[%04llX] Inst=%d size=%zu\n", pos, (int)inst, op_size);

      compile_instruction_gen(gen, inst, ptr);

      ptr += op_size;
      count++;
    }

    printf("[COMPILE] Total instructions: %d\n", count);

    gen.label("_native_exit");

    // Возвращаем результат через RAX (по соглашению x64)
    // Если нужно вернуть значение из VM, загрузите его из R0
    auto last = *(gen.code.end() - 1);
    if (last != 0xC3 && last != 0xC9) {
      gen.mov_r64_imm64(Reg::RAX, 0);  // exit code

      gen.add_r64_imm32(Reg::RSP, 8);
      gen.pop_r64(Reg::R15);
      gen.pop_r64(Reg::R14);
      gen.pop_r64(Reg::R13);
      gen.pop_r64(Reg::R12);
      gen.pop_r64(Reg::RBX);
    }
    gen.leave();
    gen.ret();
  }

  void compile_instruction_gen(x86_64_CodeGen& gen, Instruction inst, byte* ops) {
    switch (inst) {
      case Instruction_JMP: {
        u64 target;
        memcpy(&target, ops, 8);
        gen.jmp(make_label(target));
      } break;
      case Instruction_CALL: {
        u64 target;
        memcpy(&target, ops, 8);
        gen.call(make_label(target));
      } break;
      case Instruction_JE: {
        u64 t;
        memcpy(&t, ops, 8);
        gen.je(make_label(t));
      } break;
      case Instruction_JNE: {
        u64 t;
        memcpy(&t, ops, 8);
        gen.jne(make_label(t));
      } break;
      case Instruction_JL: {
        u64 t;
        memcpy(&t, ops, 8);
        gen.jl(make_label(t));
      } break;
      case Instruction_JM: {
        u64 t;
        memcpy(&t, ops, 8);
        gen.jg(make_label(t));
      } break;
      case Instruction_JEL: {
        u64 t;
        memcpy(&t, ops, 8);
        gen.jle(make_label(t));
      } break;
      case Instruction_JEM: {
        u64 t;
        memcpy(&t, ops, 8);
        gen.jge(make_label(t));
      } break;
      case Instruction_RET:
        gen.pop_r64(Reg::RAX);
        gen.ret();
        break;
      case Instruction_EXIT:
        gen.leave();
        break;
      case Instruction_ADD:
        gen.pop_r64(Reg::RCX);
        gen.pop_r64(Reg::RAX);
        gen.add_r64_r64(Reg::RAX, Reg::RCX);
        gen.push_r64(Reg::RAX);
        break;
      case Instruction_SUB:
        gen.pop_r64(Reg::RCX);
        gen.pop_r64(Reg::RAX);
        gen.sub_r64_r64(Reg::RAX, Reg::RCX);
        gen.push_r64(Reg::RAX);
        break;
      case Instruction_TEST:
        gen.pop_r64(Reg::RCX);
        gen.pop_r64(Reg::RAX);
        gen.cmp_r64_r64(Reg::RAX, Reg::RCX);
        break;
      case Instruction_POP:
        gen.add_r64_imm32(Reg::RSP, 8);
        break;
      case Instruction_PUSH:
        compile_push_gen(gen, ops);
        break;
      case Instruction_RPOP:
        compile_rpop_gen(gen, ops);
        break;
        // В compile_instruction_gen или вашем коде VM:

      case Instruction_PUTS: {
        u64 data_offset = 0;
        memcpy(&data_offset, ops, sizeof(u64));

        // Загружаем адрес строки в RCX (первый аргумент puts)
        gen.mov_r64_imm64(Reg::RCX, 0x140003000ULL + data_offset);

        // Резервируем shadow space (32 байта для Windows x64)
        gen.sub_r64_imm32(Reg::RSP, 32);

        // Вызываем puts через IAT[3] = 0x140002018
        gen.mov_r64_imm64(Reg::RAX, 0x14000206C);
        gen.call_mem64(Reg::RAX);

        // Восстанавливаем стек
        gen.add_r64_imm32(Reg::RSP, 32);
      } break;

      default:
        printf("[WARN] Unknown instruction: %d (0x%02X)\n", (int)inst, (int)inst);
        gen.nop();
        break;
    }
  }

  void compile_push_gen(x86_64_CodeGen& gen, byte* ops) {
    byte type = *ops++;
    switch (type) {
      case Instruction_NUM: {
        u32 val;
        memcpy(&val, ops, 4);
        gen.pushi32(static_cast<int32_t>(val));
      } break;

      case Instruction_BYTE: {
        u8 val = *ops;
        gen.pushi32(static_cast<int32_t>(val));
      } break;

      case Instruction_REG: {
        byte rtype = *ops++;
        byte idx = *ops;
        switch ((VM_RegType)rtype) {
          case VM_RegType::R: {
            // R регистры — 32-bit, лежат в памяти VirtualMachine._r[idx].data
            // RBX = &vm — загружаем из структуры
            // offsetof(_r[idx]) = offsetof(VirtualMachine, _r) + idx * 4
            // Но проще: держим vm* в RBX, загружаем через смещение
            int32_t off = (int32_t)(offsetof(VirtualMachine, _r) + idx * sizeof(VM_Register<4>));
            gen.mov_r64_mem(Reg::RAX, Reg::RBX, off);
            gen.push_r64(Reg::RAX);
          } break;
          case VM_RegType::RX: {
            int32_t off = (int32_t)(offsetof(VirtualMachine, _rx) + idx * sizeof(VM_Register<8>));
            gen.mov_r64_mem(Reg::RAX, Reg::RBX, off);
            gen.push_r64(Reg::RAX);
          } break;
          case VM_RegType::FX: {
            int32_t off = (int32_t)(offsetof(VirtualMachine, _fx) + idx * sizeof(VM_Register<4>));
            gen.mov_r64_mem(Reg::RAX, Reg::RBX, off);
            gen.push_r64(Reg::RAX);
          } break;
          case VM_RegType::DX: {
            int32_t off = (int32_t)(offsetof(VirtualMachine, _dx) + idx * sizeof(VM_Register<8>));
            gen.mov_r64_mem(Reg::RAX, Reg::RBX, off);
            gen.push_r64(Reg::RAX);
          } break;
          case VM_RegType::RDI: {
            int32_t off = (int32_t)offsetof(VirtualMachine, rdi);
            gen.mov_r64_mem(Reg::RAX, Reg::RBX, off);
            gen.push_r64(Reg::RAX);
          } break;
          default:
            gen.pushi32(0);
            break;
        }
      } break;

      case Instruction_MEM: {
        // MEM <offset:u64> <size:u64>
        // heap + offset → push value
        u64 offset = 0;
        memcpy(&offset, ops, sizeof(u64));
        // загружаем vm->heap
        int32_t heap_off = (int32_t)offsetof(VirtualMachine, heap);
        gen.mov_r64_mem(Reg::RAX, Reg::RBX, heap_off);  // rax = vm->heap
        gen.mov_r64_imm64(Reg::RCX, offset);
        gen.add_r64_r64(Reg::RAX, Reg::RCX);     // rax = heap + offset
        gen.mov_r64_mem(Reg::RAX, Reg::RAX, 0);  // rax = *rax
        gen.push_r64(Reg::RAX);
      } break;

      default:
        gen.pushi32(0);
        break;
    }
  }

  void compile_rpop_gen(x86_64_CodeGen& gen, byte* ops) {
    byte type = *ops++;
    if (type != Instruction_REG) {
      // просто выбрасываем верхний элемент
      gen.pop_r64(Reg::RAX);
      return;
    }

    byte rtype = *ops++;
    byte idx = *ops;

    // pop значение со стека в rax
    gen.pop_r64(Reg::RAX);

    switch ((VM_RegType)rtype) {
      case VM_RegType::R: {
        int32_t off = (int32_t)(offsetof(VirtualMachine, _r) + idx * sizeof(VM_Register<4>));
        gen.mov_mem_r64(Reg::RBX, off, Reg::RAX);
      } break;
      case VM_RegType::RX: {
        int32_t off = (int32_t)(offsetof(VirtualMachine, _rx) + idx * sizeof(VM_Register<8>));
        gen.mov_mem_r64(Reg::RBX, off, Reg::RAX);
      } break;
      case VM_RegType::FX: {
        int32_t off = (int32_t)(offsetof(VirtualMachine, _fx) + idx * sizeof(VM_Register<4>));
        gen.mov_mem_r64(Reg::RBX, off, Reg::RAX);
      } break;
      case VM_RegType::DX: {
        int32_t off = (int32_t)(offsetof(VirtualMachine, _dx) + idx * sizeof(VM_Register<8>));
        gen.mov_mem_r64(Reg::RBX, off, Reg::RAX);
      } break;
      case VM_RegType::RDI: {
        int32_t off = (int32_t)offsetof(VirtualMachine, rdi);
        gen.mov_mem_r64(Reg::RBX, off, Reg::RAX);
      } break;
      default:
        break;
    }
  }

  x86_64_CodeGen& compile(Code* code) {
    src = code;
    scan_all_positions();
    scan_jump_targets();
    scan_dll_functions();

    x86_64_CodeGen pass1;
    do_compile(pass1);

    cg = std::move(pass1);
    cg.emit_data_bytes(src->data, src->data_size);
    cg.finalize();
    return cg;
  }

  void compile_instruction(Instruction inst, byte* ops) {
    switch (inst) {
      case Instruction_NONE:
        cg.nop();
        break;
      case Instruction_PUSH:
        compile_push(ops);
        break;
      case Instruction_POP:
        // POP value - just adjust stack
        cg.add_r64_imm32(Reg::RSP, 8);
        break;
      case Instruction_RPOP:
        compile_rpop(ops);
        break;
      case Instruction_ADD:
        compile_add(ops);
        break;
      case Instruction_SUB:
        compile_sub(ops);
        break;
      case Instruction_JMP:
        compile_jmp(ops);
        break;
      case Instruction_CALL:
        compile_call(ops);
        break;
      case Instruction_RET:
        cg.jmp("_native_exit");
        break;
      case Instruction_EXIT:
        cg.jmp("_native_exit");
        break;
      case Instruction_TEST:
        compile_test(ops);
        break;
      case Instruction_JE:
        compile_jcc(ops, [this](const std::string& l) { cg.je(l); });
        break;
      case Instruction_JNE:
        compile_jcc(ops, [this](const std::string& l) { cg.jne(l); });
        break;
      case Instruction_JL:
        compile_jcc(ops, [this](const std::string& l) { cg.jl(l); });
        break;
      case Instruction_JM:
        compile_jcc(ops, [this](const std::string& l) { cg.jg(l); });
        break;
      case Instruction_JEL:
        compile_jcc(ops, [this](const std::string& l) { cg.jle(l); });
        break;
      case Instruction_JEM:
        compile_jcc(ops, [this](const std::string& l) { cg.jge(l); });
        break;
      default:
        // Для неизвестных инструкций - nop
        cg.nop();
        break;
    }
  }

  void compile_push(byte* ops) {
    byte type = *ops++;

    switch (type) {
      case Instruction_NUM: {
        u32 val;
        memcpy(&val, ops, 4);
        cg.pushi32(static_cast<int32_t>(val));
        break;
      }
      case Instruction_REG: {
        byte rtype = *ops++;
        byte idx = *ops;
        uint8_t reg = get_native_reg(rtype, idx);
        cg.push_r64(reg);
        break;
      }
      default:
        // Для других типов - пушим 0
        cg.pushi32(0);
        break;
    }
  }

  void compile_rpop(byte* ops) {
    byte type = *ops++;

    if (type == Instruction_REG) {
      byte rtype = *ops++;
      byte idx = *ops;
      uint8_t reg = get_native_reg(rtype, idx);
      cg.pop_r64(reg);
    }
  }

  void compile_add(byte* ops) {
    // ADD ST, ST (два верхних значения стека)
    cg.pop_r64(Reg::RCX);  // второй операнд
    cg.pop_r64(Reg::RAX);  // первый операнд
    cg.add_r64_r64(Reg::RAX, Reg::RCX);
    cg.push_r64(Reg::RAX);
  }

  void compile_sub(byte* ops) {
    // SUB ST, ST
    cg.pop_r64(Reg::RCX);  // вычитаемое
    cg.pop_r64(Reg::RAX);  // уменьшаемое
    cg.sub_r64_r64(Reg::RAX, Reg::RCX);
    cg.push_r64(Reg::RAX);
  }

  void compile_jmp(byte* ops) {
    u64 target;
    memcpy(&target, ops, 8);
    std::string label = make_label(target);
    printf("[NativeCompiler] JMP to %s (0x%llX)\n", label.c_str(), target);
    cg.jmp(label);
  }

  void compile_call(byte* ops) {
    u64 target;
    memcpy(&target, ops, 8);
    std::string label = make_label(target);
    printf("[NativeCompiler] CALL to %s (0x%llX)\n", label.c_str(), target);
    cg.call(label);
  }

  void compile_test(byte* ops) {
    // Сравниваем два верхних значения
    cg.pop_r64(Reg::RCX);
    cg.pop_r64(Reg::RAX);
    cg.cmp_r64_r64(Reg::RAX, Reg::RCX);
  }

  void compile_jcc(byte* ops, std::function<void(const std::string&)> jcc_op) {
    u64 target;
    memcpy(&target, ops, 8);
    std::string label = make_label(target);
    printf("[NativeCompiler] Jcc to %s (0x%llX)\n", label.c_str(), target);
    jcc_op(label);
  }

  uint8_t get_native_reg(byte vtype, byte idx) {
    switch ((VM_RegType)vtype) {
      case VM_RegType::R:
        return RegMapping::R(idx);
      case VM_RegType::RX:
        return RegMapping::RX(idx);
      case VM_RegType::FX:
        return RegMapping::FX(idx);
      case VM_RegType::DX:
        return RegMapping::DX(idx);
      default:
        return Reg::RAX;
    }
  }
};

inline x86_64_CodeGen CompileToNative(Code* code) {
  NativeCompiler compiler;
  return compiler.compile(code);
}

// Execute native code directly
inline int ExecuteNative(Code& code) {
  NativeCompiler compiler;
  x86_64_CodeGen& cg = compiler.compile(&code);

  // Create VM
  VirtualMachine vm;
  Alloc(vm, code);
  LoadMemory(vm, code);

  // Compile to executable memory
  void* native_code = cg.compile_to_memory();

  // Cast to function pointer and call
  using NativeFunc = int (*)(VirtualMachine*);
  NativeFunc func = reinterpret_cast<NativeFunc>(native_code);

  int result = func(&vm);

  // Cleanup
  x86_64_CodeGen::free_executable_memory(native_code, cg.total_size());

  return result;
}

inline void SaveNativeExecutable(Code& code, const std::string& filename) {
  printf("Saving to: %s\n", filename.c_str());

  NativeCompiler compiler;
  x86_64_CodeGen cg = compiler.compile(&code);

  if (cg.code.empty()) {
    printf("ERROR: No code generated!\n");
    return;
  }

  printf("Code bytes: %zu\n", cg.code.size());
  printf("Code hex: ");
  for (size_t i = 0; i < std::min(cg.code.size(), (size_t)16); i++) {
    printf("%02X ", cg.code[i]);
  }
  printf("\n");

#ifdef PLATFORM_WINDOWS
  create_minimal_pe64(cg, filename);
#else
  create_minimal_elf64(cg, filename);
#endif
}

}  // namespace Virtual

namespace Tests {
namespace Code_ {
using namespace Virtual;
// ============== Тест 1: Простая арифметика ==============
Code* build_arithmetic_test() {
  CodeBuilder cb;

  // Функция: return (10 + 20) * 3 - 5

  // Push 10
  cb << Instruction_PUSH << Instruction_NUM << 10;

  // Push 20
  cb << Instruction_PUSH << Instruction_NUM << 20;

  // ADD: берет два верхних значения со стека
  cb << Instruction_ADD
     << Instruction_ST << 0   // первый операнд (top-0)
     << Instruction_ST << 1;  // второй операнд (top-1)

  // Push 3
  cb << Instruction_PUSH << Instruction_NUM << 3;

  // MUL
  cb << Instruction_MUL
     << Instruction_ST << 0
     << Instruction_ST << 1;

  // Push 5
  cb << Instruction_PUSH << Instruction_NUM << 5;

  // SUB
  cb << Instruction_SUB
     << Instruction_ST << 0
     << Instruction_ST << 1;

  // Сохраняем результат в R0
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  cb << Instruction_EXIT;

  return *cb;
}

// ============== Тест 2: Условные переходы и цикл ==============
Code* build_loop_test() {
  CodeBuilder cb;

  // sum = 0 (R0), i = 1 (R1)
  cb << Instruction_PUSH << Instruction_NUM << 0;
  cb << Instruction_RPOP << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  cb << Instruction_PUSH << Instruction_NUM << 1;
  cb << Instruction_RPOP << Instruction_REG << (byte)VM_RegType::R << (byte)1;

  u64 loop_start = cb.cursor();
  printf("[Builder] loop_start = 0x%llX\n", loop_start);

  // Проверка: i <= 10?
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)1;
  cb << Instruction_PUSH << Instruction_NUM << 10;
  cb << Instruction_TEST << Instruction_ST << 0 << Instruction_ST << 1;

  // Если i > 10, выходим
  cb << Instruction_JM;
  u64 jm_patch_pos = cb.cursor();
  cb.putU64(0);  // placeholder
  printf("[Builder] JM instruction at 0x%llX, arg at 0x%llX\n", jm_patch_pos - 1, jm_patch_pos);

  // sum += i
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)1;
  cb << Instruction_ADD << Instruction_ST << 0 << Instruction_ST << 1;
  cb << Instruction_RPOP << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  // i++
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)1;
  cb << Instruction_PUSH << Instruction_NUM << 1;
  cb << Instruction_ADD << Instruction_ST << 0 << Instruction_ST << 1;
  cb << Instruction_RPOP << Instruction_REG << (byte)VM_RegType::R << (byte)1;

  // Переход в начало
  cb << Instruction_JMP;
  cb.putU64(loop_start);
  printf("[Builder] JMP to 0x%llX\n", loop_start);

  u64 loop_end = cb.cursor();
  printf("[Builder] loop_end = 0x%llX\n", loop_end);

  // Патчим JM на loop_end
  cb.putAtCode(jm_patch_pos, loop_end);
  printf("[Builder] Patched JM at 0x%llX with 0x%llX\n", jm_patch_pos, loop_end);

  // Возвращаем результат
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_EXIT;

  return *cb;
}

// ============== Тест 3: Работа с регистрами ==============
Code* build_register_test() {
  CodeBuilder cb;

  // Тест всех типов регистров

  // R0 = 42
  cb << Instruction_PUSH << Instruction_NUM << 42;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  // RX0 = 0xDEADBEEFCAFEBABE
  cb << Instruction_PUSH << Instruction_NUM << 0xCAFEBABE;
  cb << Instruction_PUSH << Instruction_NUM << 0xDEADBEEF;
  // Для 64-бит нужно специально обработать, упростим

  // FX0 = 3.14 (как int битовое представление)
  float pi = 3.14159f;
  u32 pi_bits;
  memcpy(&pi_bits, &pi, 4);
  cb << Instruction_PUSH << Instruction_NUM << pi_bits;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::FX << (byte)0;

  // Инкремент R0
  cb << Instruction_INC
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  // Декремент R0
  cb << Instruction_DEC
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  cb << Instruction_EXIT;

  return *cb;
}

// ============== Тест 4: Строковые операции ==============
Code* build_string_test() {
  CodeBuilder cb;

  // Вывод строки "Hello, Virtual Machine!\n"

  // Добавляем строку в секцию данных
  const char* msg = "Hello, Virtual Machine!\n";
  cb.AddData((byte*)msg, strlen(msg) + 1);

  // PUTS - вывод строки из данных
  cb << Instruction_PUTS
     << Instruction_MEM
     << 0ULL  // offset от начала данных
     << (u64)strlen(msg);

  cb << Instruction_EXIT;

  return *cb;
}

// ============== Тест 5: Факториал (рекурсия) ==============
Code* build_factorial_test() {
  CodeBuilder cb;

  // Функция factorial(n)
  // Вход: n в R0
  // Выход: n! в R0

  // Метка функции
  u64 func_start = cb.cursor();

  // Проверка: n <= 1?
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_NUM << 1;
  cb << Instruction_TEST
     << Instruction_ST << 0
     << Instruction_ST << 1;

  // Если n <= 1, возвращаем 1
  u64 jle_patch_pos = cb.cursor();
  cb << Instruction_JEL;
  u64 return_one_patch = cb.cursor();
  cb.putU64(0);

  // Иначе: сохраняем n
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  // n = n - 1
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_NUM << 1;
  cb << Instruction_SUB
     << Instruction_ST << 0
     << Instruction_ST << 1;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  // CALL factorial (рекурсия)
  cb << Instruction_CALL;
  cb.putU64(func_start);

  // Восстанавливаем сохраненное n
  cb << Instruction_POP << Instruction_ST << 0;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)1;

  // R0 = R0 * R1 (factorial(n-1) * n)
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)1;
  cb << Instruction_MUL
     << Instruction_ST << 0
     << Instruction_ST << 1;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  cb << Instruction_RET;

  // Метка возврата 1
  u64 return_one_label = cb.cursor();
  cb.putAtCode(return_one_patch, return_one_label);

  cb << Instruction_PUSH << Instruction_NUM << 1;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_RET;

  return *cb;
}

// ============== Тест 6: Битовые операции ==============
Code* build_bitwise_test() {
  CodeBuilder cb;

  // Тест AND, OR, XOR, NOT, сдвиги

  // R0 = 0xFF00
  cb << Instruction_PUSH << Instruction_NUM << 0xFF00;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  // R1 = 0x0FF0
  cb << Instruction_PUSH << Instruction_NUM << 0x0FF0;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)1;

  // R2 = R0 & R1
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)1;
  cb << Instruction_AND
     << Instruction_ST << 0
     << Instruction_ST << 1;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)2;

  // R3 = R0 | R1
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)1;
  cb << Instruction_OR
     << Instruction_ST << 0
     << Instruction_ST << 1;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)3;

  // R4 = R0 ^ R1
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)1;
  cb << Instruction_XOR
     << Instruction_ST << 0
     << Instruction_ST << 1;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)4;

  // Сдвиг влево: R0 << 4
  cb << Instruction_PUSH << Instruction_REG << (byte)VM_RegType::R << (byte)0;
  cb << Instruction_PUSH << Instruction_NUM << 4;
  cb << Instruction_LS
     << Instruction_ST << 0
     << Instruction_ST << 1;
  cb << Instruction_RPOP
     << Instruction_REG << (byte)VM_RegType::R << (byte)0;

  cb << Instruction_EXIT;

  return *cb;
}
}  // namespace Code_

void virtual_compile_test() {
  using namespace Virtual;
  Code* a;
  a = Code_::build_arithmetic_test();
  ExecuteNative(*a);

  a = Code_::build_bitwise_test();
  ExecuteNative(*a);
  // a = Code_::build_factorial_test();
  // ExecuteNative(*a);
  a = Code_::build_loop_test();
  ExecuteNative(*a);
  a = Code_::build_register_test();
  ExecuteNative(*a);
  a = Code_::build_string_test();
  ExecuteNative(*a);
}
}  // namespace Tests

#endif
