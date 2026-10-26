#include "common.h"
#include "scanner.h"

// ch.16: the scanner turns source text into tokens, on demand, without
// allocating anything: a token just points into the source string.
typedef struct {
  const char* start;
  const char* current;
  int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool isDigit(char c) { return c >= '0' && c <= '9'; }
static bool isAtEnd() { return *scanner.current == '\0'; }
static char advance() { scanner.current++; return scanner.current[-1]; }
static char peek() { return *scanner.current; }
static char peekNext() { return isAtEnd() ? '\0' : scanner.current[1]; }

static bool match(char expected) {
  if (isAtEnd() || *scanner.current != expected) return false;
  scanner.current++;
  return true;
}

static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

static Token errorToken(const char* message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

static void skipWhitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ': case '\r': case '\t': advance(); break;
      case '\n': scanner.line++; advance(); break;
      case '/':
        if (peekNext() == '/') {
          while (peek() != '\n' && !isAtEnd()) advance();  // comment to end of line
        } else {
          return;
        }
        break;
      default: return;
    }
  }
}

// The book builds a tiny trie; a table lookup does the same job in fewer lines.
static TokenType identifierType() {
  static const struct { const char* word; TokenType type; } keywords[] = {
    {"and", TOKEN_AND}, {"class", TOKEN_CLASS}, {"else", TOKEN_ELSE}, {"false", TOKEN_FALSE},
    {"for", TOKEN_FOR}, {"fun", TOKEN_FUN}, {"if", TOKEN_IF}, {"nil", TOKEN_NIL},
    {"or", TOKEN_OR}, {"print", TOKEN_PRINT}, {"return", TOKEN_RETURN}, {"super", TOKEN_SUPER},
    {"this", TOKEN_THIS}, {"true", TOKEN_TRUE}, {"var", TOKEN_VAR}, {"while", TOKEN_WHILE},
  };
  int length = (int)(scanner.current - scanner.start);
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if ((int)strlen(keywords[i].word) == length &&
        memcmp(scanner.start, keywords[i].word, length) == 0) {
      return keywords[i].type;
    }
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) advance();
  return makeToken(identifierType());
}

static Token number() {
  while (isDigit(peek())) advance();
  if (peek() == '.' && isDigit(peekNext())) {
    advance();  // the dot
    while (isDigit(peek())) advance();
  }
  return makeToken(TOKEN_NUMBER);
}

static Token string() {
  while (peek() != '"' && !isAtEnd()) {
    if (peek() == '\n') scanner.line++;
    advance();
  }
  if (isAtEnd()) return errorToken("Unterminated string.");
  advance();  // closing quote
  return makeToken(TOKEN_STRING);
}

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;
  if (isAtEnd()) return makeToken(TOKEN_EOF);

  char c = advance();
  if (isAlpha(c)) return identifier();
  if (isDigit(c)) return number();

  switch (c) {
    case '(': return makeToken(TOKEN_LEFT_PAREN);
    case ')': return makeToken(TOKEN_RIGHT_PAREN);
    case '{': return makeToken(TOKEN_LEFT_BRACE);
    case '}': return makeToken(TOKEN_RIGHT_BRACE);
    case ';': return makeToken(TOKEN_SEMICOLON);
    case ',': return makeToken(TOKEN_COMMA);
    case '.': return makeToken(TOKEN_DOT);
    case '-': return makeToken(TOKEN_MINUS);
    case '+': return makeToken(TOKEN_PLUS);
    case '/': return makeToken(TOKEN_SLASH);
    case '*': return makeToken(TOKEN_STAR);
    case '!': return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string();
  }
  return errorToken("Unexpected character.");
}
