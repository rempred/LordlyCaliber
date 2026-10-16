#pragma once
#include "engine/actor_art.h"
#include <functional>
#include <optional>
#include <QJsonObject>
class QWidget;
namespace lordly {
std::optional<QByteArray>
editFrameLayers(const QByteArray &frame, const QByteArray &newLayer, int artCount,
                const std::function<QVector<ActorFrameLayer>(const QByteArray &)> &render,
                QWidget *parent,
                const std::function<QJsonObject(const QByteArray &)> &recover = {});
}
