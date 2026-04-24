#ifndef _NAN_VIRTUAL_IMPL
#define _NAN_VIRTUAL_IMPL

#include <stack>
#include <stdlib.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#include "mewlib.h"
#include "mewall"
#include "mewmath"
#include "mewpack"
#include "mewtypes.h"
#include "isolate.hpp"
#include "mewallocator"
#include <variant>
#include <chrono>
#include <unordered_map>



#pragma region NOTES
/* // ! IMPORTANT 
  for dll libraries should create 'pipe' 
  `bool pipe_foo(VirtualMachine* vm);`
*/ 
#pragma endregion NOTES

#include <iostream>
#include <string>
#include <memory>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace Virtual {
  using byte = mew::byte;
  	struct VirtualMachine;
	struct Code;
  struct VirtualMachine;
  typedef void(*VM_Processor)(VirtualMachine&, byte*);

  enum Instruction: byte {
    Instruction_NONE = 0,
    Instruction_LDLL,
    Instruction_CALL,
    Instruction_PUSH,
    Instruction_POP,
    Instruction_RPOP,

    Instruction_ADD,
    Instruction_SUB,
    Instruction_MUL,
    Instruction_DIV,

    Instruction_INC,
    Instruction_DEC,

    Instruction_XOR,
    Instruction_OR,
    Instruction_NOT,
    Instruction_AND,
    Instruction_LS, // left shift
    Instruction_RS, // right shift

    Instruction_NUM,  // arg type | number
    Instruction_STRUCT,  // arg type | int
    Instruction_INT,  // arg type | int
    Instruction_FLT,  // arg type | float
    Instruction_DBL,  // arg type | double
    Instruction_UINT, // arg type | uint
    Instruction_BYTE, // arg type | char
    Instruction_MEM,  // arg type | memory
    Instruction_REG,  // arg type | memory
    Instruction_HEAP, // arg type | heap begin
    Instruction_ST,   // arg type | stack top w offset
    
    Instruction_JMP,
    Instruction_RET,
    Instruction_EXIT,
    Instruction_TEST,
    Instruction_JE,
    Instruction_JEL,
    Instruction_JEM,
    Instruction_JNE,
    Instruction_JL,
    Instruction_JM,
    Instruction_MOV,   // replace head data from stack to memory
    Instruction_SWAP,   // replace head data from stack to memory
    Instruction_MSET,

    Instruction_SWST,  // set used stream
    Instruction_WRITE, // write to used stream
    Instruction_READ,  // read used stream
    Instruction_WINE,  // write if not exist
    Instruction_OPEN,  // open file as destinator
    Instruction_CLOSE, // close file as destinator

    Instruction_LM,

    Instruction_PUTC,
    Instruction_PUTI,
    Instruction_PUTS,
    Instruction_GETCH,
    Intruction_GetVM,
    Intruction_GetIPTR,
    Instruction_MOVRDI,
    Instruction_DCALL, // dynamic library function call ~!see notes

    Instruction_GBCH,
    Instruction_SRDI,
    Instruction_ARDI,

    Instruction_WTM,

  };

  #define VIRTUAL_VERSION (Instruction_PUTS*100)+0x57
  #define GrabFromVM(var) memcpy(&var, vm.begin, sizeof(var)); vm.begin += sizeof(var);

  struct VM_MANIFEST_FLAGS {
    bool has_debug: 1 = false;
    byte ch[3];
  }; // must be 4 byte;

  struct FuncExternalLink {
    u8 type: 1;
    const char* lib_name;
    const char* func_name;
  };

  struct CodeManifestExtended {
    VM_MANIFEST_FLAGS flags;
    mew::stack<FuncExternalLink> extern_links;
  };

  struct Code {
    u64 capacity;
    Instruction* playground;
    u64 data_size = 0;
    byte* data = nullptr;
    CodeManifestExtended cme;
  };

#pragma region FILE

  void Code_WriteDebug(std::ofstream& file, FuncExternalLink& link) {
    file << link.type;
    mew::writeString(file, link.lib_name);
    mew::writeString(file, link.func_name);
  }

  // FuncExternalLink Code_ReadDebug(std::ifstream& file) {
  //   FuncExternalLink link;
  //   file.read(&(char)link.type, 1);
  //   mew::readString(file, (char*)link.lib_name);
  //   mew::readString(file, (char*)link.func_name);
  //   return link;
  // }

  void Code_SaveToFile(const Code& code, std::ofstream& file) {
    // version
    u64 version = VIRTUAL_VERSION;
    file.write((char*)&version, sizeof(u64));
    
    // flags
    file.write((char*)&code.cme.flags, sizeof(VM_MANIFEST_FLAGS));
    
    // playground
    file.write((char*)&code.capacity, sizeof(u64));
    file.write((char*)code.playground, code.capacity * sizeof(Instruction));
    
    // data
    file.write((char*)&code.data_size, sizeof(u64));
    if (code.data_size > 0 && code.data != nullptr) {
        file.write((char*)code.data, code.data_size);
    }
  }

  Code* Code_LoadFromFile(std::ifstream& file) {
    // version
    u64 file_version = 0;
    file.read((char*)&file_version, sizeof(u64));
    if (file_version != VIRTUAL_VERSION) {
        MewWarn("file version not support (%i != %i)", file_version, VIRTUAL_VERSION);
        return nullptr;
    }

    Code* code = new Code();

    // flags
    file.read((char*)&code->cme.flags, sizeof(VM_MANIFEST_FLAGS));

    // playground
    file.read((char*)&code->capacity, sizeof(u64));
    code->playground = new Instruction[code->capacity];
    file.read((char*)code->playground, code->capacity * sizeof(Instruction));

    // data
    file.read((char*)&code->data_size, sizeof(u64));
    if (code->data_size > 0) {
        code->data = new byte[code->data_size];
        file.read((char*)code->data, code->data_size);
    }

    return code;
  }

  void Code_SaveToFile(const Code& code, const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::out | std::ios::binary);
    MewAssert(file.is_open());
    file.seekp(std::ios::beg);
    /* version */
    Code_SaveToFile(code, file);
    file.close();
  }

  void Code_SaveToFile(const Code& code, const char* path) {
    std::filesystem::path __path(path);
    if (!__path.is_absolute()) {
      __path = std::filesystem::absolute(__path.lexically_normal());
    }
    Code_SaveToFile(code, __path);
  }

  Code* Code_LoadFromFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    MewAssert(file.is_open());
    file >> std::noskipws;
    Code* code = Code_LoadFromFile(file);
    file.close();
    return code;
  }

  Code* Code_LoadFromFile(const char* path) {
    std::filesystem::path __path(path);
    if (!__path.is_absolute()) {
      __path = std::filesystem::absolute(__path.lexically_normal());
    }
    return Code_LoadFromFile(__path);
  }

#pragma region VM
  enum VM_Status: byte {
    VM_Status_Panding = 0,
    VM_Status_Execute = 1 << 1,
    VM_Status_Ret     = 1 << 2,
    VM_Status_Error   = 1 << 3,
  };
  
  enum VM_TestStatus: byte {
    VM_TestStatus_Skip = 0,
    VM_TestStatus_Equal = 1 << 1,
    VM_TestStatus_Less  = 1 << 2,
    VM_TestStatus_More  = 1 << 3,
    VM_TestStatus_EqualMore  = VM_TestStatus_Equal | VM_TestStatus_More,
    VM_TestStatus_EqualLess  = VM_TestStatus_Equal | VM_TestStatus_Less,
  };
  
  enum VM_flags {
    None = 0,
    HeapLockExecute = 1 << 1,
  };

  template<u64 size>
  struct VM_Register {
    byte data[size];
  };
  
  enum struct VM_RegType: byte {
    None, R, RX, DX, FX, RDI
  };

#ifdef _WIN32
  typedef HMODULE handle_t;
#else
  typedef void* handle_t;
