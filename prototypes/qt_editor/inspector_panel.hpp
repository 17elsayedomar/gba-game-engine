#pragma once
// =============================================================================
// InspectorPanel — shows and edits the selected object, built from the
// component descriptors (proto_core/components.hpp).
//
// The retained-mode challenge: widgets keep their own copy of values. After an
// edit, undo or drag we must push the model's values back into the widgets,
// WITHOUT destroying the field the user is typing in (that would steal keyboard
// focus mid-edit). So refresh() does one of two things:
//   * same object and same components as last time -> just update the values;
//   * anything structural changed -> rebuild all widgets from the descriptors.
// =============================================================================

#include "proto_core/model.hpp"

#include <QComboBox>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class EditorContext;
class QLineEdit;
class QScrollArea;
class QSpinBox;

// A drop-down list of assets that also accepts an asset dragged onto it.
class AssetDropCombo : public QComboBox {
    Q_OBJECT
public:
    using QComboBox::QComboBox;
signals:
    void assetDropped(proto::AssetId asset);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(EditorContext* context, QWidget* parent = nullptr);
    void refresh();

private:
    void rebuild();
    std::string structureKey() const;  // describes which widgets are needed right now

    EditorContext* context_;
    QScrollArea* scroll_;
    std::string builtKey_;  // structureKey() of the widgets currently shown

    // Widgets of the current build, plus how to update each from the model.
    QLineEdit* nameEdit_ = nullptr;
    std::vector<std::function<void()>> updaters_;
    int positionSession_ = 0;
    int propertySession_ = 0;
};
