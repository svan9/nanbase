#ifndef NANASM_COMPILER_HPP
#define NANASM_COMPILER_HPP

#include "mewtypes.h"
#include "mewlib.h"
#include <unordered_map>
#include "virtual.hpp"
#include <unordered_map>
#include "mewstring"
#include "asm_defs.h"

namespace nanasm {
  using std_path = std::filesystem::path;
  namespace std_fs = std::filesystem;



  typedef mew::stack<Token> lexer_tokens_t;
  
  struct Lexer {
    lexer_tokens_t tokens;
    std_path work_folder;

    void parse(const char* source) {
      using namespace mew::string;
      TokenRow str_row(source);
      str_row.SkipToStart();
      const char* word;
      while ((word = *str_row++) != nullptr && *word != '\0') { 
        Token* _from_table = GetTokenFromTable(word);
        if (_from_table != nullptr) {
          tokens.push(*_from_table);
          continue;
        }
        if (mew::starts_with(word, "rdi")) { 
          tokens.push(Token::make(
            TokenType::RDI,
            strlen(word) == 3 ? 0: itoba(mew::stoi(word+3))
          ));
          continue;
        }
        
        if (word[0] == '[') {
          mew::stack<u8> temp;
          while((word = *str_row++) != nullptr && *word != '\0' && word[0] != ']' ) {
            if (word[0] == ',') continue;
            bool success;
            u8 val = (u8)mew::string::str_to_int(word, success);
            MewUserAssert(success, "invalid byte value in array");
            temp.push(val);
          }
          auto val = new DataValue();
          val->size = temp.count();
          val->value = (u8*)rcopy(temp.begin(), temp.count());
          tokens.push(Token::make(
            TokenType::ARRAY,
            (byte*)val
          ));
          continue;
        }
        
        const char* temp = word;
        const char* next = str_row.Current();
        if (next != nullptr && *next != '\0' && *next == ':' ) {
          tokens.push(Token::make(
            TokenType::LABEL,
            (u8*)scopy(temp)
          ));
          ++str_row;
          continue;
        }
        if (temp[0] == '&' && next != nullptr && *next != '\0') {
          tokens.push(Token::make(
            TokenType::NAMED_ADDRESS,
            (u8*)scopy(next)
          ));
          ++str_row;
          continue;
        }
        
        tokens.push(Token::make(
          TokenType::Undefined,
          scopy(word)
        ));
      }
    }

    void WatchUndefinedTokens(Token& tk) {
      bool success;
      auto number = mew::string::str_to_int((const char*)tk.value, success);
      if (success) {
        tk.type = TokenType::NUMBER;
        tk.value = itoba(number);
      }
      if (*tk.value == '"' && getLastChar((const char*)tk.value) == '"') {
        tk.type = TokenType::STRING;
        mew::fas(
          tk.value, 
          (u8*)mew::string::str_parse((const char*)tk.value+1, strlen((const char*)tk.value)-2)
        );
        return;
      }
      tk.type = TokenType::WORD;
    }

    void DecryptUndefinedTokens() {
      for (int x = 0; x < tokens.count(); ++x) {
        auto& e = tokens[x];
        if (e.type == TokenType::Undefined) {
          WatchUndefinedTokens(e);
        }
      }
    }
    
    void Tokenize(const char* src, std_path& work_folder) {
      this->work_folder = work_folder;
      parse(src);
      DecryptUndefinedTokens();
    }

    void concat(Lexer& other) {
      tokens.concat(other.tokens);
    }
  };

  struct Compiler {
    Virtual::CodeBuilder cb;
    std::unordered_map<const char*, DataViewValue*> data;
    std::unordered_map<u64, DataViewValue*> data_calles; // <where, ...>
    std::unordered_map<const char*, u64> label;
    std::unordered_map<u64, const char*> calles; // <at_code, target_offset> -- jump & call & j<?>
    std::unordered_map<const char*, u64> external_calles;
    Lexer& lexer;
    bool overload = false;
    bool is_end = false;
    u64 pos = 0;

    Compiler(Lexer& lexer): lexer(lexer) {}