#endif


  typedef u64(*vm_dll_pipe_fn)(VirtualMachine* vm);

  
	//------------------- JIT DEFS -------------------//
  static constexpr int JIT_HOT_THRESHOLD = 5;   // вызовов до компиляции
  static constexpr size_t JIT_CODE_BUF   = 4096; // байт на функцию

  typedef void(*jit_fn_t)(VirtualMachine*);

  struct JIT_FuncEntry {
    int      call_count = 0;       // счётчик вызовов
    jit_fn_t native     = nullptr; // скомпилированный код (nullptr = ещё не скомпилирован)
    void*    exec_mem   = nullptr; // память выделенная VirtualAlloc
  };

  struct JIT_Cache {
    std::unordered_map<u64, JIT_FuncEntry> entries;

    JIT_FuncEntry& get(u64 offset) {
      return entries[offset];
    }

    void free_all() {
      for (auto& [k, v] : entries) {
        if (v.exec_mem) {
          VirtualFree(v.exec_mem, 0, MEM_RELEASE);
          v.exec_mem = nullptr;
          v.native   = nullptr;
        }
      }
      entries.clear();
    }

    ~JIT_Cache() { free_all(); }
  };
  
	inline void JIT_Call(VirtualMachine& vm, u64 fn_offset, JIT_Cache& cache);

  struct VM_DEBUG {
    byte last_head_byte = 0;
    char* last_fn = 0;
  };
  struct VirtualMachine {
    FILE* std_in = stdin;
    FILE* std_out = stdout;
    Isolate fs;
    Code* src;
    VM_DEBUG debug;
    JIT_Cache jit_cache;
    VM_Register<4> _r[5];                       // 4*5(20)
    VM_Register<4> _fx[5];                      // 4*5(20)
    VM_Register<8> _rx[5];                      // 8*5(40)
    VM_Register<8> _dx[5];                      // 8*5(40)
    u64 capacity;                            // 8byte
    FILE *r_stream;                             // 8byte
    byte *memory, *heap,
        *begin, *end;                           // 4x8byte(24byte) 
    struct TestStatus {
      bytepartf(skip)
      bytepartf(equal)
      bytepartf(less)
      bytepartf(more)
    } test;                                     // 1byte
    VM_Status status;                           // 1byte
    struct Flags {
      bytepartt(heap_lock_execute)
      bytepartf(use_debug)
      bytepartf(use_isolate)
      bytepartf(in_neib_ctx)
    } flags;                                    // 1byte
    byte _pad0[1];
    mew::stack<u8, mew::MidAllocator<u8>> stack;                 // 24byte
    u64 rdi = 0;
    mew::stack<byte *, mew::MidAllocator<byte*>> begin_stack;        // 24byte             // 24byte
    mew::stack<Code*> libs;                 // 24byte
    mew::stack<handle_t> dll_handles;
    std::unordered_map<const char*, vm_dll_pipe_fn> dll_pipes;
    u64 process_cycle = 0;

    byte* getRegister(VM_RegType rt, byte idx, u64* size = nullptr) {
      MewCriticalIf(idx >= 5, "undefined register idx (%i)", idx);
      switch (rt) {
        case VM_RegType::R: 
          if (!size) {*size = 4;}
          return this->_r[idx].data;     
        case VM_RegType::RX: 
          if (!size) {*size = 8;}
          return this->_rx[idx].data;
        case VM_RegType::FX: 
          if (!size) {*size = 4;}
          return this->_fx[idx].data;
        case VM_RegType::DX: 
          if (!size) {*size = 8;}
          return this->_dx[idx].data;
        case VM_RegType::RDI:
          return (byte*)&this->rdi;
        default: return nullptr;
      }
    }
  };


  u64 VM_OpenDll(VirtualMachine& vm, const char* name) {
    handle_t handle_;
    #ifdef _WIN32
      handle_ = LoadLibraryA(name);
    #else
      handle_ = dlopen(name, RTLD_LAZY);
    #endif
    MewForUserAssert(handle_ != nullptr, "cant open library (%s)", name);
    return vm.dll_handles.push(handle_);
  }

  void VM_CloseDlls(VirtualMachine& vm) {
    for (int i = 0; i < vm.dll_handles.count(); ++i) {
      auto handle_ = vm.dll_handles.at(i);
#ifdef _WIN32
      FreeLibrary(handle_);
#else
      dlclose(handle_);
#endif
    }
  }

  vm_dll_pipe_fn VM_GetDllPipeFunction(VirtualMachine& vm, u64 dll_idx, const char* name) {
    auto it = vm.dll_pipes.find(name);
    if (it != vm.dll_pipes.end()) {return it->second;}
    MewForUserAssert(vm.dll_handles.has(dll_idx), "cant find library by identifier(%i), maybe library wasnt loaded", dll_idx);
#ifdef _WIN32
    FARPROC proc = GetProcAddress(vm.dll_handles[dll_idx], name);
#else
    void* proc = dlsym(vm.dll_handles[dll_idx], name);
#endif
    if (!proc) {
      MewWarn("cant find function(%s) from library\n", name);
      return nullptr;
    }
    vm_dll_pipe_fn fn = (vm_dll_pipe_fn)(proc);
    vm.dll_pipes.insert({name, fn});
    return fn;
  }

  struct Code_AData {
    u64 size;
  };

  
  void a() {
    sizeof(VirtualMachine);
  }

  #ifndef VM_ALLOC_ALIGN
    #define VM_ALLOC_ALIGN 512
  #endif
  #ifndef VM_MINHEAP_ALIGN
    #define VM_MINHEAP_ALIGN 128
  #endif
  #ifndef VM_CODE_ALIGN
    #define VM_CODE_ALIGN 8
  #endif
  #define __VM_ALIGN(_val, _align) (((int)((_val) / (_align)) + 1) * (_align))

	// defs
	void VM_ManualPush(VirtualMachine& vm, u32 x);
  void VM_Push(VirtualMachine& vm, byte head_byte, u32 number);
  void VM_Push(VirtualMachine& vm);
  void VM_Pop(VirtualMachine& vm);
  void VM_RPop(VirtualMachine& vm);
  void VM_StackTop(VirtualMachine& vm, byte type, u32* x, byte** mem = nullptr);
  void VM_SwitchContext(VirtualMachine& vm, Code* code);
  void VM_Call(VirtualMachine& vm);
  void VM_MathBase(VirtualMachine& vm, u32* x, u32* y, byte** mem = nullptr);
  void VM_MovRDI(VirtualMachine& vm);
  void VM_Add(VirtualMachine& vm);
  void VM_Sub(VirtualMachine& vm);
  void VM_Mul(VirtualMachine& vm);
  void VM_Div(VirtualMachine& vm);
  void VM_Inc(VirtualMachine& vm);
  void VM_Dec(VirtualMachine& vm);
  void VM_Xor(VirtualMachine& vm);
  void VM_Or(VirtualMachine& vm);
  void VM_Not(VirtualMachine& vm);
  void VM_And(VirtualMachine& vm);
  void VM_LS(VirtualMachine& vm);
  void VM_RS(VirtualMachine& vm);
  void VM_ManualJmp(VirtualMachine& vm, u32 offset);
  void VM_Jmp(VirtualMachine& vm);
  void VM_Ret(VirtualMachine& vm);
  void VM_Test(VirtualMachine& vm);
  void VM_JE(VirtualMachine& vm);
  void VM_JEL(VirtualMachine& vm);
  void VM_JEM(VirtualMachine& vm);
  void VM_JL(VirtualMachine& vm);
  void VM_JM(VirtualMachine& vm);
  void VM_JNE(VirtualMachine& vm);
  void VM_Mov(VirtualMachine& vm);
  void VM_Swap(VirtualMachine& vm);
  void VM_MSet(VirtualMachine& vm);
  void VM_Putc(VirtualMachine& vm);
  void VM_Puti(VirtualMachine& vm);
  void VM_Puts(VirtualMachine& vm);
  void VM_Getch(VirtualMachine& vm);
  void VM_LM(VirtualMachine& vm);
  void VM_Open(VirtualMachine& vm);
  void VM_Close(VirtualMachine& vm);
  void VM_Wine(VirtualMachine& vm);
  void VM_Write(VirtualMachine& vm);
  void VM_Read(VirtualMachine& vm);
  void VM_GetIternalPointer(VirtualMachine& vm);
  void VM_GetVM(VirtualMachine& vm);
  void VM_DCALL(VirtualMachine& vm);

  

struct Emitter {
  std::vector<uint8_t> buf;

  // push imm64 в rax, затем push rax
  void push_imm64(uint64_t v) {
    // mov rax, imm64
    buf.push_back(0x48); buf.push_back(0xB8);
    for (int i = 0; i < 8; ++i) buf.push_back((v >> (i*8)) & 0xFF);
    // push rax
    buf.push_back(0x50);
  }

  // call rax (indirect)
  void call_rax() {
    buf.push_back(0xFF); buf.push_back(0xD0);
  }

  // mov rcx, imm64  (первый аргумент на Windows x64)
  void mov_rcx_imm64(uint64_t v) {
    buf.push_back(0x48); buf.push_back(0xB9);
    for (int i = 0; i < 8; ++i) buf.push_back((v >> (i*8)) & 0xFF);
  }

  // mov rax, imm64
  void mov_rax_imm64(uint64_t v) {
    buf.push_back(0x48); buf.push_back(0xB8);
    for (int i = 0; i < 8; ++i) buf.push_back((v >> (i*8)) & 0xFF);
  }

  // sub rsp, 0x28  (shadow space Windows x64)
  void sub_rsp_shadow() {
    buf.push_back(0x48); buf.push_back(0x83);
    buf.push_back(0xEC); buf.push_back(0x28);
  }

  // add rsp, 0x28
  void add_rsp_shadow() {
    buf.push_back(0x48); buf.push_back(0x83);
    buf.push_back(0xC4); buf.push_back(0x28);
  }

  // push rbp / mov rbp, rsp
  void prologue() {
    buf.push_back(0x55);               // push rbp
    buf.push_back(0x48); buf.push_back(0x89); buf.push_back(0xEC); // mov rbp, rsp
    sub_rsp_shadow();
  }

  // add rsp, shadow / pop rbp / ret
  void epilogue() {
    add_rsp_shadow();
    buf.push_back(0x5D); // pop rbp
    buf.push_back(0xC3); // ret
  }

  // Вызов C-функции с одним аргументом (VirtualMachine*)
  // fn_ptr — адрес функции, vm_ptr — адрес vm
  void call_vm_fn(void* fn_ptr, void* vm_ptr) {
    mov_rcx_imm64((uint64_t)vm_ptr);
    mov_rax_imm64((uint64_t)fn_ptr);
    call_rax();
  }
};


static void jit_wrap_push (VirtualMachine* vm) { VM_Push (*vm); }
static void jit_wrap_pop  (VirtualMachine* vm) { VM_Pop  (*vm); }
static void jit_wrap_rpop (VirtualMachine* vm) { VM_RPop (*vm); }
static void jit_wrap_add  (VirtualMachine* vm) { VM_Add  (*vm); }
static void jit_wrap_sub  (VirtualMachine* vm) { VM_Sub  (*vm); }
static void jit_wrap_mul  (VirtualMachine* vm) { VM_Mul  (*vm); }
static void jit_wrap_div  (VirtualMachine* vm) { VM_Div  (*vm); }
static void jit_wrap_inc  (VirtualMachine* vm) { VM_Inc  (*vm); }
static void jit_wrap_dec  (VirtualMachine* vm) { VM_Dec  (*vm); }
static void jit_wrap_xor  (VirtualMachine* vm) { VM_Xor  (*vm); }
static void jit_wrap_or   (VirtualMachine* vm) { VM_Or   (*vm); }
static void jit_wrap_not  (VirtualMachine* vm) { VM_Not  (*vm); }
static void jit_wrap_and  (VirtualMachine* vm) { VM_And  (*vm); }
static void jit_wrap_ls   (VirtualMachine* vm) { VM_LS   (*vm); }
static void jit_wrap_rs   (VirtualMachine* vm) { VM_RS   (*vm); }
static void jit_wrap_test (VirtualMachine* vm) { VM_Test (*vm); }
static void jit_wrap_jmp  (VirtualMachine* vm) { VM_Jmp  (*vm); }
static void jit_wrap_je   (VirtualMachine* vm) { VM_JE   (*vm); }
static void jit_wrap_jne  (VirtualMachine* vm) { VM_JNE  (*vm); }
static void jit_wrap_jl   (VirtualMachine* vm) { VM_JL   (*vm); }
static void jit_wrap_jm   (VirtualMachine* vm) { VM_JM   (*vm); }
static void jit_wrap_jel  (VirtualMachine* vm) { VM_JEL  (*vm); }
static void jit_wrap_jem  (VirtualMachine* vm) { VM_JEM  (*vm); }
static void jit_wrap_mov  (VirtualMachine* vm) { VM_Mov  (*vm); }
static void jit_wrap_swap (VirtualMachine* vm) { VM_Swap (*vm); }
static void jit_wrap_mset (VirtualMachine* vm) { VM_MSet (*vm); }
static void jit_wrap_putc (VirtualMachine* vm) { VM_Putc (*vm); }
static void jit_wrap_puti (VirtualMachine* vm) { VM_Puti (*vm); }
static void jit_wrap_puts (VirtualMachine* vm) { VM_Puts (*vm); }
static void jit_wrap_getch(VirtualMachine* vm) { VM_Getch(*vm); }
static void jit_wrap_ret  (VirtualMachine* vm) { VM_Ret  (*vm); }
static void jit_wrap_call (VirtualMachine* vm) { VM_Call (*vm); }

// Продвигает vm.begin на размер операндов текущей инструкции,
// чтобы компилятор знал где начинается следующая инструкция.
// Возвращает false если не знает размер (сложный аргумент) — тогда компиляция прерывается.
static bool jit_skip_operands(byte instr, byte* bc) {
  // Инструкции без операндов
  switch (instr) {
    case Instruction_POP:
    case Instruction_RET:
    case Instruction_EXIT:
    case Instruction_TEST:
    case Instruction_NONE:
      return true;
    case Instruction_JMP:
    case Instruction_JE:
    case Instruction_JNE:
    case Instruction_JL:
    case Instruction_JM:
    case Instruction_JEL:
    case Instruction_JEM:
    case Instruction_CALL:
      return true; // 8 байт операнд, но сканер не двигает bc сам — просто говорим «стоп»
    default:
      return false; // аргументы переменной длины — не компилируем дальше
  }
}

static int jit_operand_size_one(byte* bc) {
  switch (*bc) {
    case Instruction_REG: return 3;  // REG + type + idx
    case Instruction_NUM: return 5;  // NUM + s32
    case Instruction_ST:  return 5;  // ST + u32
    default: return -1;
  }
}

static int jit_operand_size(byte instr, byte* bc) {
  switch (instr) {
    case Instruction_POP:
    case Instruction_RET:
    case Instruction_EXIT:
    case Instruction_TEST:
    case Instruction_NONE:
      return 0;
    case Instruction_JMP:
    case Instruction_JE: case Instruction_JNE:
    case Instruction_JL: case Instruction_JM:
    case Instruction_JEL: case Instruction_JEM:
      return 8; // u64 offset
    case Instruction_INC:
    case Instruction_DEC:
    case Instruction_NOT:
      // один аргумент — смотрим тип
      if (*bc == Instruction_REG) return 3; // REG + type + idx
      if (*bc == Instruction_NUM) return 5; // NUM + s32
      return -1;
    case Instruction_ADD: case Instruction_SUB:
    case Instruction_MUL: case Instruction_DIV:
    case Instruction_MOV: case Instruction_SWAP:
    case Instruction_XOR: case Instruction_OR:
    case Instruction_AND: case Instruction_LS:
    case Instruction_RS: {
      // два аргумента
      int a = jit_operand_size_one(bc);
      if (a < 0) return -1;
      int b = jit_operand_size_one(bc + a);
      if (b < 0) return -1;
      return a + b;
    }
    default:
      return -1;
  }
}



