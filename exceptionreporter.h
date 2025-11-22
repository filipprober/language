#ifndef LANG_EXCEPTIONREPORTER_H
#define LANG_EXCEPTIONREPORTER_H

using namespace std;

class ExceptionReporter
{
public:
    ExceptionReporter(const string& source, const string& filename) :
        source(source),
        filename(filename),
        hasErrors(false) {
    }

    void error(const SourceLocation& location, const string& message) {
        hasErrors = true;

        cerr << "\033[1;31mError:\033[0m " << message << endl;
        cerr << "  \033[1;34m-->\033[0m " << filename << ":" << location.line << ":" << location.column << endl;

        printSourceLine(location);
    }

    void warning(const SourceLocation& location, const string& message) {
        cerr << "\033[1;33mWarning:\033[0m " << message << endl;
        cerr << "  \033[1;34m-->\033[0m " << filename << ":" << location.line << ":" << location.column << endl;

        printSourceLine(location);
    }

    bool hasError() const {
        return hasErrors;
    }

private:
    string source;
    string filename;
    bool hasErrors;

    void printSourceLine(const SourceLocation& location) {
        vector<string> lines;
        stringstream ss(source);
        string line;
        while (getline(ss, line)) {
            lines.push_back(line);
        }

        if (location.line < 1 || location.line > static_cast<int>(lines.size())) {
            return;
        }

        string sourceLine = lines[location.line - 1];

        int lineNumWidth = to_string(location.line).length();
        string padding(lineNumWidth, ' ');

        cerr << padding << " |" << endl;
        cerr << location.line << " | " << sourceLine << endl;
        cerr << padding << " | ";

        for (int i = 0; i < location.column - 1; i++) {
            cerr << " ";
        }

        cerr << "\033[1;31m";
        for (int i = 0; i < location.length; i++) {
            cerr << "^";
        }
        cerr << "\033[0m" << endl;
        cerr << endl;
    }
};

#endif //LANG_EXCEPTIONREPORTER_H