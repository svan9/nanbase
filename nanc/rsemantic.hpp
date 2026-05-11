#ifndef NANC_RSEMANTIC
#define NANC_RSEMANTIC

#include "lsemantic.hpp"
#include "mewstack"
#include "mewstring"
#include "mewtypes.h"

namespace nanc::r {
enum struct ContextType {
  Undefined,
  Line,
  Struct,
  Expression,
  Typedef,
  Name,
  Operator,
  Block,
  Function,
  Tuple,
  Flag,
  Declare,
  Type,
  Bin,
  OperatorFunction,
  Entry,
  Call,
};

struct Context {
  ContextType type;
  u8* value;

  template <typename T>
  T* get() {
    return (T*)value;
  }

  template <typename T>
  static Context pack(T value) {
    Context ctx;
    ctx.type = value.getType();
    ctx.value = mew::pack(value);
    return ctx;
  }
};

struct Ctx4Struct {
  virtual ContextType getType() {
    return ContextType::Undefined;
  }
};

struct ExpressionContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Expression;
  }
  mew::stack<Context> contexts;
};

struct NameContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Name;
  }
  NameContext() {}
  NameContext(const char* n) {
    path.push(n);
  }
  NameContext(mew::str s) {
    auto c = s.copy();
    c.replace(".", "2");
    path.push(c);
  }
  mew::stack<mew::str> path;

  static mew::str parse(mew::str s) {
    auto c = s.copy();
    c.replace(".", "2");
    return c;
  }
  
  mew::str getName() {
    mew::str _str;
    for (u64 i = 0; i < path.count(); ++i) {
      _str += path.at(i);
      _str += '1';
    }
    return _str;
  }
};

struct TypedefContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Typedef;
  }
  NameContext name;
  mew::stack<Context> value;
};

struct OperatorContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Operator;
  }
  NameContext lhm;
  l::TokenType ops[3] = {l::TokenType::Undefined};
  NameContext rhm;
};

struct LineContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Line;
  }
  ExpressionContext ctx;
};

struct BlockContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Block;
  }
  mew::stack<LineContext> lines;
};

struct TypeContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Type;
  }
  NameContext name;
  TypedefContext* ref = nullptr;
  mew::stack<NameContext> generic;
  u64 c_size = (u64)-1;
  TypeContext* pointer_next;
  bytepartf(is_null)
  bytepartf(is_const)
  bytepartf(is_template)
  bytepartf(is_buildin)
  bytepartf(is_signed)
  bytepartf(is_reference)
  bytepartf(is_pointer)
};

struct DeclareContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Declare;
  }
  TypeContext type;
  NameContext name;
};

struct TupleContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Tuple;
  }
  mew::stack<DeclareContext> content;
};

struct FlagContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Flag;
  }
  mew::stack<const char*> names;
};

struct FunctionContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Function;
  }
  mew::stack<NameContext> generic;
  NameContext name;
  NameContext type;
  TupleContext args;
  FlagContext flags;
  BlockContext body;
};

struct OperatorFunctionContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::OperatorFunction;
  }
  l::TokenType name[3] = {l::TokenType::Undefined};
  mew::stack<NameContext> generic;
  NameContext type;
  TupleContext args;
  FlagContext flags;
  BlockContext body;
};

struct EntryContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Entry;
  }
  BlockContext body;
};

struct BinContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Bin;
  }
  mew::str content;
};

struct StructContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Struct;
  }
  mew::stack<NameContext> generic;
  NameContext name;
  mew::stack<DeclareContext> fields;
  mew::stack<FunctionContext> methods;
  mew::stack<OperatorFunctionContext> operators;
};

struct CallContext : Ctx4Struct {
  ContextType getType() override {
    return ContextType::Call;
  }
  NameContext name;
  TupleContext args;
};


static std::unordered_map<mew::str, TypeContext> buildin_types;
// static std::unordered_map<mew::str, FunctionContext> buildin_functions;
// static std::unordered_map<mew::str, StructContext> buildin_structs;

void pushBuildInType(NameContext s, u64 size, bool is_signed, bool is_null = false) {
  TypeContext ctx;
  ctx.c_size = size;
  ctx.name = s;
  ctx.is_signed = is_signed;
  ctx.is_null = is_null;
  ctx.is_buildin = true;
  auto name = s.getName();
  buildin_types.insert({name, ctx});
}

// StructContext* pushBuildInStruct(StructContext ctx, NameContext s) {
//   auto name = s.getName();
//   return &buildin_structs.insert({name, ctx}).first->second;
// }

// FunctionContext* pushBuildInFunction(NameContext s) {
//   FunctionContext ctx;
//   ctx.name = s;
//   auto name = s.getName();
//   return &buildin_functions.insert({name, ctx}).first->second;
// }

void __buildin__initTypeTable() {
  pushBuildInType("byte", 1, 1);
  pushBuildInType("__buildin__.unsigned_integer", 1, 0);
  pushBuildInType("__buildin__.signed_integer", 1, 1);
  pushBuildInType("__buildin__.stack_offset", 8, 1);
  pushBuildInType("null", 1, 1, 1);
}

// void __buildin__initFunctionTable() {
//   FunctionContext* ctx;
//   ctx = pushBuildInFunction("__buildin__.stack_offset_get");

//   ctx = pushBuildInFunction("__buildin__.epilogue");
//   ctx = pushBuildInFunction("__buildin__.prologue");
//   ctx = pushBuildInFunction("__buildin__.push3__buildin__.stack_offset");
//   DeclareContext dc;
//   dc.type = buildin_types.at("__buildin__.stack_offset");
//   dc.name = NameContext("a");
//   ctx->args.content.push(dc);
//   ctx = pushBuildInFunction("__buildin__.push3byte");
//   DeclareContext dc;
//   dc.type = buildin_types.at("byte");
//   dc.name = NameContext("a");
//   ctx->args.content.push(dc);
// }



}  // namespace nanc::r

#endif