#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <string>
#include <map>
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
    virtual void analyze(vector<string>& lines, vector<ErrorReport>& reports) = 0;
    virtual ~Analyzer() {}
};

class RuntimeAnalyzer : public Analyzer {
public:
    void analyze(vector<string>& lines, vector<ErrorReport>& reports) override {
        map<string, int> zeroVariables;

        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            smatch zeroMatch;
            regex zeroPattern("int\\s+(\\w+)\\s*=\\s*0\\s*;");
            if (regex_search(line, zeroMatch, zeroPattern)) {
                zeroVariables[zeroMatch[1]] = i + 1;
            }

            if (regex_search(line, regex("/\\s*0"))) {
                reports.push_back(ErrorReport(
                    i + 1,
                    "Division by Zero",
                    "High",
                    "The code directly divides a value by zero.",
                    "Check denominator before division."
                ));
            }

            for (auto const& item : zeroVariables) {
                string var = item.first;
                regex divVarPattern("/\\s*" + var + "\\b");

                if (regex_search(line, divVarPattern)) {
                    reports.push_back(ErrorReport(
                        i + 1,
                        "Division by Zero",
                        "High",
                        "Variable '" + var + "' is zero and used as divisor.",
                        "Check if divisor is not zero before division."
                    ));
                }
            }

            string noSpace = regex_replace(line, regex("\\s+"), "");

            if (noSpace.find("while(true)") != string::npos || noSpace.find("while(1)") != string::npos) {
                bool hasBreak = false;

                for (int j = i; j < lines.size(); j++) {
                    if (lines[j].find("break") != string::npos) {
                        hasBreak = true;
                        break;
                    }
                }

                if (!hasBreak) {
                    reports.push_back(ErrorReport(
                        i + 1,
                        "Possible Infinite Loop",
                        "Medium",
                        "Loop may run forever without a stopping condition.",
                        "Add a valid break condition."
                    ));
                }
            }

            if (line.find("cin") != string::npos) {
                bool hasTry = false;

                for (string l : lines) {
                    if (l.find("try") != string::npos) {
                        hasTry = true;
                        break;
                    }
                }

                if (!hasTry) {
                    reports.push_back(ErrorReport(
                        i + 1,
                        "Missing Exception Handling",
                        "Low",
                        "Input operation found without exception handling.",
                        "Use try-catch for safer input handling."
                    ));
                }
            }
        }
    }
};

class PointerAnalyzer : public Analyzer {
public:
    void analyze(vector<string>& lines, vector<ErrorReport>& reports) override {
        vector<pair<string, int>> uninitializedPointers;
        bool hasDelete = false;

        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (line.find("delete") != string::npos) {
                hasDelete = true;
            }

            smatch ptrMatch;
            regex ptrDeclPattern("\\b(int|float|double|char|string)\\s*\\*\\s*(\\w+)\\s*;");

            if (regex_search(line, ptrMatch, ptrDeclPattern)) {
                string pointerName = ptrMatch[2];
                uninitializedPointers.push_back({pointerName, i + 1});
            }
        }

        for (auto p : uninitializedPointers) {
            string pointerName = p.first;

            for (int i = 0; i < lines.size(); i++) {
                string line = lines[i];

                regex dereferencePattern("\\*" + pointerName + "\\b");

                if (regex_search(line, dereferencePattern) && line.find("*" + pointerName + ";") == string::npos) {
                    reports.push_back(ErrorReport(
                        i + 1,
                        "Uninitialized Pointer",
                        "High",
                        "Pointer '" + pointerName + "' is dereferenced without initialization.",
                        "Initialize pointer before dereferencing."
                    ));
                }
            }
        }

        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (line.find("new ") != string::npos && !hasDelete) {
                reports.push_back(ErrorReport(
                    i + 1,
                    "Memory Leak",
                    "High",
                    "Memory is allocated using new but not released.",
                    "Use delete or delete[] to release memory."
                ));
            }
        }
    }
};

class ArrayAnalyzer : public Analyzer {
public:
    void analyze(vector<string>& lines, vector<ErrorReport>& reports) override {
        map<string, int> arrays;

        regex arrayDeclPattern("\\b(int|float|double|char|string)\\s+(\\w+)\\[(\\d+)\\]");
        smatch match;

        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (regex_search(line, match, arrayDeclPattern)) {
                string arrayName = match[2];
                int size = stoi(match[3]);
                arrays[arrayName] = size;
            }
        }

        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (regex_search(line, arrayDeclPattern)) {
                continue;
            }

