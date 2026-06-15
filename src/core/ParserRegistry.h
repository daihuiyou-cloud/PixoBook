#ifndef PARSERREGISTRY_H
#define PARSERREGISTRY_H

#include <QString>
#include <QStringList>
#include <map>
#include <memory>
#include "core/IParser.h"
#include "models/Metadata.h"

class ParserRegistry
{
public:
    static ParserRegistry& instance();

    void registerParser(std::unique_ptr<IParser> parser);
    IParser* parserForSource(const QString &source) const;
    QStringList registeredSources() const;

    Metadata parse(const QString &filePath) const;

    static QString detectSource(const QString &filePath);

private:
    ParserRegistry() = default;
    std::map<QString, std::unique_ptr<IParser>> m_parsers;
};

#endif
