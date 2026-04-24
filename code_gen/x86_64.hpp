// code_gen.hpp - Single-file x86-64 code generator for Windows & Linux
// Supports both System V AMD64 ABI (Linux) and Microsoft x64 calling convention (Windows)

#ifndef X86_64_CODE_GEN_HPP
#define X86_64_CODE_GEN_HPP

#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <stdexcept>

#ifdef _WIN32
    #define PLATFORM_WINDOWS 1
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #define PLATFORM_LINUX 1
    #include <sys/mman.h>
    #include <unistd.h>
#endif

struct x86_64_CodeGen {
    std::vector<uint8_t> code;      // executable code section
    std::vector<uint8_t> data;      // static data section (read-only)
    std::vector<uint8_t> bss;       // zero-initialized data (not emitted in binary, just allocated)
    
    // Label management
    std::unordered_map<std::string, size_t> labels;
    std::vector<std::pair<std::string, size_t>> unresolved_jumps;
    std::vector<std::pair<std::string, size_t>> unresolved_calls;
    std::vector<std::pair<std::string, size_t>> unresolved_data_refs;
    
    // Data label management
    std::unordered_map<std::string, size_t> data_labels;
    
    size_t data_offset = 0;
    
    x86_64_CodeGen() {
        // Reserve some space to reduce reallocations
        code.reserve(4096);
        data.reserve(1024);
    }
    
    // Helper to emit bytes
    void emit_byte(uint8_t b) {
        code.push_back(b);
    }
    
    void emit_word(uint16_t w) {
        code.push_back(w & 0xFF);
        code.push_back((w >> 8) & 0xFF);
    }
    
    void emit_dword(uint32_t d) {
        code.push_back(d & 0xFF);
        code.push_back((d >> 8) & 0xFF);
        code.push_back((d >> 16) & 0xFF);
        code.push_back((d >> 24) & 0xFF);
    }
    
    void emit_qword(uint64_t q) {
        emit_dword(q & 0xFFFFFFFF);
        emit_dword(q >> 32);
    }
    
    // REX prefix encoding
    void emit_rex(bool w, bool r, bool x, bool b) {
        uint8_t rex = 0x40;
        if (w) rex |= 0x08;
        if (r) rex |= 0x04;
        if (x) rex |= 0x02;
        if (b) rex |= 0x01;
        emit_byte(rex);
    }
    
