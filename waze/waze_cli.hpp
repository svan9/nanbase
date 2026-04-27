#pragma once
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "virtual.hpp"

namespace waze_cli {

using namespace Virtual;

// --- Токенизатор строки --------------------------------------
std::vector<std::string> tokenize(const std::string& line) {
  std::vector<std::string> tokens;
  std::istringstream ss(line);
  std::string token;
  while (ss >> std::quoted(token)) {
    tokens.push_back(token);
  }
  return tokens;
}

VM_RegType parse_regtype(const std::string& s) {
  if (s == "R") return VM_RegType::R;
  if (s == "RX") return VM_RegType::RX;
  if (s == "DX") return VM_RegType::DX;
  if (s == "FX") return VM_RegType::FX;
  if (s == "RDI") return VM_RegType::RDI;
  return VM_RegType::None;
}

const char* regtype_str(VM_RegType t) {
  switch (t) {
    case VM_RegType::R:
      return "R";
    case VM_RegType::RX:
      return "RX";
    case VM_RegType::DX:
      return "DX";
    case VM_RegType::FX:
      return "FX";
    case VM_RegType::RDI:
      return "RDI";
    default:
      return "?";
  }
}

// --- Запись истории для редактора ----------------------------
struct HistoryEntry {
  std::string raw;  // оригинальная строка
};

// --- Применить одну команду к Waze ---------------------------
// Возвращает false если команда не распознана
bool apply_command(waze::Waze& w,
                   const std::vector<std::string>& tok) {
  if (tok.empty()) return true;
  const std::string& cmd = tok[0];

  // make "str" label
  // make "str" call
  // make "str" goto / gotoLabel
  // make "str" puts
  // make "str" putc
  // make "str" wine
  // make "str" open
  if (cmd == "make") {
    if (tok.size() < 3) {
      printf("usage: make <value> <command>\n");
      return false;
    }
    const std::string& val = tok[1];
    const std::string& sub = tok[2];

    if (sub == "label") {
      w << waze::make(val.c_str()) << waze::label4vm;
      return true;
    }
    if (sub == "call") {
      w << waze::make(val.c_str()) << waze::call4vm;
      return true;
    }
    if (sub == "goto" ||
        sub == "gotoLabel") {
      w << waze::make(val.c_str()) << waze::gotoLabel4vm;
      return true;
    }
    if (sub == "puts") {
      w << waze::make(val.c_str()) << waze::puts4vm;
      return true;
    }
    if (sub == "wine") {
      w << waze::make(val.c_str()) << waze::wine4vm;
      return true;
    }
    if (sub == "open") {
      w << waze::make(val.c_str()) << waze::open4vm;
      return true;
    }

    printf("unknown make subcommand: %s\n", sub.c_str());
    return false;
  }

  // make_reg RX 0 inc
  // make_reg RX 0 dec
  // make_reg RX 0 puti
  // make_reg RX 0 pushr
  // make_reg RX 0 popr
  // make_reg RX 0 close
  if (cmd == "make_reg") {
    if (tok.size() < 4) {
      printf("usage: make_reg <type> <idx> <command>\n");
      return false;
    }
    VM_RegType rt = parse_regtype(tok[1]);
    byte idx = (byte)std::stoi(tok[2]);
    const std::string& sub = tok[3];

    if (sub == "inc") {
      w << waze::make_reg(rt, idx) << waze::inc4vm;
      return true;
    }
    if (sub == "dec") {
      w << waze::make_reg(rt, idx) << waze::dec4vm;
      return true;
    }
    if (sub == "puti") {
      w << waze::make_reg(rt, idx) << waze::puti4vm;
      return true;
    }
    if (sub == "pushr") {
      w << waze::make_reg(rt, idx) << waze::pushr4vm;
      return true;
    }
    if (sub == "popr") {
      w << waze::make_reg(rt, idx) << waze::popr4vm;
      return true;
    }
    if (sub == "close") {
      w << waze::make_reg(rt, idx) << waze::close4vm;
      return true;
    }

    // math: make_reg RX 0 add RX 1
    if ((sub == "add" || sub == "sub" || sub == "mul" || sub == "div" ||
         sub == "mov" || sub == "xor" || sub == "or" || sub == "and") &&
        tok.size() >= 6) {
      VM_RegType rt2 = parse_regtype(tok[4]);
      byte idx2 = (byte)std::stoi(tok[5]);
      w << waze::make_reg(rt, idx);
      w << waze::make_reg(rt2, idx2);
      if (sub == "add") {
        w << waze::add4vm;
        return true;
      }
      if (sub == "sub") {
        w << waze::sub4vm;
        return true;
      }
      if (sub == "mul") {
        w << waze::mul4vm;
        return true;
      }
      if (sub == "div") {
        w << waze::div4vm;
        return true;
      }
      if (sub == "mov") {
        w << waze::mov4vm;
        return true;
      }
      if (sub == "xor") {
        w << waze::xor4vm;
        return true;
      }  // если есть
    }

    printf("unknown make_reg subcommand: %s\n", sub.c_str());
    return false;
  }

  if (cmd == "ret") {
    w << waze::ret4vm;
    return true;
  }
  if (cmd == "exit") {
    w << waze::exit4vm;
    return true;
  }
  if (cmd == "pop") {
    w << waze::pop4vm;
    return true;
  }
  if (cmd == "test") {
    w << waze::test4vm;
    return true;
  }
  if (cmd == "putEntry" ||
      cmd == "entry") {
    w << waze::putEntry4vm;
    return true;
  }

  // make_num N pushn
  if (cmd == "pushn") {
    if (tok.size() < 2) {
      printf("usage: pushn <number>\n");
      return false;
    }
    s32 num = std::stoi(tok[1]);
    w << waze::make((s32)num) << waze::pushn4vm;
    return true;
  }

  // je / jne / jl / jm / jel / jem <label>
  if (cmd == "je" && tok.size() >= 2) {
    w << waze::make(tok[1].c_str()) << waze::je4vm;
    return true;
  }
  if (cmd == "jne" && tok.size() >= 2) {
    w << waze::make(tok[1].c_str()) << waze::jne4vm;
    return true;
  }
  if (cmd == "jl" && tok.size() >= 2) {
    w << waze::make(tok[1].c_str()) << waze::jl4vm;
    return true;
  }
  if (cmd == "jm" && tok.size() >= 2) {
    w << waze::make(tok[1].c_str()) << waze::jm4vm;
    return true;
  }
  if (cmd == "jel" && tok.size() >= 2) {
    w << waze::make(tok[1].c_str()) << waze::jel4vm;
    return true;
  }
  if (cmd == "jem" && tok.size() >= 2) {
    w << waze::make(tok[1].c_str()) << waze::jem4vm;
    return true;
  }

  printf("unknown command: %s\n", cmd.c_str());
  return false;
}

// --- Вывод истории -------------------------------------------
void print_history(const std::vector<HistoryEntry>& history) {
  printf("\n  %-4s  %s\n", "LINE", "COMMAND");
  printf("  ----  -------------------------\n");
  for (size_t i = 0; i < history.size(); ++i) {
    printf("  %-4zu  %s\n", i + 1, history[i].raw.c_str());
  }
  printf("\n");
}

// вспомогательная — строит Code из истории, автодобавляя entry/exit
Code* build_from_history(std::vector<HistoryEntry>& history) {
  waze::Waze tmp;

  bool has_entry = false;
  bool has_exit  = false;

  for (auto& h : history) {
    auto t = tokenize(h.raw);
    if (!t.empty() && (t[0] == "putEntry" || t[0] == "entry")) has_entry = true;
    if (!t.empty() && t[0] == "exit")                           has_exit  = true;
  }

  if (!has_entry) {
    tmp << waze::putEntry4vm;
  }

  for (auto& h : history) {
    auto t = tokenize(h.raw);
    apply_command(tmp, t);
  }

  if (!has_exit) {
    tmp << waze::exit4vm;
  }

  return tmp.build();
}

// --- REPL ----------------------------------------------------
void run_repl() {
  printf("+======================================+\n");
  printf("|     waze CLI  -  nanvm assembler     |\n");
  printf("|  type 'help' for commands list       |\n");
  printf("+======================================+\n\n");

  waze::Waze w;
  std::vector<HistoryEntry> history;
  std::string line;
  bool running = true;

  while (running) {
    printf("> ");
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    auto tok = tokenize(line);
    if (tok.empty()) continue;
    const std::string& cmd = tok[0];

    // -- Мета-команды ----------------------------------------

    if (cmd == "help") {
      printf("\n  Assembly commands:\n");
      printf("    make \"str\" label|call|goto|puts|wine|open\n");
      printf("    make_reg <R|RX|DX|FX> <idx> inc|dec|puti|pushr|popr\n");
      printf("    make_reg <R|RX|DX|FX> <idx> add|sub|mul|div <R|RX|DX|FX> <idx>\n");
      printf("    pushn <number>\n");
      printf("    ret | exit | pop | test | putEntry\n");
      printf("    je|jne|jl|jm|jel|jem <label>\n");
      printf("\n  Control commands:\n");
      printf("    run                - execute current code\n");
      printf("    save \"path\"        - save .nb file\n");
      printf("    list               - show history\n");
      printf("    edit <line>        - replace line N\n");
      printf("    delete <line>      - delete line N\n");
      printf("    reset              - clear everything\n");
      printf("    disasm             — show bytecode + data dump\n");
      printf("    quit               - exit\n\n");
      continue;
    }

    if (cmd == "quit" || cmd == "exit_cli" || cmd == "q") {
      running = false;
      continue;
    }

    if (cmd == "list") {
      print_history(history);
      continue;
    }

    if (cmd == "reset") {
      w = waze::Waze();
      history.clear();
      printf("  reset ok\n");
      continue;
    }

    if (cmd == "run") {
      try {
        waze::Waze tmp;

        // проверяем есть ли putEntry в истории
        bool has_entry = false;
        for (auto& h : history) {
          auto t = tokenize(h.raw);
          if (!t.empty() && (t[0] == "putEntry" || t[0] == "entry")) {
            has_entry = true;
            break;
          }
        }

        // если нет — вставляем в начало
        if (!has_entry) {
          w << waze::putEntry4vm;
          history.insert(history.begin(), {"putEntry"});
          // перестраиваем tmp из обновлённой истории
        }

        for (auto& h : history) {
          auto t = tokenize(h.raw);
          apply_command(tmp, t);
        }

        // проверяем есть ли exit в истории
        bool has_exit = false;
        for (auto& h : history) {
          auto t = tokenize(h.raw);
          if (!t.empty() && t[0] == "exit") {
            has_exit = true;
            break;
          }
        }
        if (!has_exit) {
          tmp << waze::exit4vm;
        }

        Code* code = tmp.build();

        printf("  running...\n-------------------------\n");
        auto t0 = std::chrono::high_resolution_clock::now();
        Execute(*code);
        auto t1 = std::chrono::high_resolution_clock::now();

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        auto mcs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

        printf("\n-------------------------\n");
        printf("  done\n");
        printf("  time: %lld ms (%lld us)\n", ms, mcs);

      } catch (std::exception& e) {
        printf("  error: %s\n", e.what());
      }
      continue;
    }

    if (cmd == "disasm") {
      try {
        Code* code = w.build();
        byte* bc = (byte*)code->playground;
        u64 sz = code->capacity;

        printf("\n  Code size: %llu bytes\n", sz);
        printf("  %-6s  %-4s  %s\n", "OFFSET", "HEX", "INSTR");
        printf("  ------  ----  ---------------------------\n");

        u64 i = 0;
        while (i < sz) {
          byte b = bc[i];
          // имя инструкции
          const char* name = "???";
          switch ((Instruction)b) {
            case Instruction_NONE:
              name = "NONE";
              break;
            case Instruction_CALL:
              name = "CALL";
              break;
            case Instruction_PUSH:
              name = "PUSH";
              break;
            case Instruction_POP:
              name = "POP";
              break;
            case Instruction_RPOP:
              name = "RPOP";
              break;
            case Instruction_ADD:
              name = "ADD";
              break;
            case Instruction_SUB:
              name = "SUB";
              break;
            case Instruction_MUL:
              name = "MUL";
              break;
            case Instruction_DIV:
              name = "DIV";
              break;
            case Instruction_INC:
              name = "INC";
              break;
            case Instruction_DEC:
              name = "DEC";
              break;
            case Instruction_XOR:
              name = "XOR";
              break;
            case Instruction_OR:
              name = "OR";
              break;
            case Instruction_NOT:
              name = "NOT";
              break;
            case Instruction_AND:
              name = "AND";
              break;
            case Instruction_LS:
              name = "LS";
              break;
            case Instruction_RS:
              name = "RS";
              break;
            case Instruction_NUM:
              name = "NUM";
              break;
            case Instruction_BYTE:
              name = "BYTE";
              break;
            case Instruction_MEM:
              name = "MEM";
              break;
            case Instruction_REG:
              name = "REG";
              break;
            case Instruction_ST:
              name = "ST";
              break;
            case Instruction_JMP:
              name = "JMP";
              break;
            case Instruction_RET:
              name = "RET";
              break;
            case Instruction_EXIT:
              name = "EXIT";
              break;
            case Instruction_TEST:
              name = "TEST";
              break;
            case Instruction_JE:
              name = "JE";
              break;
            case Instruction_JEL:
              name = "JEL";
              break;
            case Instruction_JEM:
              name = "JEM";
              break;
            case Instruction_JNE:
              name = "JNE";
              break;
            case Instruction_JL:
              name = "JL";
              break;
            case Instruction_JM:
              name = "JM";
              break;
            case Instruction_MOV:
              name = "MOV";
              break;
            case Instruction_SWAP:
              name = "SWAP";
              break;
            case Instruction_MSET:
              name = "MSET";
              break;
            case Instruction_PUTC:
              name = "PUTC";
              break;
            case Instruction_PUTI:
              name = "PUTI";
              break;
            case Instruction_PUTS:
              name = "PUTS";
              break;
            case Instruction_GETCH:
              name = "GETCH";
              break;
            case Instruction_WINE:
              name = "WINE";
              break;
            case Instruction_OPEN:
              name = "OPEN";
              break;
            case Instruction_CLOSE:
              name = "CLOSE";
              break;
            case Instruction_WRITE:
              name = "WRITE";
              break;
            case Instruction_READ:
              name = "READ";
              break;
            case Instruction_MOVRDI:
              name = "MOVRDI";
              break;
            default:
              name = "RAW";
              break;
          }

          // печатаем строку: offset | hex | name | операнды
          printf("  0x%04llx  0x%02X  %s", i, b, name);

          // декодируем операнды для читаемости
          switch ((Instruction)b) {
            case Instruction_JMP:
            case Instruction_JE:
            case Instruction_JNE:
            case Instruction_JL:
            case Instruction_JM:
            case Instruction_JEL:
            case Instruction_JEM:
            case Instruction_CALL: {
              if (i + 8 < sz) {
                u64 offset = 0;
                memcpy(&offset, bc + i + 1, sizeof(u64));
                printf("  -> 0x%llx", offset);
                i += 8;
              }
            } break;

            case Instruction_PUTS:
            case Instruction_WINE:
            case Instruction_OPEN: {
              if (i + 8 < sz) {
                u64 offset = 0;
                memcpy(&offset, bc + i + 1, sizeof(u64));
                printf("  data[0x%llx]", offset);
                i += 8;
              }
            } break;

            case Instruction_REG: {
              if (i + 2 < sz) {
                byte rtype = bc[i + 1];
                byte ridx = bc[i + 2];
                const char* rt = "?";
                switch ((VM_RegType)rtype) {
                  case VM_RegType::R:
                    rt = "r";
                    break;
                  case VM_RegType::RX:
                    rt = "rx";
                    break;
                  case VM_RegType::DX:
                    rt = "dx";
                    break;
                  case VM_RegType::FX:
                    rt = "fx";
                    break;
                  case VM_RegType::RDI:
                    rt = "rdi";
                    break;
                  default:
                    break;
                }
                printf("  %s%d", rt, ridx);
                i += 2;
              }
            } break;

            case Instruction_NUM: {
              if (i + 4 < sz) {
                s32 num = 0;
                memcpy(&num, bc + i + 1, sizeof(s32));
                printf("  %d", num);
                i += 4;
              }
            } break;

            case Instruction_PUTC: {
              if (i + 2 < sz) {
                u16 ch = 0;
                memcpy(&ch, bc + i + 1, sizeof(u16));
                printf("  '%lc'", (wchar_t)ch);
                i += 2;
              }
            } break;

            default:
              break;
          }

          printf("\n");
          ++i;
        }

        // дамп данных
        if (code->data_size > 0) {
          printf("\n  Data segment: %llu bytes\n", code->data_size);
          printf("  %-6s  %-24s  %s\n", "OFFSET", "HEX", "ASCII");
          printf("  ------  ------------------------  ----------------\n");
          for (u64 d = 0; d < code->data_size; d += 16) {
            printf("  0x%04llx  ", d);
            for (u64 j = d; j < d + 16; ++j) {
              if (j < code->data_size)
                printf("%02X ", code->data[j]);
              else
                printf("   ");
            }
            printf("  ");
            for (u64 j = d; j < d + 16 && j < code->data_size; ++j) {
              byte c = code->data[j];
              printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
            }
            printf("\n");
          }
        }
        printf("\n");

        // Перестраиваем waze
        w = waze::Waze();
        for (auto& h : history) {
          auto t = tokenize(h.raw);
          apply_command(w, t);
        }
      } catch (std::exception& e) {
        printf("  error: %s\n", e.what());
      }
      continue;
    }

    if (cmd == "save") {
      if (tok.size() < 2) {
        printf("usage: save \"path\"\n");
        continue;
      }
      try {
        Code* code = w.build();
        Code_SaveToFile(*code, tok[1].c_str());
        printf("  saved to %s\n", tok[1].c_str());
      } catch (std::exception& e) {
        printf("  error: %s\n", e.what());
      }
      // Перестраиваем waze из истории
      w = waze::Waze();
      for (auto& h : history) {
        auto t = tokenize(h.raw);
        apply_command(w, t);
      }
      continue;
    }

    if (cmd == "edit") {
      if (tok.size() < 2) {
        printf("usage: edit <line>\n");
        continue;
      }
      int lineno = std::stoi(tok[1]) - 1;
      if (lineno < 0 || lineno >= (int)history.size()) {
        printf("  line %d out of range (1..%zu)\n", lineno + 1, history.size());
        continue;
      }
      printf("  editing line %d: %s\n", lineno + 1, history[lineno].raw.c_str());
      printf("  new> ");
      std::string newline;
      if (!std::getline(std::cin, newline) || newline.empty()) continue;

      auto newt = tokenize(newline);
      // Проверяем что команда валидна через временный waze
      waze::Waze tmp;
      if (!apply_command(tmp, newt)) {
        printf("  invalid command, edit cancelled\n");
        continue;
      }

      history[lineno].raw = newline;

      // Перестраиваем waze из истории
      w = waze::Waze();
      for (auto& h : history) {
        auto t = tokenize(h.raw);
        apply_command(w, t);
      }
      printf("  line %d updated\n", lineno + 1);
      continue;
    }

    if (cmd == "delete") {
      if (tok.size() < 2) {
        printf("usage: delete <line>\n");
        continue;
      }
      int lineno = std::stoi(tok[1]) - 1;
      if (lineno < 0 || lineno >= (int)history.size()) {
        printf("  line %d out of range\n", lineno + 1);
        continue;
      }
      history.erase(history.begin() + lineno);
      w = waze::Waze();
      for (auto& h : history) {
        auto t = tokenize(h.raw);
        apply_command(w, t);
      }
      printf("  line %d deleted\n", lineno + 1);
      continue;
    }

    // -- Обычная команда сборки -------------------------------
    if (apply_command(w, tok)) {
      history.push_back({line});
    }
  }

  printf("  bye\n");
}

}  // namespace waze_cli
