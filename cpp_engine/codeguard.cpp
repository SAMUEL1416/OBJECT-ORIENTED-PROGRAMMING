#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <string>
using namespace std;

class ErrorReport {
public:
    int line;
    string type, risk, message, solution;

    ErrorReport(int l, string t, string r, string m, string s) {
        line = l;
        type = t;
        risk = r;
        message = m;
        solution = s;
    }
};

class Analyzer {
public:
    virtual void analyze(vector<string>& lines, vector<ErrorReport>& reports, int& score) = 0;
    virtual ~Analyzer() {}
};

class RuntimeAnalyzer : public Analyzer {
public:
    bool containsTry(vector<string>& lines) {
        for (string line : lines) {
            if (line.find("try") != string::npos)
                return true;
        }
        return false;
    }

    void analyze(vector<string>& lines, vector<ErrorReport>& reports, int& score) override {
        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (regex_search(line, regex("/\\s*0"))) {
                reports.push_back(ErrorReport(i + 1, "Division by Zero", "High",
                    "Division by zero may crash the program.",
                    "Check denominator before division."));
                score -= 10;
            }

            string noSpace = regex_replace(line, regex("\\s+"), "");

            if (noSpace.find("while(true)") != string::npos || noSpace.find("while(1)") != string::npos) {
                reports.push_back(ErrorReport(i + 1, "Possible Infinite Loop", "Medium",
                    "Loop may run forever without a stopping condition.",
                    "Add break condition or valid loop condition."));
                score -= 5;
            }

            if (line.find("cin") != string::npos && !containsTry(lines)) {
                reports.push_back(ErrorReport(i + 1, "Missing Exception Handling", "Low",
                    "Input operation found without exception handling.",
                    "Use try-catch for safer input handling."));
                score -= 2;
            }
        }
    }
};

class PointerAnalyzer : public Analyzer {
public:
    bool containsDelete(vector<string>& lines) {
        for (string line : lines) {
            if (line.find("delete") != string::npos)
                return true;
        }
        return false;
    }

    void analyze(vector<string>& lines, vector<ErrorReport>& reports, int& score) override {
        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (regex_search(line, regex("\\w+\\s*\\*\\s*\\w+\\s*;"))) {
                reports.push_back(ErrorReport(i + 1, "Uninitialized Pointer", "High",
                    "Pointer is declared but not initialized.",
                    "Initialize pointer with nullptr or allocate memory."));
                score -= 8;
            }

            if (line.find("new ") != string::npos && !containsDelete(lines)) {
                reports.push_back(ErrorReport(i + 1, "Memory Leak", "High",
                    "Memory is allocated using new but not released.",
                    "Use delete or delete[] to free memory."));
                score -= 10;
            }
        }
    }
};

class ArrayAnalyzer : public Analyzer {
public:
    void analyze(vector<string>& lines, vector<ErrorReport>& reports, int& score) override {
        regex arrayPattern("int\\s+(\\w+)\\[(\\d+)\\]");
        smatch match;

        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (regex_search(line, match, arrayPattern)) {
                string arrayName = match[1];
                int size = stoi(match[2]);

                regex accessPattern(arrayName + "\\[(\\d+)\\]");
                smatch accessMatch;

                for (int j = 0; j < lines.size(); j++) {
                    string accessLine = lines[j];

                    if (regex_search(accessLine, accessMatch, accessPattern)) {
                        int index = stoi(accessMatch[1]);

                        if (index >= size) {
                            reports.push_back(ErrorReport(j + 1, "Array Out of Bounds", "High",
                                "Array index exceeds declared array size.",
                                "Use index within valid array range."));
                            score -= 10;
                        }
                    }
                }
            }
        }
    }
};

