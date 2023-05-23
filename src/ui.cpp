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

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>

#include "processing.h"

class DropArea : public QLabel {
public:
    DropArea() {
        setAcceptDrops(true);
        resize(400, 400);
        setMinimumSize(400, 400);
        setMaximumSize(400, 400);
        setWindowFlags(Qt::WindowStaysOnTopHint);
        setWindowTitle("Solidify 1.22");
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

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        DropArea* dropArea = new DropArea;
        setCentralWidget(dropArea);

        QMenuBar* menuBar = new QMenuBar;
        QMenu* f_menu = new QMenu("Files", menuBar);
        QMenu* r_menu = new QMenu("Reset", menuBar);
        QAction* f_Restart = new QAction("Restart", r_menu);
        QAction* f_Exit = new QAction("Exit", f_menu);
        r_menu->addAction(f_Restart);
        f_menu->addAction(f_Exit);
        menuBar->addMenu(f_menu);
        menuBar->addMenu(r_menu);
        setMenuBar(menuBar);

        // Connect the Exit action's triggered signal to QApplication's quit slot
        connect(f_Exit, &QAction::triggered, qApp, &QApplication::quit);

        // Connect the Restart action's triggered signal to a slot that restarts the app
        connect(f_Restart, &QAction::triggered, this, &MainWindow::restartApp);
    }

private slots:
    void restartApp() {
        QStringList arguments;
        QProcess::startDetached(QApplication::applicationFilePath(), arguments);

        QApplication::quit();
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    //DropArea dropArea;
    //dropArea.show();

    return app.exec();
}