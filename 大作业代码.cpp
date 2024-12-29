#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <map>
#include <set>
using namespace std;
// Token类型
enum class TokenType
{
  KEYWORD,
  IDENTIFIER,
  SYMBOL,
  NUMBER,
  STRING,
  END
};

// Token结构
struct Token
{
  TokenType type;
  string value;
};

// 词法分析器
class Lexer
{
private:
  string input;
  size_t pos;
  Token lastToken;
  bool hasPutBack = false;
  void skipWhitespace()
  {
    while (pos < input.size() && isspace(input[pos]))
      pos++;
  }

public:
  explicit Lexer() {}
  void Initial(const string &ipt)
  {
    input = ipt;
    pos = 0;
  }
  void SetZero()
  {
    input = "";
    pos = 0;
  }
  // 增加回退机制
  void putBackToken(const Token &token)
  {
    lastToken = token;
    hasPutBack = true;
  }
  Token nextToken()
  {
    if (hasPutBack)
    {
      hasPutBack = false;
      return lastToken;
    }
    skipWhitespace();

    if (pos >= input.size())
      return {TokenType::END, ""};

    if (isalpha(input[pos]))
    { // 如果以字母开头，可能是关键字或标识符
      // 统一转换为大写（或者小写）
      string word;
      while (pos < input.size() && (isalnum(input[pos]) || input[pos] == '_'))
      {
        word += input[pos++];
      }

      // 关键字和标识符的区分
      static const vector<string> keywords = {
          "CREATE", "TABLE", "SELECT", "INSERT", "INTO", "VALUES",
          "UPDATE", "SET", "DELETE", "FROM", "WHERE", "AND", "OR"};
      if (find(keywords.begin(), keywords.end(), word) != keywords.end())
      {
        return {TokenType::KEYWORD, word}; // 是关键字
      }
      return {TokenType::IDENTIFIER, word}; // 否则是标识符
    }

    if (isdigit(input[pos])) // 开头是数字-->number
    {
      string number;
      while (pos < input.size() && isdigit(input[pos]))
      {
        number += input[pos++];
      }
      return {TokenType::NUMBER, number};
    }

    if (input[pos] == '\'' || input[pos] == '"') // 字符串-->string
    {
      char quote = input[pos++];
      string str;
      while (pos < input.size() && input[pos] != quote)
      {
        str += input[pos++];
      }
      pos++; // 跳过右引号
      return {TokenType::STRING, str};
    }
    if (pos + 1 < input.size())
    {
      string twoChars = input.substr(pos, 2);
      if (twoChars == "!=" || twoChars == "<=" || twoChars == ">=")
      {
        pos += 2;
        return {TokenType::SYMBOL, twoChars};
      }
    }
    char symbol = input[pos++]; // 匹配()
    return {TokenType::SYMBOL, string(1, symbol)};
  }
};

// 抽象语法树节点
struct ASTNode
{
  string type;
  vector<shared_ptr<ASTNode>> children;
};

// 语法分析器
class Parser
{
public:
  explicit Parser() {}

  shared_ptr<ASTNode> parse(const string &input)
  {
    lexer.Initial(input);
    auto token = lexer.nextToken();
    if (token.type == TokenType::KEYWORD)
    {
      shared_ptr<ASTNode> node;
      if (token.value == "CREATE")
      {
        node = parseCreate();
        lexer.SetZero();
      }

      else if (token.value == "SELECT")
      {
        node = parseSelect();
        lexer.SetZero();
      }
      else if (token.value == "INSERT")
      {
        node = parseInsert();
        lexer.SetZero();
      }

      else if (token.value == "UPDATE")
      {
        node = parseUpdate();
        lexer.SetZero();
      }

      else if (token.value == "DELETE")
      {
        node = parseDelete();
        lexer.SetZero();
      }
      return node;
    }
    throw runtime_error("Invalid SQL statement");
  }

private:
    Lexer lexer;
    DatabaseManager& dbManager;

public:
    explicit Parser(DatabaseManager& db) : dbManager(db) {}

