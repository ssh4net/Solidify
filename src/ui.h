#pragma once

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
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QRandomGenerator>

#include "processing.h"

class DropArea : public QLabel {
    Q_OBJECT  // Macro needed to handle signals and slots

public:
    DropArea();

signals:
    void filesDropped(QList<QUrl> urls);  // New signal to be emitted when files are dropped

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};

class MainWindow : public QMainWindow {
    Q_OBJECT  // Macro needed to handle signals and slots
public:
    MainWindow();
    void emitUpdateTextSignal(const QString& text) { emit updateTextSignal(text); }  // Public method to emit the signal

signals:
    void updateTextSignal(const QString& text);

private slots:
    void restartApp();
    void toggleConsole(bool cheked);
    void startProcessing(QList<QUrl> urls);  // New slot

private:
    QFutureWatcher<bool> processingWatcher;
    QProgressBar* progressBar;
};

void setPBarColor(QProgressBar* progressBar, const QColor& color = QColor("#05B8CC"));