    // ModR/M byte encoding
    void emit_modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
        emit_byte((mod << 6) | ((reg & 0x07) << 3) | (rm & 0x07));
    }
    
    // SIB byte encoding
    void emit_sib(uint8_t scale, uint8_t index, uint8_t base) {
        emit_byte((scale << 6) | ((index & 0x07) << 3) | (base & 0x07));
    }
    
    // ============== Instructions ==============
    
    // Push 32-bit immediate
    void pushi32(int32_t val) {
        if (val >= -128 && val <= 127) {
            emit_byte(0x6A); // push imm8
            emit_byte(static_cast<uint8_t>(val));
        } else {
            emit_byte(0x68); // push imm32
            emit_dword(static_cast<uint32_t>(val));
        }
    }
    
    // Push 64-bit immediate (only valid in 64-bit mode)
    void pushi64(int64_t val) {
        // For values that fit in 32 bits, use push imm32
        if (val >= INT32_MIN && val <= INT32_MAX) {
            pushi32(static_cast<int32_t>(val));
        } else {
            emit_byte(0x68); // push imm32 (sign-extended to 64-bit)
            emit_dword(static_cast<uint32_t>(val));
        }
    }
    
    // Push register
    void push_r64(uint8_t reg) { // reg: 0=rax, 1=rcx, 2=rdx, 3=rbx, 4=rsp, 5=rbp, 6=rsi, 7=rdi, +8 for r8-r15
        if (reg >= 8) {
            emit_rex(false, false, false, true);
            emit_byte(0x50 + (reg - 8));
        } else {
            emit_byte(0x50 + reg);
        }
    }
    
    // Pop register
    void pop_r64(uint8_t reg) {
        if (reg >= 8) {
            emit_rex(false, false, false, true);
            emit_byte(0x58 + (reg - 8));
        } else {
            emit_byte(0x58 + reg);
        }
    }
    
    // Move immediate to register
    void mov_r64_imm64(uint8_t reg, uint64_t val) {
        if (reg >= 8) {
            emit_rex(true, false, false, true);
            emit_byte(0xB8 + (reg - 8));
        } else {
            emit_rex(true, false, false, false);
            emit_byte(0xB8 + reg);
        }
        emit_qword(val);
    }
    
    void mov_r32_imm32(uint8_t reg, uint32_t val) {
        if (reg >= 8) {
            emit_rex(false, false, false, true);
            emit_byte(0xB8 + (reg - 8));
        } else {
            emit_byte(0xB8 + reg);
        }
        emit_dword(val);
    }
    
    // Move between registers
    void mov_r64_r64(uint8_t dst, uint8_t src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit_byte(0x89);
        emit_modrm(3, src & 0x07, dst & 0x07);
    }
    
    void mov_r32_r32(uint8_t dst, uint8_t src) {
        if (dst >= 8 || src >= 8) {
            emit_rex(false, dst >= 8, false, src >= 8);
        }
        emit_byte(0x89);
        emit_modrm(3, src & 0x07, dst & 0x07);
    }
    
    // Add
    void add_r64_r64(uint8_t dst, uint8_t src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit_byte(0x01);
        emit_modrm(3, src & 0x07, dst & 0x07);
    }
    
    void add_r64_imm32(uint8_t dst, int32_t val) {
        if (val >= -128 && val <= 127) {
            emit_rex(true, false, false, dst >= 8);
            emit_byte(0x83);
            emit_modrm(3, 0, dst & 0x07);
            emit_byte(static_cast<uint8_t>(val));
        } else {
            emit_rex(true, false, false, dst >= 8);
            emit_byte(0x81);
            emit_modrm(3, 0, dst & 0x07);
            emit_dword(static_cast<uint32_t>(val));
        }
    }
    
    // Subtract
    void sub_r64_r64(uint8_t dst, uint8_t src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit_byte(0x29);
        emit_modrm(3, src & 0x07, dst & 0x07);
    }
    
    void sub_r64_imm32(uint8_t dst, int32_t val) {
        if (val >= -128 && val <= 127) {
            emit_rex(true, false, false, dst >= 8);
            emit_byte(0x83);
            emit_modrm(3, 5, dst & 0x07);
            emit_byte(static_cast<uint8_t>(val));
        } else {
            emit_rex(true, false, false, dst >= 8);
            emit_byte(0x81);
            emit_modrm(3, 5, dst & 0x07);
            emit_dword(static_cast<uint32_t>(val));
        }
    }
    
    // Multiply
    void imul_r64_r64(uint8_t dst, uint8_t src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit_byte(0x0F);
        emit_byte(0xAF);
        emit_modrm(3, dst & 0x07, src & 0x07);
    }
    
    // Divide (idiv - signed divide, RDX:RAX / src -> RAX=quotient, RDX=remainder)
    void idiv_r64(uint8_t src) {
        emit_rex(true, false, false, src >= 8);
        emit_byte(0xF7);
        emit_modrm(3, 7, src & 0x07);
    }
    
    // AND
    void and_r64_r64(uint8_t dst, uint8_t src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit_byte(0x21);
        emit_modrm(3, src & 0x07, dst & 0x07);
    }
    
    // OR
    void or_r64_r64(uint8_t dst, uint8_t src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit_byte(0x09);
        emit_modrm(3, src & 0x07, dst & 0x07);
    }
    
    // XOR
    void xor_r64_r64(uint8_t dst, uint8_t src) {
        emit_rex(true, dst >= 8, false, src >= 8);
        emit_byte(0x31);
        emit_modrm(3, src & 0x07, dst & 0x07);
    }
    
    // Compare
    void cmp_r64_r64(uint8_t left, uint8_t right) {
        emit_rex(true, left >= 8, false, right >= 8);
        emit_byte(0x39);
        emit_modrm(3, right & 0x07, left & 0x07);
    }
    
    void cmp_r64_imm32(uint8_t left, int32_t val) {
        if (val >= -128 && val <= 127) {
            emit_rex(true, false, false, left >= 8);
            emit_byte(0x83);
            emit_modrm(3, 7, left & 0x07);
            emit_byte(static_cast<uint8_t>(val));
        } else {
            emit_rex(true, false, false, left >= 8);
            emit_byte(0x81);
            emit_modrm(3, 7, left & 0x07);
            emit_dword(static_cast<uint32_t>(val));
        }
    }
    
    // Conditional jumps
    void je(const std::string& label) { jcc(0x84, label); }
    void jne(const std::string& label) { jcc(0x85, label); }
    void jl(const std::string& label) { jcc(0x8C, label); }
    void jle(const std::string& label) { jcc(0x8E, label); }
    void jg(const std::string& label) { jcc(0x8F, label); }
    void jge(const std::string& label) { jcc(0x8D, label); }
    void jb(const std::string& label) { jcc(0x82, label); }
    void jbe(const std::string& label) { jcc(0x86, label); }
    void ja(const std::string& label) { jcc(0x87, label); }
    void jae(const std::string& label) { jcc(0x83, label); }
    
    void jcc(uint8_t cc, const std::string& label) {
        auto it = labels.find(label);
        if (it != labels.end()) {
            // Near jump
            int32_t offset = static_cast<int32_t>(it->second - (code.size() + 2));
            if (offset >= -128 && offset <= 127) {
                emit_byte(cc & 0xFE); // Short jump opcode (e.g., 0x74 for je)
                emit_byte(static_cast<uint8_t>(offset));
            } else {
                emit_byte(0x0F);
                emit_byte(cc);
                unresolved_jumps.push_back({label, code.size()});
                emit_dword(0); // Placeholder
            }
        } else {
            emit_byte(0x0F);
            emit_byte(cc);
            unresolved_jumps.push_back({label, code.size()});
            emit_dword(0);
        }
    }
    
    // Unconditional jump
    void jmp(const std::string& label) {
        auto it = labels.find(label);
        if (it != labels.end()) {
            int32_t offset = static_cast<int32_t>(it->second - (code.size() + 2));
            if (offset >= -128 && offset <= 127) {
                emit_byte(0xEB);
                emit_byte(static_cast<uint8_t>(offset));
            } else {
                emit_byte(0xE9);
                unresolved_jumps.push_back({label, code.size()});
                emit_dword(0);
            }
        } else {
            emit_byte(0xE9);
            unresolved_jumps.push_back({label, code.size()});
            emit_dword(0);
        }
    }
    
    void jmp_r64(uint8_t reg) {
        if (reg >= 8) {
            emit_rex(false, false, false, true);
        }
        emit_byte(0xFF);
        emit_modrm(3, 4, reg & 0x07);
    }
    
    // Call
    void call(const std::string& label) {
        emit_byte(0xE8);
        unresolved_calls.push_back({label, code.size()});
        emit_dword(0);
    }
    
    void call_r64(uint8_t reg) {
        if (reg >= 8) {
            emit_rex(false, false, false, true);
        }
        emit_byte(0xFF);
        emit_modrm(3, 2, reg & 0x07);
    }
    
    // Return
    void ret() {
        emit_byte(0xC3);
    }
    
    void ret_imm16(uint16_t bytes) {
        emit_byte(0xC2);
        emit_word(bytes);
    }
    
    // System call / syscall
    void syscall() {
        emit_byte(0x0F);
        emit_byte(0x05);
    }
    
    // Leave (destroy stack frame)
    void leave() {
        emit_byte(0xC9);
    }
    
    // No operation
    void nop() {
        emit_byte(0x90);
    }
    
    void nop_multi(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            nop();
        }
    }
    
    // Breakpoint
    void int3() {
        emit_byte(0xCC);
    }
    
    // ============== Memory operations ==============
    
    // Load effective address
    void lea_r64_mem(uint8_t dst, uint8_t base, int32_t disp = 0) {
        emit_rex(true, dst >= 8, false, base >= 8);
        emit_byte(0x8D);
        
        if (base == 4) { // RSP requires SIB
            if (disp == 0) {
                emit_modrm(0, dst & 0x07, 4);
                emit_sib(0, 4, 4);
            } else if (disp >= -128 && disp <= 127) {
                emit_modrm(1, dst & 0x07, 4);
                emit_sib(0, 4, 4);
                emit_byte(static_cast<uint8_t>(disp));
            } else {
                emit_modrm(2, dst & 0x07, 4);
                emit_sib(0, 4, 4);
                emit_dword(static_cast<uint32_t>(disp));
            }
        } else {
            if (disp == 0 && (base & 0x07) != 5) {
                emit_modrm(0, dst & 0x07, base & 0x07);
            } else if (disp >= -128 && disp <= 127) {
                emit_modrm(1, dst & 0x07, base & 0x07);
                emit_byte(static_cast<uint8_t>(disp));
            } else {
                emit_modrm(2, dst & 0x07, base & 0x07);
                emit_dword(static_cast<uint32_t>(disp));
            }
        }
    }
    
    // Move from memory to register
    void mov_r64_mem(uint8_t dst, uint8_t base, int32_t disp = 0) {
        emit_rex(true, dst >= 8, false, base >= 8);
        emit_byte(0x8B);
        
        if (base == 4) {
            if (disp == 0) {
                emit_modrm(0, dst & 0x07, 4);
                emit_sib(0, 4, 4);
            } else if (disp >= -128 && disp <= 127) {
                emit_modrm(1, dst & 0x07, 4);
                emit_sib(0, 4, 4);
                emit_byte(static_cast<uint8_t>(disp));
            } else {
                emit_modrm(2, dst & 0x07, 4);
                emit_sib(0, 4, 4);
                emit_dword(static_cast<uint32_t>(disp));
            }
        } else {
            if (disp == 0 && (base & 0x07) != 5) {
                emit_modrm(0, dst & 0x07, base & 0x07);
            } else if (disp >= -128 && disp <= 127) {
                emit_modrm(1, dst & 0x07, base & 0x07);
                emit_byte(static_cast<uint8_t>(disp));
            } else {
                emit_modrm(2, dst & 0x07, base & 0x07);
                emit_dword(static_cast<uint32_t>(disp));
            }
        }
    }
    
    // Move from register to memory
    void mov_mem_r64(uint8_t base, int32_t disp, uint8_t src) {
        emit_rex(true, src >= 8, false, base >= 8);
        emit_byte(0x89);
        
        if (base == 4) {
            if (disp == 0) {
                emit_modrm(0, src & 0x07, 4);
                emit_sib(0, 4, 4);
            } else if (disp >= -128 && disp <= 127) {
                emit_modrm(1, src & 0x07, 4);
                emit_sib(0, 4, 4);
                emit_byte(static_cast<uint8_t>(disp));
            } else {
                emit_modrm(2, src & 0x07, 4);
                emit_sib(0, 4, 4);
                emit_dword(static_cast<uint32_t>(disp));
            }
        } else {
            if (disp == 0 && (base & 0x07) != 5) {
                emit_modrm(0, src & 0x07, base & 0x07);
            } else if (disp >= -128 && disp <= 127) {
                emit_modrm(1, src & 0x07, base & 0x07);
                emit_byte(static_cast<uint8_t>(disp));
            } else {
                emit_modrm(2, src & 0x07, base & 0x07);
                emit_dword(static_cast<uint32_t>(disp));
            }
        }
    }
    
    // ============== Data Section ==============
    
    // Define data label at current data position
    void data_label(const std::string& name) {
        data_labels[name] = data.size();
    }
    
    // Emit raw bytes to data section
    void emit_data_byte(uint8_t b) {
        data.push_back(b);
    }
    
    void emit_data_bytes(const void* ptr, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(ptr);
        data.insert(data.end(), bytes, bytes + size);
    }
    
    void emit_data_string(const std::string& str) {
        emit_data_bytes(str.c_str(), str.size());
        emit_data_byte(0); // null terminator
    }
    
    void emit_data_dword(uint32_t val) {
        emit_data_byte(val & 0xFF);
        emit_data_byte((val >> 8) & 0xFF);
        emit_data_byte((val >> 16) & 0xFF);
        emit_data_byte((val >> 24) & 0xFF);
    }
    
    void emit_data_qword(uint64_t val) {
        emit_data_dword(val & 0xFFFFFFFF);
        emit_data_dword(val >> 32);
    }
    
    // Get address of data label as immediate in code
    void lea_r64_data(uint8_t dst, const std::string& label) {
        auto it = data_labels.find(label);
        if (it != data_labels.end()) {
            // Use RIP-relative addressing
            // The displacement will be calculated at finalization time
            emit_rex(true, dst >= 8, false, false);
            emit_byte(0x8D);
            emit_modrm(0, dst & 0x07, 5); // RIP-relative
            
            unresolved_data_refs.push_back({label, code.size()});
            emit_dword(0); // Placeholder for RIP-relative offset
        } else {
            throw std::runtime_error("Data label not found: " + label);
        }
    }
    
    // ============== Labels ==============
    
    void label(const std::string& name) {
        labels[name] = code.size();
    }
    
    // ============== Function prologue/epilogue ==============
    
    void prologue() {
        push_r64(5); // push rbp
        mov_r64_r64(5, 4); // mov rbp, rsp
    }
    
    void epilogue() {
        leave(); // mov rsp, rbp; pop rbp
        ret();
    }
    
    void prologue_with_stack(size_t local_vars_size) {
        push_r64(5);
        mov_r64_r64(5, 4);
        if (local_vars_size > 0) {
            sub_r64_imm32(4, static_cast<int32_t>(local_vars_size));
        }
    }
    
    // ============== Platform-specific exit ==============
    
    // Exit program
    void exit(int32_t code) {
        mov_r32_imm32(0, static_cast<uint32_t>(code)); // mov ecx, code
        
#ifdef PLATFORM_WINDOWS
        // Windows: call ExitProcess
        // We need to load kernel32!ExitProcess address
        mov_r64_imm64(1, reinterpret_cast<uint64_t>(&ExitProcess)); // mov rax, ExitProcess
        call_r64(1);
#else
        // Linux: sys_exit
        mov_r32_imm32(0, 60); // mov eax, 60 (sys_exit)
        syscall();
#endif
    }
    
    // Print string (for debugging)
    void print_string(const std::string& str_label) {
#ifdef PLATFORM_WINDOWS
        // Windows: GetStdHandle + WriteConsoleA
        // Simplified version - store string in data section first
        mov_r64_imm64(0, reinterpret_cast<uint64_t>(GetStdHandle));
        mov_r32_imm32(1, static_cast<uint32_t>(-11)); // STD_OUTPUT_HANDLE
        call_r64(0);
        // rax = handle
        
        // WriteConsole
        mov_r64_r64(1, 0); // rcx = handle (first arg)
        lea_r64_data(2, str_label); // rdx = string pointer
        mov_r32_imm32(3, static_cast<uint32_t>(strlen(str_label))); // r8 = length
        sub_r64_imm32(4, 8); // allocate space for lpNumberOfCharsWritten
        lea_r64_mem(9, 4, 0); // r9 = &written
        pushi32(0); // lpReserved
        sub_r64_imm32(4, 32); // shadow space
        
        mov_r64_imm64(0, reinterpret_cast<uint64_t>(WriteConsoleA));
        call_r64(0);
        
        add_r64_imm32(4, 40); // cleanup stack
#else
        // Linux: write syscall
        mov_r32_imm32(0, 1); // mov eax, 1 (sys_write)
        mov_r32_imm32(7, 1); // mov edi, 1 (stdout)
        lea_r64_data(6, str_label); // lea rsi, [str]
        mov_r32_imm32(2, static_cast<uint32_t>(strlen(str_label))); // mov edx, len
        syscall();
#endif
    }
    
    // Helper for string length in data section
    static size_t strlen(const std::string& s) { return s.size(); }
    
    // ============== Finalization ==============
    
    // Resolve all forward references
    void finalize() {
        // Resolve jumps
        for (const auto& jump : unresolved_jumps) {
            auto it = labels.find(jump.first);
            if (it == labels.end()) {
                throw std::runtime_error("Undefined label: " + jump.first);
            }
            int32_t offset = static_cast<int32_t>(it->second - (jump.second + 4));
            uint8_t* ptr = &code[jump.second];
            ptr[0] = offset & 0xFF;
            ptr[1] = (offset >> 8) & 0xFF;
            ptr[2] = (offset >> 16) & 0xFF;
            ptr[3] = (offset >> 24) & 0xFF;
        }
        
        // Resolve calls
        for (const auto& call : unresolved_calls) {
            auto it = labels.find(call.first);
            if (it == labels.end()) {
                throw std::runtime_error("Undefined label: " + call.first);
            }
            int32_t offset = static_cast<int32_t>(it->second - (call.second + 4));
            uint8_t* ptr = &code[call.second];
            ptr[0] = offset & 0xFF;
            ptr[1] = (offset >> 8) & 0xFF;
            ptr[2] = (offset >> 16) & 0xFF;
            ptr[3] = (offset >> 24) & 0xFF;
        }
        
        // Resolve data references (RIP-relative)
        for (const auto& ref : unresolved_data_refs) {
            auto it = data_labels.find(ref.first);
            if (it == data_labels.end()) {
                throw std::runtime_error("Undefined data label: " + ref.first);
            }
            // Calculate offset from current instruction to data
            // The instruction is at ref.second, RIP = ref.second + 7 (next instruction)
            int32_t offset = static_cast<int32_t>((it->second) - (ref.second + 7));
            uint8_t* ptr = &code[ref.second];
            ptr[0] = offset & 0xFF;
            ptr[1] = (offset >> 8) & 0xFF;
            ptr[2] = (offset >> 16) & 0xFF;
            ptr[3] = (offset >> 24) & 0xFF;
        }
    }
    
    // Get total binary size (code + data)
    size_t total_size() const {
        return code.size() + data.size();
    }
    
    // Write to binary buffer (code first, then data)
    void write_to_buffer(void* buffer) const {
        uint8_t* ptr = static_cast<uint8_t*>(buffer);
        std::memcpy(ptr, code.data(), code.size());
        std::memcpy(ptr + code.size(), data.data(), data.size());
    }
    
    // ============== Execution ==============
    
    // Execute the generated code
    // Returns pointer to executable memory (caller must free)
    void* compile_to_memory() {
        finalize();
        
        size_t total = total_size();
        
#ifdef PLATFORM_WINDOWS
        void* mem = VirtualAlloc(nullptr, total, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!mem) {
            throw std::runtime_error("Failed to allocate executable memory");
        }
#else
        void* mem = mmap(nullptr, total, PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED) {
            throw std::runtime_error("Failed to allocate executable memory");
        }
#endif
        
        write_to_buffer(mem);
        return mem;
    }
    
    // Free executable memory
    static void free_executable_memory(void* mem, size_t size) {
#ifdef PLATFORM_WINDOWS
        VirtualFree(mem, 0, MEM_RELEASE);
#else
        munmap(mem, size);
#endif
    }
    
    // Compile to file
    void compile_to_file(const std::string& filename) {
        finalize();
        
        FILE* f = fopen(filename.c_str(), "wb");
        if (!f) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
        
        // Write simple binary format: [code_size:8][code][data]
        uint64_t code_sz = code.size();
        fwrite(&code_sz, sizeof(code_sz), 1, f);
        fwrite(code.data(), 1, code.size(), f);
        fwrite(data.data(), 1, data.size(), f);
        
        fclose(f);
    }
    
    // Load from file
    static x86_64_CodeGen load_from_file(const std::string& filename) {
        FILE* f = fopen(filename.c_str(), "rb");
        if (!f) {
            throw std::runtime_error("Failed to open file for reading: " + filename);
        }
        
        x86_64_CodeGen cg;
        
        uint64_t code_sz;
        fread(&code_sz, sizeof(code_sz), 1, f);
        
        cg.code.resize(code_sz);
        fread(cg.code.data(), 1, code_sz, f);
        
        // Read rest as data
        fseek(f, 0, SEEK_END);
        size_t file_sz = ftell(f);
        size_t data_sz = file_sz - sizeof(code_sz) - code_sz;
        
        cg.data.resize(data_sz);
        fseek(f, sizeof(code_sz) + code_sz, SEEK_SET);
        fread(cg.data.data(), 1, data_sz, f);
        
        fclose(f);
        return cg;
    }
};