    Token& Current() {
      if (isEnd()) return lexer.tokens.at(-1);
      return lexer.tokens.at(pos);
    }
    void RM_Cur() {
      lexer.tokens.erase(pos);
      if (pos != 0) --pos;
    }

    Token& Next() {
      if (overload) {
        overload = false;
        return Current();
      }
      if (lexer.tokens.count() <= ++pos) {
        pos = lexer.tokens.count();
      }
      return Current();
    }

    bool isEnd() {
      return pos >= lexer.tokens.count();
    }

    Token& Prev() {
      if (0 >= -pos) pos = 0;
      return Current();
    }
    
    u64 GetFunctionAddress(Token& tk) {
      const char* name = (const char*)tk.value;
      auto fd = label.find(name);
      calles.insert({cb.cursor(), name});
      if (fd == label.end()) {
        return 0;
      }
      return fd->second;
    }

    void UpdateCalles(const char* name) {
      auto fd = label.find(name);
      u64 cur = fd->second;
      for (auto [k, v]: calles) {
        if (!mew::strcmp(v, name)) continue;
        cb.putAtCode(k, cur);
      }
    }

    DataViewValue* GetNameAddress(Token& tk) {
      const char* name = (const char*)tk.value;
      auto fd = data.find(name);
      for (auto [k, v]: data) {
        if (!mew::strcmp(k, name)) continue;
        return v;
      }
      MewUserAssert(false, "Cannot find data");
    }



    inline bool isRegister(Token& tk) {
      return (u64)tk.type >= (u64)TokenType::R1 && (u64)tk.type <= (u64)TokenType::FX5;
    }

    Virtual::VM_REG_INFO GetRegInfo(Token& tk) {
      Virtual::VM_REG_INFO reg;
      reg.idx = ((u64)tk.type - (u64)TokenType::R1) % 5; 
      reg.type = (Virtual::VM_RegType)(
        (((u64)tk.type - (u64)TokenType::R1) % 4) + 1
      );
      return reg;
    }

    void PutArg(Token& tk) {
      if (isRegister(tk)) {
        cb.putRegister(GetRegInfo(tk));
        return;
      }
      switch(tk.type) {
        case TokenType::NAMED_ADDRESS: {
          auto dvv = GetNameAddress(tk);
          data_calles.insert({cb.cursor(), dvv});
          cb.putMem(dvv->offset, dvv->size);
        } break;
        case TokenType::NUMBER: {
          cb.putNumber((batos32(tk.value)));
        } break;
        case TokenType::RDI: {
          cb.putRdiOffset((batos32(tk.value)));
        } break;
        default: MewUserAssert(false, "NOT FOR VM_ARG");
      }
    }


    void compile() {
      for (Token& tk = Current(); !isEnd(); tk = Next()) {
        DoOne(tk);
      }
      
      cb << Virtual::Instruction_EXIT;
      if (!label.contains("end-entry")) {
        label.insert({"end-entry", cb.cursor()});
      }
      if (!label.contains("entry")) {
        label.insert({"entry", 0});
      }
    }

