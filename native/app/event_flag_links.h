#pragma once
#include <functional>
class QWidget;
namespace lordly {
class Project;
QWidget *eventFlagLinks(Project *, int row, int flag,
                       std::function<void(int, int)> open, QWidget *parent = nullptr);
}
