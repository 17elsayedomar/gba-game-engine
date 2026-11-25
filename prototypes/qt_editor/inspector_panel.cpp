#include "inspector_panel.hpp"

#include "editor_context.hpp"
#include "scene_tree_model.hpp"  // kAssetMimeType

#include "proto_core/components.hpp"

#include <QDragEnterEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

using namespace proto;

void AssetDropCombo::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kAssetMimeType)) event->acceptProposedAction();
}

void AssetDropCombo::dropEvent(QDropEvent* event) {
    emit assetDropped(event->mimeData()->data(kAssetMimeType).toLongLong());
    event->acceptProposedAction();
}

InspectorPanel::InspectorPanel(EditorContext* context, QWidget* parent)
    : QWidget(parent), context_(context), scroll_(new QScrollArea(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll_);
    scroll_->setWidgetResizable(true);
    connect(context_, &EditorContext::sceneChanged, this, &InspectorPanel::refresh);
    connect(context_, &EditorContext::selectionChanged, this, &InspectorPanel::refresh);
    refresh();
}

std::string InspectorPanel::structureKey() const {
    // Selected object id + its component types (+ the asset list, which fills the
    // drop-downs). If this text is unchanged, the existing widgets still fit.
    const GameObject* object = context_->scene().findObject(context_->selected());
    if (!object) return "none";
    std::string key = std::to_string(object->id);
    for (const Component& component : object->components) key += "|" + component.type;
    key += "|assets";
    for (const Asset& asset : context_->scene().assets) key += ":" + std::to_string(asset.id);
    return key;
}

void InspectorPanel::refresh() {
    std::string key = structureKey();
    if (key != builtKey_) {
        rebuild();
        builtKey_ = key;
    }
    for (auto& update : updaters_) update();
}

