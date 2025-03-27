#include <iostream>

#include <map>
#include <vector>
#include <string>
#include <utility>
#include <variant>
#include <optional>

namespace json {
    namespace JsonNode {
        // 代表json节点的结构体
        class Node;

        // 空状态
        using MonoNode = std::monostate;

        // 以下是json中的各种基础数据类型
        using BoolNode = bool;
        using IntNode = int64_t;
        using FloatNode = double;
        using StringNode = std::string;

        // 以下是json中的复合类型
        using ArrayNode = std::vector<Node>;
        using ObjectNode = std::map<std::string, Node>;

        // json中的所有值类型
        using ValueType = std::variant<MonoNode, BoolNode, IntNode, FloatNode, StringNode, ArrayNode, ObjectNode>;

        // json节点定义
        class Node {
        protected:
            ValueType value;
        public:
            // 构造函数
            Node() : value(MonoNode{}) {}
            explicit Node(ValueType _value) : value(std::move(_value)) {}

            // 如果是json对象
            auto& operator[](const std::string& key) {
                if (auto object = std::get_if<ObjectNode>(&value)) {
                    return  (*object)[key];
                }
                throw std::runtime_error("not an object");
            }

            // 如果是json数组
            auto operator[](size_t index) {
                if (auto array = std::get_if<ArrayNode>(&value)) {
                    return array->at(index);
                }
                throw std::runtime_error("not an array");
            }
            void push(const Node& rhs) {
                if (auto array = std::get_if<ArrayNode>(&value)) {
                    array->push_back(rhs);
                }
            }

            [[nodiscard]] auto Value() const -> const ValueType& {
                return value;
            }

            template<typename Ty>
            auto as() -> std::optional<Ty> {
                if (std::holds_alternative<Ty>(value)) {
                    return std::get<Ty>(value);
                }
                return std::nullopt;
            }

            static auto from_str(const std::string& json_str) -> std::optional<Node>;

            [[nodiscard]] auto to_str() const -> std::string;

            friend auto operator<<(std::ostream& out, const Node& rhs) -> std::ostream& {
                return out << rhs.to_str();
            }
        };
    }

    namespace JsonParser {
        using JsonNode::Node;
        using JsonNode::ArrayNode, JsonNode::ObjectNode;
        using JsonNode::MonoNode, JsonNode::ValueType, JsonNode::BoolNode, JsonNode::IntNode, JsonNode::FloatNode, JsonNode::StringNode;

        class Parser {
        protected:
            std::string_view json_str;
            size_t pos = 0;

            void skip_whitespace() {
                while (pos < json_str.size() && std::isspace(json_str[pos])) {
                    ++pos;
                }
            }

            auto parse_null() -> std::optional<ValueType> {
                if (json_str.substr(pos, 4) == "null") {
                    pos += 4;
                    return MonoNode{};
                }
                return{};
            }

            auto parse_true() -> std::optional<ValueType> {
                if (json_str.substr(pos, 4) == "true") {
                    pos += 4;
                    return true;
                }
                return {};
            }

            auto parse_false() -> std::optional<ValueType> {
                if (json_str.substr(pos, 5) == "false") {
                    pos += 5;
                    return false;
                }
                return {};
            }

            auto parse_number()->std::optional<ValueType> {
                static auto is_Float = [](const std::string& number_str) {
                    return number_str.find('.') != std::string::npos ||
                        number_str.find('e') != std::string::npos;
                };

                size_t endpos = pos;
                while (endpos < json_str.size() && (
                    std::isdigit(json_str[endpos]) ||
                    json_str[endpos] == 'e' ||
                    json_str[endpos] == '.')) {
                    endpos++;
                    }
                const auto number_str = std::string{ json_str.substr(pos, endpos - pos) };

                pos = endpos;
                if (is_Float(number_str)) {
                    try {
                        FloatNode ret = std::stod(number_str);
                        return ret;
                    }
                    catch (...) {
                        return {};
                    }
                }

                try {
                    IntNode ret = std::stoi(number_str);
                    return ret;
                }
                catch (...) {
                    return {};
                }
            }

            auto parse_string()->std::optional<ValueType> {
                size_t end_pos = ++pos;  // 去掉 `"`
                while (pos < json_str.size() && json_str[end_pos] != '"') {
                    end_pos++;
                }

                const auto string_str = json_str.substr(pos, end_pos - pos);
                pos = end_pos + 1;   // 去掉 `"`
                return std::string(string_str);
            }