    void DoOne(Token& tk) {
        switch(tk.type) {
          case TokenType::IMPORT: {
            tk = Next();
            MewUserAssert(tk.type == TokenType::STRING, "import <name>");
            const char* name = (const char*)tk.value;
            const char* file_name = mew::strjoin(name, ".ns");
            auto path = mew::search_file_in_dir(lexer.work_folder, file_name);
            if (path.empty()) {
              printf("Error: at (.., ..) \n  cant find file in import `%s`", file_name);
              exit(1);
            }
            const char* content = mew::ReadFullFile(path);
            lexer.tokens.erase(pos-1, 2);
            if (pos != 0) --pos;
            Lexer lex;
            lex.Tokenize(content, lexer.work_folder);
            lexer.concat(lex);
            overload = true;
          } break;
          case TokenType::DATA: {
            tk = Next();
            MewUserAssert(tk.type == TokenType::WORD, "data <name> STRING | Byte Array | Number");
            const char* name = (const char*)tk.value;
            tk = Next();
            MewUserAssert(tk.type == TokenType::STRING || tk.type == TokenType::ARRAY || tk.type == TokenType::NUMBER, "data <name> STRING | Byte Array | Number");
            DataViewValue* dvv = MEW_NEW(DataViewValue);
            dvv->offset = cb.data_size();
            switch (tk.type) {
              case TokenType::STRING:
                dvv->size = strlen((const char*)tk.value);
                data.insert({name, dvv});
                cb.AddData(tk.value, dvv->size);
              break;
              case TokenType::ARRAY:
                dvv->size = ((DataValue*)tk.value)->size;
                data.insert({name, dvv});
                cb.AddData(((DataValue*)tk.value)->value, ((DataValue*)tk.value)->size);
              break;
              case TokenType::NUMBER:
                dvv->size = sizeof(u32);
                data.insert({name, dvv});
                cb.AddData(tk.value, sizeof(u32));
              break;
            }
          } break;
          case TokenType::LABEL: {
            if (!label.contains("end-entry")) {
              if (label.contains("entry")) {
                label.insert({"end-entry", cb.cursor()});
              } else {
                label.insert({"entry", cb.cursor()});
              }
            }
            label.insert({(const char*)tk.value, cb.cursor()});
            UpdateCalles((const char*)tk.value);
          } break;
#pragma region PUSH
          case TokenType::PUSHN: {
            cb << Virtual::Instruction_PUSH;
            tk = Next();
            PutArg(tk);
          } break;
          case TokenType::PUSHM: {
            cb << Virtual::Instruction_PUSH;
            tk = Next();
            u64 dest;
            u64 size;
            if (tk.type == TokenType::NUMBER) {
              dest = (u64)tk.value;
              tk = Next();
              size = (u64)tk.value;
            } else if (tk.type == TokenType::NAMED_ADDRESS) {
              auto na = GetNameAddress(tk);
              dest = na->offset;
              size = na->size;
              tk = Next();
            } else {
              MewUserAssert(false, "invalid pushm arg");
            }
            cb.putMem(dest, size);
          } break;
          case TokenType::PUSHB: {
            cb << Virtual::Instruction_PUSH;
            tk = Next();
            MewAssert(tk.type == TokenType::NUMBER);
            u8 dest = (u8)(u64)tk.value;
            cb.putByte(dest);
          } break;
          case TokenType::PUSHR: {
            cb << Virtual::Instruction_PUSH;
            tk = Next();
            auto ri = GetRegInfo(tk);
            cb.putRegister(ri);
          } break;
          case TokenType::PUSH: {
            cb << Virtual::Instruction_PUSH;
            auto arg = (tk = Next());
            tk = Next(); // Skip Comma;
            if (tk.type == TokenType::COMMA) {
              tk = Next();
            }
            auto offset = tk;
            MewAssert(offset.type == TokenType::NUMBER);
            cb << ((u64)offset.value);
            PutArg(arg);
          } break;
#pragma endregion PUSH
#pragma region POP
          case TokenType::POP: {
            cb << Virtual::Instruction_POP;
          } break;
          case TokenType::POPR: {
            cb << Virtual::Instruction_RPOP;
            tk = Next();
            auto ri = GetRegInfo(tk);
            cb.putRegister(ri);
          } break;

          case TokenType::CALL: {
            cb << Virtual::Instruction_CALL;
            tk = Next();
            u64 offset = GetFunctionAddress(tk);
            cb.putU64(offset);
          } break;

          case TokenType::CALLE: {
            cb << Virtual::Instruction_CALL;
            tk = Next();
            external_calles.insert({(const char*)tk.value, cb.cursor()});
            cb.putU64(0);
          } break;
#pragma endregion POP
#pragma region MATH
          case TokenType::ADD: {
            cb << Virtual::Instruction_ADD;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::SUB: {
            cb << Virtual::Instruction_SUB;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::MUL: {
            cb << Virtual::Instruction_MUL;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::DIV: {
            cb << Virtual::Instruction_DIV;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::INC: {
            cb << Virtual::Instruction_INC;
            PutArg(tk=Next());
          } break;
          case TokenType::DEC: {
            cb << Virtual::Instruction_DEC;
            PutArg(tk=Next());
          } break;
          case TokenType::NOT: {
            cb << Virtual::Instruction_NOT;
            PutArg(tk=Next());
          } break;
          case TokenType::XOR: {
            cb << Virtual::Instruction_XOR;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::OR: {
            cb << Virtual::Instruction_OR;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::AND: {
            cb << Virtual::Instruction_AND;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::LS: {
            cb << Virtual::Instruction_LS;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::RS: {
            cb << Virtual::Instruction_RS;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
#pragma endregion MATH
#pragma region LOGIC
          case TokenType::JMP: {
            cb << Virtual::Instruction_JMP;
            u64 offset = GetFunctionAddress(tk = Next());
            cb.putU64(offset);
          } break;
          case TokenType::RET: {
            cb << Virtual::Instruction_RET;
          } break;
          case TokenType::EXIT: {
            cb << Virtual::Instruction_EXIT;
          } break;
          case TokenType::TEST: {
            cb << Virtual::Instruction_TEST;
          } break;
          case TokenType::JE: {
            cb << Virtual::Instruction_JE;
            u64 offset = GetFunctionAddress(tk = Next());
            cb.putU64(offset);
          } break;
          case TokenType::JEL: {
            cb << Virtual::Instruction_JEL;
            u64 offset = GetFunctionAddress(tk = Next());
            cb.putU64(offset);
          } break;
          case TokenType::JEM: {
            cb << Virtual::Instruction_JEM;
            u64 offset = GetFunctionAddress(tk = Next());
            cb.putU64(offset);
          } break;
          case TokenType::JNE: {
            cb << Virtual::Instruction_JNE;
            u64 offset = GetFunctionAddress(tk = Next());
            cb.putU64(offset);
          } break;
          case TokenType::JL: {
            cb << Virtual::Instruction_JL;
            u64 offset = GetFunctionAddress(tk = Next());
            cb.putU64(offset);
          } break;
          case TokenType::JM: {
            cb << Virtual::Instruction_JM;
            u64 offset = GetFunctionAddress(tk = Next());
            cb.putU64(offset);
          } break;
          
          case TokenType::MOV: {
            cb << Virtual::Instruction_MOV;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::SWAP: {
            cb << Virtual::Instruction_SWAP;
            PutArg(tk=Next());
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
          } break;
          case TokenType::MSET: {
            cb << Virtual::Instruction_MSET;
            tk = Next();
            u64 start, size, value;
            if (tk.type == TokenType::NAMED_ADDRESS) {
              start = GetNameAddress(tk)->offset;
            } else if (tk.type == TokenType::NUMBER) {
              start = batos32(tk.value);
            } else {
              MewUserAssert(false, "discard arg for mset[0]");
            }
            tk = Next();
            MewUserAssert(tk.type == TokenType::NUMBER, "discard arg for mset[1]");
            size = batos32(tk.value);
            tk = Next();
            MewUserAssert(tk.type == TokenType::NUMBER, "discard arg for mset[2]");
            value = batos32(tk.value);
            cb.putU64(start).putU64(size).putU64(value);
          } break;
#pragma endregion LOGIC
#pragma region ETC
          case TokenType::PUTC: {
            cb << Virtual::Instruction_PUTC;
            tk = Next();
            MewUserAssert(tk.type == TokenType::NUMBER, "discard arg for putc");
            u16 _s = batou16(tk.value);
            cb << _s;
          } break;
          case TokenType::PUTI: {
            cb << Virtual::Instruction_PUTI;
            tk = Next();
            PutArg(tk);
          } break;
          case TokenType::PUTS: {
            cb << Virtual::Instruction_PUTS;
            tk = Next();
            if (tk.type == TokenType::NAMED_ADDRESS) {
              cb.putU64(GetNameAddress(tk)->offset);
            } else if (tk.type == TokenType::NUMBER) {
              cb.putU64(batos32(tk.value));
            } else {
              MewUserAssert(false, "discard arg for puts");
            }
          } break;
          case TokenType::GETCH: {
            cb << Virtual::Instruction_GETCH;
            tk = Next();
            PutArg(tk);
          } break;
#pragma endregion ETC
#pragma region FILE
          case TokenType::WINE: {
            cb << Virtual::Instruction_WINE;
            tk = Next();
            if (tk.type == TokenType::NAMED_ADDRESS) {
              cb.putU64(GetNameAddress(tk)->offset);
            } else if (tk.type == TokenType::NUMBER) {
              cb.putU64(batos32(tk.value));
            } else {
              MewUserAssert(false, "discard arg for wine");
            }
          } break;
          case TokenType::OPEN: {
            cb << Virtual::Instruction_OPEN;
            tk = Next();
            if (tk.type == TokenType::NAMED_ADDRESS) {
              cb.putU64(GetNameAddress(tk)->offset);
            } else if (tk.type == TokenType::NUMBER) {
              cb.putU64(batos32(tk.value));
            } else {
              MewUserAssert(false, "discard arg for wine");
            }
          } break;
          case TokenType::WRITE: {
            cb << Virtual::Instruction_WRITE;
            tk = Next();
            PutArg(tk);
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            MewUserAssert(tk.type == TokenType::NUMBER, "discard arg for write[3](size)");
            cb.putU64((u64)batos32(tk.value));
          } break;
          case TokenType::READ: {
            cb << Virtual::Instruction_READ;
            tk = Next();
            PutArg(tk);
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            PutArg(tk);
            tk = Next();
            if (tk.type == TokenType::COMMA) tk = Next(); 
            MewUserAssert(tk.type == TokenType::NUMBER, "discard arg for read[3](size)");
            cb.putU64((u64)batos32(tk.value));
          } break;
          case TokenType::CLOSE: {
            cb << Virtual::Instruction_CLOSE;
            tk = Next();
            PutArg(tk);
          } break;
#pragma endregion FILE
        }

    }

    static Compiler compile(const char* path) {
      auto __path = mew::GetAbsPath(path);
      auto __folder = __path.parent_path();
      const char* src = mew::ReadFullFile(__path); 
      Lexer lexer;
      lexer.Tokenize(src, __folder);
      Compiler comp(lexer);
      comp.compile();
      return comp;
    }

    void save(const char* path) {
      Virtual::Isolate::CreateFileIfNotExists(path);
      auto code = *cb;
      Virtual::Code_SaveToFile(*code, path);
    }

    Virtual::Code* operator*(int) {
      return *cb;
    }
    Virtual::Code* operator*() {
      return *cb;
    }

    static Virtual::CodeBuilder CompileString(const char* src) {
      auto comp = compile(src);
      return comp.cb;
    }
    
    // void debugSave(const char* path) {
    //   Virtual::Isolate::CreateFileIfNotExists(path);
    //   auto code = *cb;
    //   auto __path = mew::GetAbsPath(path);
    //   std::ofstream file(__path, std::ios::out | std::ios::binary);
    //   file.seekp(std::ios::beg);
    //   const char* debug_manifest = GetAdditionalData();
    //   file << debug_manifest;
    //   Virtual::Code_SaveToFile(*code, file);
    //   file.close(); 
    // }

    // static Compiler debugRead(const char* path) {
    //   auto __path = mew::GetAbsPath(path);
    //   std::ifstream file(__path, std::ios::out | std::ios::binary);
    //   MewForUserAssert(file.is_open(), "cant open file(%s)", path);
    //   Compiler comp;
    //   const char* buffer = mew::ReadFullFile(path);
    //   u64 offset = comp.ReadAdditionalData(buffer);
    //   file.seekp((const std::ios_base::seekdir)offset);
    //   Virtual::Code* code = Virtual::Code_LoadFromFile(file);
    //   comp.cb = Virtual::CodeBuilder(code);
    //   file.close();
    // }

    // static Compiler debugRead(std::ifstream& file) {
    //   Compiler comp;
    //   const char* buffer = mew::ReadFullFile(path);
    //   u64 offset = comp.ReadAdditionalData(buffer);
    //   file.seekp((const std::ios_base::seekdir)offset);
    //   Virtual::Code* code = Virtual::Code_LoadFromFile(file);
    //   comp.cb = Virtual::CodeBuilder(code);
    // }
    
};

}
#endif