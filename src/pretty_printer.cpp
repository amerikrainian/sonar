#include "sonar/pretty_printer.hpp"

#include <sstream>
#include <string>
#include <variant>

namespace sonar {

namespace {

std::string render(const Expression& expression);
std::string render(const Statement& statement);

std::string format_number(double value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

struct Printer {
    std::string operator()(const Expression::Number& number) const {
        return format_number(number.value);
    }

    std::string operator()(const Expression::Prefix& prefix) const {
        std::string result = "(" + to_string(prefix.op) + " " + render(*prefix.right) + ")";
        return result;
    }

    std::string operator()(const Expression::Infix& infix) const {
        std::string result = "(" + to_string(infix.op) + " " + render(*infix.left) + " " + render(*infix.right) + ")";
        return result;
    }

    std::string operator()(const Expression::Grouping& grouping) const {
        return "(group " + render(*grouping.expression) + ")";
    }

    std::string operator()(const Expression::Unit&) const {
        return "(unit)";
    }

    std::string operator()(const Expression::Assign& assign) const {
        return "(assign " + assign.name + " = " + render(*assign.value) + ")";
    }

    std::string operator()(const Expression::Variable& variable) const {
        return variable.name;
    }

    std::string operator()(const Expression::Block& block) const {
        std::string result = "{ ";
        for (const auto& stmt : block.statements) {
            result += render(*stmt) + " ";
        }
        if (block.value) {
            result += render(*block.value) + " ";
        }
        result += "}";
        return result;
    }

    std::string operator()(const Expression::If& if_expr) const {
        std::string result = "(if " + render(*if_expr.condition) + " " + render(*if_expr.then);
        if (if_expr.else_branch) {
            result += " else " + render(*if_expr.else_branch);
        }
        result += ")";
        return result;
    }

    std::string operator()(const Expression::While& while_expr) const {
        return "(while " + render(*while_expr.condition) + " " + render(*while_expr.body) + ")";
    }

    std::string operator()(const Expression::For& for_expr) const {
        return "(for " + render(*for_expr.pattern) + " in " + render(*for_expr.iterable) + " " + render(*for_expr.body) + ")";
    }

    std::string operator()(const Expression::Function& fn_expr) const {
        std::ostringstream oss;
        oss << "(fn (";
        for (std::size_t i = 0; i < fn_expr.parameters.size(); ++i) {
            if (i > 0) {
                oss << ' ';
            }
            oss << fn_expr.parameters[i].name;
        }
        oss << ") " << render(*fn_expr.body) << ")";
        return oss.str();
    }
};

std::string render(const Expression& expression) {
    return std::visit(Printer{}, expression.node);
}

struct StatementPrinter {
    std::string operator()(const Statement::Let& let) const {
        return "(let " + let.name + " = " + render(*let.value) + ")";
    }

    std::string operator()(const Statement::Expression& expr_stmt) const {
        return "(expr " + render(*expr_stmt.expression) + ")";
    }
};

std::string render(const Statement& statement) {
    return std::visit(StatementPrinter{}, statement.node);
}

}  // namespace

std::string pretty_print(const Expression& expression) {
    return render(expression);
}

}  // namespace sonar