            auto parse_array()->std::optional<ValueType> {
                pos++;// [
                ArrayNode arr;
                while (pos < json_str.size() && json_str[pos] != ']') {
                    auto value = parse_value();
                    arr.emplace_back(value.value());
                    skip_whitespace();
                    if (pos < json_str.size() && json_str[pos] == ',') {
                        pos++;// ,
                    }
                    skip_whitespace();
                }
                pos++;// ]
                return arr;
            }

            auto parse_object() ->std::optional<ValueType> {
                pos++;// {
                ObjectNode obj;
                while (pos < json_str.size() && json_str[pos] != '}') {
                    auto key = parse_value();
                    skip_whitespace();
                    if (!std::holds_alternative<StringNode>(key.value())) {
                        return {};
                    }
                    if (pos < json_str.size() && json_str[pos] == ':') {
                        pos++;// ,
                    }
                    skip_whitespace();
                    auto val = parse_value();
                    obj[std::get<StringNode>(key.value())] = Node{val.value()};
                    skip_whitespace();
                    if (pos < json_str.size() && json_str[pos] == ',') {
                        pos++;// ,
                    }
                    skip_whitespace();
                }
                pos++;// }
                return obj;

            }

            auto parse_value() ->std::optional<ValueType> {
                skip_whitespace();
                switch (json_str[pos]) {
                    case 'n':
                        return parse_null();
                    case 't':
                        return parse_true();
                    case 'f':
                        return parse_false();
                    case '"':
                        return parse_string();
                    case '[':
                        return parse_array();
                    case '{':
                        return parse_object();
                    default:
                        return parse_number();
                }
            }

        public:
            explicit Parser(const std::string& json_str): json_str(json_str) {}

            auto parse() ->std::optional<Node> {
                skip_whitespace();
                auto value = parse_value();
                if (!value) {
                    return {};
                }
                return Node{ *value };
            }
        };
    }

    namespace JsonSerializer {
        using JsonNode::Node;
        using JsonNode::ArrayNode, JsonNode::ObjectNode;
        using JsonNode::MonoNode, JsonNode::ValueType, JsonNode::BoolNode, JsonNode::IntNode, JsonNode::FloatNode, JsonNode::StringNode;

        class Serializer {
        public:
            static auto generate(const Node &node) -> std::string {
                return std::visit(
                    []<typename T0>(const T0 &arg) -> std::string {
                        using T = std::decay_t<T0>;
                        if constexpr (std::is_same_v<T, MonoNode>) {
                            return "null";
                        } else if constexpr (std::is_same_v<T, BoolNode>) {
                            return arg ? "true" : "false";
                        } else if constexpr (std::is_same_v<T, IntNode>) {
                            return std::to_string(arg);
                        } else if constexpr (std::is_same_v<T, FloatNode>) {
                            return std::to_string(arg);
                        } else if constexpr (std::is_same_v<T, StringNode>) {
                            return generate_string(arg);
                        } else if constexpr (std::is_same_v<T, ArrayNode>) {
                            return generate_array(arg);
                        } else if constexpr (std::is_same_v<T, ObjectNode>) {
                            return generate_object(arg);
                        }
                    },
                    node.Value());
            }
            static auto generate_string(const StringNode& str) -> std::string {
                std::string json_str = "\"";
                json_str += str;
                json_str += '"';
                return json_str;
            }
            static auto generate_array(const ArrayNode& array) -> std::string {
                std::string json_str = "[";
                for (const auto& node : array) {
                    json_str += generate(node);
                    json_str += ',';
                }
                if (!array.empty()) json_str.pop_back();
                json_str += ']';
                return json_str;
            }
            static auto generate_object(const ObjectNode& object) -> std::string {
                std::string json_str = "{";
                for (const auto& [key, node] : object) {
                    json_str += generate_string(key);
                    json_str += ':';
                    json_str += generate(node);
                    json_str += ',';
                }
                if (!object.empty()) json_str.pop_back();
                json_str += '}';
                return json_str;
            }
        };
    }

    using JsonNode::Node;
    using JsonNode::ArrayNode, JsonNode::ObjectNode;
    using JsonNode::MonoNode, JsonNode::ValueType, JsonNode::BoolNode, JsonNode::IntNode, JsonNode::FloatNode, JsonNode::StringNode;

    auto JsonNode::Node::from_str(const std::string& json_str) -> std::optional<Node> {
        auto parser = JsonParser::Parser{json_str};
        return parser.parse();
    }

    auto JsonNode::Node::to_str() const -> std::string {
        return JsonSerializer::Serializer::generate(*this);
    }
};

int main() {
    auto node = *json::Node::from_str("{\"test\": 10};");

    auto tmp = node["test"].as<int64_t>();

    if (tmp.has_value()) {
        std::cout << tmp.value() << '\n';
    }
    else {
        std::cout<<"none"<<std::endl;
    }

    return 0;
}