    shared_ptr<ASTNode> parse(const string& input) {
        lexer.Initial(input);
        auto token = lexer.nextToken();
        if (token.type == TokenType::KEYWORD) {
            shared_ptr<ASTNode> node;
            if (token.value == "CREATE") {
                node = parseCreate();
            } else if (token.value == "SELECT") {
                node = parseSelect();
            } else if (token.value == "INSERT") {
                node = parseInsert();
            } else if (token.value == "UPDATE") {
                node = parseUpdate();
            } else if (token.value == "DELETE") {
                node = parseDelete();
            }
            lexer.SetZero();
            return node;
        }
        throw runtime_error("Invalid SQL statement");
    }

private:
  Lexer lexer;

  // CREATE TABLE解析
  shared_ptr<ASTNode> parseCreate()
  {
    auto node = make_shared<ASTNode>();
    node->type = "CREATE";

    expectKeyword("TABLE");
    node->children.push_back(parseIdentifier("table name"));

    expectSymbol("(");
    while (true)
    {
      auto column = make_shared<ASTNode>();
      column->type = "COLUMN";
      column->children.push_back(parseIdentifier("column name"));
      column->children.push_back(parseIdentifier("data type"));
      node->children.push_back(column);

      auto token = lexer.nextToken();
      if (token.type == TokenType::SYMBOL && token.value == ")")
        break;
      if (token.type != TokenType::SYMBOL || token.value != ",")
      {
        throw runtime_error("Expected ',' or ')'");
      }
    }
    return node;
  }

  // SELECT解析
  shared_ptr<ASTNode> parseSelect()
  {
    auto node = make_shared<ASTNode>();
    node->type = "SELECT";
    // 解析列名
    node->children.push_back(parseColumns());
    // 确保 "FROM" 关键字存在
    expectKeyword("FROM");
    // 解析表名
    node->children.push_back(parseIdentifier("table name"));
    // 检查是否存在 "WHERE" 子句
    auto token = lexer.nextToken();
    if (token.type == TokenType::KEYWORD && token.value == "WHERE")
    {
      node->children.push_back(parseCondition());
    }
    else
    {
      // 回退非 "WHERE" 的 token
      lexer.putBackToken(token);
    }
    return node;
  }

  // INSERT INTO解析
  shared_ptr<ASTNode> parseInsert()
  {
    auto node = make_shared<ASTNode>();
    node->type = "INSERT";

    expectKeyword("INTO");
    node->children.push_back(parseIdentifier("table name"));

    expectKeyword("VALUES");
    expectSymbol("(");
    auto values = make_shared<ASTNode>();
    values->type = "VALUES";
    while (true)
    {
      values->children.push_back(parseValue());
      auto token = lexer.nextToken();
      if (token.type == TokenType::SYMBOL && token.value == ")")
        break;
      if (token.type != TokenType::SYMBOL || token.value != ",")
      {
        throw runtime_error("Expected ',' or ')'");
      }
    }
    node->children.push_back(values);
    return node;
  }

  // UPDATE解析
  shared_ptr<ASTNode> parseUpdate()
  {
    auto node = make_shared<ASTNode>();
    node->type = "UPDATE";

    node->children.push_back(parseIdentifier("table name"));
    expectKeyword("SET");

    auto updates = make_shared<ASTNode>();
    updates->type = "UPDATES";
    while (true)
    {
      auto update = make_shared<ASTNode>();
      update->type = "UPDATE_FIELD";
      update->children.push_back(parseIdentifier("column name"));
      expectSymbol("=");
      update->children.push_back(parseValue());
      updates->children.push_back(update);

      auto token = lexer.nextToken();
      if (token.type == TokenType::KEYWORD && token.value == "WHERE")
        break;
      if (token.type != TokenType::SYMBOL || token.value != ",")
      {
        throw runtime_error("Expected ',' or 'WHERE'");
      }
    }
    node->children.push_back(updates);
    node->children.push_back(parseCondition());
    return node;
  }

