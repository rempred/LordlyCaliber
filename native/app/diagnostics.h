#pragma once
#include <QString>
#include <QStringList>
namespace lordly {
class Diagnostics {
  public:
    explicit Diagnostics(QString folder = {});
    ~Diagnostics();
    static QString folder();
    static QStringList crashReports();
  private:
    bool installed_ = false;
};
}
