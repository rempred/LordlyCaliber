#pragma once

#include "engine/project.h"
#include <QDir>
#include <QFileInfo>

namespace lordly {

class ProjectSaveTarget {
public:
    explicit ProjectSaveTarget(const QString &path) : path_(path) {
        if (!QFileInfo(path).isDir()) {
            if (path.endsWith(".json", Qt::CaseInsensitive))
                format_ = Format::Json;
            else if (path.endsWith(".lcproject", Qt::CaseInsensitive))
                format_ = Format::Archive;
        }
    }

    QString filePath() const {
        return format_ == Format::Folder ? QDir(path_).filePath("project.json") : path_;
    }

    void save(const Project &project) const {
        switch (format_) {
        case Format::Folder: project.saveFolder(path_); break;
        case Format::Json: project.saveJsonFile(path_); break;
        case Format::Archive: project.saveArchive(path_); break;
        }
    }

private:
    enum class Format { Folder, Json, Archive };
    QString path_;
    Format format_ = Format::Folder;
};

} // namespace lordly
