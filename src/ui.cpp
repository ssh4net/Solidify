#include "ui.h"

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

DropArea::DropArea() {
    setAcceptDrops(true);
    setText("Drag & drop files here"); // Set text
    setAlignment(Qt::AlignCenter); // Set alignment to center

    QFont font = this->font();
    font.setPointSize(16);
    setFont(font);
}

void DropArea::dragEnterEvent(QDragEnterEvent* event) {
    event->acceptProposedAction();
}

void DropArea::dropEvent(QDropEvent* event)  {
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

MainWindow::MainWindow() {
    DropArea* dropArea = new DropArea;
    dropArea->setFixedSize(400, 400);  // Set the size of the drop area
    setCentralWidget(dropArea);

    setWindowFlags(Qt::WindowStaysOnTopHint);
    setWindowTitle("Solidify 1.22");

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

void MainWindow::restartApp() {
    QStringList arguments;
    QProcess::startDetached(QApplication::applicationFilePath(), arguments);

    QApplication::quit();
}