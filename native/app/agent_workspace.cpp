#include "app/agent_workspace.h"
#include "app/main_window.h"
#include "app/theme.h"
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSemaphore>
#include <QSlider>
#include <QSpinBox>
#include <QTabBar>
#include <QTextEdit>
#include <QTimer>
#include <QTreeView>
#include <QWheelEvent>
#include <iostream>
#include <thread>

namespace lordly {
namespace {
void send(const QJsonObject &o) {
    std::cout << QJsonDocument(o).toJson(QJsonDocument::Compact).constData() << std::endl;
}
class Workspace {
  public:
    explicit Workspace(MainWindow &window) : window_(window) {}
    QJsonValue handle(const QJsonObject &command) {
        const auto op = command["op"].toString();
        auto root = QApplication::activeModalWidget();
        if (!root)
            root = QApplication::activePopupWidget();
        if (!root)
            root = &window_;
        if (op == "controls") {
            QJsonArray out;
            const auto query = command["query"].toString();
            auto consider = [&](QObject *o) {
                auto w = qobject_cast<QWidget *>(o);
                auto action = qobject_cast<QAction *>(o);
                if ((!w || !w->isVisible()) && (!action || !action->isVisible()))
                    return;
                auto info = describe(o);
                if (query.isEmpty() ||
                    QString::fromUtf8(QJsonDocument(info).toJson(QJsonDocument::Compact))
                        .contains(query, Qt::CaseInsensitive))
                    out.append(info);
            };
            consider(root);
            for (auto *o : root->findChildren<QObject *>())
                consider(o);
            return QJsonObject{
                {"window", root->windowTitle()}, {"root", id(root)}, {"controls", out}};
        }
        if (op == "clipboard") {
            if (command["text"].isString())
                QApplication::clipboard()->setText(command["text"].toString());
            return QApplication::clipboard()->text();
        }
        if (op == "navigate") {
            if (root != &window_)
                throw Error("Finish the open dialog before navigating");
            const auto loc = command["location"].toObject();
            if (loc["editor"].toString().isEmpty())
                throw Error("Expected an editor location");
            window_.navigateTo({loc["editor"].toString(), loc["id"].toInt(-1),
                                loc["child"].toInt(-1), loc["key"].toString()});
            return true;
        }
        if (op == "save") {
            if (root != &window_)
                throw Error("Finish the open dialog before saving");
            const auto path = command["path"].toString();
            if (path.isEmpty())
                throw Error("save needs a destination folder or .lcproject archive");
            if (!window_.saveProjectTo(path, path.endsWith(".lcproject", Qt::CaseInsensitive)))
                throw Error("The editor did not save the project");
            return path;
        }
        QObject *obj = root;
        if (command.contains("id")) {
            auto i = command["id"].toInt(-1);
            obj = i >= 0 && i < objects_.size() ? objects_[i].data() : nullptr;
        } else if (command.contains("name")) {
            const auto name = command["name"].toString();
            obj = nullptr;
            for (auto *candidate : root->findChildren<QObject *>(name)) {
                auto w = qobject_cast<QWidget *>(candidate);
                if (w && !w->isVisible())
                    continue;
                if (obj)
                    throw Error("Ambiguous control name. Use an id from controls.");
                obj = candidate;
            }
        }
        if (!obj)
            throw Error("Control is absent or has been destroyed");
        auto w = qobject_cast<QWidget *>(obj);
        Qt::KeyboardModifiers modifiers;
        for (const auto &v : command["modifiers"].toArray()) {
            const auto key = v.toString().toLower();
            if (key == "shift")
                modifiers |= Qt::ShiftModifier;
            else if (key == "ctrl")
                modifiers |= Qt::ControlModifier;
            else if (key == "alt")
                modifiers |= Qt::AltModifier;
            else if (key == "meta")
                modifiers |= Qt::MetaModifier;
            else
                throw Error("Unknown keyboard modifier: " + key);
        }
        if (root != &window_ && obj != root && !root->findChildren<QObject *>().contains(obj))
            throw Error("An open dialog or popup owns the current input");
        auto view = qobject_cast<QAbstractItemView *>(obj);
        auto combo = qobject_cast<QComboBox *>(obj);
        auto tabs = qobject_cast<QTabBar *>(obj);
        auto button = qobject_cast<QAbstractButton *>(obj);
        auto action = qobject_cast<QAction *>(obj);
        if (op == "inspect")
            return describe(obj);
        if (op == "image" && w) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!w->grab().save(&buffer, "PNG"))
                throw Error("Could not render the control");
            return QJsonObject{{"mime", "image/png"},
                               {"base64", QString::fromLatin1(bytes.toBase64())}};
        }
        if (op == "items" && (combo || tabs || view)) {
            if (view && !view->model())
                throw Error("This view has no model");
            const auto parent =
                view ? modelIndex(view, command["parent"].toArray()) : QModelIndex{};
            const int count = combo  ? combo->count()
                              : tabs ? tabs->count()
                                     : view->model()->rowCount(parent);
            const int start = command["start"].toInt(0), limit = command["limit"].toInt(100);
            if (start < 0 || limit < 1 || limit > 4096)
                throw Error("Invalid items page");
            QJsonArray items;
            for (int i = start; i < count && i - start < limit; ++i) {
                if (combo)
                    items.append(combo->itemText(i));
                else if (tabs)
                    items.append(tabs->tabText(i));
                else {
                    QJsonArray cells;
                    for (int col = 0; col < view->model()->columnCount(parent); ++col)
                        cells.append(view->model()->index(i, col, parent).data().toString());
                    items.append(cells);
                }
            }
            QJsonArray headers;
            if (view)
                for (int col = 0; col < view->model()->columnCount(parent); ++col)
                    headers.append(view->model()->headerData(col, Qt::Horizontal).toString());
            return QJsonObject{
                {"count", count}, {"start", start}, {"items", items}, {"headers", headers}};
        }
        if ((w && (!w->isEnabled() || !w->isVisible())) || (action && !action->isEnabled()))
            throw Error("Control is currently unavailable");
        if (op == "activate" && action)
            action->trigger();
        else if (op == "activate" && button)
            button->click();
        else if (op == "activate" && qobject_cast<QMenu *>(obj))
            qobject_cast<QMenu *>(obj)->popup(w->mapToGlobal(QPoint(0, 0)));
        else if (op == "select" && (combo || tabs)) {
            const int count = combo ? combo->count() : tabs->count();
            int n = command["index"].toInt(-1);
            if (command["text"].isString()) {
                n = -1;
                for (int i = 0; i < count; ++i)
                    if ((combo ? combo->itemText(i) : tabs->tabText(i)) ==
                        command["text"].toString()) {
                        n = i;
                        break;
                    }
            }
            if (n < 0 || n >= count || (tabs && !tabs->isTabEnabled(n)))
                throw Error("Unavailable item");
            if (combo) {
                if (!(combo->model()->flags(
                          combo->model()->index(n, combo->modelColumn(), combo->rootModelIndex())) &
                      Qt::ItemIsEnabled))
                    throw Error("This choice is disabled");
                QPointer<QComboBox> guard(combo);
                combo->setCurrentIndex(n);
                if (guard)
                    emit guard->activated(n);
            } else
                tabs->setCurrentIndex(n);
        } else if ((op == "select" || op == "expand" || op == "edit") && view) {
            const auto index =
                modelIndex(view, command["index"].toArray(), command["column"].toInt());
            if (!index.isValid())
                throw Error("Select a valid row path");
            if (op == "expand") {
                auto tree = qobject_cast<QTreeView *>(view);
                if (!tree)
                    throw Error("Expected a tree");
                tree->setExpanded(index, command["expanded"].toBool(true));
            } else if (op == "edit") {
                if (!(index.flags() & Qt::ItemIsEditable) || !(index.flags() & Qt::ItemIsEnabled) ||
                    !view->model()->setData(index, command["value"].toVariant(), Qt::EditRole))
                    throw Error("Cell rejected edit");
            } else {
                view->setCurrentIndex(index);
                view->scrollTo(index);
            }
        } else if (op == "set" && w) {
            const auto value = command["value"];
            if (auto input = qobject_cast<QLineEdit *>(w)) {
                if (input->isReadOnly() || !value.isString())
                    throw Error("Expected editable text");
                input->setFocus();
                input->selectAll();
                input->insert(value.toString());
            } else if (auto text = qobject_cast<QTextEdit *>(w)) {
                if (text->isReadOnly() || !value.isString())
                    throw Error("Expected editable text");
                text->setPlainText(value.toString());
            } else if (auto plain = qobject_cast<QPlainTextEdit *>(w)) {
                if (plain->isReadOnly() || !value.isString())
                    throw Error("Expected editable text");
                plain->setPlainText(value.toString());
            } else if (auto spin = qobject_cast<QSpinBox *>(w)) {
                if (!value.isDouble() || value.toDouble() != value.toInt() ||
                    value.toInt() < spin->minimum() || value.toInt() > spin->maximum())
                    throw Error("Integer outside the control range");
                spin->setValue(value.toInt());
            } else if (auto decimal = qobject_cast<QDoubleSpinBox *>(w)) {
                if (!value.isDouble() || value.toDouble() < decimal->minimum() ||
                    value.toDouble() > decimal->maximum())
                    throw Error("Number outside the control range");
                decimal->setValue(value.toDouble());
            } else if (auto slider = qobject_cast<QAbstractSlider *>(w)) {
                if (!value.isDouble() || value.toDouble() != value.toInt() ||
                    value.toInt() < slider->minimum() || value.toInt() > slider->maximum())
                    throw Error("Integer outside the slider range");
                slider->setValue(value.toInt());
            } else if (button && button->isCheckable()) {
                if (!value.isBool())
                    throw Error("Expected a boolean value");
                if (value.toBool() != button->isChecked())
                    button->click();
            } else
                throw Error("Use items/select, key or pointer for this control");
        } else if (op == "file" && qobject_cast<QFileDialog *>(obj)) {
            auto dialog = qobject_cast<QFileDialog *>(obj);
            dialog->selectFile(command["path"].toString());
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        } else if (op == "reject" && qobject_cast<QDialog *>(obj))
            qobject_cast<QDialog *>(obj)->reject();
        else if (op == "focus" && w)
            w->setFocus();
        else if (op == "key" && w) {
            const QKeySequence seq(command["key"].toString(), QKeySequence::PortableText);
            if (seq.isEmpty() || seq.count() != 1)
                throw Error("Supply one portable key combination");
            auto *target = obj == root && root->focusWidget() ? root->focusWidget() : w;
            QPointer<QWidget> targetGuard(target);
            for (auto type : {QEvent::KeyPress, QEvent::KeyRelease}) {
                if (!targetGuard)
                    break;
                QKeyEvent e(type, seq[0].key(), seq[0].keyboardModifiers(),
                            command["text"].toString());
                QApplication::sendEvent(targetGuard, &e);
            }
        } else if (op == "wheel" && w) {
            const QPoint at(command["x"].toInt(), command["y"].toInt());
            QWheelEvent e(QPointF(at), QPointF(w->mapToGlobal(at)), {},
                          QPoint(command["dx"].toInt(), command["dy"].toInt()), held_, modifiers,
                          Qt::NoScrollPhase, false);
            QApplication::sendEvent(w, &e);
        } else if (op == "drop" && w) {
            QMimeData mime;
            QList<QUrl> urls;
            for (const auto &f : command["paths"].toArray())
                urls.append(QUrl::fromLocalFile(f.toString()));
            if (!urls.isEmpty())
                mime.setUrls(urls);
            if (command.contains("text"))
                mime.setText(command["text"].toString());
            if (command.contains("mime"))
                mime.setData(command["mime"].toString(),
                             QByteArray::fromBase64(command["base64"].toString().toLatin1()));
            const QPoint at(command["x"].toInt(), command["y"].toInt());
            QDragEnterEvent enter(QPointF(at), Qt::CopyAction, &mime, Qt::LeftButton, modifiers);
            QApplication::sendEvent(w, &enter);
            if (!enter.isAccepted())
                throw Error("The control rejected the drag");
            QDropEvent drop(QPointF(at), Qt::CopyAction, &mime, Qt::LeftButton, modifiers);
            QApplication::sendEvent(w, &drop);
            if (!drop.isAccepted())
                throw Error("The control rejected the drop");
        } else if (op == "pointer" && w) {
            const QPoint at(command["x"].toInt(), command["y"].toInt());
            const auto event = command["event"].toString();
            const auto buttonName = command["button"].toString("left");
            auto b = buttonName == "right"    ? Qt::RightButton
                     : buttonName == "middle" ? Qt::MiddleButton
                                              : Qt::LeftButton;
            auto type = event == "press"     ? QEvent::MouseButtonPress
                        : event == "release" ? QEvent::MouseButtonRelease
                        : event == "move"    ? QEvent::MouseMove
                        : event == "double"  ? QEvent::MouseButtonDblClick
                                             : QEvent::None;
            if (event == "context") {
                QContextMenuEvent e(QContextMenuEvent::Mouse, at, w->mapToGlobal(at));
                QApplication::sendEvent(w, &e);
            } else {
                if (type == QEvent::None)
                    throw Error("Expected press, release, move, double or context");
                if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick)
                    held_ |= b;
                if (type == QEvent::MouseButtonRelease)
                    held_ &= ~b;
                QMouseEvent e(type, QPointF(at), QPointF(w->mapToGlobal(at)),
                              type == QEvent::MouseMove ? Qt::NoButton : b, held_, modifiers);
                QApplication::sendEvent(w, &e);
            }
        } else
            throw Error("Unsupported workspace operation for this control: " + op);
        return true;
    }

  private:
    int id(QObject *o) {
        auto i = ids_.value(o, -1);
        if (i >= 0 && objects_[i] == o)
            return i;
        i = int(objects_.size());
        objects_.append(o);
        ids_.insert(o, i);
        return i;
    }
    QJsonObject describe(QObject *o) {
        QStringList verbs{"inspect"};
        QJsonObject d{
            {"id", id(o)}, {"type", o->metaObject()->className()}, {"name", o->objectName()}};
        if (auto w = qobject_cast<QWidget *>(o)) {
            if (auto a = QAccessible::queryAccessibleInterface(w))
                d["label"] = a->text(QAccessible::Name);
            d["enabled"] = w->isEnabled();
            d["width"] = w->width();
            d["height"] = w->height();
            const auto pos = w->mapTo(w->window(), QPoint(0, 0));
            d["x"] = pos.x();
            d["y"] = pos.y();
            if (w->parentWidget())
                d["parent"] = id(w->parentWidget());
            if (!w->toolTip().isEmpty())
                d["tooltip"] = w->toolTip();
            if (auto l = qobject_cast<QLabel *>(w))
                d["text"] = l->text();
            else if (auto b = qobject_cast<QAbstractButton *>(w)) {
                d["text"] = b->text();
                d["checked"] = b->isChecked();
            } else if (auto c = qobject_cast<QComboBox *>(w))
                d["text"] = c->currentText();
            else if (auto line = qobject_cast<QLineEdit *>(w))
                d["text"] = line->text();
            else if (auto text = qobject_cast<QTextEdit *>(w))
                d["text"] = text->toPlainText();
            else if (auto plain = qobject_cast<QPlainTextEdit *>(w))
                d["text"] = plain->toPlainText();
            else if (auto tabs = qobject_cast<QTabBar *>(w))
                d["text"] = tabs->tabText(tabs->currentIndex());
            else if (auto s = qobject_cast<QAbstractSpinBox *>(w))
                d["text"] = s->text();
        } else if (auto a = qobject_cast<QAction *>(o)) {
            d["text"] = a->text();
            d["enabled"] = a->isEnabled();
            d["shortcut"] = a->shortcut().toString();
        }
        if (qobject_cast<QWidget *>(o))
            verbs << "image" << "focus" << "pointer" << "wheel" << "key" << "drop";
        if (qobject_cast<QAction *>(o) || qobject_cast<QAbstractButton *>(o))
            verbs << "activate";
        if (qobject_cast<QComboBox *>(o) || qobject_cast<QTabBar *>(o) ||
            qobject_cast<QAbstractItemView *>(o))
            verbs << "items" << "select";
        if (qobject_cast<QTreeView *>(o))
            verbs << "expand";
        if (qobject_cast<QAbstractItemView *>(o))
            verbs << "edit";
        if (qobject_cast<QFileDialog *>(o))
            verbs << "file";
        if (qobject_cast<QDialog *>(o))
            verbs << "reject";
        auto button = qobject_cast<QAbstractButton *>(o);
        if (qobject_cast<QLineEdit *>(o) || qobject_cast<QTextEdit *>(o) ||
            qobject_cast<QPlainTextEdit *>(o) || qobject_cast<QAbstractSpinBox *>(o) ||
            qobject_cast<QAbstractSlider *>(o) || (button && button->isCheckable()))
            verbs << "set";
        d["operations"] = QJsonArray::fromStringList(verbs);
        return d;
    }
    static QModelIndex modelIndex(QAbstractItemView *view, const QJsonArray &path, int column = 0) {
        if (!view->model())
            throw Error("Control has no model");
        QModelIndex index;
        for (int i = 0; i < path.size(); ++i) {
            index =
                view->model()->index(path[i].toInt(-1), i + 1 == path.size() ? column : 0, index);
            if (!index.isValid())
                throw Error("Model index outside this view");
        }
        return index;
    }
    MainWindow &window_;
    QVector<QPointer<QObject>> objects_;
    QHash<QObject *, int> ids_;
    Qt::MouseButtons held_;
};
} // namespace
int runAgentWorkspace(QApplication &app, Project project, const QString &input) {
    applyEditorTheme(app);
    app.setQuitOnLastWindowClosed(false);
    QFile script;
    if (input != "-") {
        script.setFileName(input);
        if (!script.open(QIODevice::ReadOnly))
            throw Error("Cannot open workspace command file");
    }
    MainWindow window;
    window.installProject(std::move(project));
    window.show();
    Workspace workspace(window);
    send({{"ready", true}, {"protocol", 1}});
    QSemaphore replied;
    bool failed = false;
    // The reader waits for one reply at a time. Qt remains on its owning thread,
    // including when a clicked button enters a dialog's nested event loop.
    std::thread reader([&] {
        for (;;) {
            QByteArray line;
            if (input == "-") {
                std::string s;
                if (!std::getline(std::cin, s))
                    break;
                line = QByteArray::fromStdString(s);
            } else {
                if (script.atEnd())
                    break;
                line = script.readLine();
            }
            if (line.trimmed().isEmpty())
                continue;
            QMetaObject::invokeMethod(
                &window,
                [&, line] {
                    auto reply = std::make_shared<QJsonObject>();
                    QTimer::singleShot(0, &window, [&, reply] {
                        if (reply->value("ok").toBool() && !reply->contains("result")) {
                            (*reply)["state"] = "awaitingDesktopInteraction";
                            if (auto modal = QApplication::activeModalWidget())
                                (*reply)["window"] = modal->windowTitle();
                        }
                        failed = !reply->value("ok").toBool();
                        send(*reply);
                        replied.release();
                    });
                    try {
                        auto doc = QJsonDocument::fromJson(line);
                        if (!doc.isObject())
                            throw Error("Expected one JSON command object per line");
                        (*reply)["ok"] = true;
                        (*reply)["result"] = workspace.handle(doc.object());
                    } catch (const std::exception &e) {
                        (*reply) = {{"ok", false}, {"error", QString::fromUtf8(e.what())}};
                    }
                },
                Qt::QueuedConnection);
            replied.acquire();
            if (failed && input != "-")
                break;
        }
        QMetaObject::invokeMethod(
            &app, [&] { QCoreApplication::exit(failed ? 1 : 0); }, Qt::QueuedConnection);
    });
    const int code = app.exec();
    reader.join();
    return code;
}
} // namespace lordly