class OOPAnalyzer : public Analyzer {
public:
    void analyze(vector<string>& lines, vector<ErrorReport>& reports, int& score) override {
        bool hasClass = false;
        bool hasAccessSpecifier = false;
        bool hasInheritance = false;
        bool hasVirtual = false;
        bool hasNew = false;
        bool hasDestructor = false;

        for (string line : lines) {
            if (line.find("class ") != string::npos)
                hasClass = true;

            if (line.find("public:") != string::npos ||
                line.find("private:") != string::npos ||
                line.find("protected:") != string::npos)
                hasAccessSpecifier = true;

            if (line.find(": public") != string::npos ||
                line.find(": private") != string::npos ||
                line.find(": protected") != string::npos)
                hasInheritance = true;

            if (line.find("virtual") != string::npos)
                hasVirtual = true;

            if (line.find("new ") != string::npos)
                hasNew = true;

            if (line.find("~") != string::npos)
                hasDestructor = true;
        }

        if (hasClass && !hasAccessSpecifier) {
            reports.push_back(ErrorReport(0, "Poor OOP Design", "Medium",
                "Class is used without access specifiers.",
                "Use public, private, or protected sections."));
            score -= 5;
        }

        if (hasNew && !hasDestructor) {
            reports.push_back(ErrorReport(0, "Missing Destructor", "Medium",
                "Dynamic memory is used but destructor is missing.",
                "Add destructor to release memory."));
            score -= 5;
        }

        if (hasInheritance && !hasVirtual) {
            reports.push_back(ErrorReport(0, "Polymorphism Warning", "Low",
                "Inheritance is used but no virtual function is found.",
                "Use virtual functions for runtime polymorphism."));
            score -= 2;
        }
    }
};

class CodeGuardSystem {
private:
    vector<Analyzer*> analyzers;
    vector<ErrorReport> reports;
    int score;

public:
    CodeGuardSystem() {
        score = 100;
        analyzers.push_back(new RuntimeAnalyzer());
        analyzers.push_back(new PointerAnalyzer());
        analyzers.push_back(new ArrayAnalyzer());
        analyzers.push_back(new OOPAnalyzer());
    }

    ~CodeGuardSystem() {
        for (Analyzer* a : analyzers)
            delete a;
    }

    vector<string> readFile(string fileName) {
        vector<string> lines;
        ifstream file(fileName);
        string line;

        while (getline(file, line)) {
            lines.push_back(line);
        }

        return lines;
    }

    void analyzeCode(string inputFile, string outputFile) {
        vector<string> lines = readFile(inputFile);

        for (Analyzer* analyzer : analyzers) {
            analyzer->analyze(lines, reports, score);
        }

        if (reports.empty()) {
            score = 100;
        } else {
            if (score < 10)
                score = 10;
            if (score > 100)
                score = 100;
        }

        generateReport(outputFile);
    }

    void generateReport(string outputFile) {
        ofstream out(outputFile);

        out << "{\n";
        out << "\"score\": " << score << ",\n";

        if (reports.empty())
            out << "\"total_errors\": 0,\n";
        else
            out << "\"total_errors\": " << reports.size() << ",\n";

        out << "\"errors\": [\n";

        if (reports.empty()) {
            out << "{";
            out << "\"line\":\"-\",";
            out << "\"type\":\"No Major Issue Found\",";
            out << "\"risk\":\"Safe\",";
            out << "\"message\":\"No common runtime error patterns detected.\",";
            out << "\"solution\":\"Code appears safe based on basic analysis.\"";
            out << "}";
        } else {
            for (int i = 0; i < reports.size(); i++) {
                out << "{";
                out << "\"line\":\"" << (reports[i].line == 0 ? "-" : to_string(reports[i].line)) << "\",";
                out << "\"type\":\"" << reports[i].type << "\",";
                out << "\"risk\":\"" << reports[i].risk << "\",";
                out << "\"message\":\"" << reports[i].message << "\",";
                out << "\"solution\":\"" << reports[i].solution << "\"";
                out << "}";

                if (i != reports.size() - 1)
                    out << ",";
                out << "\n";
            }
        }

        out << "]\n";
        out << "}\n";
        out.close();
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage: codeguard input.cpp output.json";
        return 1;
    }

    CodeGuardSystem system;
    system.analyzeCode(argv[1], argv[2]);

    return 0;
}