  // DELETE解析
  shared_ptr<ASTNode> parseDelete()
  {
    auto node = make_shared<ASTNode>();
    node->type = "DELETE";

    expectKeyword("FROM");
    node->children.push_back(parseIdentifier("table name"));

    auto token = lexer.nextToken();
    if (token.type == TokenType::KEYWORD && token.value == "WHERE")
    {
      node->children.push_back(parseCondition());
    }
    return node;
  }

  // 解析列名列表
  shared_ptr<ASTNode> parseColumns()
  {
    auto node = std::make_shared<ASTNode>();
    node->type = "COLUMNS";

    auto token = lexer.nextToken();
    if (token.type == TokenType::SYMBOL && token.value == "*")
    {
      // 识别 SELECT * 的情况
      node->children.push_back(std::make_shared<ASTNode>(ASTNode{"ALL_COLUMNS", {}}));
    }
    else
    {
      lexer.putBackToken(token); // 回退第一个 Token，交给解析列名逻辑
      while (true)
      {
        // 第一个标识符是列名
        node->children.push_back(parseIdentifier("column name"));

        // 检查下一个 token，是否是逗号继续
        token = lexer.nextToken();
        if (token.type == TokenType::SYMBOL && token.value == ",")
        {
          continue; // 继续解析下一个列名
        }
        else
        {
          lexer.putBackToken(token); // 使用公有方法回退 Token
          break;
        }
      }

      // 将非列名的 token 传回给解析器
      lexer.putBackToken(token);
    }

    return node;
  }

  // 解析条件
  shared_ptr<ASTNode> parseCondition()
  {
    auto condition = std::make_shared<ASTNode>();
    condition->type = "CONDITION";

    // 第一个标识符是列名
    condition->children.push_back(parseIdentifier("column name"));

    // 接下来是运算符
    auto token = lexer.nextToken();
    if (token.type != TokenType::SYMBOL ||
        (token.value != "=" && token.value != ">" && token.value != "<" &&
         token.value != ">=" && token.value != "<=" && token.value != "!="))
    {
      throw runtime_error("Expected comparison operator");
    }
    condition->children.push_back(make_shared<ASTNode>(ASTNode{token.value, {}}));

    // 最后是值
    condition->children.push_back(parseValue());

    return condition;
  }

  //  解析值
  shared_ptr<ASTNode> parseValue()
  {
    auto token = lexer.nextToken();
    if (token.type == TokenType::NUMBER || token.type == TokenType::STRING)
    {
      return make_shared<ASTNode>(ASTNode{token.value, {}});
    }
    throw runtime_error("Expected a value");
  }

  // 解析标识符
  shared_ptr<ASTNode> parseIdentifier(const string &description)
  {
    auto token = lexer.nextToken();
    if (token.type != TokenType::IDENTIFIER)
    {
      throw runtime_error("Expected " + description);
    }
    return make_shared<ASTNode>(ASTNode{token.value, {}});
  }

  // 检查关键词
  void expectKeyword(const string &keyword)
  {
    auto token = lexer.nextToken();
    if (token.type != TokenType::KEYWORD || token.value != keyword)
    {
      throw runtime_error("Expected keyword '" + keyword + "'");
    }
  }

