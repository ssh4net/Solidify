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

#define VERSION_MAJOR 1
#define VERSION_MINOR 31

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
    void startProcessing(QList<QUrl> urls);
    void normSettings();
    void sldfSettings(bool checked);
    void rngSettings();
    void frmtSettings();
    void bitSettings();
    void alfSettings(bool checked);
    void rawSettings();

private:
    QFutureWatcher<bool> processingWatcher;
    QProgressBar* progressBar;

    QAction* sld_enable;
    QAction* alf_enable;
    QAction* con_enable;

    QAction* nrm_Dis;
    QAction* nrm_Smrt;
    QAction* nrm_Force;
    QAction* rng_Unsg;
    QAction* rng_Sign;
    QAction* rng_SU;
    QAction* rng_US;

    QAction* frmt_Org; // Original
    QAction* frmt_Png; // PNG
    QAction* frmt_Jpg; // JPG
    QAction* frmt_Tif; // TIFF
    QAction* frmt_Exr; // OpenEXR
    QAction* frmt_Jp2; // JPEG 2000
    QAction* frmt_Ppm; // PPM

    QAction* bit_orig;
    QAction* bit_uint8;
    QAction* bit_uint16;
    QAction* bit_uint32;
    QAction* bit_uint64;
    QAction* bit_flt16;
    QAction* bit_flt32;
    QAction* bit_flt64;

    QAction* raw_rot_A;
    QAction* raw_rot_0;
    QAction* raw_rot_90;
    QAction* raw_rot_180;
    QAction* raw_rot_270;
};

void setPBarColor(QProgressBar* progressBar, const QColor& color = QColor("#05B8CC"));