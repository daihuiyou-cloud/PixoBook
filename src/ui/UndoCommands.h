#ifndef UNDOCOMMANDS_H
#define UNDOCOMMANDS_H

#include <QUndoCommand>
#include <QVector>
#include <QString>
#include "models/Asset.h"
#include "models/Metadata.h"

class LibraryController;

class DeleteAssetsCommand : public QUndoCommand
{
public:
    DeleteAssetsCommand(LibraryController *ctrl, const QVector<Asset> &assets,
                        const QVector<Metadata> &metadataList,
                        const QVector<QVector<int>> &tagIdList,
                        QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    LibraryController *m_ctrl;
    QVector<Asset> m_assets;
    QVector<Metadata> m_metadataList;
    QVector<QVector<int>> m_tagIdList;
};

class ToggleFavoriteCommand : public QUndoCommand
{
public:
    ToggleFavoriteCommand(LibraryController *ctrl, const QString &assetId,
                          bool oldState, bool newState,
                          QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    LibraryController *m_ctrl;
    QString m_assetId;
    bool m_oldState;
    bool m_newState;
};

class AddTagToAssetsCommand : public QUndoCommand
{
public:
    AddTagToAssetsCommand(LibraryController *ctrl, const QVector<QString> &assetIds,
                          int tagId, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    LibraryController *m_ctrl;
    QVector<QString> m_assetIds;
    int m_tagId;
};

class RemoveTagFromAssetCommand : public QUndoCommand
{
public:
    RemoveTagFromAssetCommand(LibraryController *ctrl, const QString &assetId,
                              int tagId, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    LibraryController *m_ctrl;
    QString m_assetId;
    int m_tagId;
};

#endif
