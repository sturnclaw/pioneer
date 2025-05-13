// Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Config {

	struct Token {
		// Very lightweight tokenization, just enough to parse a barebones
		// configuration file.
		enum Type : uint8_t {
			Discard = 0, // not a real token type
			String = 1,
			Number = 2,
			Identifier = 3,
			EndOfFile = 26,

			LBrace = '{',
			RBrace = '}',
			LBracket = '[',
			RBracket = ']',
			Equals = '=',
			Hash = '#'
		};

		Type type;
		std::string_view range;
		double value;

		std::string_view trim(int32_t start, int32_t end) const
		{
			size_t pos = start < 0 ? range.size() - start : start;
			size_t len = (end < 0 ? range.size() + end : end) - start;
			return range.substr(pos, len);
		}

		// Return the contents of the token; for strings this is the string data without quotes;
		// for all other tokens it is the textual representation of the token.
		std::string_view contents() const { return type == Token::String ? trim(1, -1) : range; }

		// Convert the contents of the token to a string.
		std::string toString() const { return std::string{ contents() }; }

		bool isKeyword(std::string_view keyword) const
		{
			return type == Type::Identifier && range.compare(keyword) == 0;
		}
	};

	struct Tokenizer {
	public:
		Tokenizer() = default;
		Tokenizer(std::string_view data);

		// Returns the next token in the stream.
		// TODO: support for pushing the last token back on the stream (for look-ahead)
		Token advance();

		// Discard all tokens on the current line, such that a subsequent call to
		// advance() will return the first token on the next non-empty line.
		// Returns true if any tokens were discarded.
		bool discardLine();
		bool peekIsNextLine() const { return nextLine != currentLine; }

		const Token &peek() const { return next; }
		uint32_t getCurrentLine() const { return currentLine; }
		uint32_t getCurrentChar() const { return currentOffset; }

	private:
		std::string_view consume(size_t num);

		Token next;
		std::string_view remaining;
		uint32_t currentLine;
		uint32_t currentOffset;
		uint32_t nextLine;
		uint32_t nextOffset;
	};

	struct Parser {
	public:
		enum class Result {
			DidNotMatch,
			ParseFailure,
			Parsed
		};

		struct Context {
			using Result = Parser::Result;
			virtual ~Context() = default;

			// Parse the given token (and any required following tokens).
			// Returns DidNotMatch if the token is unknown, ParseFailure if a syntax error occurred,
			// or Parsed if the token(s) were successfully parsed.
			virtual Result operator()(Parser *parser, const Token &tok) = 0;
			bool finished = false;
		};

		Parser() = default;
		Parser(Context *initialCtx, Token::Type lineComment = Token::Discard) :
			lineComment(lineComment)
		{
			PushContext(initialCtx);
		}

		// Initialize the parser to read from the given file contents
		void Init(std::string_view path, std::string_view data)
		{
			fileName = path;
			state = Tokenizer(data);
		}

		// Destroy all contexts and clear tokenizer state and filename data.
		void Reset()
		{
			fileName = "";
			state = Tokenizer();
			m_contexts.clear();
		}

		// Process the file data through the current context.
		Result Parse();

		// Check the next token type and advance the parser state if valid, storing the parsed token in outTok if not nullptr.
		// Logs an error message if the token is not of a valid type.
		bool Acquire(Token::Type type, Token *outTok = nullptr);
		// Returns true if the passed token is of the given type. Otherwise, logs an error message.
		bool Expect(Token::Type type, const Token &tok) const;
		// Return the next valid token, discarding line comments of the configured token type. Advances the tokenizer state.
		Token Advance();

		// Format the current position in the source data
		std::string MakeLineInfo() const;

		// Returns a dummy Result::Parsed
		Result PushContext(Context *);
		// Returns a dummy Result::Parsed
		Result PopContext(Context *);

		Tokenizer state;
		std::string fileName;
		Token::Type lineComment = Token::Discard;
	private:
		std::vector<std::unique_ptr<Context>> m_contexts;
	};

} // namespace Config
