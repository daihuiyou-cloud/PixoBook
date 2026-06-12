#ifndef METADATA_H
#define METADATA_H

#include <QString>

struct Metadata {
    QString assetId;
    QString source;
    QString prompt;
    QString negativePrompt;
    long long seed = 0;
    int steps = 0;
    double cfgScale = 0.0;
    QString modelName;
    QString sampler;
    QString rawJson;
};

#endif