jit_fn_t JIT_Compile(VirtualMachine& vm, u64 fn_offset, void** out_mem) {
  // printf("JIT_Compile: fn_offset=%llu\n", fn_offset);
  byte* bc     = vm.memory + fn_offset;
  byte* bc_end = vm.end;

  Emitter em;
  em.prologue();

  uint64_t begin_addr = (uint64_t)&vm.begin;

  while (bc < bc_end) {
    byte instr = *bc++;
    // printf("  JIT emit instr=0x%02X(%d) at bc_offset=%llu\n", 
          //  instr, instr, (u64)(bc - 1 - vm.memory));
    void* wrapper = nullptr;
    bool terminal = false; // инструкция завершает блок (ret/exit/jmp)

    switch (instr) {
      case Instruction_PUSH:  wrapper = (void*)jit_wrap_push;  break;
      case Instruction_POP:   wrapper = (void*)jit_wrap_pop;   break;
      case Instruction_RPOP:  wrapper = (void*)jit_wrap_rpop;  break;
      case Instruction_ADD:   wrapper = (void*)jit_wrap_add;   break;
      case Instruction_SUB:   wrapper = (void*)jit_wrap_sub;   break;
      case Instruction_MUL:   wrapper = (void*)jit_wrap_mul;   break;
      case Instruction_DIV:   wrapper = (void*)jit_wrap_div;   break;
      case Instruction_INC:   wrapper = (void*)jit_wrap_inc;   break;
      case Instruction_DEC:   wrapper = (void*)jit_wrap_dec;   break;
      case Instruction_XOR:   wrapper = (void*)jit_wrap_xor;   break;
      case Instruction_OR:    wrapper = (void*)jit_wrap_or;    break;
      case Instruction_NOT:   wrapper = (void*)jit_wrap_not;   break;
      case Instruction_AND:   wrapper = (void*)jit_wrap_and;   break;
      case Instruction_LS:    wrapper = (void*)jit_wrap_ls;    break;
      case Instruction_RS:    wrapper = (void*)jit_wrap_rs;    break;
      case Instruction_TEST:  wrapper = (void*)jit_wrap_test;  break;
      case Instruction_MOV:   wrapper = (void*)jit_wrap_mov;   break;
      case Instruction_SWAP:  wrapper = (void*)jit_wrap_swap;  break;
      case Instruction_MSET:  wrapper = (void*)jit_wrap_mset;  break;
      case Instruction_PUTC:  wrapper = (void*)jit_wrap_putc;  break;
      case Instruction_PUTI:  wrapper = (void*)jit_wrap_puti;  break;
      case Instruction_PUTS:  wrapper = (void*)jit_wrap_puts;  break;
      case Instruction_GETCH: wrapper = (void*)jit_wrap_getch; break;
      // case Instruction_CALL:  wrapper = (void*)jit_wrap_call;  break;

      // Переходы — вызываем обёртку и заканчиваем блок
      case Instruction_JMP:   wrapper = (void*)jit_wrap_jmp;  terminal = true; break;
      case Instruction_JE:    wrapper = (void*)jit_wrap_je;   terminal = true; break;
      case Instruction_JNE:   wrapper = (void*)jit_wrap_jne;  terminal = true; break;
      case Instruction_JL:    wrapper = (void*)jit_wrap_jl;   terminal = true; break;
      case Instruction_JM:    wrapper = (void*)jit_wrap_jm;   terminal = true; break;
      case Instruction_JEL:   wrapper = (void*)jit_wrap_jel;  terminal = true; break;
      case Instruction_JEM:   wrapper = (void*)jit_wrap_jem;  terminal = true; break;

      case Instruction_RET:
        // vm.begin не трогаем — VM_Ret восстановит его из begin_stack
        em.call_vm_fn((void*)jit_wrap_ret, &vm);
        em.epilogue();
        goto done;
        
      // case Instruction_CALL:
      //   wrapper = (void*)jit_wrap_call;
      //   terminal = true;  // ← добавь это
      //   break;
      // case Instruction_RET:
      //   em.mov_rax_imm64(begin_addr);
      //   em.mov_rcx_imm64((uint64_t)bc);
      //   em.buf.push_back(0x48); em.buf.push_back(0x89); em.buf.push_back(0x08);
      //   em.call_vm_fn((void*)jit_wrap_ret, &vm);
      //   em.epilogue();
      //   goto done;

      case Instruction_EXIT: {
        uint64_t status_addr = (uint64_t)&vm.status;
        em.mov_rax_imm64(status_addr);
        // mov byte ptr [rax], VM_Status_Ret
        em.buf.push_back(0xC6); em.buf.push_back(0x00);
        em.buf.push_back((uint8_t)VM_Status_Ret);
        em.epilogue();
        goto done;
      }

      default:
        // Неизвестная инструкция — заканчиваем блок, bc уже продвинут
        // Обновляем vm.begin чтобы интерпретатор продолжил с этой инструкции
        bc--; // вернёмся на неизвестную инструкцию
        em.mov_rax_imm64(begin_addr);
        em.mov_rcx_imm64((uint64_t)bc);
        em.buf.push_back(0x48); em.buf.push_back(0x89); em.buf.push_back(0x08);
        em.epilogue();
        goto done;
    }

    if (wrapper) {
      int op_size = jit_operand_size(instr, bc);
      if (op_size < 0) {
        // неизвестный размер — завершаем блок, интерпретатор возьмёт управление
        bc--; // вернуться на текущую инструкцию
        em.mov_rax_imm64(begin_addr);
        em.mov_rcx_imm64((uint64_t)bc);
        em.buf.push_back(0x48); em.buf.push_back(0x89); em.buf.push_back(0x08);
        em.epilogue();
        goto done;
      }

      // Устанавливаем vm.begin на операнды
      em.mov_rax_imm64(begin_addr);
      em.mov_rcx_imm64((uint64_t)bc);
      em.buf.push_back(0x48); em.buf.push_back(0x89); em.buf.push_back(0x08);
      em.call_vm_fn(wrapper, &vm);

      bc += op_size; // продвигаем bc статически — теперь знаем размер
    }

    if (terminal) {
      em.epilogue();
      goto done;
    }
  }

  em.epilogue();

done:
  if (em.buf.empty()) return nullptr;

  void* exec = VirtualAlloc(nullptr, em.buf.size(),
                            MEM_COMMIT | MEM_RESERVE,
                            PAGE_EXECUTE_READWRITE);
  if (!exec) return nullptr;

  memcpy(exec, em.buf.data(), em.buf.size());
  *out_mem = exec;
  return (jit_fn_t)exec;
}