void InspectorPanel::rebuild() {
    // Old widgets are deleted LATER (after the current event finishes), because
    // the click that triggered this rebuild may have come from one of them.
    if (QWidget* old = scroll_->takeWidget()) old->deleteLater();
    updaters_.clear();
    nameEdit_ = nullptr;

    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    scroll_->setWidget(content);

    GameObject* object = context_->scene().findObject(context_->selected());
    if (!object) {
        auto* hint = new QLabel("Select an object in the Hierarchy or the Scene.");
        hint->setEnabled(false);
        layout->addWidget(hint);
        layout->addStretch();
        return;
    }
    const ObjectId id = object->id;

    // --- Name: one Rename command when the user presses Enter or leaves the field.
    auto* form = new QFormLayout;
    layout->addLayout(form);
    nameEdit_ = new QLineEdit;
    form->addRow("Name", nameEdit_);
    connect(nameEdit_, &QLineEdit::editingFinished, this, [this, id] {
        const GameObject* current = context_->scene().findObject(id);
        std::string name = nameEdit_->text().toStdString();
        if (current && name != current->name) context_->execute(std::make_unique<RenameObjectCommand>(id, name));
    });
    updaters_.push_back([this, id] {
        const GameObject* current = context_->scene().findObject(id);
        if (current && !nameEdit_->hasFocus()) nameEdit_->setText(QString::fromStdString(current->name));
    });

    // --- Transform. Changes merge into one undo step until the field loses focus.
    auto* transform = new QGroupBox("Transform");
    auto* transformForm = new QFormLayout(transform);
    layout->addWidget(transform);
    positionSession_ = context_->newSession();
    const char* labels[3] = {"X", "Y", "Z"};
    for (int axis = 0; axis < 3; ++axis) {
        auto* spin = new QSpinBox;
        spin->setRange(-512, 512);
        transformForm->addRow(labels[axis], spin);
        connect(spin, &QSpinBox::valueChanged, this, [this, id, axis](int value) {
            const GameObject* current = context_->scene().findObject(id);
            if (!current) return;
            int xyz[3] = {current->x, current->y, current->z};
            xyz[axis] = value;
            context_->execute(std::make_unique<SetPositionCommand>(id, xyz[0], xyz[1], xyz[2], positionSession_));
        });
        connect(spin, &QSpinBox::editingFinished, this, [this] { positionSession_ = context_->newSession(); });
        updaters_.push_back([this, id, spin, axis] {
            const GameObject* current = context_->scene().findObject(id);
            if (!current) return;
            int xyz[3] = {current->x, current->y, current->z};
            // QSignalBlocker: changing the value from code must not look like a
            // user edit, or it would create a new command (and an endless loop).
            QSignalBlocker block(spin);
            if (spin->value() != xyz[axis]) spin->setValue(xyz[axis]);  // equal: leave the text cursor alone
        });
    }

    // --- Components: one group box per component, fields generated from the descriptor.
    for (const Component& component : object->components) {
        const ComponentDescriptor* descriptor = findComponentDescriptor(component.type);
        if (!descriptor) continue;
        const std::string type = component.type;
        auto* box = new QGroupBox(QString::fromStdString(descriptor->displayName));
        auto* boxLayout = new QVBoxLayout(box);
        auto* boxForm = new QFormLayout;
        boxLayout->addLayout(boxForm);
        layout->addWidget(box);

        for (int i = 0; i < static_cast<int>(descriptor->properties.size()); ++i) {
            const PropertyDescriptor& property = descriptor->properties[i];
            auto currentValue = [this, id, type, i]() -> std::int64_t {
                const GameObject* o = context_->scene().findObject(id);
                const Component* c = o ? o->findComponent(type) : nullptr;
                return c ? c->values[i] : 0;
            };
            QWidget* field = nullptr;
            if (property.kind == PropertyKind::Int) {
                auto* spin = new QSpinBox;
                spin->setRange(property.minValue, property.maxValue);
                connect(spin, &QSpinBox::valueChanged, this, [this, id, type, i](int value) {
                    context_->execute(std::make_unique<SetPropertyCommand>(id, type, i, value, propertySession_));
                });
                connect(spin, &QSpinBox::editingFinished, this, [this] { propertySession_ = context_->newSession(); });
                updaters_.push_back([spin, currentValue] {
                    QSignalBlocker block(spin);
                    int value = static_cast<int>(currentValue());
                    if (spin->value() != value) spin->setValue(value);
                });
                field = spin;
            } else {  // AssetRef: a drop-down of assets that also accepts drops
                auto* combo = new AssetDropCombo;
                combo->setAcceptDrops(true);
                combo->addItem("(none)", QVariant::fromValue<qlonglong>(0));
                for (const Asset& asset : context_->scene().assets)
                    combo->addItem(QIcon(context_->pixmapFor(asset.id)), QString::fromStdString(asset.name),
                                   QVariant::fromValue<qlonglong>(asset.id));
                auto setAsset = [this, id, type, i](AssetId asset) {
                    context_->execute(std::make_unique<SetPropertyCommand>(id, type, i, asset, context_->newSession()));
                };
                connect(combo, &QComboBox::activated, this,
                        [combo, setAsset](int index) { setAsset(combo->itemData(index).toLongLong()); });
                connect(combo, &AssetDropCombo::assetDropped, this, setAsset);
                updaters_.push_back([combo, currentValue] {
                    QSignalBlocker block(combo);
                    combo->setCurrentIndex(std::max(0, combo->findData(QVariant::fromValue<qlonglong>(currentValue()))));
                });
                field = combo;
            }
            field->setToolTip(QString::fromStdString(property.help));
            boxForm->addRow(QString::fromStdString(property.name), field);
        }

        auto* remove = new QPushButton("Remove " + QString::fromStdString(descriptor->displayName));
        connect(remove, &QPushButton::clicked, this,
                [this, id, type] { context_->execute(std::make_unique<RemoveComponentCommand>(id, type)); });
        boxLayout->addWidget(remove, 0, Qt::AlignRight);
    }

    // --- Add Component: a menu listing only the types this object doesn't have yet.
    auto* add = new QToolButton;
    add->setText("Add Component");
    add->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(add);
    for (const ComponentDescriptor& descriptor : componentRegistry()) {
        if (object->findComponent(descriptor.type)) continue;
        const std::string type = descriptor.type;
        menu->addAction(QString::fromStdString(descriptor.displayName), this,
                        [this, id, type] { context_->execute(std::make_unique<AddComponentCommand>(id, type)); });
    }
    add->setMenu(menu);
    add->setEnabled(!menu->isEmpty());
    layout->addWidget(add);
    layout->addStretch();
}
