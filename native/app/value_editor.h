#pragma once
#include "engine/project.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <functional>
namespace lordly {
// Numeric storage remains visible alongside human-readable choices, including
// unknown values retained from a modified source ROM.
class ValueEditor : public QWidget {
  public:
    ValueEditor(const Field &field, const QJsonObject &catalog, std::function<void(int)> edited,
                QWidget *parent = nullptr)
        : QWidget(parent) {
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        spin_ = new QSpinBox;
        spin_->setRange(field.minimum(), field.maximum());
        spin_->setKeyboardTracking(false);
        layout->addWidget(spin_);
        auto labels = catalog[field.choices].toObject();
        if (!labels.isEmpty()) {
            choice_ = new QComboBox;
            choice_->setMinimumContentsLength(12);
            choice_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            QMap<int, QString> sorted;
            for (auto it = labels.begin(); it != labels.end(); ++it) {
                bool ok = false;
                auto id = it.key().toInt(&ok);
                if (ok && it.value().isString() && id >= field.minimum() && id <= field.maximum())
                    sorted[id] = it.value().toString();
            }
            for (auto it = sorted.begin(); it != sorted.end(); ++it)
                choice_->addItem(it.value(), it.key());
            layout->addWidget(choice_, 1);
            connect(choice_, &QComboBox::currentIndexChanged, this, [this] {
                if (choice_->currentIndex() >= 0)
                    spin_->setValue(choice_->currentData().toInt());
            });
        }
        connect(spin_, &QSpinBox::valueChanged, this, [this, edited](int value) {
            syncChoice(value);
            edited(value);
        });
    }
    void setValue(int value) {
        QSignalBlocker block(spin_);
        spin_->setValue(value);
        syncChoice(value);
    }

  private:
    void syncChoice(int value) {
        if (!choice_)
            return;
        QSignalBlocker block(choice_);
        auto index = choice_->findData(value);
        if (index < 0) {
            choice_->addItem(QString("Unmapped value %1").arg(value), value);
            index = choice_->count() - 1;
        }
        choice_->setCurrentIndex(index);
    }
    QSpinBox *spin_;
    QComboBox *choice_ = nullptr;
};
} // namespace lordly