// ─────────────────────────────────────────────
//  Точка входа: вызвать функцию через JIT или интерпретатор
// ─────────────────────────────────────────────
inline void JIT_Call(VirtualMachine& vm, u64 fn_offset, JIT_Cache& cache) {
  auto& entry = cache.get(fn_offset);
  entry.call_count++;
  // printf("JIT_Call: fn_offset=%llu count=%d native=%p\n",
  //        fn_offset, entry.call_count, (void*)entry.native);

  if (entry.native) {
    // printf("JIT hot call: fn_offset=%llu, begin_before=%llu\n",
    //       fn_offset, (u64)(vm.begin - vm.memory));
    vm.begin_stack.push(vm.begin);
    entry.native(&vm);
    // printf("JIT hot call done: begin_after=%llu\n",
    //       (u64)(vm.begin - vm.memory));
    return;
  }

  if (entry.call_count >= JIT_HOT_THRESHOLD) {
    void* mem = nullptr;
    jit_fn_t fn = JIT_Compile(vm, fn_offset, &mem);
    if (fn) {
      entry.native   = fn;
      entry.exec_mem = mem;
      vm.begin_stack.push(vm.begin);
      entry.native(&vm);
      return;
    }
  }

  // Холодный путь
  vm.begin_stack.push(vm.begin);
  vm.begin = vm.memory + fn_offset;
}


  void Alloc(VirtualMachine& vm) {
    if (vm.memory != nullptr) {
      free(vm.memory);
    }
    vm.memory = mew::mem::alloc(VM_ALLOC_ALIGN);
    memset(vm.memory, Instruction_NONE, VM_ALLOC_ALIGN);
    vm.capacity = VM_ALLOC_ALIGN;
  }

  void Alloc(VirtualMachine& vm, Code& code) {
    u64 size = __VM_ALIGN(code.capacity+code.data_size, VM_ALLOC_ALIGN);
    if ((size - code.capacity - code.data_size) <= 0) {
      size += VM_MINHEAP_ALIGN;
    }
    vm.memory = mew::mem::alloc(size);
    memset(vm.memory, Instruction_NONE, size);
    vm.capacity = size;
  }

  u32 DeclareProccessor(VirtualMachine& vm, VM_Processor proc) {
    MewNotImpl();
    // vm.procs.push_back(proc);
    // return vm.procs.size()-1;
  }

  void LoadMemory(VirtualMachine& vm, Code& code) {
    memcpy(vm.memory, code.playground, code.capacity);
    // // todo load from .nlib file 
    // for (int i = 0; i < code.cme.libs.size(); ++i) {
    //   Code* lib = Code_LoadFromFile(code.cme.libs.at(i));
    //   vm.libs.push(lib);
    // }
  }

  #pragma region VM_ARG 
  typedef struct {
    VM_RegType type;
    byte idx;
  } VM_REG_INFO;

  class VM_ARG {
  public:
    VM_ARG() {}
    byte* data;
    byte* data2;
    u32 size;

    byte type;
    int& getInt() {
      return (int&)(*this->data);
    }
    lli& getLong() {
      return (lli&)(*this->data);
    }
    float& getFloat() {
      return (float&)(*this->data);
    }
    double& getDouble() {
      return (double&)(*this->data);
    }
    byte getByte() {
      return (byte)(*this->data);
    }

    byte* getMem() {
      return this->data;
    }

    static void do_math(VM_ARG& a, mew::asgio fn, bool depr_float = false) { 
      switch (a.type) {
        case Instruction_ST: mew::gen_asgio(fn, a.getInt()); break;
        case Instruction_REG: {
          VM_REG_INFO* ri = (VM_REG_INFO*)a.data2;
          switch (ri->type) {
            case VM_RegType::R: mew::gen_asgio(fn, a.getInt()); break;
            case VM_RegType::RX: mew::gen_asgio(fn, a.getLong()); break;
            case VM_RegType::FX: if (!depr_float) mew::gen_asgio(fn, a.getFloat()); break;
            case VM_RegType::DX: if (!depr_float) mew::gen_asgio(fn, a.getDouble()); break;
            default: MewCritical("undefined reg type (%i)", ri->type); 
          }
          break;
        }
        default: MewCritical("undefined arg type (%i)", a.type);
      } 
    }

    static void do_math(VM_ARG& a, VM_ARG& b, mew::adgio fn, bool depr_float = false) {
      switch (a.type) {
        case Instruction_ST: {
          switch (b.type) {
            case Instruction_ST: mew::gen_adgio(fn, a.getInt(), b.getInt()); break;
            case Instruction_REG: {
              VM_REG_INFO* ri = (VM_REG_INFO*)a.data2;
              switch (ri->type) {
                case VM_RegType::R: mew::gen_adgio(fn, a.getInt(), b.getInt()); break;
                case VM_RegType::RX: mew::gen_adgio(fn, a.getInt(), b.getLong()); break;
                case VM_RegType::FX: if (!depr_float) mew::gen_adgio(fn, a.getInt(), b.getFloat()); break;
                case VM_RegType::DX: if (!depr_float) mew::gen_adgio(fn, a.getInt(), b.getDouble()); break;
                default: MewCritical("undefined reg type (%i)", ri->type);
              }
            } break;
            case Instruction_NUM: {
              mew::gen_adgio(fn, a.getInt(), b.getInt());
            } break;
          }
        } break;
        case Instruction_REG: {
          VM_REG_INFO* ri = (VM_REG_INFO*)a.data2;
          switch (ri->type) {
            case VM_RegType::R: { 
              switch (b.type) {
                case Instruction_ST: mew::gen_adgio(fn, a.getInt(), b.getInt()); break;
                case Instruction_REG: {
                  VM_REG_INFO* ri = (VM_REG_INFO*)a.data2;
                  switch (ri->type) {
                    case VM_RegType::R: mew::gen_adgio(fn, a.getInt(), b.getInt()); break;
                    case VM_RegType::RX: mew::gen_adgio(fn, a.getInt(), b.getLong()); break;
                    case VM_RegType::FX: if (!depr_float) mew::gen_adgio(fn, a.getInt(), b.getFloat()); break;
                    case VM_RegType::DX: if (!depr_float) mew::gen_adgio(fn, a.getInt(), b.getDouble()); break;
                    default: MewCritical("undefined reg type (%i)", ri->type);
                  }
                } break;
                case Instruction_NUM: {
                  mew::gen_adgio(fn, a.getInt(), b.getInt());
                } break;
              }
            } break;
            case VM_RegType::RX: { 
              switch (b.type) {
                case Instruction_ST: mew::gen_adgio(fn, a.getLong(), b.getInt()); break;
                case Instruction_REG: {
                  VM_REG_INFO* ri = (VM_REG_INFO*)a.data2;
                  switch (ri->type) {
                    case VM_RegType::R: mew::gen_adgio(fn, a.getLong(), b.getInt()); break;
                    case VM_RegType::RX: mew::gen_adgio(fn, a.getLong(), b.getLong()); break;
                    case VM_RegType::FX: if (!depr_float) mew::gen_adgio(fn, a.getLong(), b.getFloat()); break;
                    case VM_RegType::DX: if (!depr_float) mew::gen_adgio(fn, a.getLong(), b.getDouble()); break;
                    default: MewCritical("undefined reg type (%i)", ri->type);
                  }
                } break;
                case Instruction_NUM: {
                  mew::gen_adgio(fn, a.getLong(), b.getInt());
                } break;
              }
            } break;
            case VM_RegType::FX: { 
              if (depr_float) break;
              switch (b.type) {
                case Instruction_ST: mew::gen_adgio(fn, a.getFloat(), b.getInt()); break;
                case Instruction_REG: {
                  VM_REG_INFO* ri = (VM_REG_INFO*)a.data2;
                  switch (ri->type) {
                    case VM_RegType::R: mew::gen_adgio(fn, a.getFloat(), b.getInt()); break;
                    case VM_RegType::RX: mew::gen_adgio(fn, a.getFloat(), b.getLong()); break;
                    case VM_RegType::FX: mew::gen_adgio(fn, a.getFloat(), b.getFloat()); break;
                    case VM_RegType::DX: mew::gen_adgio(fn, a.getFloat(), b.getDouble()); break;
                    default: MewCritical("undefined reg type (%i)", ri->type);
                  }
                } break;
                case Instruction_NUM: {
                  mew::gen_adgio(fn, a.getFloat(), b.getInt());
                } break;
              }
            } break;
            case VM_RegType::DX: { 
              if (depr_float) break;
              switch (b.type) {
                case Instruction_ST: mew::gen_adgio(fn, a.getDouble(), b.getInt()); break;
                case Instruction_REG: {
                  VM_REG_INFO* ri = (VM_REG_INFO*)a.data2;
                  switch (ri->type) {
                    case VM_RegType::R: mew::gen_adgio(fn, a.getDouble(), b.getInt()); break;
                    case VM_RegType::RX: mew::gen_adgio(fn, a.getDouble(), b.getLong()); break;
                    case VM_RegType::FX: mew::gen_adgio(fn, a.getDouble(), b.getFloat()); break;
                    case VM_RegType::DX: mew::gen_adgio(fn, a.getDouble(), b.getDouble()); break;
                    default: MewCritical("undefined reg type (%i)", ri->type);
                  }
                } break;
                case Instruction_NUM: {
                  mew::gen_adgio(fn, a.getDouble(), b.getInt());
                } break;
              }
            } break;
            default: MewCritical("undefined reg type (%i)", a.type);
          }
        } break;
        default: MewCritical("undefined arg type (%i)", a.type);
      } 
    }

    VM_ARG& operator++() {
      do_math(*this, mew::aginc);
      return *this;
    }

    VM_ARG& operator--() {
      do_math(*this, mew::agdec);
      return *this;
    }

    static void mov(VM_ARG& a, VM_ARG& b) {
      do_math(a, b, mew::agmov);
    }
    static void swap(VM_ARG& a, VM_ARG& b) {
      do_math(a, b, mew::agswap);
    }
  };

  VM_ARG& operator+(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agadd);
    return a;
  }
  VM_ARG& operator-(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agsub);
    return a;
  }
  VM_ARG& operator/(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agdiv);
    return a;
  }
  VM_ARG& operator*(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agmul);
    return a;
  }
  VM_ARG& operator>>(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agrs, true);
    return a;
  }
  VM_ARG& operator<<(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agls, true);
    return a;
  }
  VM_ARG& operator^(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agxor, true);
    return a;
  }
  VM_ARG& operator~(VM_ARG& a) {
    VM_ARG::do_math(a, mew::agnot, true);
    return a;
  }
  VM_ARG& operator|(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agor, true);
    return a;
  }
  VM_ARG& operator&(VM_ARG& a, VM_ARG& b) {
    VM_ARG::do_math(a, b, mew::agand, true);
    return a;
  }

  VM_ARG VM_GetArg(VirtualMachine& vm) {
    byte type = *vm.begin++;
    switch (type) {
      case Instruction_ST: {
        u32 offset;
        GrabFromVM(offset);
        VM_ARG arg;
        MewCriticalIf(vm.stack.size() <= offset, "out of stack");
        byte* num = vm.stack.begin()+(vm.stack.size() - 1 - offset);
        arg.data = num;
        arg.type = type;
        arg.size = sizeof(u32);
        return arg;
      };
      case Instruction_REG: {
        byte rtype = *vm.begin++;
        byte ridx = *vm.begin++;
        u64 size;
        VM_ARG arg;
        arg.data = vm.getRegister((VM_RegType)rtype, ridx, &size);
        VM_REG_INFO* ri = new VM_REG_INFO();
        ri->idx = ridx;
        ri->type = (VM_RegType)rtype;
        arg.data2 = (byte*)ri;
        arg.type = type;
        arg.size = (u32)size;
        return arg;
      }
      case Instruction_NUM: {
        s32 num;
        GrabFromVM(num);
        VM_ARG arg;
        arg.data = itoba(num);
        arg.type = type;
        arg.size = sizeof(num);
        return arg;
      }
      case Instruction_MEM: {
        u64 offset;
        GrabFromVM(offset);
        MewCriticalIf(!(vm.heap+offset < vm.end), "out of memory");
        byte* pointer = vm.heap+offset;
        u64 size;
        GrabFromVM(size);
        MewCriticalIf(!(pointer+size < vm.end), "out of memory");
        VM_ARG arg;
        arg.data = pointer;
        arg.type = type;
        arg.size = size;
        return arg;
      }
    
      default: MewCritical("undefined arg type (%i)", type);
    }
  }

