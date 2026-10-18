// PixoBook benchmark: fixed-cost measurements for the parts users feel.
//
//   pixobook_bench [--assets N] [--runs N]
//
// It builds a synthetic library (prompts, tags, metadata) in a temporary
// database, then reports batched-import time, search latency (p50/p95) and
// pagination cost. The numbers are meant to be compared across commits, not
// to model a specific machine.

#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>
#include <algorithm>
#include <functional>

#include "database/DatabaseManager.h"

namespace {

struct Stats {
    double minMs = 0.0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
};

Stats summarize(QVector<double> samples)
{
    Stats stats;
    if (samples.isEmpty())
        return stats;
    std::sort(samples.begin(), samples.end());
    stats.minMs = samples.first();
    stats.p50Ms = samples.at(samples.size() / 2);
    const int p95Index = qMin(samples.size() - 1,
                              static_cast<int>(samples.size() * 0.95));
    stats.p95Ms = samples.at(p95Index);
    return stats;
}

QStringList promptWords()
{
    return {QStringLiteral("portrait"), QStringLiteral("cinematic"),
            QStringLiteral("neon"), QStringLiteral("forest"),
            QStringLiteral("cyberpunk"), QStringLiteral("watercolor"),
            QStringLiteral("macro"), QStringLiteral("studio"),
            QStringLiteral("sunset"), QStringLiteral("portrait"),
            QStringLiteral("architecture"), QStringLiteral("minimal"),
            QStringLiteral("dramatic"), QStringLiteral("fog"),
            QStringLiteral("detail"), QStringLiteral("wide-angle")};
}

QString makePrompt(QRandomGenerator &rng)
{
    const QStringList words = promptWords();
    const int count = 5 + static_cast<int>(rng.bounded(5));
    QStringList parts;
    parts.reserve(count);
    for (int i = 0; i < count; ++i)
        parts << words.at(static_cast<int>(rng.bounded(words.size())));
    // A rare token so "needle" matches a small subset.
    if (rng.bounded(100) == 0)
        parts << QStringLiteral("needle");
    // A CJK phrase so CJK search has something to find.
    if (rng.bounded(50) == 0)
        parts << QStringLiteral("一只猫在月光下");
    return parts.join(QLatin1Char(' '));
}

Asset makeAsset(int index, const QDateTime &created)
{
    Asset a;
    a.id = QStringLiteral("asset-%1").arg(index, 7, 10, QLatin1Char('0'));
    a.filePath = QStringLiteral("C:/library/img-%1.png").arg(index, 7, 10, QLatin1Char('0'));
    a.fileName = QStringLiteral("img-%1.png").arg(index, 7, 10, QLatin1Char('0'));
    a.fileSize = 1024 + index;
    a.width = 1024;
    a.height = 1024;
    a.format = QStringLiteral("png");
    a.hash = QStringLiteral("hash-%1").arg(index, 8, 16, QLatin1Char('0'));
    a.folderId = QStringLiteral("C:/library");
    a.createdAt = created;
    a.updatedAt = created;
    return a;
}

void printStats(QTextStream &out, const QString &label, const Stats &stats)
{
    out << QStringLiteral("%1  min %2 ms  p50 %3 ms  p95 %4 ms\n")
               .arg(label, -34)
               .arg(stats.minMs, 8, 'f', 2)
               .arg(stats.p50Ms, 8, 'f', 2)
               .arg(stats.p95Ms, 8, 'f', 2);
}

double measureMs(const std::function<void()> &work)
{
    QElapsedTimer timer;
    timer.start();
    work();
    return static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    int assetCount = 10000;
    int runs = 50;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QLatin1String("--assets") && i + 1 < args.size())
            assetCount = args.at(++i).toInt();
        else if (args.at(i) == QLatin1String("--runs") && i + 1 < args.size())
            runs = qMax(1, args.at(++i).toInt());
        else if (args.at(i) == QLatin1String("--help")) {
            out << QStringLiteral("usage: pixobook_bench [--assets N] [--runs N]\n");
            return 0;
        }
    }
    assetCount = qMax(1, assetCount);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        out << QStringLiteral("could not create temporary directory\n");
        return 1;
    }

    DatabaseManager db(tempDir.filePath(QStringLiteral("bench.db")));
    if (!db.initialize()) {
        out << QStringLiteral("could not initialize database\n");
        return 1;
    }

    QRandomGenerator rng(20240101u);
    const QDateTime base = QDateTime::fromString(QStringLiteral("2024-01-01T00:00:00"),
                                                 Qt::ISODate);
    if (!base.isValid())
        return 1;

    // Tags are created up front so the import transaction only inserts rows.
    QVector<int> tagIds;
    for (int t = 0; t < 20; ++t) {
        Tag tag;
        tag.name = QStringLiteral("tag-%1").arg(t);
        tag.color = QColor::fromHsv((t * 18) % 360, 200, 220);
        const int id = db.insertTag(tag);
        if (id < 0) {
            out << QStringLiteral("could not create tags\n");
            return 1;
        }
        tagIds.append(id);
    }

    out << QStringLiteral("PixoBook benchmark - %1 assets, %2 runs per query\n")
               .arg(assetCount)
               .arg(runs);
    out << QStringLiteral("search backend                 %1\n")
               .arg(db.isFullTextSearchEnabled() ? QStringLiteral("FTS5")
                                                 : QStringLiteral("LIKE (fallback)"));

    const double importMs = measureMs([&]() {
        if (!db.beginTransaction()) {
            out << QStringLiteral("could not begin import transaction\n");
            return;
        }
        for (int i = 0; i < assetCount; ++i) {
            const QString prompt = makePrompt(rng);
            const QDateTime created = base.addSecs(i);
            Asset a = makeAsset(i, created);
            a.isFavorite = (rng.bounded(100) < 5);
            if (!db.insertAsset(a))
                continue;
            Metadata m;
            m.assetId = a.id;
            m.source = QStringLiteral("stable-diffusion");
            m.prompt = prompt;
            m.negativePrompt = QStringLiteral("blurry, low quality");
            m.seed = static_cast<long long>(rng.bounded(1000000));
            m.steps = 20 + static_cast<int>(rng.bounded(20));
            m.cfgScale = 7.0;
            m.modelName = QStringLiteral("sdxl");
            m.sampler = QStringLiteral("dpmpp_2m");
            (void)db.upsertMetadata(m);

            const int tagCount = (rng.bounded(100) < 60) ? 1 + static_cast<int>(rng.bounded(2)) : 0;
            for (int t = 0; t < tagCount; ++t) {
                const int tagId = tagIds.at(static_cast<int>(rng.bounded(tagIds.size())));
                (void)db.addTagToAsset(a.id, tagId);
            }
        }
        if (!db.commitTransaction()) {
            out << QStringLiteral("could not commit import transaction\n");
            (void)db.rollbackTransaction();
        }
    });
    out << QStringLiteral("import (one transaction)        %1 ms  (%2 assets/s)\n")
               .arg(importMs, 8, 'f', 1)
               .arg(assetCount * 1000.0 / qMax(importMs, 0.001), 9, 'f', 0);

    const struct QueryCase {
        const char *label;
        const char *keyword;
    } cases[] = {
        {"search common word (\"portrait\")", "portrait"},
        {"search selective word (\"needle\")", "needle"},
        {"search miss (\"zzzzzz\")", "zzzzzz"},
        {"search CJK (\"月光\")", "月光"},
    };

    for (const auto &c : cases) {
        QVector<double> samples;
        samples.reserve(runs);
        for (int i = 0; i < 3; ++i) {
            int total = 0;
            (void)db.searchAssets(QString::fromLatin1(c.keyword), {}, {}, false,
                                  QStringLiteral("created_at"), false, 0, 200, &total);
        }
        for (int i = 0; i < runs; ++i) {
            int total = 0;
            samples.append(measureMs([&]() {
                (void)db.searchAssets(QString::fromLatin1(c.keyword), {}, {}, false,
                                      QStringLiteral("created_at"), false, 0, 200, &total);
            }));
        }
        printStats(out, QString::fromLatin1(c.label), summarize(samples));
    }

    {
        QVector<double> samples;
        samples.reserve(runs);
        for (int i = 0; i < runs; ++i) {
            int total = 0;
            samples.append(measureMs([&]() {
                (void)db.searchAssets(QString(), {}, {}, false,
                                      QStringLiteral("created_at"), false, 2000, 200, &total);
            }));
        }
        printStats(out, QStringLiteral("page depth 11 (offset 2000)"), summarize(samples));
    }

    {
        QVector<double> samples;
        samples.reserve(runs);
        for (int i = 0; i < runs; ++i) {
            samples.append(measureMs([&]() { (void)db.countMissingAssets(); }));
        }
        printStats(out, QStringLiteral("count missing records"), summarize(samples));
    }

    out << QStringLiteral("database size                  %1 MB\n")
               .arg(QFileInfo(tempDir.filePath(QStringLiteral("bench.db"))).size()
                        / (1024.0 * 1024.0), 0, 'f', 1);
    out.flush();
    return 0;
}