// ============== Convenience functions ==============

inline x86_64_CodeGen& pushi32(x86_64_CodeGen& cg, int32_t val) { cg.pushi32(val); return cg; }
inline x86_64_CodeGen& pushi64(x86_64_CodeGen& cg, int64_t val) { cg.pushi64(val); return cg; }
inline x86_64_CodeGen& push_r64(x86_64_CodeGen& cg, uint8_t reg) { cg.push_r64(reg); return cg; }
inline x86_64_CodeGen& pop_r64(x86_64_CodeGen& cg, uint8_t reg) { cg.pop_r64(reg); return cg; }
inline x86_64_CodeGen& mov_r64_imm64(x86_64_CodeGen& cg, uint8_t reg, uint64_t val) { cg.mov_r64_imm64(reg, val); return cg; }
inline x86_64_CodeGen& mov_r32_imm32(x86_64_CodeGen& cg, uint8_t reg, uint32_t val) { cg.mov_r32_imm32(reg, val); return cg; }
inline x86_64_CodeGen& mov_r64_r64(x86_64_CodeGen& cg, uint8_t dst, uint8_t src) { cg.mov_r64_r64(dst, src); return cg; }
inline x86_64_CodeGen& add_r64_r64(x86_64_CodeGen& cg, uint8_t dst, uint8_t src) { cg.add_r64_r64(dst, src); return cg; }
inline x86_64_CodeGen& sub_r64_r64(x86_64_CodeGen& cg, uint8_t dst, uint8_t src) { cg.sub_r64_r64(dst, src); return cg; }
inline x86_64_CodeGen& imul_r64_r64(x86_64_CodeGen& cg, uint8_t dst, uint8_t src) { cg.imul_r64_r64(dst, src); return cg; }
inline x86_64_CodeGen& idiv_r64(x86_64_CodeGen& cg, uint8_t src) { cg.idiv_r64(src); return cg; }
inline x86_64_CodeGen& cmp_r64_r64(x86_64_CodeGen& cg, uint8_t left, uint8_t right) { cg.cmp_r64_r64(left, right); return cg; }
inline x86_64_CodeGen& label(x86_64_CodeGen& cg, const std::string& name) { cg.label(name); return cg; }
inline x86_64_CodeGen& jmp(x86_64_CodeGen& cg, const std::string& label) { cg.jmp(label); return cg; }
inline x86_64_CodeGen& call(x86_64_CodeGen& cg, const std::string& label) { cg.call(label); return cg; }
inline x86_64_CodeGen& ret(x86_64_CodeGen& cg) { cg.ret(); return cg; }
inline x86_64_CodeGen& prologue(x86_64_CodeGen& cg) { cg.prologue(); return cg; }
inline x86_64_CodeGen& epilogue(x86_64_CodeGen& cg) { cg.epilogue(); return cg; }
inline x86_64_CodeGen& exit(x86_64_CodeGen& cg, int32_t code) { cg.exit(code); return cg; }

// Register constants
namespace Reg {
    constexpr uint8_t RAX = 0;
    constexpr uint8_t RCX = 1;
    constexpr uint8_t RDX = 2;
    constexpr uint8_t RBX = 3;
    constexpr uint8_t RSP = 4;
    constexpr uint8_t RBP = 5;
    constexpr uint8_t RSI = 6;
    constexpr uint8_t RDI = 7;
    constexpr uint8_t R8  = 8;
    constexpr uint8_t R9  = 9;
    constexpr uint8_t R10 = 10;
    constexpr uint8_t R11 = 11;
    constexpr uint8_t R12 = 12;
    constexpr uint8_t R13 = 13;
    constexpr uint8_t R14 = 14;
    constexpr uint8_t R15 = 15;
}

#endif // CODE_GEN_HPP