#pragma endregion VM_ARG


  void VM_ManualPush(VirtualMachine& vm, u32 x) {
    vm.stack.push(x);
  }

  void VM_Push(VirtualMachine& vm, byte head_byte, u32 number) {
    switch (head_byte) {
      case 0:
      case Instruction_FLT:
      case Instruction_NUM: {
        vm.stack.push(number);
      } break;
      case Instruction_MEM: {
        MewCriticalIf(vm.heap+number < vm.end, "out of memory");
        byte* pointer = vm.heap+number;
        u32 x; memcpy(&x, pointer, sizeof(x));
        vm.stack.push(x, vm.rdi);
      } break;
      case Instruction_REG: {
        MewCriticalIf(vm.heap+number < vm.end, "out of memory");
        vm.stack.push(number, vm.rdi);
      } break;
      case Instruction_ST: {
        MewCriticalIf(vm.stack.size() <= number, "out of stack");
        vm.stack.push(vm.stack.at((int)number), vm.rdi);
      } break;
      default: MewNot(); break;
    }
  }

  byte* VM_GetReg(VirtualMachine& vm, u64* size = nullptr) {
    Virtual::VM_RegType rtype = (Virtual::VM_RegType)(*vm.begin++);
    byte ridx = *vm.begin++;
    return vm.getRegister(rtype, ridx);
  }

  void VM_Push(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    Instruction head_byte = (Instruction)*vm.begin++;
    switch (head_byte) {
      case 0:
      case Instruction_FLT:
      case Instruction_NUM: {
        u32 number = 0;
        memcpy(&number, vm.begin, sizeof(number));
        vm.stack.push(number);
        vm.begin += sizeof(number);
      } break;
      case Instruction_STRUCT: {
        auto arg = VM_GetArg(vm);
        vm.stack.push_array(arg.data, arg.size);
      } break;
      case Instruction_BYTE: { // <value:8>
        u8 number = 0;
        memcpy(&number, vm.begin, sizeof(number));
        vm.stack.push(number);
        vm.begin += sizeof(number);
      } break;
      case Instruction_MEM: { // <offset:32>
        u32 number = 0;
        memcpy(&number, vm.begin, sizeof(number));
        MewCriticalIf(vm.heap+number < vm.end, "out of memory");
        byte* pointer = vm.heap+number;
        u32 x; memcpy(&x, pointer, sizeof(x));
        vm.stack.push(x);
        vm.begin += sizeof(number);
      } break;
      case Instruction_REG: { 
        u64 size;
        byte* reg = VM_GetReg(vm, &size);
        MewCriticalIf(reg == nullptr, "invalid register");
        vm.stack.push((u32)*reg);
        if (size == 8) {
          vm.stack.push((u32)*(reg+sizeof(u32)));
        }
      } break;
      case Instruction_ST: { // offset:4 + arg
        int offset = 0; // byte offset
        GrabFromVM(offset);
        MewCriticalIf(vm.stack.size() <= offset, "out of stack");
        auto arg = VM_GetArg(vm);
        MewCriticalIf(0 >= vm.stack.size()-offset || vm.stack.size() <= offset+arg.size, "out of stack");
        byte* value = vm.stack.begin()+(vm.stack.size() - offset - 1);
        memcpy(value, arg.data, arg.size);
      } break;
      default: MewNot(); break;
    }
  }
  
  void VM_Pop(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    MewCriticalIf(vm.stack.empty(), "stack is empty");
    vm.stack.asc_pop(1);
  }

  void VM_RPop(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    MewCriticalIf(vm.stack.empty(), "stack is empty");
    u64 size;
    u8* raw_reg = VM_GetReg(vm, &size);
    if (size == 4) {
      u32* value = (u32*)&vm.stack.top(vm.rdi+sizeof(u32));
      u32* reg = (u32*)raw_reg;
      *reg = *value;
      vm.stack.asc_pop(sizeof(u32));
    } else if (size == 8) {
      u64* value = (u64*)&vm.stack.top(vm.rdi+sizeof(u64));
      u64* reg = (u64*)raw_reg;
      *reg = *value;
      vm.stack.asc_pop(sizeof(u64));
    }
  }
  
  void VM_StackTop(VirtualMachine& vm, byte type, u32* x, byte** mem = nullptr) {
    switch (type) {
      case 0:
      case Instruction_FLT:
      case Instruction_ST:
      case Instruction_NUM: {
        MewCriticalIf(vm.stack.empty(), "stack is empty");
        u32 _top = vm.stack.top(vm.rdi);
        memmove(x, &_top, sizeof(_top));
      } break;
      case Instruction_MEM: {
        MewCriticalIf(vm.stack.empty(), "stack is empty");
        u32 _top = vm.stack.top(vm.rdi);
        u32 offset = _top;
        MewCriticalIf(vm.heap+offset < vm.end, "out of memory");
        byte* pointer = vm.heap+offset;
        if (mem != nullptr) {
          *mem = pointer;
        }
        memmove(x, pointer, sizeof(*x));
      } break;

      default: MewCritical("undefined arg type (%i)", type); break;
    }
  }

  void VM_SwitchContext(VirtualMachine& vm, Code* code) {
    vm.flags.in_neib_ctx = true;
  }

  void VM_Call(VirtualMachine& vm) {
    u64 offset;
    GrabFromVM(offset);
    MewCriticalIf(vm.begin >= vm.end, "segmentation fault, cant call out of code");
    #ifdef _WIN32
    JIT_Call(vm, offset, vm.jit_cache);
    #else
    vm.begin_stack.push(vm.begin);
    vm.begin = vm.memory + offset;
    #endif
  }

  void VM_MathBase(VirtualMachine& vm, u32* x, u32* y, byte** mem = nullptr) {
    byte type_x = *vm.begin++;
    byte type_y = *vm.begin++;
    vm.rdi += sizeof(u32);
    VM_StackTop(vm, type_x, x, mem);
    vm.rdi -= sizeof(u32);
    VM_StackTop(vm, type_y, y);
  }

  int VM_GetOffset(VirtualMachine& vm) {
    int offset;
    memcpy(&offset, vm.begin, sizeof(int)); vm.begin += sizeof(int);
    return offset/4;
  }


  void VM_MovRDI(VirtualMachine& vm) {
    int offset = VM_GetOffset(vm);
    vm.rdi = offset;
  }

  void VM_Add(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a + b;
  }

  void VM_Sub(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a - b;
  }
  
  void VM_Mul(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a * b;
  }
  void VM_Div(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a / b;
  }
  
  void VM_Inc(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    ++a;
  }

  void VM_Dec(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    --a;
  }

  void VM_Xor(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a ^ b;
  }

  void VM_Or(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a | b;
  }

  void VM_Not(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    ~a;
  }
  
  void VM_And(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a & b;
  }

  void VM_LS(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a << b;
  }

  void VM_RS(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    a >> b;
  }
  
  void VM_ManualJmp(VirtualMachine& vm, u32 offset) {
    MewCriticalIf(!MEW_IN_RANGE(vm.memory, vm.end, vm.begin+offset), 
      "out of memory");
    vm.begin = vm.memory + offset;
    MewCriticalIf(vm.begin >= vm.end, "segmentation fault, cant call out of code");
  }

  
  void VM_Jmp(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u64 offset;
    GrabFromVM(offset);
    vm.begin = vm.memory + offset;
    MewCriticalIf(vm.begin >= vm.end, "segmentation fault, cant call out of code");
  }

  void VM_Ret(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    if (vm.begin_stack.empty()) {
      vm.status = VM_Status_Ret; return;
    }
    byte* begin = vm.begin_stack.top();
    vm.begin = begin;
    vm.begin_stack.pop();
  }

  void VM_Test(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u32 x, y;
    vm.test = {0};
    VM_MathBase(vm, (u32*)&x, (u32*)&y);
    int result = memcmp(&x, &y, sizeof(x));
    if (result > 0) {
      vm.test.more = 1;
    } else if (result < 0) {
      vm.test.less = 1;
    } else {
      vm.test.equal = 1;
    }
  }

  void VM_JE(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    int offset; 
    memcpy(&offset, vm.begin, sizeof(int));
    if (vm.test.equal) {
      VM_ManualJmp(vm, offset);
    } else {
      vm.begin += sizeof(int);
    }
  }
  void VM_JEL(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    int offset; 
    memcpy(&offset, vm.begin, sizeof(int));
    if (vm.test.equal || vm.test.less) {
      VM_ManualJmp(vm, offset);
    } else {
      vm.begin += sizeof(int);
    }
  }
  void VM_JEM(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    int offset; 
    memcpy(&offset, vm.begin, sizeof(int));
    if (vm.test.equal || vm.test.more) {
      VM_ManualJmp(vm, offset);
    } else {
      vm.begin += sizeof(int);
    }
  }
  void VM_JL(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    int offset; 
    memcpy(&offset, vm.begin, sizeof(int));
    if (vm.test.less) {
      VM_ManualJmp(vm, offset);
    } else {
      vm.begin += sizeof(int);
    }
  }
  void VM_JM(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    int offset; 
    memcpy(&offset, vm.begin, sizeof(int));
    if (vm.test.more) {
      VM_ManualJmp(vm, offset);
    } else {
      vm.begin += sizeof(int);
    }
  }
  void VM_JNE(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    int offset; 
    memcpy(&offset, vm.begin, sizeof(int));
    if (!vm.test.equal) {
      VM_ManualJmp(vm, offset);
    } else {
      vm.begin += sizeof(int);
    }
  }

  void VM_Mov(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    VM_ARG::mov(a, b);
  }

  void VM_Swap(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    VM_ARG::swap(a, b);
  }

  void VM_MSet(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u64 x; /* start */
    u64 y; /* size  */
    u64 z; /* value */
    GrabFromVM(x);
    GrabFromVM(y);
    GrabFromVM(z);
    MewCriticalIf(!(vm.heap+x < vm.end), "out of memory");
    MewCriticalIf(!(vm.heap+x+y < vm.end), "out of memory");
    memset(vm.heap+x, z, y);
  }

  void VM_Putc(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    wchar_t long_char;
    memcpy(&long_char, vm.begin, sizeof(wchar_t)); vm.begin+=sizeof(wchar_t);
    fputwc(long_char, vm.std_out);
  }
  
  void VM_Puti(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto x = VM_GetArg(vm);
    int xi = x.getInt();
    char str[12] = {0};
    mew::_itoa10(xi, str);
    fputs(str, vm.std_out);
  }

  void VM_Puts(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u64 offset;
    GrabFromVM(offset);
    MewCriticalIf(!(vm.heap+offset < vm.end), "out of memory");
    byte* pointer = vm.heap+offset;
    char* begin = (char*)pointer;
    while (*(begin) != 0) {
      putchar(*(begin++));
    }
  }

  void VM_Getch(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    int& a = VM_GetArg(vm).getInt();
    a = mew::wait_char();
  }
  
  // 
  void VM_LM(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto a = VM_GetArg(vm);
    auto b = VM_GetArg(vm);
    VM_ARG::mov(a, b);
  }

  void VM_Open(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u64 offset;
    GrabFromVM(offset);
    MewCriticalIf(!(vm.heap+offset < vm.end), "out of memory");
    byte* path = vm.heap+offset;
    u32 descr = vm.fs.Open((const char*)path);
    VM_ManualPush(vm, descr);
  }

  void VM_Close(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto descr_arg = VM_GetArg(vm);
    u32 descr = (u32)descr_arg.getLong();
    vm.fs.Close(descr);
  }

  void VM_Wine(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u64 offset;
    GrabFromVM(offset);
    MewCriticalIf(!(vm.heap+offset < vm.end), "out of memory");
    byte* path = vm.heap+offset;
    vm.fs.CreateFileIfNotExist((const char*)path);
  }
  
  void VM_Write(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u32 descr;
    GrabFromVM(descr);
    auto content = VM_GetArg(vm);
    u8* raw_content = content.getMem();
    u64 size = content.size;
    vm.fs.WriteToFile(descr, raw_content, size);
  }

  void VM_Read(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u32 descr;
    GrabFromVM(descr);
    auto dest = VM_GetArg(vm);
    u8* raw_dest = dest.getMem();
    u64 size = dest.size;
    vm.fs.ReadFromFile(descr, raw_dest, size);
  }
  
  void VM_GetIternalPointer(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    auto _from = VM_GetArg(vm);
    auto _size = VM_GetArg(vm);
    auto _where = VM_GetArg(vm);
    u8* from = (u8*)_from.getLong();
    u64 size = _size.getLong();
    u8* where = (u8*)_where.getLong();
    memcpy(where, from, size);
  }
  
  // put into rx4 vm pointer;
  void VM_GetVM(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u64 rx_size;
    auto rx4 = vm.getRegister(VM_RegType::RX, 4, &rx_size);
    byte* vm_ptr = (byte*)&vm;
    memcpy(rx4, vm_ptr, rx_size);
  }
  
  void VM_DCALL(VirtualMachine& vm) {
    vm.debug.last_fn = (char*)__func__;
    u64 lib_idx;
    GrabFromVM(lib_idx);
    u64 offset;
    GrabFromVM(offset);
    MewCriticalIf(!(vm.heap+offset < vm.end), "out of memory");
    auto name = (const char*)vm.heap+offset;
    auto proc = VM_GetDllPipeFunction(vm, lib_idx, name);
    auto result = proc(&vm);
    vm.stack.push(result);
  }

  void RunLine(VirtualMachine& vm) {
    u64 offset_before = (u64)(vm.begin - vm.memory);
    byte head_byte = *vm.begin++;
    // printf("RunLine: offset=%llu head=0x%02X(%d)\n", 
    //        offset_before, head_byte, head_byte);
    if (vm.flags.heap_lock_execute) { 
      MewCriticalIf(!(vm.begin < vm.heap), "segmentation fault");
    }
    switch (head_byte) {
      case Instruction_NONE: break;
      case Instruction_PUSH: {
        VM_Push(vm);
      } break;
      case Instruction_POP: {
        VM_Pop(vm);
      } break;
      case Instruction_RPOP: {
        VM_RPop(vm);
      } break;
      case Instruction_ADD: {
        VM_Add(vm);
      } break;
      case Instruction_SUB: {
        VM_Sub(vm);
      } break;
      case Instruction_MUL: {
        VM_Mul(vm);
      } break;
      case Instruction_DIV: {
        VM_Div(vm);
      } break;
      case Instruction_INC: {
        VM_Inc(vm);
      } break;
      case Instruction_DEC: {
        VM_Dec(vm);
      } break;
      case Instruction_XOR: {
        VM_Xor(vm);
      } break;
      case Instruction_OR: {
        VM_Or(vm);
      } break;
      case Instruction_NOT: {
        VM_Not(vm);
      } break;
      case Instruction_LS: {
        VM_LS(vm);
      } break;
      case Instruction_RS: {
        VM_RS(vm);
      } break;
      case Instruction_JMP: {
        VM_Jmp(vm);
      } break;
      case Instruction_RET: {
        VM_Ret(vm);
      } break;
      case Instruction_TEST: {
        VM_Test(vm);
      } break;
      case Instruction_JE: {
        VM_JE(vm);
      } break;
      case Instruction_JEL: {
        VM_JEL(vm);
      } break;
      case Instruction_JEM: {
        VM_JEM(vm);
      } break;
      case Instruction_JL: {
        VM_JL(vm);
      } break;
      case Instruction_JM: {
        VM_JM(vm);
      } break;
      case Instruction_JNE: {
        VM_JNE(vm);
      } break;
      case Instruction_MOV: {
        VM_Mov(vm);
      } break;
      case Instruction_SWAP: {
        VM_Swap(vm);
      } break;
      case Instruction_MSET: {
        VM_MSet(vm);
      } break;
      case Instruction_PUTC: {
        VM_Putc(vm);
      } break;
      case Instruction_PUTI: {
        VM_Puti(vm);
      } break;
      case Instruction_PUTS: {
        VM_Puts(vm);
      } break;
      case Instruction_GETCH: {
        VM_Getch(vm);
      } break;
      case Instruction_MOVRDI: {
        VM_MovRDI(vm);
      } break;
      case Instruction_CALL: {
        VM_Call(vm);
      } break;
      case Instruction_WINE: {
        VM_Wine(vm);
      } break;
      case Instruction_WRITE: {
        VM_Write(vm);
      } break;
      case Instruction_READ: {
        VM_Read(vm);
      } break;
      case Intruction_GetVM: {
        VM_GetVM(vm);
      } break;
      case Intruction_GetIPTR: {
        VM_GetIternalPointer(vm);
      } break;
      case Instruction_OPEN: {
        VM_Open(vm);
      } break;
      case Instruction_CLOSE: {
        VM_Close(vm);
      } break;
      case Instruction_LM: {
        VM_LM(vm);
      } break;
      case Instruction_DCALL: {
        VM_DCALL(vm);
      } break;
      case Instruction_EXIT: {
        vm.status = VM_Status_Ret;
      } break;
      default: MewCritical("unsupported instruction (0x%02X)", head_byte); break;
    }
  }

  int Run(VirtualMachine& vm, Code& code) {
    u64 code_size = __VM_ALIGN(code.capacity, VM_CODE_ALIGN);
    MewCriticalIf(!(vm.capacity > code_size), "not enough memory");
    byte* begin = vm.memory;
    byte* end   = begin+vm.capacity;
    byte* alloc_space = begin+code_size+1;
    vm.flags.use_debug = code.cme.flags.has_debug;
    vm.src = &code;
    vm.heap = alloc_space;
    vm.begin = begin;
    vm.end = end;
    vm.status = VM_Status_Execute;
    if (code.data != nullptr) {
      memcpy(vm.heap, code.data, code.data_size*sizeof(*code.data));
    }
  
    while (vm.begin < vm.end && vm.status != VM_Status_Ret) {
      ++vm.process_cycle; RunLine(vm);
    }
    vm.status = VM_Status_Panding;
    if (vm.stack.empty()) {
      return 0;
    }
    return vm.stack.top();
  }

  int Execute(VirtualMachine& vm, Code& code) {
    Alloc(vm, code);
    LoadMemory(vm, code);
    return Run(vm, code);
  }
  
  int Execute(Code& code) {
    VirtualMachine vm;
    Alloc(vm, code);
    LoadMemory(vm, code);
    return Run(vm, code);
  }

  int Execute(const char* path) {
    Code* code = Code_LoadFromFile(path);
    return Execute(*code);
  }

  class VM_Async {
  public:
    struct ExecuteInfo {
      enum struct Status {
        Execute, Errored, Done
      } status;
      int result;
    }; 
  private:
    mew::stack<VirtualMachine*> m_vms;
    mew::stack<ExecuteInfo> m_execs;
  public:
    VM_Async() { }

    void hardStop() {
      for (int i = 0; i < m_vms.size(); ++i) {
        delete m_vms[i];
      }
      m_vms.clear();
      m_execs.clear();
    }
    
    int Append(Code& code) {
      VirtualMachine* vm = new VirtualMachine();
      Alloc(*vm, code);
      LoadMemory(*vm, code);
      u64 code_size = __VM_ALIGN(code.capacity, VM_CODE_ALIGN);
      MewCriticalIf(!(vm->capacity > code_size), "not enough memory");
      vm->flags.use_debug = code.cme.flags.has_debug;
      vm->src = &code;
      vm->heap = vm->begin+code_size+1;
      vm->begin = vm->memory;
      vm->end = vm->begin+vm->capacity;
      vm->status = VM_Status_Execute;
      if (code.data != nullptr) {
        memcpy(vm->heap, code.data, code.data_size*sizeof(*code.data));
      }
      m_execs.push((ExecuteInfo){ExecuteInfo::Status::Execute, -1});
      return m_vms.push(vm);
    }

    bool is_ends(int id) {
      return m_vms[id]->status == VM_Status_Ret;
    }

    int get_status_code(int id) {
      return m_execs[id].result;
    }
        
    VirtualMachine* GetById(int id) {
      return m_vms[id];
    }

    void ExecuteStep() {
      for (int i = 0; i < m_vms.size(); ++i) {
        m_execs[i].result = this->Run(*m_vms[i], *m_vms[i]->src);
        if (m_vms[i]->status == VM_Status_Panding) {
          m_execs[i].status = ExecuteInfo::Status::Done;
        }
        if (m_vms[i]->status == VM_Status_Error) {
          m_execs[i].status = ExecuteInfo::Status::Errored;
        }
      }
    }
    
    int Run(VirtualMachine& vm, Code& code) {
      if (!(vm.begin < vm.end && vm.status != VM_Status_Ret)) {
        vm.status = VM_Status_Panding;
        return vm.stack.top();
      }
      ++vm.process_cycle;
      // try {
        RunLine(vm);
      // } catch(std::exception& e) {
      //   u64 cursor = vm.capacity - (u64)(vm.end-vm.begin);
      //   for (int i = 0; i < code.cme.debug.size(); ++i) {
      //     if (code.cme.debug[i].cursor >= cursor) {
      //       fprintf(vm.std_out, "\n[DEBUG_ERROR] at (%i) in (%s)\n", code.cme.debug[i].line, vm.debug.last_fn);
      //       vm.status = VM_Status_Error;
      //       break;
      //     }
      //   }
      // }
      MewFlush();
      return -1;
    }
  };

  class CodeBuilder {
  public:
    struct untyped_pair {
      byte* data;
      byte size;
    };
    static const u64 alloc_size = 8;
  private:
    u64 capacity, _code_size, _data_size = 0;
    mew::stack<u64> _adatas;
    byte* code = nullptr;
    byte* data = nullptr;
    u64 stack_head = 0;
  public:
    CodeBuilder()
      : capacity(alloc_size), _code_size(0), _data_size(0), code(mew::mem::alloc(alloc_size))
      { memset(code, 0, alloc_size); }

    CodeBuilder(Code* code)
      : capacity(code->capacity), 
        _code_size(code->capacity), 
        _data_size(code->data_size), 
        code((byte*)code->playground),
        data(code->data)
      { }
    
    byte* GetData() const noexcept {
      return data;
    }

    inline u64 code_size() const noexcept {
      return _code_size;
    }

    inline u64 cursor() const noexcept {
      return _code_size;
    }

    inline u64 data_size() const noexcept {
      return _data_size;
    }

    template<typename K>
    u64 push(K& value) {
      *this
        << Instruction_PUSH 
        << Instruction_NUM
        << Instruction_STRUCT
        << sizeof(value);
      insert(value);
      return cursor();
    }

    inline u64 putAtCode(u64 idx, byte* val, u64 _size) {
      MewCriticalIf(idx+_size > _code_size, "out of code");
      memcpy(code, val, _size);
      return cursor();
    }

    inline u64 putAtCode(u64 idx, u64 val) {
      MewCriticalIf(idx+sizeof(val) > _code_size, "out of code");
      memcpy(code + idx, &val, sizeof(val));  // было: code, надо: code + idx
      return cursor();
    }
    
    inline u64 insertAtCode(u64 idx, byte* val, u64 size) {
      MewAssert(idx <= _code_size);
      u8* buffer = mew::mem::alloc(size+_code_size);
      memcpy(buffer, code, idx);
      memcpy(buffer+idx, val, size);
      memcpy(buffer+idx+size, code+idx, _code_size - idx);
      delete[] code;
      code = buffer;
      _code_size += size;
      return cursor();
    }
    
    inline u64 putAtData(u64 idx, byte* val, u64 _size) {
      MewCriticalIf(idx+_size > _data_size, "out of data");
      memcpy(data, val, _size);
      return _data_size;
    }

    inline u64 putAtData(u64 idx, u64 val) {
      MewCriticalIf(idx+sizeof(val) > _data_size, "out of data");
      memcpy(data, &val, sizeof(val));
      return cursor();
    }

    inline u64 putRegister(VM_REG_INFO reg) {
      *this
        << Instruction_REG
        << (byte)reg.type
        << (byte)reg.idx;
      return cursor();
    }

    inline u64 putNumber(s32 num) {
      *this
        << Instruction_NUM
        << num;
      return cursor();
    }

    inline u64 putMem(u64 offset, u64 size) {
      *this
        << Instruction_MEM
        << offset
        << size;
      return cursor();
    }

    inline u64 putByte(u8 num) {
      *this
        << Instruction_BYTE
        << num;
      return cursor();
    }

    inline u64 putRdiOffset(u64 offset) {
      *this
        << Instruction_ST
        << offset;
      return cursor();
    }
    
    void Upsize(u64 _size = alloc_size) {
      byte* __temp_p = mew::mem::realloc(code, capacity, capacity+_size);
      MewAssert(__temp_p);
      code = __temp_p;
      capacity += _size;
    }
    
    void getBuilder_raw_push(byte* data, u64 size) {
      UpsizeIfNeeds(size);
      memcpy(code + _code_size, data, size);
      _code_size += size;
    }
    
    void UpsizeIfNeeds(u64 needs_size) {
      if (_code_size+needs_size > capacity) {
        Upsize(needs_size);
      }
    }
        
    void AddData(byte* row, u64 size) {
      u64 __new_size = _data_size+size;
      data = mew::mem::realloc(data, _data_size, __new_size);
      MewCriticalIf(!data, "failed to reallocate data");
      memcpy(data+_data_size, row, size);
      _data_size = __new_size;
    }

    CodeBuilder& operator+=(const char* text) {
      AddData((byte*)text, strlen(text));
      return *this;
    }

    friend CodeBuilder& operator<<(CodeBuilder& cb, byte i) {
      cb.UpsizeIfNeeds(sizeof(i));
      cb.code[cb._code_size++] = i;
      return cb;
    } 

    friend CodeBuilder& operator<<(CodeBuilder& cb, u32 i) {
      cb.UpsizeIfNeeds(sizeof(i));
      memcpy(cb.code+cb._code_size, &i, sizeof(i));
      cb._code_size += sizeof(i);
      return cb;
    }

    CodeBuilder& putU64(u64 i) {
      UpsizeIfNeeds(sizeof(i));
      memcpy(code+_code_size, &i, sizeof(i));
      _code_size += sizeof(i);
      return *this;
    }

    friend CodeBuilder& operator<<(CodeBuilder& cb, Instruction i) {
      cb.UpsizeIfNeeds(sizeof(i));
      cb.code[cb._code_size++] = i;
      return cb;
    }
    friend CodeBuilder& operator<<(CodeBuilder& cb, int i) {
      cb.UpsizeIfNeeds(sizeof(i));
      memcpy(cb.code+cb._code_size, &i, sizeof(i));
      cb._code_size += sizeof(i);
      return cb;
    }
    friend CodeBuilder& operator<<(CodeBuilder& cb, size_t i) {
      cb.UpsizeIfNeeds(sizeof(i));
      memcpy(cb.code+cb._code_size, &i, sizeof(i));
      cb._code_size += sizeof(i);
      return cb;
    }
    
    template<typename K>
    CodeBuilder& insert(K& value) {
      UpsizeIfNeeds(sizeof(value));
      memcpy(code+_code_size, &value, sizeof(value));
      _code_size += sizeof(value);
      return *this;
    }

    friend CodeBuilder& operator<<(CodeBuilder& cb, untyped_pair i) {
      cb.UpsizeIfNeeds(i.size);
      memcpy(cb.code+cb._code_size, &i.data, i.size);
      cb._code_size += i.size;
      return cb;
    }

    friend CodeBuilder& operator>>(CodeBuilder& cb, untyped_pair i) {
      cb.UpsizeIfNeeds(i.size);
      memmove(cb.code+i.size, cb.code, cb._code_size);
      memcpy(cb.code, &i.data, i.size);
      cb._code_size += i.size;
      return cb;
    }

    void concat(CodeBuilder& other) {
      UpsizeIfNeeds(other._code_size);
      memcpy(code+_code_size, other.code, other._code_size);
      _code_size += other._code_size;
      if (other.data != nullptr) {
        AddData(other.data, other._data_size);
      }
    }

    void push_adata(u64 size) {
      _adatas.push(size);
    }

    Code* operator*() {
      Code* c = new Code();
      c->capacity   = _code_size;
      c->playground = (Instruction*)(code);
      c->data_size  = _data_size;
      c->data       = data;
      return c;
    }
    Code operator*(int) {
      Code c;
      c.capacity    = _code_size;
      c.playground  = (Instruction*)code;
      c.data_size   = _data_size;
      c.data        = data;
      return c;
    }

    byte* at(int idx) {
      u32 real_idx = (_code_size + idx) % _code_size;
      MewAssert(real_idx < _code_size);
      return (code+real_idx);
    }

    byte* operator[](int idx) {
      return at(idx);
    }

    void force_data(u32 _size) {
      byte* _ndata = mew::mem::alloc(_data_size+_size);
      memcpy(_ndata, data, _data_size);
      _data_size += _size;
      data = _ndata;
    }
    
  };

  constexpr u64 GetVersion() {
    return VIRTUAL_VERSION;
  }

  namespace waze {
    class Waze;
    typedef void (*func4cb)(Waze&);

    class Waze {
    private:
      CodeBuilder builder;
      mew::stack<byte*> store;
      std::unordered_map<const char*, u64> labels;
      std::unordered_map<const char*, mew::stack<u64>> unresolved; // метки ещё не объявленные
      u64 entry = 0;
    public:
      Waze() { 
        builder << Instruction_JMP << (u64)0;
      }

      Code* build() {
        resolve_all();
        return *builder;
      }

      void setEntry(const char* label) {
        MewCriticalIf(labels.find(label) == labels.end(), "label not found");
        entry = labels[label];
        builder.putAtCode(1, entry);
      }
      
      void setEntry() {
        entry = builder.cursor();
        builder.putAtCode(1, entry);
      }

      byte* pop() {
        MewCriticalIf(store.empty(), "store is empty");
        return store.pop();
      }

      void push(const char* str) {
        store.push((byte*)str);
      }

      void putLabel(const char* label) {
        u64 cur = builder.cursor();
        labels[label] = cur;
        // патчим все forward-ссылки на эту метку
        auto it = unresolved.find(label);
        if (it != unresolved.end()) {
          for (int i = 0; i < it->second.count(); ++i) {
            builder.putAtCode(it->second[i], cur);
          }
          unresolved.erase(it);
        }
      }

      u64 getLabel(const char* label) {
        MewCriticalIf(labels.find(label) == labels.end(), "label not found");
        return labels[label];
      }

      // Записать jmp/call с возможным forward-ref
      void emitJump(Instruction instr, const char* label) {
        builder << instr;
        auto it = labels.find(label);
        if (it != labels.end()) {
          builder.putU64(it->second);
        } else {
          // запомним позицию для патчинга позже
          unresolved[label].push(builder.cursor());
          builder.putU64((u64)0);
        }
      }

      void resolve_all() {
        MewCriticalIf(!unresolved.empty(), "unresolved labels remain after build");
      }

      CodeBuilder& getBuilder() {
        return builder;
      }

      friend Waze& operator<<(Waze& w, func4cb func) {
        func(w);
        return w;
      }

      friend Waze& operator<<(Waze& w, byte* b) {
        w.store.push(b);
        return w;
      }

      // Удобный синтаксис: w["label"] >> Instruction_CALL
      struct LabelRef {
        Waze& w;
        const char* label;
      };
      LabelRef operator[](const char* label) {
        return {*this, label};
      }
    };

    // ─── базовые колбэки ───────────────────────────────────────

    void putEntry4vm(Waze& w) { w.setEntry(); }

    void label4vm(Waze& w) {
      const char* label = (const char*)w.pop();
      w.putLabel(label);
    }

    void gotoLabel4vm(Waze& w) {
      const char* label = (const char*)w.pop();
      w.emitJump(Instruction_JMP, label);
    }

    void call4vm(Waze& w) {
      const char* label = (const char*)w.pop();
      w.emitJump(Instruction_CALL, label);
    }

    void ret4vm(Waze& w) {
      w.getBuilder() << Instruction_RET;
    }

    void exit4vm(Waze& w) {
      w.getBuilder() << Instruction_EXIT;
    }

    // ─── вывод ────────────────────────────────────────────────

    void puts4vm(Waze& w) {
      u64 offset = w.getBuilder().data_size();
      const char* str = (const char*)w.pop();
      w.getBuilder() << Instruction_PUTS;
      w.getBuilder().putU64(offset);
      w.getBuilder() += str;
    }

    void putc4vm(Waze& w) {
      byte* raw = w.pop();
      wchar_t ch = (wchar_t)(u64)raw;
      w.getBuilder() << Instruction_PUTC;
      u16 v; memcpy(&v, &ch, sizeof(v));
      w.getBuilder() << v;
    }

    void puti4vm(Waze& w) {
      w.getBuilder() << Instruction_PUTI;
      // аргумент — регистр, кладётся через make_reg
      byte* raw = w.pop();
      w.getBuilder().getBuilder_raw_push(raw, 3); // REG + type + idx
    }

    // ─── стек / регистры ──────────────────────────────────────

    void pushn4vm(Waze& w) {
      byte* raw = w.pop();
      s32 num; memcpy(&num, raw, sizeof(num));
      w.getBuilder() << Instruction_PUSH << Instruction_NUM << num;
    }

    void pushr4vm(Waze& w) {
      byte* raw = w.pop(); // [REG_TYPE, IDX]
      w.getBuilder() << Instruction_PUSH << Instruction_REG;
      w.getBuilder().getBuilder_raw_push(raw, 2);
    }

    void pop4vm(Waze& w) {
      w.getBuilder() << Instruction_POP;
    }

    void popr4vm(Waze& w) {
      byte* raw = w.pop();
      w.getBuilder() << Instruction_RPOP;
      w.getBuilder().getBuilder_raw_push(raw, 2);
    }

    // ─── арифметика ───────────────────────────────────────────

    void inc4vm(Waze& w) {
      byte* raw = w.pop();
      w.getBuilder() << Instruction_INC;
      w.getBuilder().getBuilder_raw_push(raw, 3);
    }

    void dec4vm(Waze& w) {
      byte* raw = w.pop();
      w.getBuilder() << Instruction_DEC;
      w.getBuilder().getBuilder_raw_push(raw, 3);
    }

    void add4vm(Waze& w) {
      byte* rb = w.pop();
      byte* ra = w.pop();
      w.getBuilder() << Instruction_ADD;
      w.getBuilder().getBuilder_raw_push(ra, 3);
      w.getBuilder().getBuilder_raw_push(rb, 3);
    }

    void sub4vm(Waze& w) {
      byte* rb = w.pop();
      byte* ra = w.pop();
      w.getBuilder() << Instruction_SUB;
      w.getBuilder().getBuilder_raw_push(ra, 3);
      w.getBuilder().getBuilder_raw_push(rb, 3);
    }

    void mul4vm(Waze& w) {
      byte* rb = w.pop();
      byte* ra = w.pop();
      w.getBuilder() << Instruction_MUL;
      w.getBuilder().getBuilder_raw_push(ra, 3);
      w.getBuilder().getBuilder_raw_push(rb, 3);
    }

    void div4vm(Waze& w) {
      byte* rb = w.pop();
      byte* ra = w.pop();
      w.getBuilder() << Instruction_DIV;
      w.getBuilder().getBuilder_raw_push(ra, 3);
      w.getBuilder().getBuilder_raw_push(rb, 3);
    }

    // ─── переходы с условием ──────────────────────────────────

    void test4vm(Waze& w) {
      byte* rb = w.pop();
      byte* ra = w.pop();
      w.getBuilder() << Instruction_TEST;
      w.getBuilder().getBuilder_raw_push(ra, 3);
      w.getBuilder().getBuilder_raw_push(rb, 3);
    }

    void je4vm(Waze& w)  { const char* l = (const char*)w.pop(); w.emitJump(Instruction_JE,  l); }
    void jne4vm(Waze& w) { const char* l = (const char*)w.pop(); w.emitJump(Instruction_JNE, l); }
    void jl4vm(Waze& w)  { const char* l = (const char*)w.pop(); w.emitJump(Instruction_JL,  l); }
    void jm4vm(Waze& w)  { const char* l = (const char*)w.pop(); w.emitJump(Instruction_JM,  l); }
    void jel4vm(Waze& w) { const char* l = (const char*)w.pop(); w.emitJump(Instruction_JEL, l); }
    void jem4vm(Waze& w) { const char* l = (const char*)w.pop(); w.emitJump(Instruction_JEM, l); }

    // ─── mov ──────────────────────────────────────────────────

    void mov4vm(Waze& w) {
      byte* rb = w.pop();
      byte* ra = w.pop();
      w.getBuilder() << Instruction_MOV;
      w.getBuilder().getBuilder_raw_push(ra, 3);
      w.getBuilder().getBuilder_raw_push(rb, 3);
    }

    // ─── файлы ────────────────────────────────────────────────

    void wine4vm(Waze& w) {
      u64 offset = w.getBuilder().data_size();
      const char* path = (const char*)w.pop();
      w.getBuilder() << Instruction_WINE;
      w.getBuilder().putU64(offset);
      w.getBuilder() += path;
    }

    void open4vm(Waze& w) {
      u64 offset = w.getBuilder().data_size();
      const char* path = (const char*)w.pop();
      w.getBuilder() << Instruction_OPEN;
      w.getBuilder().putU64(offset);
      w.getBuilder() += path;
    }

    void close4vm(Waze& w) {
      byte* raw = w.pop();
      w.getBuilder() << Instruction_CLOSE;
      w.getBuilder().getBuilder_raw_push(raw, 3);
    }

    // ─── make-хелперы ─────────────────────────────────────────

    byte* make(const char* str) {
      return (byte*)str;
    }

    byte* make(u64 num) {
      byte* buffer = new byte[sizeof(num)];
      memcpy(buffer, &num, sizeof(num));
      return buffer;
    }

    byte* make(s32 num) {
      byte* buffer = new byte[sizeof(num)];
      memcpy(buffer, &num, sizeof(num));
      return buffer;
    }

    // Регистр: [Instruction_REG, type, idx]
    byte* make_reg(VM_RegType type, byte idx) {
      byte* buffer = new byte[3];
      buffer[0] = (byte)Instruction_REG;
      buffer[1] = (byte)type;
      buffer[2] = idx;
      return buffer;
    }

    // Числовой аргумент: [Instruction_NUM, 4 байта s32]
    byte* make_num(s32 num) {
      byte* buffer = new byte[5];
      buffer[0] = (byte)Instruction_NUM;
      memcpy(buffer+1, &num, sizeof(num));
      return buffer;
    }

  } // namespace waze

  #undef VIRTUAL_VERSION
