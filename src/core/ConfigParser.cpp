// Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ConfigParser.h"
#include "core/Log.h"

using namespace Config;

std::string to_string(Token::Type type)
{
	switch (type) {
	case Token::Discard: return "Discard";
	case Token::String: return "String";
	case Token::Number: return "Number";
	case Token::Identifier: return "Identifier";
	case Token::EndOfFile: return "EOF";
	default:
		return "<unknown>";
	}
}

// ============================================================================
// Tokenizer
//

Tokenizer::Tokenizer(std::string_view data) :
	remaining(data),
	nextLine(1),
	nextOffset(1)
{
	advance();
}

// Use the current character to return the best-guess type of
// the next token
static Token::Type guess_next_token(std::string_view current)
{
	if (current.empty())
		return Token::EndOfFile;

	if (current[0] == ' ' || current[0] == '\t' || current[0] == '\r' || current[0] == '\n')
		return Token::Discard;

	if (current[0] == '"')
		return Token::String;

	if (isalpha(current[0]) || current[0] == '_')
		return Token::Identifier;

	if (isdigit(current[0]))
		return Token::Number;

	return Token::Type(current[0]);
}

// Pop the given number of characters from the remaining buffer and
// update line-offset information.
// Returns a string_view of the consumed characters.
std::string_view Tokenizer::consume(size_t num)
{
	num = std::min(num, remaining.size());
	std::string_view ret(remaining.data(), num);
	nextOffset += num;
	remaining.remove_prefix(num);
	return ret;
}

// Where the magic happens. Returns the cached next token and adds the
// subsequent token to the lookahead cache.
Token Tokenizer::advance()
{
	Token returnTok = next;
	currentLine = nextLine;
	currentOffset = nextOffset - next.range.size();

	Token::Type nextType = guess_next_token(remaining);

	// eat as much whitespace as possible
	while (nextType == Token::Discard) {
		// if this is a newline, eat it and reset line tracking
		if (remaining[0] == '\n') {
			nextLine += 1;
			nextOffset = 1;
			remaining.remove_prefix(1);
		}

		// eat all contiguous whitespace characters and skip ahead to
		// the next newline or valid token
		consume(remaining.find_first_not_of(" \t\r"));
		nextType = guess_next_token(remaining);
	}

	size_t length = 1;

	// if this is an identifier, we want to grab all consecutive alphanumberic
	// characters that make up the identifer
	if (nextType == Token::Identifier) {
		while (isalnum(remaining[length]) || remaining[length] == '_')
			length++;
	}

	// potential leading minus-sign for a number
	if (nextType == '-') {
		if (isdigit(remaining[length]))
			nextType = Token::Number;
	}

	// a Number token type means there's an optional '-' and at least one leading digit;
	// parse its value as an optionally-floating point number
	if (nextType == Token::Number) {
		char *end = nullptr;
		next.value = std::strtod(remaining.data(), &end);
		length = std::distance(remaining.data(), const_cast<const char *>(end));
	}

	// if it's a string, continue until we find a string terminator character
	if (nextType == Token::String) {
		for (; length < remaining.size(); length++) {
			if (remaining[length] == '"') {
				length++;
				break;
			}

			// skip escaped string terminators
			if (remaining[length] == '\\' && length + 1 < remaining.size())
				length++;
		}
	}

	// Otherwise, it's a token that doesn't need special-case handling

	next.type = nextType;
	next.range = consume(length);

	return returnTok;
}

bool Tokenizer::discardLine()
{
	// if the nextLine value is different than the current token's line,
	// the current token was the last one on this line, making this a no-op
	if (nextLine != currentLine)
		return false;

	// consume all (potentially-tokenizable) data until the next newline character,
	// then throw away the old cached token and cache the first token on the next line.
	consume(remaining.find_first_of('\n'));
	advance();
	return true;
}

// ============================================================================

Parser::Result Parser::Parse()
{
	while (state.peek().type != Token::EndOfFile && !m_contexts.empty()) {
		// Context list can change due to PushContext()
		size_t activeCtx = m_contexts.size() - 1;
		Context &ctx = *m_contexts[activeCtx].get();

		Token tok = Advance();
		Result res = ctx(this, tok);

		if (res == Result::ParseFailure) {
			return res;
		}

		if (res == Result::DidNotMatch) {
			// Unknown token type; ignore the rest of the line
			// TODO: fallback parser function or immediate parse failure?
			state.discardLine();
		}

		if (ctx.finished) {
			m_contexts.erase(m_contexts.begin() + activeCtx);
		}
	}

	// Partially parsed but tokens still remaining; report the failure.
	if (state.peek().type != Token::EndOfFile)
		return Result::DidNotMatch;

	return Result::Parsed;
}

// Returns true and consumes the next token if it is of the specified type
bool Parser::Acquire(Token::Type type, Token *outTok)
{
	if (!Expect(type, state.peek()))
		return false;

	if (outTok != nullptr) {
		*outTok = Advance();
	} else {
		Advance();
	}

	return true;
}

// Returns true if the passed token is of the given type. Otherwise, prints an error message.
bool Parser::Expect(Token::Type type, const Token &tok) const
{
	if (tok.type == type) return true;

	if (type <= Token::EndOfFile)
		Log::Warning("{} Unknown token '{}', expected a {}\n", MakeLineInfo(), tok.range, to_string(type));
	else
		Log::Warning("{} Unknown token '{}', expected '{}'\n", MakeLineInfo(), tok.range, char(type));

	return false;
}

// Advance a token, discarding line comments
Token Parser::Advance()
{
	while (state.peek().type == lineComment) {
		state.discardLine();
	}

	return state.advance();
}

// Format the current position in the source data
std::string Parser::MakeLineInfo() const
{
	return fmt::format("{}:{}:{}", fileName, state.getCurrentLine(), state.getCurrentChar());
}

// Push a new context onto the stack
Parser::Result Parser::PushContext(Context *ctx)
{
	m_contexts.emplace_back(ctx);
	return Result::Parsed;
}

// Doesn't actually pop the context from the stack as that would delete it while it's still evaluating
Parser::Result Parser::PopContext(Context *ctx)
{
	ctx->finished = true;
	return Result::Parsed;
}
