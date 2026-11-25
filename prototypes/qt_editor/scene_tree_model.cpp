#include "scene_tree_model.hpp"

#include "editor_context.hpp"

#include <QMimeData>

using namespace proto;

SceneTreeModel::SceneTreeModel(EditorContext* context, QObject* parent)
    : QAbstractItemModel(parent), context_(context) {
    for (const GameObject& object : context_->scene().objects) shownIds_.push_back(object.id);
}

void SceneTreeModel::refresh() {
    std::vector<ObjectId> ids;
    for (const GameObject& object : context_->scene().objects) ids.push_back(object.id);
    if (ids == shownIds_) {
        // Same rows: only names may have changed. Tell the view to re-read them.
        if (!ids.empty()) emit dataChanged(index(0, 0), index(static_cast<int>(ids.size()) - 1, 0));
        return;
    }
    // Rows were added or removed. A full reset is the simplest correct choice;
    // a big real-world tree would use beginInsertRows/beginRemoveRows instead
    // so the view keeps its scroll position and expanded folders.
    beginResetModel();
    shownIds_ = std::move(ids);
    endResetModel();
}

ObjectId SceneTreeModel::idAt(const QModelIndex& index) const {
    if (!index.isValid() || index.row() >= static_cast<int>(shownIds_.size())) return 0;
    return shownIds_[index.row()];
}

QModelIndex SceneTreeModel::indexOf(ObjectId id) const {
    for (int row = 0; row < static_cast<int>(shownIds_.size()); ++row)
        if (shownIds_[row] == id) return index(row, 0);
    return {};
}

QModelIndex SceneTreeModel::index(int row, int column, const QModelIndex& parent) const {
    // Flat list: only the invisible root has children.
    if (parent.isValid() || column != 0 || row < 0 || row >= static_cast<int>(shownIds_.size())) return {};
    return createIndex(row, column);
}

QModelIndex SceneTreeModel::parent(const QModelIndex&) const { return {}; }

int SceneTreeModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(shownIds_.size());
}

int SceneTreeModel::columnCount(const QModelIndex&) const { return 1; }

QVariant SceneTreeModel::data(const QModelIndex& index, int role) const {
    const GameObject* object = context_->scene().findObject(idAt(index));
    if (!object) return {};
    if (role == Qt::DisplayRole) return object->name.empty() ? QStringLiteral("(unnamed)") : QString::fromStdString(object->name);
    if (role == Qt::EditRole) return QString::fromStdString(object->name);
    if (role == Qt::ToolTipRole) return QStringLiteral("Double-click or F2 to rename. Drop an asset here to assign it.");
    return {};
}

bool SceneTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    // The view calls this when the user finishes typing a new name.
    const GameObject* object = context_->scene().findObject(idAt(index));
    if (!object || role != Qt::EditRole) return false;
    std::string name = value.toString().toStdString();
    if (name == object->name) return false;
    context_->execute(std::make_unique<RenameObjectCommand>(object->id, name));
    return true;
}

Qt::ItemFlags SceneTreeModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;  // dropping between rows is not supported
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDropEnabled;
}

QStringList SceneTreeModel::mimeTypes() const { return {kAssetMimeType}; }

Qt::DropActions SceneTreeModel::supportedDropActions() const { return Qt::CopyAction; }

bool SceneTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction, int, int,
                                     const QModelIndex& parent) const {
    // `parent` is the row under the mouse when dropping ONTO a row.
    return data->hasFormat(kAssetMimeType) && idAt(parent) != 0;
}

bool SceneTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                  const QModelIndex& parent) {
    if (!canDropMimeData(data, action, row, column, parent)) return false;
    AssetId asset = data->data(kAssetMimeType).toLongLong();
    ObjectId target = idAt(parent);
    context_->execute(std::make_unique<AssignSpriteCommand>(target, asset));
    context_->select(target);
    return true;
}