#include "mewpop"
}
namespace Tests {
  bool test_Virtual() {
    try {
      using namespace Virtual;
      waze::Waze w;
      w
        << waze::make("counter") << waze::label4vm
          << waze::make_reg(VM_RegType::RX, 0) << waze::inc4vm
        << waze::ret4vm

        << waze::putEntry4vm
          << waze::make("loop") << waze::label4vm
          << waze::make("counter") << waze::call4vm
          << waze::make_reg(VM_RegType::RX, 0) << waze::puti4vm
          << waze::make("\n") << waze::puts4vm
          << waze::make("loop") << waze::gotoLabel4vm
        << waze::exit4vm;


      Code* code = w.build();
      Code_SaveToFile(*code, "./hellow_word.nb");
      // printf("[%u|%u]\n", code->capacity, code->data_size);
      Execute("./hellow_word.nb");
    } catch (std::exception e) {
      MewPrintError(e);
      return false;
    }
    return true;
  }
// ─── Тест 1: инкремент N раз → rx0 == N ─────────────────────
u64 test_counter() {
  try {
    using namespace Virtual;
    waze::Waze w;

    // do_inc: rx0++, rx1-- (rx1 = счётчик)
    w << waze::make("do_inc") << waze::label4vm
        << waze::make_reg(VM_RegType::RX, 0) << waze::inc4vm
        << waze::make_reg(VM_RegType::RX, 1) << waze::dec4vm
      << waze::ret4vm

      << waze::putEntry4vm;

    // rx1 = 100
    for (int i = 0; i < 100; ++i)
      w << waze::make_reg(VM_RegType::RX, 1) << waze::inc4vm;

    w << waze::make("loop") << waze::label4vm
        << waze::make("do_inc") << waze::call4vm
        << waze::make("loop") << waze::gotoLabel4vm
      << waze::exit4vm;

    Code* code = w.build();
    VirtualMachine vm;
    Alloc(vm, *code);
    LoadMemory(vm, *code);
    Run(vm, *code);

    u64 rx0 = 0;
    memcpy(&rx0, vm._rx[0].data, sizeof(u64));
    return rx0; // ожидаем 100
  } catch (std::exception& e) { MewPrintError(e); return 0; }
}

// ─── Тест 2: сложение rx0 += rx1(=10), 50 раз → rx0 == 500 ──
u64 test_add() {
  try {
    using namespace Virtual;
    waze::Waze w;

    // do_add: rx0 += rx1, rx2--
    w << waze::make("do_add") << waze::label4vm
        << waze::make_reg(VM_RegType::RX, 0)
        << waze::make_reg(VM_RegType::RX, 1)
        << waze::add4vm
        << waze::make_reg(VM_RegType::RX, 2) << waze::dec4vm
      << waze::ret4vm

      << waze::putEntry4vm;

    // rx1 = 10
    for (int i = 0; i < 10; ++i)
      w << waze::make_reg(VM_RegType::RX, 1) << waze::inc4vm;
    // rx2 = 50 (счётчик)
    for (int i = 0; i < 50; ++i)
      w << waze::make_reg(VM_RegType::RX, 2) << waze::inc4vm;

    w << waze::make("add_loop") << waze::label4vm
        << waze::make("do_add") << waze::call4vm
        << waze::make("add_loop") << waze::gotoLabel4vm
      << waze::exit4vm;

    Code* code = w.build();
    VirtualMachine vm;
    Alloc(vm, *code);
    LoadMemory(vm, *code);
    Run(vm, *code);

    u64 rx0 = 0;
    memcpy(&rx0, vm._rx[0].data, sizeof(u64));
    return rx0; // ожидаем 500
  } catch (std::exception& e) { MewPrintError(e); return 0; }
}

// ─── Тест 3: умножение 7*8=56 через сложение ─────────────────
u64 test_mul_by_add() {
  try {
    using namespace Virtual;
    waze::Waze w;

    // mul_step: rx2 += rx0, rx1--
    w << waze::make("mul_step") << waze::label4vm
        << waze::make_reg(VM_RegType::RX, 2)
        << waze::make_reg(VM_RegType::RX, 0)
        << waze::add4vm
        << waze::make_reg(VM_RegType::RX, 1) << waze::dec4vm
      << waze::ret4vm

      << waze::putEntry4vm;

    // rx0 = 7
    for (int i = 0; i < 7; ++i)
      w << waze::make_reg(VM_RegType::RX, 0) << waze::inc4vm;
    // rx1 = 8
    for (int i = 0; i < 8; ++i)
      w << waze::make_reg(VM_RegType::RX, 1) << waze::inc4vm;

    w << waze::make("mul_loop") << waze::label4vm
        << waze::make("mul_step") << waze::call4vm
        << waze::make("mul_loop") << waze::gotoLabel4vm
      << waze::exit4vm;

    Code* code = w.build();
    VirtualMachine vm;
    Alloc(vm, *code);
    LoadMemory(vm, *code);
    Run(vm, *code);

    u64 rx2 = 0;
    memcpy(&rx2, vm._rx[2].data, sizeof(u64));
    return rx2; // ожидаем 56
  } catch (std::exception& e) { MewPrintError(e); return 0; }
}

// ─── Тест 4: две горячих функции, 100 итераций ───────────────
// rx0 == 100, rx1 == 200 → возвращаем rx0 + rx1 == 300
u64 test_two_hot_funcs() {
  try {
    using namespace Virtual;
    waze::Waze w;

    w << waze::make("inc_a") << waze::label4vm
        << waze::make_reg(VM_RegType::RX, 0) << waze::inc4vm
      << waze::ret4vm

      << waze::make("inc_b") << waze::label4vm
        << waze::make_reg(VM_RegType::RX, 1) << waze::inc4vm
        << waze::make_reg(VM_RegType::RX, 1) << waze::inc4vm
      << waze::ret4vm

      << waze::putEntry4vm;

    // rx2 = 100 (счётчик)
    for (int i = 0; i < 100; ++i)
      w << waze::make_reg(VM_RegType::RX, 2) << waze::inc4vm;

    w << waze::make("main_loop") << waze::label4vm
        << waze::make("inc_a") << waze::call4vm
        << waze::make("inc_b") << waze::call4vm
        << waze::make_reg(VM_RegType::RX, 2) << waze::dec4vm
        << waze::make("main_loop") << waze::gotoLabel4vm
      << waze::exit4vm;

    Code* code = w.build();
    VirtualMachine vm;
    Alloc(vm, *code);
    LoadMemory(vm, *code);
    Run(vm, *code);

    u64 rx0 = 0, rx1 = 0;
    memcpy(&rx0, vm._rx[0].data, sizeof(u64));
    memcpy(&rx1, vm._rx[1].data, sizeof(u64));
    return rx0 + rx1; // ожидаем 300
  } catch (std::exception& e) { MewPrintError(e); return 0; }
}

// ─── Тест 5: декремент 50 раз → rx0 == 0 ────────────────────
u64 test_dec_to_zero() {
  try {
    using namespace Virtual;
    waze::Waze w;

    // do_dec: rx0--, rx1-- (rx1 = счётчик)
    w << waze::make("do_dec") << waze::label4vm
        << waze::make_reg(VM_RegType::RX, 0) << waze::dec4vm
        << waze::make_reg(VM_RegType::RX, 1) << waze::dec4vm
      << waze::ret4vm

      << waze::putEntry4vm;

    // rx0 = rx1 = 50
    for (int i = 0; i < 50; ++i) {
      w << waze::make_reg(VM_RegType::RX, 0) << waze::inc4vm;
      w << waze::make_reg(VM_RegType::RX, 1) << waze::inc4vm;
    }

    w << waze::make("dec_loop") << waze::label4vm
        << waze::make("do_dec") << waze::call4vm
        << waze::make("dec_loop") << waze::gotoLabel4vm
      << waze::exit4vm;

    Code* code = w.build();
    VirtualMachine vm;
    Alloc(vm, *code);
    LoadMemory(vm, *code);
    Run(vm, *code);

    u64 rx0 = 0;
    memcpy(&rx0, vm._rx[0].data, sizeof(u64));
    return rx0; // ожидаем 0
  } catch (std::exception& e) { MewPrintError(e); return 0; }
}

// ─── Тест 6: производительность — 4 inc * N итераций ────────
u64 test_perf_inc(u64 iterations) {
  try {
    using namespace Virtual;
    waze::Waze w;

    // hot_inc: rx0++ rx1++ rx2++ rx3++, rx4-- (счётчик)
    w << waze::make("hot_inc") << waze::label4vm
        << waze::make_reg(VM_RegType::RX, 0) << waze::inc4vm
        << waze::make_reg(VM_RegType::RX, 1) << waze::inc4vm
        << waze::make_reg(VM_RegType::RX, 2) << waze::inc4vm
        << waze::make_reg(VM_RegType::RX, 3) << waze::inc4vm
        << waze::make_reg(VM_RegType::RX, 4) << waze::dec4vm
      << waze::ret4vm

      << waze::putEntry4vm;

    // rx4 = iterations
    for (u64 i = 0; i < iterations; ++i)
      w << waze::make_reg(VM_RegType::RX, 4) << waze::inc4vm;

    w << waze::make("perf_loop") << waze::label4vm
        << waze::make("hot_inc") << waze::call4vm
        << waze::make("perf_loop") << waze::gotoLabel4vm
      << waze::exit4vm;

    Code* code = w.build();
    VirtualMachine vm;
    Alloc(vm, *code);
    LoadMemory(vm, *code);
    Run(vm, *code);

    u64 rx0 = 0;
    memcpy(&rx0, vm._rx[0].data, sizeof(u64));
    return rx0; // ожидаем iterations
  } catch (std::exception& e) { MewPrintError(e); return 0; }
}

// ─── Главная функция ─────────────────────────────────────────
void run_all_benchmarks() {
  using namespace std::chrono;

  struct Entry {
    const char* name;
    std::function<u64()> fn;
    u64 expected;
  };

  Entry tests[] = {
    // { "counter:   inc x100",           test_counter,                        100 },
    // { "add:       rx0 += 10, x50",     test_add,                            500 },
    // { "mul:       7 * 8 via add",      test_mul_by_add,                      56 },
    { "two funcs: rx0+rx1 after x100", test_two_hot_funcs,                  300 },
    { "dec:       50 -> 0",            test_dec_to_zero,                      0 },
    { "perf:      4*inc x1000",        []{ return test_perf_inc(1000); },   1000 },
  };

  u64 total = sizeof(tests) / sizeof(tests[0]);
  u64 passed = 0, failed = 0, total_ms = 0;

  printf("-----------------nanvm JIT benchmark suite-----------------\n");

  struct Result { const char* name; bool ok; u64 ms; u64 got; u64 exp; };
  Result results[6];

  for (u64 i = 0; i < total; ++i) {
    auto& t = tests[i];
    auto t0 = high_resolution_clock::now();
    u64 got = t.fn();
    auto t1 = high_resolution_clock::now();
    u64 ms  = duration_cast<milliseconds>(t1 - t0).count();
    bool ok = (got == t.expected);

    results[i] = { t.name, ok, ms, got, t.expected };
    total_ms += ms;
    if (ok) ++passed; else ++failed;

    printf("---------------------------------------------------\n");
    printf("| %-40s %s\n", t.name, ok ? "✓ PASS" : "✗ FAIL");
    printf("|   got=%-10llu expected=%-10llu time=%llu ms\n",
           got, t.expected, ms);
  }

  printf("|---------------------------------------------------|\n");
  printf("|  total=%llu  passed=%llu  failed=%llu  time=%llu ms\n",
         total, passed, failed, total_ms);
  printf("\\---------------------------------------------------/\n\n");
}
}

#endif