#ifndef NANC_LSEMANTIC
#define NANC_LSEMANTIC

#include <unordered_map>

namespace nanc::l {
enum struct TokenType {
  Undefined,
  Text,                  // text
  String,                // "String"
  CharString,            // 'c'
  Number,                // [0-9]+
  OpenRoundBreaket,      // (
  CloseRoundBreaket,     // )
  OpenSquareBreaket,     // [
  CloseSquareBreaket,    // ]
  OpenTriangleBreaket,   // <
  CloseTriangleBreaket,  // >
  OpenFigureBreaket,     // {
  CloseFigureBreaket,    // }
  Plus,                  // +
  Minus,                 // -
  Star,                  // *
  Slash,                 // /
  Equal,                 // =
  BackSlash,             /* \ */
  VerticalBar,           // |
  Dot,                   // .
  Comma,                 // ,
  Colon,                 // :
  SemiColon,             // ;
  Backtick,              // `
  Underscore,            // _
  Percent,               // %
  Sharp,                 // #
  Tilde,                 //'~'
  Caret,                 // ^
  Dollar,                // $
  At,                    // @
  ExclamationMark,       // !
  QuestionMark,          // ?
  // keywords
  KW_Struct,
  KW_Void,
  KW_Operator,
  KW_Extra,
  KW_Type,
  KW_Expand,  // reserved
  KW_Narrow,  // reserved
  KW_Class,
  KW_Throw,
  KW_For,
  KW_While,
  KW_Break,
  KW_Template,  // reserved
  KW_Entry,
  KW_Null,
  KW_Important,
  KW_Bin,
};

static std::unordered_map<const char*, TokenType> token_semantic = {
    {"(", TokenType::OpenRoundBreaket},
    {")", TokenType::CloseRoundBreaket},
    {"[", TokenType::OpenSquareBreaket},
    {"]", TokenType::CloseSquareBreaket},
    {"<", TokenType::OpenTriangleBreaket},
    {">", TokenType::CloseTriangleBreaket},
    {"{", TokenType::OpenFigureBreaket},
    {"}", TokenType::CloseFigureBreaket},
    {"+", TokenType::Plus},
    {"-", TokenType::Minus},
    {"*", TokenType::Star},
    {"/", TokenType::Slash},
    {"= ", TokenType::Equal},
    {"\\", TokenType::BackSlash},
    {"|", TokenType::VerticalBar},
    {".", TokenType::Dot},
    {",", TokenType::Comma},
    {":", TokenType::Colon},
    {";", TokenType::SemiColon},
    {"`", TokenType::Backtick},
    {"_", TokenType::Underscore},
    {"%", TokenType::Percent},
    {"#", TokenType::Sharp},
    {"~'", TokenType::Tilde},
    {"^", TokenType::Caret},
    {"$", TokenType::Dollar},
    {"@", TokenType::At},
    {"!", TokenType::ExclamationMark},
    {"?", TokenType::QuestionMark},
  // kws
    {"struct", TokenType::KW_Struct},
    {"void", TokenType::KW_Void},
    {"operator", TokenType::KW_Operator},
    {"extra", TokenType::KW_Extra},
    {"type", TokenType::KW_Type},
    {"expand", TokenType::KW_Expand},
    {"narrow", TokenType::KW_Narrow},
    {"class", TokenType::KW_Class},
    {"throw", TokenType::KW_Throw},
    {"for", TokenType::KW_For},
    {"while", TokenType::KW_While},
    {"break", TokenType::KW_Break},
    {"template", TokenType::KW_Template},
    {"entry", TokenType::KW_Entry},
    {"null", TokenType::KW_Null},
    {"important", TokenType::KW_Important},
    {"bin", TokenType::KW_Bin},

};
}  // namespace nanc::l

#endif