  // 检查符号
  void expectSymbol(const string &symbol)
  {
    auto token = lexer.nextToken();
    if (token.type != TokenType::SYMBOL || token.value != symbol)
    {
      throw runtime_error("Expected symbol '" + symbol + "'");
    }
  }
};
string SQL_Parser(Parser parser, map<string, vector<string>> &results, string &sql_type, string sql,Parser& parser, DatabaseManager& dbManager, 
                 map<string, vector<string>>& results, string& sql_type, const string& sql)
{
  string table_name;
  // SQL解析的结果存放在results
  results = {
      {"VALUES", vector<string>{}},
      {"COLUMN", vector<string>{}},
      {"COLUMNS", vector<string>{}},      // 有些值可能为ALL_COLUMNS，只是一个
      {"UPDATE_FIELD", vector<string>{}}, // UPDATE_FIELD在第三层
      {"CONDITION", vector<string>{}},
  };

  map<int, vector<string>> tree = {
      {0, vector<string>{}},
      {1, vector<string>{}},  // 表名在第二层
      {2, vector<string>{}},  //
      {3, vector<string>{}}}; // 语法解析树最多四层
  // cout << "------------------------------------------------------" << endl;
  // cout << "SQL语句是:" << sql << endl;
  try
  {

    auto ast = parser.parse(sql);

    // 打印AST
    function<void(shared_ptr<ASTNode>, int)> printAST = [&](shared_ptr<ASTNode> node, int depth)
    {
      // for (int i = 0; i < depth; i++)
      //   cout << "  ";
      tree[depth].push_back(node->type);
      // cout << node->type << endl;
      for (auto &child : node->children)
      {
        printAST(child, depth + 1);
      }
    };

    printAST(ast, 0);
  }
  catch (const exception &ex)
  {
    cerr << "Error: " << ex.what() << endl;
  }
  sql_type = tree[0][0];
  size_t i, j, k;
  j = k = 0;
  // 如果是select查询，则第一个标识符是COLUMNS，否则是表名
  if (tree[0][0] == "SELECT")
  {
    i = 0;
    table_name = tree[1][1];
  }

  else
  {
    i = 1;
    table_name = tree[1][0];
  }

  while (i < tree[1].size())
  {
    if (tree[1][i] == "COLUMNS")
    {
      if (tree[2][j] == "ALL_COLUMNS")
      {
        results["ALL_COLUMNS"].push_back("*");
        ++j;
      }
      else
      {
        while (true)
        {
          if (tree[2][j + 1] == ">" || tree[2][j + 1] == "<" || tree[2][j + 1] == ">=" || tree[2][j + 1] == "<=" || tree[2][j + 1] == "=" || tree[2][j + 1] == "!=")
            break;
          results["ALL_COLUMNS"].push_back(tree[2][j++]);
        }
      }
    }
    else if (tree[1][i] == "CONDITION")
    {
      if (j + 1 < tree[2].size())
      {
        if (tree[2][j + 1] == ">" || tree[2][j + 1] == "<" || tree[2][j + 1] == ">=" || tree[2][j + 1] == "<=" || tree[2][j + 1] == "=" || tree[2][j + 1] == "!=")
        {
          results["CONDITION"].push_back(tree[2][j] + tree[2][j + 1] + tree[2][j + 2]);
          j += 3;
        }
      }
    }
    else if (tree[1][i] == "COLUMN")
    {
      results["COLUMN"].push_back(tree[2][j] + "-" + tree[2][j + 1]);
      j += 2;
    }
    else if (tree[1][i] == "VALUES")
    {
      for (auto &s : tree[2])
        results["VALUES"].push_back(s);
    }
    else if (tree[1][i] == "UPDATES")
    {
      while (tree[2][j] == "UPDATE_FIELD")
      {
        results["UPDATE_FIELD"].push_back(tree[3][k] + "-" + tree[3][k + 1]);
        k += 2;
        ++j;
      }
    }

    ++i;
  }
  // for (const auto &[key, value] : results)
  // {
  //   cout << "------key-------" << key << endl;
  //   for (auto &s : value)
  //   {
  //     cout << s << "@";
  //   }
  //   cout << "\n------------------------------------------------------" << endl;
  // }
  // cout << "------------------------------------------------------" << endl;
  // 返回表名
     string table_name;
    results = {
        {"VALUES", vector<string>{}},
        {"COLUMN", vector<string>{}},
        {"COLUMNS", vector<string>{}},
        {"UPDATE_FIELD", vector<string>{}},
        {"CONDITION", vector<string>{}}
    };

    try {
        auto ast = parser.parse(sql);
        // ... [解析逻辑保持不变] ...

        // 根据SQL类型执行相应操作
        if (sql_type == "CREATE") {
            vector<string> colNames, colTypes;
            for (const auto& col : results["COLUMN"]) {
                size_t pos = col.find('-');
                colNames.push_back(col.substr(0, pos));
                colTypes.push_back(col.substr(pos + 1));
            }
            dbManager.createTable(table_name, colNames, colTypes);
        }
        else if (sql_type == "INSERT") {
            dbManager.insertIntoTable(table_name, results["VALUES"]);
        }
        // ... [其他SQL操作的实现] ...

        return table_name;
    }
    catch (const exception& ex) {
        cerr << "Error: " << ex.what() << endl;
        return "";
    }
  return table_name;
}

