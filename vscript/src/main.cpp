#include <unordered_map>
#include <filesystem>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <string>

constexpr auto VSCRIPT_BUILD_ID = "v0.1a";

void Help()
{
	std::cout << "VScript" << ' ' << VSCRIPT_BUILD_ID << '\n'
		<< "Commands:\n"
		<< "-c Compile\n"
		<< "-r Execute\n";
}

struct Position
{
	std::string Filename;
	uint32_t Column;
	uint32_t Row;

	friend std::ostream& operator<<(std::ostream& stream, const Position& pos)
	{
		return stream << pos.Filename << ':' << pos.Row + 1 << ':' << pos.Column + 1;
	}
};

enum class Keyword
{
	Fn,
	Return,
};

static std::unordered_map<std::string, Keyword> s_Keywords =
{
	{ "fn", Keyword::Fn },
	{ "return", Keyword::Return },
};

enum class TokenType
{
	None = 0,
	Keyword,
	Ident,
	OCurly,
	CCurly,
	OParen,
	CParen,
	Colon,
	Semicolon,
	Arrow,
	String,
	Number,
	Plus,
	Minus,
	Multiply,
	Divide,
	Equals,
};

static const char* TokenTypeToString(TokenType type)
{
	switch (type)
	{
		case TokenType::Keyword:   return "Keyword";
		case TokenType::Ident:     return "Ident";
		case TokenType::OCurly:    return "OCurly";
		case TokenType::CCurly:    return "CCurly";
		case TokenType::OParen:    return "OParen";
		case TokenType::CParen:    return "CParen";
		case TokenType::Colon:     return "Colon";
		case TokenType::Semicolon: return "Semicolon";
		case TokenType::Arrow:     return "Arrow";
		case TokenType::String:    return "String";
		case TokenType::Number:    return "Number";
		case TokenType::Plus:      return "Plus";
		case TokenType::Minus:     return "Minus";
		case TokenType::Multiply:  return "Multiply";
		case TokenType::Divide:    return "Divide";
		case TokenType::Equals:    return "Equals";
	}

	return "None";
}

struct Token
{
	TokenType Type = TokenType::None;
	Position Pos{};
	std::string Value = "";

	Token() = default;
	Token(TokenType type, const Position& pos, const std::string& value)
		: Type(type), Pos(pos), Value(value) { }
};

class Lexer
{
public:
	Lexer(const std::string& filename)
		: m_Filename(filename) { }

	void ParseLine(const std::string& line)
	{
		uint32_t cursor = 0;

		auto valid = [&]() { return cursor < line.size(); };
		auto currentPos = [&]() { return Position{ m_Filename, cursor, m_Row }; };

		while (cursor < line.size())
		{
			auto pos = currentPos();
			char first = line[cursor];
			uint32_t start = 0;
			Token token = { TokenType::None, pos, "" };

			if (std::isspace(first))
			{
				first = line[++cursor];
				pos = currentPos();
				token.Pos = pos;
			}

			if (first == '{')
				token = Token(TokenType::OCurly, pos, std::string(&first, 1));
			else if (first == '}')
				token = Token(TokenType::CCurly, pos, std::string(&first, 1));
			else if (first == '(')
				token = Token(TokenType::OParen, pos, std::string(&first, 1));
			else if (first == ')')
				token = Token(TokenType::CParen, pos, std::string(&first, 1));

			if (first == ';')
				token = Token(TokenType::Semicolon, pos, std::string(&first, 1));
			else if (first == ':')
				token = Token(TokenType::Colon, pos, std::string(&first, 1));

			if (first == '+')
				token = Token(TokenType::Plus, pos, std::string(&first, 1));
			else if (first == '-')
			{
				start = cursor;
				cursor++;

				if (line[cursor] == '>')
				{
					std::string value(line.data() + start, 2);
					token = Token(TokenType::Arrow, pos, value);
				}
				else
				{
					token = Token(TokenType::Minus, pos, std::string(&first, 1));
				}
			}
			else if (first == '*')
				token = Token(TokenType::Multiply, pos, std::string(&first, 1));
			else if (first == '/')
				token = Token(TokenType::Divide, pos, std::string(&first, 1));
			else if (first == '=')
				token = Token(TokenType::Equals, pos, std::string(&first, 1));

			if (first == '"')
			{
				start = cursor;
				while (valid() && std::isalnum(line[cursor]) || line[cursor] == '_')
					cursor++;

				if (line[cursor] != '"')
				{
					Expected(TokenType::String, "unterminated string literal", currentPos());
				}

				std::string value(line.data() + start, cursor - start);
				token = Token(TokenType::String, pos, value);
				cursor--;
			}

			if (std::isdigit(first))
			{
				start = cursor;
				while (valid() && std::isdigit(line[cursor]))
					cursor++;

				std::string value(line.data() + start, cursor - start);
				token = Token(TokenType::Number, pos, value);
				cursor--;
			}
			else if (std::isalnum(first))
			{
				start = cursor;
				while (valid() && std::isalnum(line[cursor]) || line[cursor] == '_')
					cursor++;

				std::string value(line.data() + start, cursor - start);

				if (s_Keywords.contains(value))
					token = Token(TokenType::Keyword, pos, value);
				else
					token = Token(TokenType::Ident, pos, value);

				cursor--;
			}

			m_Tokens.push_back(token);
			cursor++;
		}

		m_Row++;
	}

	void Expected(TokenType type, const std::string& msg, const Position& pos)
	{
		std::cout << pos << ", Expected " << TokenTypeToString(type) << ", " << msg;
	}

	const std::vector<Token>& Tokens() const { return m_Tokens; }

private:
	std::vector<Token> m_Tokens;
	std::string m_Filename = "";
	uint32_t m_Row = 0;
};

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		Help();
		return -1;
	}

	std::string filepath = argv[1];
	std::ifstream stream(filepath);
	if (!stream.is_open())
	{
		Help();
		return -1;
	}

	std::string filename = std::filesystem::path(filepath).filename().string();
	Lexer lexer(filename);

	std::string line;
	while (std::getline(stream, line))
		lexer.ParseLine(line);

	for (const auto& token : lexer.Tokens())
	{
		auto type = TokenTypeToString(token.Type);
		std::cout << '<' << type << ' ' << token.Pos << "> " << token.Value << '\n';
	}

	std::cout << lexer.Tokens().size() << '\n';

	std::cin.get();
	return 0;
}