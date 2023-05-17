/*
 * Solidify (Push Pull) algorithm implementation using OpenImageIO
 * Copyright (c) 2022 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>

#include "processing.h"

class DropArea : public QLabel {
public:
    DropArea() {
        setAcceptDrops(true);
        resize(400, 400);
        setMinimumSize(400, 400);
        setMaximumSize(400, 400);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowTitle("Solidify 1.21");
        setText("Drag & drop files here"); // Set text
        setAlignment(Qt::AlignCenter); // Set alignment to center

        QFont font = this->font();
        font.setPointSize(16);
        setFont(font);
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        event->acceptProposedAction();
    }

    void dropEvent(QDropEvent* event) override {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.isEmpty()) {
            return;
        }

        //QString fileName = urls.first().toLocalFile();
        //if (fileName.isEmpty()) {
        //    return;
        //}

        // Processing

        bool success = doProcessing(urls);

        qDebug() << "Done!";

        return;
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    DropArea dropArea;
    dropArea.show();

    return app.exec();
}