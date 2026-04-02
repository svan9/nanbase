#ifndef NANASM_DEFS_COMPILER_HPP
#define NANASM_DEFS_COMPILER_HPP

#include <unordered_map>
#include "mewtypes.h"
#include "mewlib.h"

namespace nanasm {
  enum struct TokenType: u16 {
    Undefined,
    LABEL,
    R1,  R2,	R3,	 R4,  R5,
    RX1, RX2, RX3, RX4, RX5,
    DX1, DX2, DX3, DX4, DX5,
    FX1, FX2, FX3, FX4, FX5,
    RDI, NUMBER, BYTE, WORD,
    DATA, STRING, ARRAY, NAMED_ADDRESS,
    COMMA,
    PUSHN, PUSHM, PUSHB, PUSHR, PUSH,
    POP, POPR,
    CALL, CALLE,
    ADD, SUB, MUL, DIV, INC, DEC, NOT, XOR, OR, AND, LS, RS,
    JMP, RET, EXIT, TEST, JE, JEL, JEM, JNE, JL, JM, MOV, SWAP, MSET,
    PUTC, PUTI, PUTS, GETCH,
    WINE, OPEN, WRITE, READ, CLOSE, IMPORT,
  };

static std::unordered_map<const char *, TokenType> token_semantic = {
    {"r1", TokenType::R1},
    {"r2", TokenType::R2},
    {"r3", TokenType::R3},
    {"r4", TokenType::R4},
    {"r5", TokenType::R5},
    {"fx1", TokenType::FX1},
    {"fx2", TokenType::FX2},
    {"fx3", TokenType::FX3},
    {"fx4", TokenType::FX4},
    {"fx5", TokenType::FX5},
    {"dx1", TokenType::DX1},
    {"dx2", TokenType::DX2},
    {"dx3", TokenType::DX3},
    {"dx4", TokenType::DX4},
    {"dx5", TokenType::DX5},
    {"rx1", TokenType::RX1},
    {"rx2", TokenType::RX2},
    {"rx3", TokenType::RX3},
    {"rx4", TokenType::RX4},
    {"rx5", TokenType::RX5},
    {"data", TokenType::DATA},
    {",", TokenType::COMMA},
    {"pushn", TokenType::PUSHN},
    {"pushm", TokenType::PUSHM},
    {"pushb", TokenType::PUSHB},
    {"pushr", TokenType::PUSHR},
    {"push", TokenType::PUSH},
    {"pop", TokenType::POP},
    {"popr", TokenType::POPR},
    {"call", TokenType::CALL},
    {"calle", TokenType::CALLE},
    {"add", TokenType::ADD},
    {"sub", TokenType::SUB},
    {"mul", TokenType::MUL},
    {"div", TokenType::DIV},
    {"inc", TokenType::INC},
    {"dec", TokenType::DEC},
    {"not", TokenType::NOT},
    {"xor", TokenType::XOR},
    {"or", TokenType::OR},
    {"and", TokenType::AND},
    {"ls", TokenType::LS},
    {"rs", TokenType::RS},
    {"jmp", TokenType::JMP},
    {"ret", TokenType::RET},
    {"exit", TokenType::EXIT},
    {"test", TokenType::TEST},
    {"je", TokenType::JE},
    {"jel", TokenType::JEL},
    {"jem", TokenType::JEM},
    {"jne", TokenType::JNE},
    {"jl", TokenType::JL},
    {"jm", TokenType::JM},
    {"mov", TokenType::MOV},
    {"swap", TokenType::SWAP},
    {"mset", TokenType::MSET},
    {"mset", TokenType::MSET},
    {"putc", TokenType::PUTC},
    {"puti", TokenType::PUTI},
    {"puti", TokenType::PUTI},
    {"puts", TokenType::PUTS},
    {"getch", TokenType::GETCH},
    {"wine", TokenType::WINE},
    {"open", TokenType::OPEN},
    {"mov", TokenType::MOV},
    {"write", TokenType::WRITE},
    {"read", TokenType::READ},
    {"close", TokenType::CLOSE},
    {"import", TokenType::IMPORT},

  };

  struct DataValue {
    u8* value; // string or array of bytes
    u64 size;
  };

  struct DataViewValue {
    u64 offset;
    u64 size;
  };

  struct Token {
    TokenType type;
    u8* value;

    Token() {}
    Token(TokenType type, u8* value): type(type), value(value) {}

    template<typename T>
    Token(TokenType type, T value): type(type), value((u8*)value) {}
  
    static Token make(TokenType type, u8* value) {
      Token tk(type, value);
      return tk;
    }
    
    template<typename T>
    static Token make(TokenType type, T* value) {
      Token tk(type, (u8*)value);
      return tk;
    }
  };

  inline Token* GetTokenFromTable(const char* word) {
		bool is_find = false;
		for (auto it = token_semantic.begin(); it != token_semantic.end(); ++it) {
			if (mew::strcmp(it->first, word)) {
				Token* tk = new Token;
				tk->type = it->second;
        tk->value = nullptr;
        return tk;
			}
		}
		return nullptr;
	}
}

#endif