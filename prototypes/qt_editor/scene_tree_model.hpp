#pragma once
// =============================================================================
// SceneTreeModel — feeds the Hierarchy panel (a QTreeView).
//
// Qt's "model/view" design: the view (QTreeView) draws rows but owns no data;
// it asks a model "how many rows?", "what text is in row 3?". This model
// answers by reading the proto_core scene. Edits made in the view (renaming,
// dropping an asset on a row) come back here and become proto_core commands.
//
// QAbstractItemModel supports full trees (children inside children). This
// prototype's hierarchy is flat, but the real editor's parent/child objects and
// prefabs fit the same class without changing the view.
// =============================================================================

#include "proto_core/model.hpp"

#include <QAbstractItemModel>

#include <vector>

class EditorContext;

// The MIME type (a name for the kind of data) used when dragging assets.
inline constexpr const char* kAssetMimeType = "application/x-gbe-asset-id";

class SceneTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit SceneTreeModel(EditorContext* context, QObject* parent = nullptr);

    // Called after any scene change. Cheap when only names changed; resets the
    // model when objects were added or removed.
    void refresh();

    proto::ObjectId idAt(const QModelIndex& index) const;
    QModelIndex indexOf(proto::ObjectId id) const;

    // --- the QAbstractItemModel interface ---
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // --- drag and drop: accept assets dropped onto a row ---
    QStringList mimeTypes() const override;
    Qt::DropActions supportedDropActions() const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                         const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

private:
    EditorContext* context_;
    std::vector<proto::ObjectId> shownIds_;  // what the view currently believes the rows are
};
