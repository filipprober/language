#ifndef LANG_SOURCELOCATION_H
#define LANG_SOURCELOCATION_H

struct SourceLocation
{
    int line;
    int column;
    int length;

    SourceLocation(int line, int column, int length) :
        line(line),
        column(column),
        length(length) {
    }
};

#endif //LANG_SOURCELOCATION_H