            for (auto const& item : arrays) {
                string arrayName = item.first;
                int size = item.second;

                regex accessPattern(arrayName + "\\[(\\d+)\\]");
                smatch accessMatch;

                if (regex_search(line, accessMatch, accessPattern)) {
                    int index = stoi(accessMatch[1]);

                    if (index >= size) {
                        reports.push_back(ErrorReport(
                            i + 1,
                            "Array Out of Bounds",
                            "High",
                            "Array index exceeds declared array size.",
                            "Use index within valid array range."
                        ));
                    }
                }
            }
        }
    }
};

class OOPAnalyzer : public Analyzer {
public:
    void analyze(vector<string>& lines, vector<ErrorReport>& reports) override {
        bool hasClass = false;
        bool hasAccessSpecifier = false;
        bool hasInheritance = false;
        bool hasVirtual = false;
        bool hasNew = false;
        bool hasDestructor = false;
        int classLine = 0;

        for (int i = 0; i < lines.size(); i++) {
            string line = lines[i];

            if (line.find("class ") != string::npos) {
                hasClass = true;
                classLine = i + 1;
            }

            if (line.find("public:") != string::npos ||
                line.find("private:") != string::npos ||
                line.find("protected:") != string::npos) {
                hasAccessSpecifier = true;
            }

            if (line.find(": public") != string::npos ||
                line.find(": private") != string::npos ||
                line.find(": protected") != string::npos) {
                hasInheritance = true;
            }

            if (line.find("virtual") != string::npos) {
                hasVirtual = true;
            }

            if (line.find("new ") != string::npos) {
                hasNew = true;
            }

            if (line.find("~") != string::npos) {
                hasDestructor = true;
            }
        }

        if (hasClass && !hasAccessSpecifier) {
            reports.push_back(ErrorReport(
                classLine,
                "Poor OOP Design",
                "Medium",
                "Class is used without access specifiers.",
                "Use public, private, or protected sections."
            ));
        }

        if (hasNew && !hasDestructor) {
            reports.push_back(ErrorReport(
                classLine,
                "Missing Destructor",
                "Medium",
                "Dynamic memory is used but destructor is missing.",
                "Add destructor to release allocated memory."
            ));
        }

        if (hasInheritance && !hasVirtual) {
            reports.push_back(ErrorReport(
                classLine,
                "Polymorphism Warning",
                "Low",
                "Inheritance is used without virtual function.",
                "Use virtual functions for runtime polymorphism."
            ));
        }
    }
};

class CodeGuardSystem {
private:
    vector<Analyzer*> analyzers;
    vector<ErrorReport> reports;

public:
    CodeGuardSystem() {
        analyzers.push_back(new RuntimeAnalyzer());
        analyzers.push_back(new PointerAnalyzer());
        analyzers.push_back(new ArrayAnalyzer());
        analyzers.push_back(new OOPAnalyzer());
    }

    ~CodeGuardSystem() {
        for (Analyzer* a : analyzers) {
            delete a;
        }
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
            analyzer->analyze(lines, reports);
        }

        generateReport(outputFile);
    }

    void generateReport(string outputFile) {
        ofstream out(outputFile);

        out << "{\n";
        out << "\"errors\": [\n";

        if (reports.empty()) {
            out << "{";
            out << "\"line\":\"-\",";
            out << "\"type\":\"No Major Issue Found\",";
            out << "\"risk\":\"Safe\",";
            out << "\"message\":\"No runtime, logical, syntax, or OOP issue detected by current analyzer.\",";
            out << "\"solution\":\"Code appears safe based on current analysis.\"";
            out << "}";
        } else {
            for (int i = 0; i < reports.size(); i++) {
                out << "{";
                out << "\"line\":\"" << reports[i].line << "\",";
                out << "\"type\":\"" << reports[i].type << "\",";
                out << "\"risk\":\"" << reports[i].risk << "\",";
                out << "\"message\":\"" << reports[i].message << "\",";
                out << "\"solution\":\"" << reports[i].solution << "\"";
                out << "}";

                if (i != reports.size() - 1) {
                    out << ",";
                }

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