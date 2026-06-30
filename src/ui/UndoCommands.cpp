#include "UndoCommands.h"
#include "core/IDatabaseManager.h"
#include "services/LibraryController.h"

// --- DeleteAssetsCommand ---

DeleteAssetsCommand::DeleteAssetsCommand(LibraryController *ctrl,
                                         const QVector<Asset> &assets,
                                         const QVector<Metadata> &metadataList,
                                         const QVector<QVector<int>> &tagIdList,
                                         QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_ctrl(ctrl)
    , m_assets(assets)
    , m_metadataList(metadataList)
    , m_tagIdList(tagIdList)
{
    setText(QObject::tr("删除 %1 张图片").arg(assets.size()));
}

void DeleteAssetsCommand::undo()
{
    IDatabaseManager *db = m_ctrl->db();
    if (!db->beginTransaction()) return;
    bool ok = true;
    for (int i = 0; i < m_assets.size() && ok; ++i) {
        ok = db->insertAsset(m_assets[i]);
        if (ok && i < m_metadataList.size() && !m_metadataList[i].source.isEmpty()) {
            Metadata meta = m_metadataList[i];
            meta.assetId = m_assets[i].id;
            ok = db->upsertMetadata(meta);
        }
        if (ok && i < m_tagIdList.size()) {
            for (int tagId : qAsConst(m_tagIdList[i]))
                ok = db->addTagToAsset(m_assets[i].id, tagId);
        }
    }
    if (ok) {
        if (db->commitTransaction())
            emit m_ctrl->dataChanged();
        else
            (void)db->rollbackTransaction();
    } else {
        (void)db->rollbackTransaction();
    }
}

void DeleteAssetsCommand::redo()
{
    (void)m_ctrl->deleteAssets(m_assets);
}

// --- ToggleFavoriteCommand ---

ToggleFavoriteCommand::ToggleFavoriteCommand(LibraryController *ctrl,
                                             const QString &assetId,
                                             bool oldState, bool newState,
                                             QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_ctrl(ctrl)
    , m_assetId(assetId)
    , m_oldState(oldState)
    , m_newState(newState)
{
    setText(newState ? QObject::tr("收藏图片") : QObject::tr("取消收藏"));
}

void ToggleFavoriteCommand::undo()
{
    m_ctrl->toggleFavorite(m_assetId, m_oldState);
}

void ToggleFavoriteCommand::redo()
{
    m_ctrl->toggleFavorite(m_assetId, m_newState);
}

// --- AddTagToAssetsCommand ---

AddTagToAssetsCommand::AddTagToAssetsCommand(LibraryController *ctrl,
                                             const QVector<QString> &assetIds,
                                             int tagId,
                                             QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_ctrl(ctrl)
    , m_assetIds(assetIds)
    , m_tagId(tagId)
{
    setText(QObject::tr("添加标签"));
}

void AddTagToAssetsCommand::undo()
{
    IDatabaseManager *db = m_ctrl->db();
    for (const auto &aid : qAsConst(m_assetIds))
        (void)db->removeTagFromAsset(aid, m_tagId);
    emit m_ctrl->dataChanged();
}

void AddTagToAssetsCommand::redo()
{
    m_ctrl->addTagToAssets(m_assetIds, m_tagId);
}

// --- RemoveTagFromAssetCommand ---

RemoveTagFromAssetCommand::RemoveTagFromAssetCommand(LibraryController *ctrl,
                                                     const QString &assetId,
                                                     int tagId,
                                                     QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_ctrl(ctrl)
    , m_assetId(assetId)
    , m_tagId(tagId)
{
    setText(QObject::tr("移除标签"));
}

void RemoveTagFromAssetCommand::undo()
{
    IDatabaseManager *db = m_ctrl->db();
    (void)db->addTagToAsset(m_assetId, m_tagId);
    emit m_ctrl->dataChanged();
}

void RemoveTagFromAssetCommand::redo()
{
    m_ctrl->removeTagFromAsset(m_assetId, m_tagId);
}