// 增加表结构定义
struct TableData {
    string name;
    vector<string> columnNames;
    vector<string> columnTypes;
    vector<vector<string>> rows;
};

// 数据库管理类
class DatabaseManager {
private:
    map<string, TableData> tables;

public:
    void createTable(const string& name, const vector<string>& colNames, 
                    const vector<string>& colTypes) {
        if (tables.find(name) != tables.end()) {
            throw runtime_error("Table already exists: " + name);
        }
        TableData newTable;
        newTable.name = name;
        newTable.columnNames = colNames;
        newTable.columnTypes = colTypes;
        tables[name] = newTable;
    }

    void insertIntoTable(const string& name, const vector<string>& values) {
        auto it = tables.find(name);
        if (it == tables.end()) {
            throw runtime_error("Table not found: " + name);
        }
        if (values.size() != it->second.columnNames.size()) {
            throw runtime_error("Column count mismatch");
        }
        it->second.rows.push_back(values);
    }

    TableData& getTable(const string& name) {
        auto it = tables.find(name);
        if (it == tables.end()) {
            throw runtime_error("Table not found: " + name);
        }
        return it->second;
    }

    bool tableExists(const string& name) const {
        return tables.find(name) != tables.end();
    }
};

int main()
{
  Parser parser; // 定义一个解析器
  /*
  缺点：
  1. 关键字只能大写
  2. 条件只能有一个
  */
  vector<string> sqls = {
      // "SELECT id, name FROM table WHERE age > 25;",
      // "CREATE TABLE users (id INT,name VARCHAR,age INT,email VARCHAR,created_at DATETIME);",
      // "SELECT * FROM users WHERE id=1;",
      // "DELETE FROM users WHERE id < 1;",
      // "DELETE FROM users;",
      // "INSERT INTO users VALUES (1, 'Alice', 25, 'alice@example.com', '2024-11-21');",
      "UPDATE users SET name = 'Bob', age = 30 WHERE id <= 2;"};
  for (auto &sql : sqls)
  {
    map<string, vector<string>> results;
    string sql_type;
    string table_name = SQL_Parser(parser, results, sql_type, sql);
    cout << "表名为：" << table_name << endl;
    cout << "sql_type为：" << sql_type << endl;
  }
  DatabaseManager dbManager;
    Parser parser(dbManager);

    vector<string> sqls = {
        "CREATE TABLE users (id INT,name VARCHAR,age INT,email VARCHAR);",
        "INSERT INTO users VALUES (1, 'Alice', 25, 'alice@example.com');",
        "CREATE TABLE products (id INT,name VARCHAR,price INT);",
        "INSERT INTO products VALUES (1, 'Phone', 999);",
        "SELECT * FROM users WHERE age > 20;",
        "SELECT * FROM products WHERE price < 1000;"
    };

    for (const auto& sql : sqls) {
        try {
            map<string, vector<string>> results;
            string sql_type;
            string table_name = SQL_Parser(parser, dbManager, results, sql_type, sql);
            cout << "Executed SQL on table: " << table_name << endl;
            cout << "SQL type: " << sql_type << endl;
            
            // 如果是SELECT语句，显示结果
            if (sql_type == "SELECT" && !table_name.empty()) {
                auto& table = dbManager.getTable(table_name);
                cout << "Columns: ";
                for (const auto& col : table.columnNames) {
                    cout << col << " ";
                }
                cout << "\nData: \n";
                for (const auto& row : table.rows) {
                    for (const auto& value : row) {
                        cout << value << " ";
                    }
                    cout << endl;
                }
            }
        }
        catch (const exception& ex) {
            cerr << "Error executing SQL: " << ex.what() << endl;
        }
    }

  return 0;
}