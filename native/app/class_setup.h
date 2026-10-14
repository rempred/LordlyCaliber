#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
class QLabel;
namespace lordly {
class ClassSetup : public QWidget {
  public:
    ClassSetup(Project *, std::function<void(const EditLocation &)> navigate, QWidget *parent = nullptr);
    void selectClass(int);
  private:
    Project *project_;
    int id_ = -1;
    QLabel *title_, *portrait_, *stats_, *equipment_, *actions_, *family_, *checklist_;
};
}
